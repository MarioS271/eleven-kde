# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`eleven-kde` is a Qt 6 widget style plugin (WinUI 3 / Windows 11 look) for KDE Plasma, plus an optional QML overlay for
Kirigami controls. The README states that the project is AI-written ("vibecoded"); keep that statement accurate. There
is no test suite and no linter.

## Commands

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # needs qt6-base-dev AND qt6-base-private-dev
cmake --build build                                        # produces build/plugins/styles/eleven-kde.so
tools/make-qml-overlay.sh                                  # (re)builds build/qml-overlay from the system module

QT_PLUGIN_PATH=$PWD/build/plugins dolphin -style eleven-kde
QML_IMPORT_PATH=$PWD/build/qml-overlay QT_PLUGIN_PATH=$PWD/build/plugins systemsettings -style eleven-kde
```

Single-instance apps (Prism Launcher, System Settings) ignore `-style` and just raise the running window, so the old
instance must be closed first. Rebuild after every source change; running apps keep the old plugin loaded.

Verifying without the desktop (preferred over screenshots):

* `GALLERY_OUT=/tmp/x QT_PLUGIN_PATH=build/plugins build/gallery -style eleven-kde` renders a widget window, a menu and
  combo popups to `/tmp/x_*.png` via `QWidget::grab`. `GALLERY_HOVER=1` fakes hover on a few buttons.
* `build/qmltest` (env vars in `tools/qmltest.cpp`) shows QML controls with the overlay.
* To reproduce a real app in isolation: run it with `XDG_CONFIG_HOME=<scratch>/cfg` (copy `~/.config/kdeglobals` there,
  set e.g. `dolphinrc [General] EditableUrl=true`) and, if it must not talk to the user's session, a private
  `dbus-launch` bus. Capture with `spectacle -b -n -a` (active window only). Never capture the full screen, it
  includes the user's other windows. Do not use `pkill -f` with a pattern that also appears in your own command line.
* Temporary debugging that worked well: env-gated `qWarning` in `drawPrimitive`/`drawControl`/`sizeFromContents`/`polish`
  filtered by `widget->inherits("SomeClass")` to find out which element/widget paints something. Remove it afterwards.

## Architecture

`src/elevenstyle.cpp` (one ~3000 line file) holds everything. `ElevenStyle` is a `QProxyStyle` whose base style is
Breeze (`QStyleFactory::create("breeze")`, Fusion as fallback). It was created by porting Qt's `QWindows11Style` from
the **v6.10.2 tag** of qtbase (the `dev` branch differs a lot and uses APIs missing in 6.10). That is why the file still
follows Qt's structure (`drawPrimitive`/`drawControl`/`drawComplexControl`/`subControlRect`/`sizeFromContents`/
`pixelMetric`/`polish`) and many comments are Qt's. Anything the port does not handle falls through to
`QProxyStyle::...` and therefore to Breeze.

Things that need several files or outside knowledge to understand:

* **Colors** are `WINUI3Colors[colorSchemeIndex][token]` translucent overlays (index 0 light, 1 dark, derived from
  `QStyleHints::colorScheme()` or palette lightness in `updateColorScheme`). The base colors come from the Plasma
  palette. The WinUI alphas are too faint on Plasma's flat backgrounds, so dark-mode values are overridden in
  `controlFillBrush`, `inputFillBrush` and the dark token table. Do not return `palette.button()`/palette brushes when
  `isBrushSet` is true (Qt's original does, Plasma sets every role, so it turns everything into scheme colors).
* **Design rules from the owner:** highlights, hover and "checked" states are gray, never accent blue. Accent stays only for
  check boxes/radios, sliders, progress bars, the list selection marker and the focus underline of text fields.
  Buttons have no border; text fields and combo boxes have no frame (the KUrlNavigator location bar is the one
  exception, via `inUrlNavigator`).
* **Breeze underneath causes surprises.** Breeze's `polish(QWidget)` installs event filters that paint on their own
  (combo popup container frame, scroll area frames). Breeze's `drawControl` draws its own highlights (menu bar items).
  When something blue/outlined survives, look for a Breeze path: either draw the element yourself instead of proxying, or
  paint over it (see the `QComboBoxPrivateContainer` handling in `CE_ShapedFrame`).
* **Apps that paint themselves.** The style only sees the primitives an app calls. Class-name keyed workarounds:
  `KFilePlacesView` (Dolphin sidebar: paints via `PE_PanelItemViewItem` only, icon fixed at `rect.left()+4`, row height
  from `CT_ItemViewItem`), `KUrlNavigator` (location bar; editable combo, `PE_PanelButtonCommand` flat for its buttons),
  `QComboBoxPrivateContainer`/`QComboBoxListView` (delegate swapped to `QStyledItemDelegate`), Prism's `InstanceView`
  (only the palette can be changed) and `LabeledToolButton` (composite buttons must keep Breeze's size, see
  `CT_ToolButton`). Dolphin 25.12 draws file view selection from `QPalette::Accent` itself; that cannot be styled.
* **KDE custom style elements:** `KCapacityBar` asks the style for `"CE_CapacityBar"` through `styleHint(0xff000001)`
  and only does so if the style has `Q_CLASSINFO("X-KDE-CustomElements")`; `drawControl` answers with our own element id.
* **QML/Kirigami is out of reach of the style.** Its colors come from the KDE color scheme, not `QPalette`, so palette
  overrides do nothing there. `qml/org/...` mirrors the module paths of the QML files it replaces (`org.kde.desktop` incl. `private`,
  `org.kde.plasma.components`, `org.kde.kirigami.controls`; see the table in the README). The overlay works because
  `QML_IMPORT_PATH` beats the system path and `make-qml-overlay.sh` copies those whole system modules and strips the
  `prefer` lines of their `qmldir` files (otherwise the compiled-in QML wins). Some lists take their look from the Plasma
  theme SVGs (`widgets/listitem`), which is why the Plasma components background is replaced by a QML rectangle.
  `QMLTEST_OUT=x.png build/qmltest` renders a QML window with popups to a PNG. QQC2 CheckBox/RadioButton are `StyleItem`s that do call the
  style, so `PE_IndicatorCheckBox` sizing (`PM_IndicatorWidth` 18, box 16) matters for clipping there.
* **Qt private API:** the plugin links private symbols (`QStyleAnimation`, `QCachedPainter`, `QStyleHelper`) and
  includes private headers, so it is tied to the exact Qt build (6.10.2). Helpers copied from `QCommonStyle`
  (text elision, view item text) live at the end of `elevenstyle.cpp` because `QCommonStylePrivate` is not usable.
* Glyphs (check, chevrons, carets) are drawn as vectors in `drawGlyph`; the Segoe icon font is never used. Monochrome
  dark icons in buttons are recolored by `adaptIcon` (plain Qt icon themes, e.g. Prism, do not recolor symbolic icons).

## Conventions

* Commits: author is Claude, no `Co-Authored-By` trailer (owner's request). Nothing is pushed without being asked.
* Docs and README: no em dashes; Markdown tables with padded, aligned columns.
* License is LGPL-3.0-only (Qt-derived files keep Qt's LGPL/GPL choice, QML files keep KDE's). New files need an SPDX
  header as in the existing ones. Names of Microsoft/Qt/KDE marks must stay descriptive only.
* The owner tests visually by running the apps; report what was measured or rendered versus what was not looked at.
