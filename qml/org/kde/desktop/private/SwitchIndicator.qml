/*
    Based on org.kde.desktop.private from KDE's qqc2-desktop-style, modified for eleven-kde (AI-assisted).

    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2017 The Qt Company Ltd.
    SPDX-FileCopyrightText: 2022 Tanbir Jishan <tantalising007@gmail.com>
    SPDX-FileCopyrightText: 2026 the eleven-kde authors
    SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-or-later
*/
// WinUI-style toggle: outlined pill with a small gray knob when off, accent-filled pill with a light knob when on.
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami

Item {
    id: indicator
    implicitHeight: Kirigami.Units.gridUnit + 2
    implicitWidth: Math.round(implicitHeight * 2.1)
    layer.enabled: control.opacity < 1.0

    property T.AbstractButton control
    property alias handle: handle

    Kirigami.Theme.colorSet: Kirigami.Theme.Button
    Kirigami.Theme.inherit: false

    readonly property int pad: 4
    readonly property bool checked: control.checked

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: indicator.checked ? Kirigami.Theme.highlightColor
                                 : (control.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.07) : "transparent")
        border.width: indicator.checked ? 0 : 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, control.enabled ? 0.6 : 0.3)
        Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
    }

    Rectangle {
        id: handle
        readonly property real size: control.pressed ? indicator.height * 0.75
                                    : control.hovered ? indicator.height * 0.7 : indicator.height * 0.6
        width: size
        height: size
        radius: size / 2
        anchors.verticalCenter: parent.verticalCenter
        x: indicator.pad + control.visualPosition * (indicator.width - width - 2 * indicator.pad)
        color: indicator.checked ? Kirigami.Theme.highlightedTextColor
                                 : Qt.alpha(Kirigami.Theme.textColor, control.enabled ? 0.75 : 0.4)

        Behavior on x {
            enabled: !control.pressed && Kirigami.Units.shortDuration > 0
            SmoothedAnimation { duration: Kirigami.Units.shortDuration }
        }
        Behavior on width { NumberAnimation { duration: Kirigami.Units.veryShortDuration } }
        Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
    }
}
