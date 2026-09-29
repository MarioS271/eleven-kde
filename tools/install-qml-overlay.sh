#!/bin/sh
# Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
# SPDX-License-Identifier: LGPL-3.0-only
#
# Optional, per user (no root): installs
#   - the QML overlay (WinUI switch, gray list highlights and popups for Kirigami/QtQuick apps such as System Settings)
#     and makes Plasma export QML_IMPORT_PATH at login, and
#   - a copy of the network settings page (kcm_networkmanagement) with a gray connection list, in
#     ~/.local/share/kcm_networkmanagement (System Settings prefers it over the system copy).
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
data=${XDG_DATA_HOME:-$HOME/.local/share}
dest=$data/eleven-kde/qml-overlay
envdir=${XDG_CONFIG_HOME:-$HOME/.config}/plasma-workspace/env
kcm_sys=/usr/share/kcm_networkmanagement/qml
kcm_dest=$data/kcm_networkmanagement/qml

"$root/tools/make-qml-overlay.sh" "$dest"
mkdir -p "$envdir"
cat > "$envdir/eleven-kde-qml.sh" <<EOF
# written by eleven-kde tools/install-qml-overlay.sh
export QML_IMPORT_PATH="$dest\${QML_IMPORT_PATH:+:\$QML_IMPORT_PATH}"
EOF

if [ -d "$kcm_sys" ]; then
    if [ -e "$data/kcm_networkmanagement" ] && [ ! -e "$data/kcm_networkmanagement/.eleven-kde" ]; then
        echo "skipping the network settings copy: $data/kcm_networkmanagement exists and is not ours" >&2
    else
        rm -rf "$data/kcm_networkmanagement"
        mkdir -p "$kcm_dest"
        cp "$kcm_sys"/*.qml "$kcm_dest"/
        cp "$root"/kcm/kcm_networkmanagement/qml/*.qml "$kcm_dest"/
        touch "$data/kcm_networkmanagement/.eleven-kde"
        echo "installed the network settings copy into $data/kcm_networkmanagement"
    fi
fi

echo "installed. Log out and in again for it to take effect."
echo "remove with: tools/uninstall-eleven-kde.sh"
