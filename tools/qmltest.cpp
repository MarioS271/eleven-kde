// Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
// SPDX-License-Identifier: LGPL-3.0-only

// Tiny QML host for looking at the org.kde.desktop controls with the overlay:
// QMLTEST_OUT=/tmp/x.png renders the window to a PNG and exits.
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
import org.kde.plasma.components as PlasmaComponents
QQC2.ApplicationWindow {
    visible: true; width: 360; height: 600
    color: Kirigami.Theme.backgroundColor
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 10
        QQC2.Switch { text: "Off"; checked: false }
        QQC2.Switch { text: "On"; checked: true }
        QQC2.Switch { text: "Off, disabled"; enabled: false }
        QQC2.ItemDelegate { text: "Normal item"; Layout.fillWidth: true }
        QQC2.ItemDelegate { text: "Highlighted item"; highlighted: true; Layout.fillWidth: true }
        QQC2.MenuItem { text: "Highlighted MenuItem"; highlighted: true; Layout.fillWidth: true }
        Kirigami.NavigationTabBar {
            Layout.fillWidth: true
            actions: [
                Kirigami.Action { text: "Integrierter Bildschirm"; icon.name: "video-display"; checked: true },
                Kirigami.Action { text: "AOC 27G2WG3"; icon.name: "video-display" }
            ]
        }
        PlasmaComponents.ItemDelegate { text: "Plasma delegate, highlighted"; highlighted: true; Layout.fillWidth: true }
        PlasmaComponents.ItemDelegate { text: "Plasma delegate"; Layout.fillWidth: true }
        QQC2.Button {
            text: "Menu"
            onClicked: menu.popup()
            Timer { interval: 600; running: true; onTriggered: menu.popup(120, 150) } // opens a popup for screenshots
        }
        QQC2.Menu {
            id: menu
            QQC2.MenuItem { text: "165.00 Hz"; highlighted: true }
            QQC2.MenuItem { text: "&60.00 Hz" }
            QQC2.MenuItem { text: "&Checked entry"; checkable: true; checked: true }
        }
    }
})", QUrl("qrc:/test.qml"));
    if (const QByteArray out = qgetenv("QMLTEST_OUT"); !out.isEmpty()) {
        // render the window (with its popups) into a PNG instead of needing a screenshot of the desktop
        QTimer::singleShot(1500, [&, out] {
            for (QObject *o : engine.rootObjects())
                if (auto *w = qobject_cast<QQuickWindow *>(o))
                    w->grabWindow().save(QString::fromLocal8Bit(out));
            app.quit();
        });
    }
    return app.exec();
}
