#!/bin/sh
# Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
# SPDX-License-Identifier: LGPL-3.0-only
#
# Removes everything eleven-kde put outside the source tree:
#   - the style plugin (system Qt plugin directory and ~/.local/lib/qt6/plugins)
#   - the QML overlay, its Plasma login environment script and the network settings copy
#   - the Application Style setting, if it still points to eleven-kde (switched back to Breeze)
# Usage: tools/uninstall-eleven-kde.sh [--keep-style] [--dry-run]
#   --keep-style   do not touch the Application Style setting
# Removing the plugin from the system directory needs root; the script asks sudo only for that file.
set -u

dry=0; keep_style=0
for arg in "$@"; do
    case "$arg" in
        --dry-run) dry=1 ;;
        --keep-style) keep_style=1 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

say() { printf '%s\n' "$*"; }
run() {
    if [ "$dry" -eq 1 ]; then say "  would run: $*"; else "$@"; fi
}

removed=0

remove_file() { # remove_file <path> [sudo]
    [ -e "$1" ] || return 0
    say "removing $1"
    if [ "${2:-}" = "sudo" ] && [ ! -w "$(dirname "$1")" ]; then
        run sudo rm -f "$1"
    else
        run rm -f "$1"
    fi
    removed=1
}

remove_dir() {
    [ -d "$1" ] || return 0
    say "removing $1"
    run rm -r "$1"
    removed=1
}

# --- style plugin ---
qmake=$(command -v qmake6 || command -v qmake || true)
if [ -n "$qmake" ]; then
    remove_file "$("$qmake" -query QT_INSTALL_PLUGINS)/styles/eleven-kde.so" sudo
fi
remove_file "$HOME/.local/lib/qt6/plugins/styles/eleven-kde.so"

# --- QML overlay ---
remove_dir "${XDG_DATA_HOME:-$HOME/.local/share}/eleven-kde"
# the copy of the network settings page made by install-qml-overlay.sh (only if it is ours)
kcm_copy=${XDG_DATA_HOME:-$HOME/.local/share}/kcm_networkmanagement
[ -e "$kcm_copy/.eleven-kde" ] && remove_dir "$kcm_copy"
remove_file "${XDG_CONFIG_HOME:-$HOME/.config}/plasma-workspace/env/eleven-kde-qml.sh"

# --- Application Style setting ---
if [ "$keep_style" -eq 0 ] && command -v kreadconfig6 >/dev/null 2>&1; then
    current=$(kreadconfig6 --file kdeglobals --group KDE --key widgetStyle)
    if [ "$current" = "eleven-kde" ]; then
        say "Application Style is still eleven-kde, switching back to Breeze"
        run kwriteconfig6 --file kdeglobals --group KDE --key widgetStyle Breeze
        removed=1
    fi
fi

if [ "$removed" -eq 0 ]; then
    say "nothing to remove"
elif [ "$dry" -eq 1 ]; then
    say "dry run, nothing was changed"
else
    say "done. Log out and in again so running sessions and the login environment are clean."
fi
