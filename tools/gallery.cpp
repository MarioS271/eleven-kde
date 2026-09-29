// Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
// SPDX-License-Identifier: LGPL-3.0-only

// Small widget gallery to look at the style without clicking through real apps.
// Usage: QT_PLUGIN_PATH=build/plugins gallery [-style eleven-kde]   (menu opens automatically)
#include <QtWidgets>
#ifdef GALLERY_HAS_KF
#include <KCapacityBar>
#endif

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget w;
    w.setWindowTitle("eleven-kde gallery");
    auto *lay = new QGridLayout(&w);

    auto *menu = new QMenu(&w);
    auto *group = new QActionGroup(menu);
    auto *view = menu->addAction(QIcon::fromTheme("view-list-icons"), "Symbole");
    view->setCheckable(true); view->setChecked(true); group->addAction(view);
    auto *details = menu->addAction(QIcon::fromTheme("view-list-details"), "Details");
    details->setCheckable(true); group->addAction(details);
    menu->addSeparator();
    menu->addAction(QIcon::fromTheme("view-sort"), "Sortieren nach");
    menu->addAction(QIcon::fromTheme("view-preview"), "Vorschau anzeigen")->setCheckable(true);
    menu->addAction(QIcon::fromTheme("view-hidden"), "Versteckte Dateien")->setCheckable(true);

    auto *split = new QToolButton; split->setIcon(QIcon::fromTheme("view-list-icons"));
    split->setMenu(menu); split->setPopupMode(QToolButton::MenuButtonPopup); split->setAutoRaise(true);
    auto *checked = new QToolButton; checked->setIcon(QIcon::fromTheme("view-list-icons"));
    checked->setCheckable(true); checked->setChecked(true); checked->setAutoRaise(true);
    auto *plain = new QToolButton; plain->setIcon(QIcon::fromTheme("view-list-icons")); plain->setAutoRaise(true);
    auto *back = new QToolButton; back->setIcon(QIcon::fromTheme("go-previous"));
    back->setMenu(menu); back->setPopupMode(QToolButton::DelayedPopup); back->setAutoRaise(true);
    lay->addWidget(back, 0, 4);
    lay->addWidget(new QLabel("ToolButtons:"), 0, 0);
    lay->addWidget(split, 0, 1); lay->addWidget(checked, 0, 2); lay->addWidget(plain, 0, 3);

    // stand-in for Dolphin's breadcrumb buttons: they call PE_PanelButtonCommand (flat) themselves when highlighted
    struct FlatCmd : QPushButton {
        using QPushButton::QPushButton;
        void paintEvent(QPaintEvent *) override {
            QPainter p(this);
            QStyleOptionButton o; o.initFrom(this);
            o.rect = rect(); o.features = QStyleOptionButton::Flat;
            o.state |= QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_KeyboardFocusChange;
            style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &o, &p, this);
            p.drawText(rect(), Qt::AlignCenter, text());
        }
    };
    lay->addWidget(new FlatCmd("Persönlicher Ordner"), 1, 2, 1, 3);
    lay->addWidget(new QPushButton("Button"), 1, 0);
    auto *def = new QPushButton("Default"); def->setDefault(true); lay->addWidget(def, 1, 1);
    auto *cb = new QCheckBox("Check"); cb->setChecked(true); lay->addWidget(cb, 2, 0);
    lay->addWidget(new QRadioButton("Radio"), 2, 1);
    auto *combo = new QComboBox; combo->addItems({"Eins", "Zwei", "Drei"}); combo->insertSeparator(1); lay->addWidget(combo, 3, 0);
    auto *ecombo = new QComboBox; ecombo->setEditable(true); ecombo->addItems({"/home/marios271/", "/home/marios271/Desktop"}); lay->addWidget(ecombo, 3, 2, 1, 3);
    auto *spin = new QSpinBox; lay->addWidget(spin, 3, 1);
    lay->addWidget(new QLineEdit("Text"), 4, 0);
    auto *sl = new QSlider(Qt::Horizontal); sl->setValue(40); lay->addWidget(sl, 4, 1);
    auto *pb = new QProgressBar; pb->setValue(60); lay->addWidget(pb, 5, 0, 1, 2);
#ifdef GALLERY_HAS_KF
    auto *cap = new KCapacityBar(KCapacityBar::DrawTextInline); cap->setValue(45);
    cap->setText("251,8 GiB von 456,9 GiB frei (45 % belegt)");
    lay->addWidget(cap, 5, 2, 1, 3);
#endif
    auto *tabs = new QTabWidget; tabs->addTab(new QLabel("A"), "Tab A"); tabs->addTab(new QLabel("B"), "Tab B");
    lay->addWidget(tabs, 6, 0, 1, 2);
    if (qEnvironmentVariableIsSet("GALLERY_HOVER")) { // fake hover on a few buttons for rendering
        for (QWidget *b : std::initializer_list<QWidget *>{plain, back, w.findChild<QPushButton *>()})
            b->setAttribute(Qt::WA_UnderMouse, true);
    }
    w.resize(420, 480);
    w.show();
    if (const QByteArray out = qgetenv("GALLERY_OUT"); !out.isEmpty()) {
        // headless-ish: render window and menu into PNGs (no screen capture needed)
        QTimer::singleShot(400, [&, out] {
            w.grab().save(QString::fromLocal8Bit(out) + "_window.png");
            menu->ensurePolished();
            menu->resize(menu->sizeHint());
            menu->grab().save(QString::fromLocal8Bit(out) + "_menu.png");
            for (QComboBox *cb : w.findChildren<QComboBox *>()) { // popup lists, editable one included
                cb->showPopup();
                QWidget *pop = cb->view()->window();
                pop->ensurePolished();
                pop->grab().save(QString::fromLocal8Bit(out) + (cb->isEditable() ? "_combo_editable.png" : "_combo.png"));
                cb->hidePopup();
            }
            app.quit();
        });
    } else if (qEnvironmentVariableIsSet("GALLERY_COMBO")) { // real popup window, run with QT_QPA_PLATFORM=xcb
        QTimer::singleShot(600, [combo] { combo->showPopup(); });
    } else {
        QTimer::singleShot(600, [&] { menu->popup(split->mapToGlobal(QPoint(0, split->height()))); });
    }
    return app.exec();
}
