#!/bin/sh
# Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
# SPDX-License-Identifier: LGPL-3.0-only
#
# Builds eleven-kde (if needed) and installs everything: the style plugin into the Qt plugin directory of the system Qt
# (root is needed for that, sudo is asked only for the install step), the QML overlay for System Settings and other
# QtQuick apps, and it sets the Application Style to eleven-kde.
# Usage: tools/install-eleven-kde.sh [--no-style] [--no-qml-overlay] [--dry-run]
#   --no-style        do not set the Application Style (choose it yourself in System Settings)
#   --no-qml-overlay  do not install the QML overlay (see tools/install-qml-overlay.sh)
#   --dry-run         only print what would be done
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
build=$root/build
dry=0; style=1; overlay=1
for arg in "$@"; do
    case "$arg" in
        --dry-run) dry=1 ;;
        --no-style) style=0 ;;
        --no-qml-overlay) overlay=0 ;;
        -h|--help) sed -n '5,11p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

say() { printf '%s\n' "$*"; }
run() {
    if [ "$dry" -eq 1 ]; then say "  would run: $*"; else "$@"; fi
}

# --- configure and build ---
if [ ! -f "$build/CMakeCache.txt" ]; then
    say "configuring"
    if command -v ninja >/dev/null 2>&1; then
        run cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release
    else
        run cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release
    fi
fi
say "building"
run cmake --build "$build"

# --- install the plugin (system plugin directory of the Qt found by qmake) ---
qmake=$(command -v qmake6 || command -v qmake || true)
[ -n "$qmake" ] || { echo "qmake6 not found, cannot locate the Qt plugin directory" >&2; exit 1; }
plugdir=$("$qmake" -query QT_INSTALL_PLUGINS)/styles
say "installing eleven-kde.so into $plugdir"
if [ -w "$plugdir" ]; then
    run cmake --install "$build"
else
    say "(the directory is not writable for you, using sudo)"
    run sudo cmake --install "$build"
fi

# --- optional extras ---
if [ "$overlay" -eq 1 ]; then
    say "installing the QML overlay"
    run "$root/tools/install-qml-overlay.sh"
fi

if [ "$style" -eq 1 ]; then
    say "setting the Application Style to eleven-kde"
    run kwriteconfig6 --file kdeglobals --group KDE --key widgetStyle eleven-kde
else
    say "not applied: select it in System Settings > Appearance > Application Style"
fi

if [ "$dry" -eq 1 ]; then
    say "dry run, nothing was changed"
else
    say "done. Programs started from now on use it; log out and in again for everything (and for --qml-overlay)."
fi
