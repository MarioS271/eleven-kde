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
* Qt applications installed as Flatpak are not affected (see "Sandboxed applications").
* No rounded corners where applications paint rectangles themselves.

## Known issue: right click in Dolphin does nothing (KDE Connect hangs)

After installing and logging in again, the context menu of files and folders in Dolphin may stop appearing. The author
saw this every time this was set up, so expect it. The cause found so far is not the menu itself but **`kdeconnectd`**
(KDE Connect): one of its threads spins at about 100 % CPU and the daemon stops answering on D-Bus. Dolphin asks it
synchronously for the "send to device" entry, so the menu never opens.

Check and fix it:

```
top -b -n1 | grep kdeconnectd                 # a hung daemon shows about 100 % CPU
gdbus call --session --dest org.kde.kdeconnect --object-path /modules/kdeconnect \
    --method org.kde.kdeconnect.daemon.devices false false     # should answer at once, not after a timeout
pkill -x kdeconnectd                          # D-Bus starts a fresh one on demand
```

What is and is not known:

* In one observed case the hung `kdeconnectd` had **never loaded this style** (it had been started while the style was
  uninstalled), and a `kdeconnectd` that had loaded the style ran normally. So the style is not proven to be the cause,
  and the hang may be a KDE Connect problem that only becomes visible around a fresh login. It was not investigated
  further (a stack trace of the hung thread needs `sudo eu-stack -p <pid>` because of `ptrace_scope`).
* The daemon may not hang on every setup. Do not rely on either statement without checking on your machine.
* Avoiding it altogether: turn off the KDE Connect entry of Dolphin's context menu, or disable KDE Connect.

## Sandboxed applications (Flatpak, and probably Snap)

**Qt applications installed as Flatpak do not use this style.** A Flatpak app runs inside a sandbox with the Qt and the
Breeze style of its own runtime (for example `org.kde.Platform` 6.11), so:

* the plugin installed on the host is not visible inside the sandbox, and
* it would not load there anyway: it is built against the host's Qt and uses Qt private APIs, which must match the Qt
  build of the application exactly.

Such applications simply keep looking like Breeze. This was observed with Qalculate! (Qt) from Flathub. Flatpak apps
that use GTK or Electron are not affected by a Qt style in the first place. Other bundled formats (Snap, AppImages that
ship their own Qt) are expected to behave the same way but were not checked.

Options, none of which is implemented here:

* install the application natively (for example from your distribution) instead of as a Flatpak;
* build the plugin once per Flatpak runtime version and provide it to the sandbox as a Flatpak extension (the way other
  third-party Qt styles are distributed); whether the runtime SDKs contain the needed private Qt headers was not
  verified.

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

The easy way installs everything: it builds if needed, installs the plugin (needs `sudo` once, only for that step),
installs the optional QML overlay (see below) and sets the Application Style to eleven-kde. Each part can be switched
off:

```
tools/install-eleven-kde.sh                     # plugin + QML overlay + Application Style
tools/install-eleven-kde.sh --no-qml-overlay    # plugin + Application Style
tools/install-eleven-kde.sh --no-style          # plugin + QML overlay, pick the style yourself in System Settings
tools/install-eleven-kde.sh --no-style --no-qml-overlay   # plugin only
tools/install-eleven-kde.sh --dry-run           # show what would be done, change nothing
```

Log out and in again afterwards (the QML overlay needs it, and it makes every running KDE service pick up the style).

The manual steps behind it:


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
tools/uninstall-eleven-kde.sh --dry-run    # shows what would be removed
tools/uninstall-eleven-kde.sh
```

The script removes the style plugin (it uses `sudo` for the file in the system plugin directory only), the QML overlay
and its login script, and switches the Application Style back to Breeze if it still points to eleven-kde. Log out and
in afterwards.

If a broken plugin makes the settings windows unusable, switch back to Breeze from a terminal:

```
kwriteconfig6 --file kdeglobals --group KDE --key widgetStyle Breeze
```

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
