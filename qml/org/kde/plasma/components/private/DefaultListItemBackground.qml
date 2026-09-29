/*
    Based on org.kde.plasma.components.private.DefaultListItemBackground (Plasma), modified for eleven-kde.
    The original draws the "widgets/listitem" element of the Plasma theme (blue hover and selection); this one draws a
    gray rounded highlight instead and keeps the (transparent) "normal" frame item so that the margins the delegates read
    from it stay the same.

    SPDX-FileCopyrightText: 2016 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2026 the eleven-kde authors (AI-assisted)
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import QtQuick.Templates as T
import org.kde.ksvg as KSvg
import org.kde.kirigami as Kirigami

KSvg.FrameSvgItem {
    id: background

    required property T.ItemDelegate control

    imagePath: "widgets/listitem"
    prefix: "normal"
    visible: control.ListView.view ? control.ListView.view.highlight === null : true

    Rectangle {
        anchors.fill: parent
        radius: Kirigami.Units.cornerRadius
        color: (background.control.highlighted || background.control.down)
                   ? Qt.alpha(Kirigami.Theme.textColor, 0.14)
                   : (background.control.hovered && !Kirigami.Settings.isMobile ? Qt.alpha(Kirigami.Theme.textColor, 0.07)
                                                                                : "transparent")
    }
}
