// Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
// SPDX-License-Identifier: LGPL-3.0-only

// Tiny QML host for looking at the org.kde.desktop controls with the overlay:
// QT_QUICK_CONTROLS_STYLE=org.kde.desktop QML_IMPORT_PATH=build/qml-overlay QT_PLUGIN_PATH=build/plugins qmltest -style eleven-kde
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QQmlComponent>
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.loadData(R"(
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
QQC2.ApplicationWindow {
    visible: true; width: 360; height: 260
    color: Kirigami.Theme.backgroundColor
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 10
        QQC2.Switch { text: "Off"; checked: false }
        QQC2.Switch { text: "On"; checked: true }
        QQC2.Switch { text: "Off, disabled"; enabled: false }
        QQC2.ItemDelegate { text: "Normal item"; Layout.fillWidth: true }
        QQC2.ItemDelegate { text: "Highlighted item"; highlighted: true; Layout.fillWidth: true }
    }
})", QUrl("qrc:/test.qml"));
    return app.exec();
}
