#!/bin/sh
# Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
# SPDX-License-Identifier: LGPL-3.0-only
#
# Optional, per user: installs the QML overlay (WinUI switch, gray list highlight for Kirigami/QtQuick apps such as
# System Settings) and makes Plasma export QML_IMPORT_PATH at login. Needs no root.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
dest=${XDG_DATA_HOME:-$HOME/.local/share}/eleven-kde/qml-overlay
envdir=${XDG_CONFIG_HOME:-$HOME/.config}/plasma-workspace/env
"$root/tools/make-qml-overlay.sh" "$dest"
mkdir -p "$envdir"
cat > "$envdir/eleven-kde-qml.sh" <<EOF
# written by eleven-kde tools/install-qml-overlay.sh
export QML_IMPORT_PATH="$dest\${QML_IMPORT_PATH:+:\$QML_IMPORT_PATH}"
EOF
echo "installed. Log out and in again for it to take effect."
echo "remove with: rm -r '$dest' '$envdir/eleven-kde-qml.sh'"
