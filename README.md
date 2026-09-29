# eleven-kde

A Windows 11 (WinUI 3) inspired style for Qt 6 widgets on KDE Plasma: rounded controls, translucent gray fills,
gray hover highlights, thin accent underline on text fields, no blue focus rectangles.

> **This project is vibecoded.** It was written almost entirely by an AI (Claude, by Anthropic) in an interactive
> chat session, steered by a human who described what should look different and tested the result by eye. Nobody has
> audited the code line by line, there is no test suite, and it was only looked at in a handful of applications
> (see "Status"). It also depends on Qt private APIs. Use it at your own risk and expect rough edges.

## What it is (and how it works)

`eleven-kde` is a `QProxyStyle` that wraps Breeze. It is **not** a from-scratch style and **not** a Plasma global
theme (no colors, icons, cursors, window decoration or Plasma panel styling):

* The drawing code is a port of Qt's own `QWindows11Style` (`qtbase`, `src/plugins/styles/modernwindows`), which
  implements the WinUI 3 look for Windows 11. That style depends on Windows APIs and is not available in Ubuntu's Qt build, so it
  was adapted here to run on Linux.
* Everything the port does not draw, and all behavior (metrics for KDE integration, palettes, shadows), comes from
  Breeze underneath.
* Colors come from your Plasma color scheme. The WinUI "Fluent" tokens are used as translucent overlays on top of it.
* Some KDE applications draw parts of their UI themselves. For a few of them there are small workarounds keyed on
  class names (Dolphin's places panel and location bar, KDE's capacity bar, Prism Launcher's instance list, the icon
  buttons of Prism). These will break or become unnecessary when those applications change.

## Status

Looked at (and adjusted for): Dolphin (including the properties dialog and the location bar), Okular, System
Settings, Prism Launcher, and the two demo programs in `tools/`. Dark color schemes only. **Light color schemes,
high contrast mode and most other applications were not examined.** GTK applications and the Plasma shell (panel,
widgets) are not affected by a Qt style at all.

Known limitations:

* Tied to the Qt build it was compiled against (uses Qt private headers and exported private symbols). After a Qt
  update with a different minor version, rebuild it. Whether Qt refuses or crashes on a mismatching plugin was not
  tested.
* Anything drawn by the application itself with `fillRect`/QML colors ignores the style (see the QML overlay below).
* No rounded corners where applications paint rectangles themselves.

## Build

Dependencies (Ubuntu/Kubuntu package names): `cmake`, `ninja-build`, a C++20 compiler, `qt6-base-dev`,
`qt6-base-private-dev`. Optional for the demo tools: `qt6-declarative-dev`, `libkf6widgetsaddons-dev` (package names not verified).

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Try it without installing

```
QT_PLUGIN_PATH=$PWD/build/plugins dolphin -style eleven-kde
```

`-style eleven-kde` works for any Qt application. Already running single-instance applications (Prism Launcher, System
Settings) ignore the option and just raise the existing window, so close them first.

## Install

Style plugin, system wide (needs root, uses the plugin directory of the Qt found by `qmake6`):

```
sudo cmake --install build
```

Then choose **System Settings > Appearance > Application Style > eleven-kde**. If you would rather not touch system
directories, install with `-DELEVEN_PLUGIN_DIR=$HOME/.local/lib/qt6/plugins` and make Plasma export
`QT_PLUGIN_PATH=$HOME/.local/lib/qt6/plugins` at login (a script in `~/.config/plasma-workspace/env/`).

### Optional: QML overlay (System Settings and other QtQuick applications)

KDE's Kirigami/QtQuick controls (switches, sidebar lists in System Settings) are drawn by QML with the color scheme
and do not use the widget style. `qml/private/` contains replacements for two components of `org.kde.desktop.private`
(a WinUI toggle switch, a gray borderless list highlight). To use them for your user:

```
tools/install-qml-overlay.sh
```

This copies the system module to `~/.local/share/eleven-kde/qml-overlay`, swaps in the two files, and adds a
`QML_IMPORT_PATH` export for Plasma at login. Run it again after Plasma updates, because the overlay is a copy of
system files. It applies to every QML application, not just System Settings.

### Uninstall / recovery

```
sudo rm /usr/lib/x86_64-linux-gnu/qt6/plugins/styles/eleven-kde.so
rm -r ~/.local/share/eleven-kde ~/.config/plasma-workspace/env/eleven-kde-qml.sh
kwriteconfig6 --file kdeglobals --group KDE --key widgetStyle Breeze
```

The last line switches back to Breeze from a terminal if a broken plugin makes the settings windows unusable.

## Development

* `tools/gallery.cpp`: a widget gallery. `GALLERY_OUT=/tmp/x QT_PLUGIN_PATH=build/plugins build/gallery -style eleven-kde`
  renders the window, a menu and combo popups to PNGs without needing a screenshot of your desktop.
* `tools/qmltest.cpp`: a QML window with a few controls for the overlay.
* The style lives in `src/elevenstyle.cpp`. It still follows the structure of Qt's original, which is why many
  comments are Qt's.

## License

Because this is derived from Qt code, the license is inherited from it:

* `src/`, `tools/` and `CMakeLists.txt`: **LGPL-3.0-only** (the Qt-derived files are offered under Qt's
  `LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`; the commercial Qt license alternative cannot be granted here).
* `qml/private/*.qml`: derived from KDE's qqc2-desktop-style, **LGPL-3.0-only OR GPL-2.0-or-later**.

The license texts are in `LICENSES/`. Each file carries an SPDX header.

### Attribution and trademarks

* Original `QWindows11Style` and `QCommonStyle` code: Copyright The Qt Company Ltd. (`qtbase` v6.10.2).
* QML replacements are based on `org.kde.desktop.private` from KDE's qqc2-desktop-style (Copyright Marco Martin,
  The Qt Company Ltd., Tanbir Jishan, ivan tkachenko and others).
* Design values (colors, radii) follow the WinUI 3 look as implemented in Qt; Qt's source names the WinUI 3 Figma
  community kit as its reference. No Microsoft source code is used.
* "Windows", "Windows 11", "WinUI" and "Fluent" are trademarks of Microsoft Corporation. This project is not
  affiliated with or endorsed by Microsoft, KDE, or The Qt Company.
