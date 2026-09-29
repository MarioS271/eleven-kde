#!/bin/sh
# Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
# SPDX-License-Identifier: LGPL-3.0-only
#
# Builds a QML overlay from the system's org.kde.desktop.private module, with the files in qml/private replaced.
# Usage: make-qml-overlay.sh [output-dir]   (default: <project>/build/qml-overlay)
# Use it with QML_IMPORT_PATH=<output-dir> (import paths win over the system module).
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
sys=/usr/lib/x86_64-linux-gnu/qt6/qml/org/kde/desktop/private
dest=${1:-$root/build/qml-overlay}
out=$dest/org/kde/desktop/private
[ -d "$sys" ] || { echo "system module not found: $sys" >&2; exit 1; }
rm -rf "$dest"
mkdir -p "$out"
cp -a "$sys"/. "$out"/
# the system module prefers the QML compiled into its library; drop that so our files on disk are used
sed -i '/^prefer /d' "$out/qmldir"
cp "$root"/qml/private/*.qml "$out"/
echo "overlay ready: $dest"
