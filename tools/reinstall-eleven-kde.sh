#!/bin/sh
# Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
# SPDX-License-Identifier: LGPL-3.0-only
#
# Uninstalls eleven-kde completely and installs it again from the current source tree
# (tools/uninstall-eleven-kde.sh, then tools/install-eleven-kde.sh).
# Usage: tools/reinstall-eleven-kde.sh [--no-style] [--no-qml-overlay] [--dry-run]
#   --no-style, --no-qml-overlay   passed to the install step, see tools/install-eleven-kde.sh
#                                  (--no-style also keeps the current Application Style during the uninstall step)
#   --dry-run                      passed to both steps: only print what would be done
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
uninstall_args=""
for arg in "$@"; do
    case "$arg" in
        --dry-run) uninstall_args="$uninstall_args --dry-run" ;;
        --no-style) uninstall_args="$uninstall_args --keep-style" ;;  # do not reset a style you chose yourself
        --no-qml-overlay) ;;
        -h|--help) sed -n '5,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

echo "=== uninstalling ==="
# shellcheck disable=SC2086
"$root/tools/uninstall-eleven-kde.sh" $uninstall_args
echo
echo "=== installing ==="
"$root/tools/install-eleven-kde.sh" "$@"
