// Copyright (C) 2022 The Qt Company Ltd.       (original QWindows11Style, qtbase v6.10.2)
// Copyright (C) 2016 The Qt Company Ltd.       (QCommonStyle helper code copied from qtbase v6.10.2)
// Copyright (C) 2026 the eleven-kde authors   (port to a Linux/Plasma style plugin; AI-assisted, see README.md)
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Derived from QWindows11Style (src/plugins/styles/modernwindows/qwindows11style.cpp in qtbase). Changes: Breeze
// proxy base instead of QWindowsVistaStyle, no title bar/MDI/DWM code, vector glyphs instead of the Segoe icon
// font, plus many appearance and integration changes for Plasma/KDE apps.

// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "elevenstyle.h"
#include <qstylehints.h>
#include <private/qstyleanimation_p.h>
#include <private/qstyle_p.h>
#include <private/qstylehelper_p.h>
#include <private/qcombobox_p.h>
#include <qstyleoption.h>
#include <qpainter.h>
#include <qpainterstateguard.h>
#include <QLatin1StringView>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/qstylefactory.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qtableview.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qtabbar.h>
#include <QtWidgets/qtoolbar.h>
#include <QtGui/QFontInfo>
#include <QtGui/QPixmapCache>
#include <QtGui/QGuiApplication>
#include <QtGui/QIconEngine>
#include <QtGui/QTextLayout>
#include <QtGui/QPainterPath>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <private/qtextengine_p.h>
#if QT_CONFIG(commandlinkbutton)
#include <QtWidgets/qcommandlinkbutton.h>
#endif
#include <QtWidgets/qgraphicsview.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qmenu.h>
#if QT_CONFIG(mdiarea)
#include <QtWidgets/qmdiarea.h>
#endif
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qtextedit.h>
#include <QtWidgets/qtreeview.h>
#if QT_CONFIG(datetimeedit)
#  include <QtWidgets/qdatetimeedit.h>
#endif
#if QT_CONFIG(tabwidget)
#  include <QtWidgets/qtabwidget.h>
#endif
#if QT_CONFIG(menubar)
#  include <QtWidgets/qmenubar.h>
#endif
#include "qdrawutil.h"
#include <chrono>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

// KDE custom style element (KStyleExtensions): KCapacityBar asks for "CE_CapacityBar" via styleHint 0xff000001.
static constexpr int kStyleCustomElementHint = int(0xff000001);
static constexpr QStyle::ControlElement kCapacityBarElement = static_cast<QStyle::ControlElement>(0xff000fffu);

// Symbolic icons from plain Qt icon themes (no KDE icon engine, e.g. Prism Launcher) stay black/dark gray and are
// unreadable on a dark surface. Recolor an icon to the text color only if every visible pixel is a dark neutral
// gray (a monochrome symbolic icon); anything colored or light is left alone.
static QPixmap adaptIcon(const QPixmap &pm, const QColor &text)
{
    if (pm.isNull() || text.lightness() < 128)
        return pm;
    const QString key = QStringLiteral("eleven-icon-%1-%2").arg(pm.cacheKey()).arg(text.rgba(), 0, 16);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached))
        return cached;

    QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    bool monochrome = true, anyVisible = false;
    for (int y = 0; y < img.height() && monochrome; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = line[x];
            if (qAlpha(c) < 40)
                continue;
            anyVisible = true;
            const int hi = qMax(qRed(c), qMax(qGreen(c), qBlue(c)));
            const int lo = qMin(qRed(c), qMin(qGreen(c), qBlue(c)));
            if (hi - lo > 28 || hi > 120) {
                monochrome = false;
                break;
            }
        }
    }
    QPixmap out = pm;
    if (monochrome && anyVisible) {
        for (int y = 0; y < img.height(); ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x)
                line[x] = qRgba(text.red(), text.green(), text.blue(), qAlpha(line[x]) * text.alpha() / 255);
        }
        out = QPixmap::fromImage(img);
        out.setDevicePixelRatio(pm.devicePixelRatio());
    }
    QPixmapCache::insert(key, out);
    return out;
}

// Dolphin's location bar is the one input that keeps a frame.
static bool inUrlNavigator(const QWidget *w)
{
    for (; w; w = w->parentWidget())
        if (w->inherits("KUrlNavigator"))
            return true;
    return false;
}

static constexpr int topLevelRoundingRadius    = 8; //Radius for toplevel items like popups for round corners
static constexpr int secondLevelRoundingRadius = 4; //Radius for second level items like hovered menu item round corners
static constexpr int contentItemHMargin = 4;        // margin between content items (e.g. text and icon)
static constexpr int contentHMargin = 2 * 3;        // margin between rounded border and content (= rounded border margin * 3)
namespace StyleOptionHelper
{
inline bool isChecked(const QStyleOption *option)
{
    return option->state.testAnyFlags(QStyle::State_On | QStyle::State_NoChange);
}
inline bool isDisabled(const QStyleOption *option)
{
    return !option->state.testFlag(QStyle::State_Enabled);
}
inline bool isPressed(const QStyleOption *option)
{
    return option->state.testFlag(QStyle::State_Sunken);
}
inline bool isHover(const QStyleOption *option)
{
    return option->state.testFlag(QStyle::State_MouseOver);
}
inline bool isAutoRaise(const QStyleOption *option)
{
    return option->state.testFlag(QStyle::State_AutoRaise);
}
inline bool hasFocus(const QStyleOption *option)
{
    return option->state.testFlag(QStyle::State_HasFocus);
}
enum class ControlState { Normal, Hover, Pressed, Disabled };
inline ControlState calcControlState(const QStyleOption *option)
{
    if (isDisabled(option))
        return ControlState::Disabled;
    if (isPressed(option))
        return ControlState::Pressed;
    if (isHover(option))
        return ControlState::Hover;
    return ControlState::Normal;
};

} // namespace StyleOptionHelper

enum class Icon : ushort
{
    AcceptMedium = 0xF78C,
    Dash12 = 0xE629,
    CheckMark = 0xE73E,
    CaretLeftSolid8 = 0xEDD9,
    CaretRightSolid8 = 0xEDDA,
    CaretUpSolid8 = 0xEDDB,
    CaretDownSolid8 = 0xEDDC,
    ChevronDown = 0xE70D,
    ChevronUp = 0xE70E,
    ChevronUpMed = 0xE971,
    ChevronDownMed = 0xE972,
    ChevronLeftMed = 0xE973,
    ChevronRightMed = 0xE974,
    ChevronUpSmall = 0xE96D,
    ChevronDownSmall = 0xE96E,
    Close = 0xE8BB,
    More = 0xE712,
    Help = 0xE897,
    Clear = 0xE894,
};

static void drawGlyph(QPainter *p, const QRectF &rect, Icon icon);
static int glyphAdvance(const QFont &font);

template <typename R, typename P, typename B>
static inline void drawRoundedRect(QPainter *p, R &&rect, P &&pen, B &&brush)
{
    p->setPen(pen);
    p->setBrush(brush);
    p->drawRoundedRect(rect, secondLevelRoundingRadius, secondLevelRoundingRadius);
}

// Menus and popups: take the window colour of the current colour scheme (so they match the app
// instead of the fixed near-black WinUI token), lift it a little in dark mode.
static QColor menuFillColor(const QStyleOption *option)
{
    QColor c = option->palette.window().color();
    if (c.lightness() < 128)
        c = c.lighter(118);
    c.setAlpha(255); // fully opaque; the window itself stays translucent only for the rounded corners
    return c;
}

static constexpr int percentToAlpha(double percent)
{
    return qRound(percent * 255. / 100.);
}

static constexpr std::array<QColor, 34> WINUI3ColorsLight {
    QColor(0x00,0x00,0x00,percentToAlpha(3.73)), // subtleHighlightColor (fillSubtleSecondary)
    QColor(0x00,0x00,0x00,percentToAlpha(2.41)), // subtlePressedColor (fillSubtleTertiary)
    QColor(0x00,0x00,0x00,0x0F), //frameColorLight
    QColor(0x00,0x00,0x00,percentToAlpha(60.63)),   //frameColorStrong
    QColor(0x00,0x00,0x00,percentToAlpha(21.69)),   //frameColorStrongDisabled
    QColor(0x00,0x00,0x00,0x72), //controlStrongFill
    QColor(0x00,0x00,0x00,0x29), //controlStrokeSecondary
    QColor(0x00,0x00,0x00,0x14), //controlStrokePrimary
    QColor(0xFF,0xFF,0xFF,0xFF), //menuPanelFill
    QColor(0x00,0x00,0x00,0x66), //controlStrokeOnAccentSecondary
    QColor(0xFF,0xFF,0xFF,0xFF), //controlFillSolid
    QColor(0x75,0x75,0x75,0x66), //surfaceStroke
    QColor(0xFF,0xFF,0xFF,0xFF), //focusFrameInnerStroke
    QColor(0x00,0x00,0x00,0xFF), //focusFrameOuterStroke
    QColor(0xFF,0xFF,0xFF,percentToAlpha(70)),      // fillControlDefault
    QColor(0xF9,0xF9,0xF9,percentToAlpha(50)),      // fillControlSecondary
    QColor(0xF9,0xF9,0xF9,percentToAlpha(30)),      // fillControlTertiary
    QColor(0xF9,0xF9,0xF9,percentToAlpha(30)),      // fillControlDisabled
    QColor(0xFF,0xFF,0xFF,percentToAlpha(100)),     // fillControlInputActive
    QColor(0x00,0x00,0x00,percentToAlpha(2.41)),    // fillControlAltSecondary
    QColor(0x00,0x00,0x00,percentToAlpha(5.78)),    // fillControlAltTertiary
    QColor(0x00,0x00,0x00,percentToAlpha(9.24)),    // fillControlAltQuarternary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(0.00)),    // fillControlAltDisabled
    QColor(0x00,0x00,0x00,percentToAlpha(100)),     // fillAccentDefault
    QColor(0x00,0x00,0x00,percentToAlpha(90)),      // fillAccentSecondary
    QColor(0x00,0x00,0x00,percentToAlpha(80)),      // fillAccentTertiary
    QColor(0x00,0x00,0x00,percentToAlpha(21.69)),   // fillAccentDisabled
    QColor(0x00,0x00,0x00,percentToAlpha(89.56)),   // textPrimary
    QColor(0x00,0x00,0x00,percentToAlpha(60.63)),   // textSecondary
    QColor(0x00,0x00,0x00,percentToAlpha(36.14)),   // textDisabled
    QColor(0xFF,0xFF,0xFF,percentToAlpha(100)),     // textOnAccentPrimary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(70)),      // textOnAccentSecondary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(100)),     // textOnAccentDisabled
    QColor(0x00,0x00,0x00,percentToAlpha(8.03)),    // dividerStrokeDefault
};

static constexpr std::array<QColor, 34> WINUI3ColorsDark {
    QColor(0xFF,0xFF,0xFF,percentToAlpha(10.0)), // subtleHighlightColor (fillSubtleSecondary); WinUI 6.05, raised for Plasma
    QColor(0xFF,0xFF,0xFF,percentToAlpha(6.5)), // subtlePressedColor (fillSubtleTertiary); WinUI 4.19
    QColor(0xFF,0xFF,0xFF,0x12), //frameColorLight
    QColor(0xFF,0xFF,0xFF,percentToAlpha(60.47)),   //frameColorStrong
    QColor(0xFF,0xFF,0xFF,percentToAlpha(15.81)),   //frameColorStrongDisabled
    QColor(0xFF,0xFF,0xFF,0x8B), //controlStrongFill
    QColor(0xFF,0xFF,0xFF,0x18), //controlStrokeSecondary
    QColor(0xFF,0xFF,0xFF,0x22), //controlStrokePrimary (WinUI 0x12, raised for Plasma)
    QColor(0x0F,0x0F,0x0F,0xFF), //menuPanelFill
    QColor(0xFF,0xFF,0xFF,0x14), //controlStrokeOnAccentSecondary
    QColor(0x45,0x45,0x45,0xFF), //controlFillSolid
    QColor(0x75,0x75,0x75,0x66), //surfaceStroke
    QColor(0x00,0x00,0x00,0xFF), //focusFrameInnerStroke
    QColor(0xFF,0xFF,0xFF,0xFF), //focusFrameOuterStroke
    QColor(0xFF,0xFF,0xFF,percentToAlpha(6.05)),    // fillControlDefault
    QColor(0xFF,0xFF,0xFF,percentToAlpha(8.37)),    // fillControlSecondary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(3.26)),    // fillControlTertiary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(4.19)),    // fillControlDisabled
    QColor(0x1E,0x1E,0x1E,percentToAlpha(70)),      // fillControlInputActive
    QColor(0x00,0x00,0x00,percentToAlpha(10.0)),    // fillControlAltDefault
    QColor(0xFF,0xFF,0xFF,percentToAlpha(4.19)),    // fillControlAltSecondary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(6.98)),    // fillControlAltTertiafillCy
    QColor(0xFF,0xFF,0xFF,percentToAlpha(0.00)),    // controlAltDisabled
    QColor(0x00,0x00,0x00,percentToAlpha(100)),     // fillAccentDefault
    QColor(0x00,0x00,0x00,percentToAlpha(90)),      // fillAccentSecondary
    QColor(0x00,0x00,0x00,percentToAlpha(80)),      // fillAccentTertiary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(15.81)),   // fillAccentDisabled
    QColor(0xFF,0xFF,0xFF,percentToAlpha(100)),     // textPrimary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(78.6)),    // textSecondary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(36.28)),   // textDisabled
    QColor(0x00,0x00,0x00,percentToAlpha(100)),     // textOnAccentPrimary
    QColor(0x00,0x00,0x00,percentToAlpha(70)),      // textOnAccentSecondary
    QColor(0xFF,0xFF,0xFF,percentToAlpha(53.02)),   // textOnAccentDisabled
    QColor(0xFF,0xFF,0xFF,percentToAlpha(8.37)),    // dividerStrokeDefault
};

static constexpr std::array<std::array<QColor,34>, 2> WINUI3Colors {
    WINUI3ColorsLight,
    WINUI3ColorsDark
};

// Row highlight of item views: gray, stronger when selected than when only hovered. Qt's original switches to the
// solid accent color for views with alternating row colors (e.g. Ark's file tree); that is not wanted here.
static QColor itemRowHighlight(int colorScheme, bool selected)
{
    if (selected)
        return colorScheme == 1 ? QColor(0xFF, 0xFF, 0xFF, 34) : QColor(0x00, 0x00, 0x00, 28);
    return WINUI3Colors[colorScheme][subtleHighlightColor];
}


// Color of close Button in Titlebar (default + hover)
static constexpr QColor shellCaptionCloseFillColorPrimary(0xC4,0x2B,0x1C,0xFF);
static constexpr QColor shellCaptionCloseTextFillColorPrimary(0xFF,0xFF,0xFF,0xFF);
// Color of close Button in Titlebar (pressed + disabled)
static constexpr QColor shellCaptionCloseFillColorSecondary(0xC4,0x2B,0x1C,0xE6);
static constexpr QColor shellCaptionCloseTextFillColorSecondary(0xFF,0xFF,0xFF,0xB3);


#if QT_CONFIG(toolbutton)
static void drawArrow(const QStyle *style, const QStyleOptionToolButton *toolbutton,
                      const QRect &rect, QPainter *painter, const QWidget *widget = nullptr)
{
    QStyle::PrimitiveElement pe;
    switch (toolbutton->arrowType) {
    case Qt::LeftArrow:
        pe = QStyle::PE_IndicatorArrowLeft;
        break;
    case Qt::RightArrow:
        pe = QStyle::PE_IndicatorArrowRight;
        break;
    case Qt::UpArrow:
        pe = QStyle::PE_IndicatorArrowUp;
        break;
    case Qt::DownArrow:
        pe = QStyle::PE_IndicatorArrowDown;
        break;
    default:
        return;
    }
    QStyleOption arrowOpt = *toolbutton;
    arrowOpt.rect = rect;
    style->drawPrimitive(pe, &arrowOpt, painter, widget);
}
#endif // QT_CONFIG(toolbutton)

static qreal radioButtonInnerRadius(QStyle::State state)
{
    qreal radius = 7.0;
    if (state & QStyle::State_Sunken)
        radius = 4.0f;
    else if (state & QStyle::State_MouseOver && !(state & QStyle::State_On))
        radius = 7.0f;
    else if (state & QStyle::State_MouseOver && (state & QStyle::State_On))
        radius = 5.0f;
    else if (state & QStyle::State_On)
        radius = 4.0f;
    return radius;
}

static qreal sliderInnerRadius(QStyle::State state, bool insideHandle)
{
    const bool isEnabled = state & QStyle::State_Enabled;
    if (isEnabled) {
        if (state & QStyle::State_Sunken)
            return 0.29;
        else if (insideHandle)
            return 0.71;
    }
    return 0.43;
}
/*!
  \class ElevenStyle
  \brief The ElevenStyle class provides a look and feel suitable for applications on Microsoft Windows 11.
  \since 6.6
  \ingroup appearance
  \inmodule QtWidgets
  \internal

  \warning This style is only available on the Windows 11 platform and above.

  \sa ElevenStyle QWindowsVistaStyle, QMacStyle, QFusionStyle
*/

/*!
  Constructs a ElevenStyle object.
*/
ElevenStyle::ElevenStyle()
    : QProxyStyle(QStyleFactory::keys().contains(u"breeze", Qt::CaseInsensitive)
                      ? QStyleFactory::create(QStringLiteral("breeze"))
                      : QStyleFactory::create(QStringLiteral("fusion")))
{
    setObjectName(QStringLiteral("eleven-kde"));
    updateColorScheme();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this] { updateColorScheme(); });
}

void ElevenStyle::updateColorScheme()
{
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    highContrastTheme = false;
    if (scheme == Qt::ColorScheme::Unknown)
        colorSchemeIndex = QGuiApplication::palette().window().color().lightness() < 128 ? 1 : 0;
    else
        colorSchemeIndex = scheme == Qt::ColorScheme::Light ? 0 : 1;
}

ElevenStyle::~ElevenStyle() = default;

/*!
  \internal
  see drawPrimitive for comments on the animation support

 */
void ElevenStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                         QPainter *painter, const QWidget *widget) const
{

    State state = option->state;
    SubControls sub = option->subControls;
    State flags = option->state;
    if (widget && widget->testAttribute(Qt::WA_UnderMouse) && widget->isActiveWindow())
        flags |= State_MouseOver;

    QPainterStateGuard psg(painter);
    painter->setRenderHint(QPainter::Antialiasing);
    if (transitionsEnabled() && option->styleObject) {
        if (control == CC_Slider) {
            if (const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
                QObject *styleObject = option->styleObject; // Can be widget or qquickitem

                QRectF thumbRect = proxy()->subControlRect(CC_Slider, option, SC_SliderHandle, widget);
                const qreal outerRadius = qMin(8.0, (slider->orientation == Qt::Horizontal ? thumbRect.height() / 2.0 : thumbRect.width() / 2.0) - 1);
                bool isInsideHandle = option->activeSubControls == SC_SliderHandle;

                bool oldIsInsideHandle = styleObject->property("_q_insidehandle").toBool();
                State oldState = State(styleObject->property("_q_stylestate").toInt());
                SubControls oldActiveControls = SubControls(styleObject->property("_q_stylecontrols").toInt());

                QRectF oldRect = styleObject->property("_q_stylerect").toRect();
                styleObject->setProperty("_q_insidehandle", isInsideHandle);
                styleObject->setProperty("_q_stylestate", int(state));
                styleObject->setProperty("_q_stylecontrols", int(option->activeSubControls));
                styleObject->setProperty("_q_stylerect", option->rect);
                if (option->styleObject->property("_q_end_radius").isNull())
                    option->styleObject->setProperty("_q_end_radius", outerRadius * 0.43);

                bool doTransition = (((state & State_Sunken) != (oldState & State_Sunken)
                                     || (oldIsInsideHandle != isInsideHandle)
                                     || (oldActiveControls != option->activeSubControls))
                                     && state & State_Enabled);

                if (oldRect != option->rect) {
                    doTransition = false;
                    stopAnimation(styleObject);
                    styleObject->setProperty("_q_inner_radius", outerRadius * 0.43);
                }

                if (doTransition) {
                    QNumberStyleAnimation *t = new QNumberStyleAnimation(styleObject);
                    t->setStartValue(styleObject->property("_q_inner_radius").toFloat());
                    t->setEndValue(outerRadius * sliderInnerRadius(state, isInsideHandle));
                    styleObject->setProperty("_q_end_radius", t->endValue());

                    t->setStartTime(animationTime());
                    t->setDuration(150);
                    startAnimation(t);
                }
            }
        }
    }

    switch (control) {
#if QT_CONFIG(spinbox)
    case CC_SpinBox:
        if (const QStyleOptionSpinBox *sb = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            QCachedPainter cp(painter, QLatin1StringView("win11_spinbox") % HexString<uint8_t>(colorSchemeIndex),
                              sb, sb->rect.size());
            if (cp.needsPainting()) {
                const auto frameRect = QRectF(option->rect).marginsRemoved(QMarginsF(1.5, 1.5, 1.5, 1.5));
                drawRoundedRect(cp.painter(), frameRect, Qt::NoPen, inputFillBrush(option, widget));

                if (sb->frame && (sub & SC_SpinBoxFrame))
                    drawLineEditFrame(cp.painter(), frameRect, option);

                const auto drawUpDown = [&](QStyle::SubControl sc) {
                    const bool isEnabled = state & QStyle::State_Enabled;
                    const bool isUp = sc == SC_SpinBoxUp;
                    const QRect rect = proxy()->subControlRect(CC_SpinBox, option, sc, widget);
                    if (isEnabled && sb->activeSubControls & sc)
                        drawRoundedRect(cp.painter(), rect.adjusted(1, 1, -1, -2), Qt::NoPen,
                                        winUI3Color(subtleHighlightColor));

                    cp->setFont(assetFont);
                    cp->setPen(sb->palette.buttonText().color());
                    cp->setBrush(Qt::NoBrush);
                    drawGlyph(cp.painter(), rect, isUp ? Icon::ChevronUp : Icon::ChevronDown);
                };
                if (sub & SC_SpinBoxUp) drawUpDown(SC_SpinBoxUp);
                if (sub & SC_SpinBoxDown) drawUpDown(SC_SpinBoxDown);
                if (state & State_KeyboardFocusChange && state & State_HasFocus) {
                    QStyleOptionFocusRect fropt;
                    fropt.QStyleOption::operator=(*option);
                    proxy()->drawPrimitive(PE_FrameFocusRect, &fropt, cp.painter(), widget);
                }
            }
        }
        break;
#endif // QT_CONFIG(spinbox)
#if QT_CONFIG(slider)
    case CC_Slider:
        if (const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            const auto &slrect = slider->rect;
            const bool isHorizontal = slider->orientation == Qt::Horizontal;
            const QRectF handleRect(proxy()->subControlRect(CC_Slider, option, SC_SliderHandle, widget));
            const QPointF handleCenter(handleRect.center());

            if (sub & SC_SliderGroove) {
                QRectF rect = proxy()->subControlRect(CC_Slider, option, SC_SliderGroove, widget);
                QRectF leftRect;
                QRectF rightRect;

                if (isHorizontal) {
                    rect = QRectF(rect.left() + 2, rect.center().y() - 2, rect.width() - 2, 4);
                    leftRect = QRectF(rect.left(), rect.top(), handleCenter.x() - rect.left(),
                                      rect.height());
                    rightRect = QRectF(handleCenter.x(), rect.top(),
                                       rect.width() - handleCenter.x(),
                                       rect.height());
                } else {
                    rect = QRect(rect.center().x() - 2, rect.top() + 2, 4, rect.height() - 2);
                    leftRect = QRectF(rect.left(), rect.top(), rect.width(),
                                      handleCenter.y() - rect.top());
                    rightRect = QRectF(rect.left(), handleCenter.y(), rect.width(),
                                       rect.height() - handleCenter.y());
                }
                if (slider->upsideDown)
                    qSwap(leftRect, rightRect);

                painter->setPen(Qt::NoPen);
                painter->setBrush(calculateAccentColor(option));
                painter->drawRoundedRect(leftRect,1,1);
                painter->setBrush(WINUI3Colors[colorSchemeIndex][controlStrongFill]);
                painter->drawRoundedRect(rightRect,1,1);
            }
            if (sub & SC_SliderTickmarks) {
                int tickOffset = proxy()->pixelMetric(PM_SliderTickmarkOffset, slider, widget);
                int ticks = slider->tickPosition;
                int thickness = proxy()->pixelMetric(PM_SliderControlThickness, slider, widget);
                int len = proxy()->pixelMetric(PM_SliderLength, slider, widget);
                int available = proxy()->pixelMetric(PM_SliderSpaceAvailable, slider, widget);
                int interval = slider->tickInterval;
                if (interval <= 0) {
                    interval = slider->singleStep;
                    if (QStyle::sliderPositionFromValue(slider->minimum, slider->maximum, interval,
                                                        available)
                                - QStyle::sliderPositionFromValue(slider->minimum, slider->maximum,
                                                                  0, available) < 3)
                        interval = slider->pageStep;
                }
                if (!interval)
                    interval = 1;
                int fudge = len / 2;
                painter->setPen(slider->palette.text().color());
                QVarLengthArray<QLineF, 32> lines;
                int v = slider->minimum;
                while (v <= slider->maximum + 1) {
                    if (v == slider->maximum + 1 && interval == 1)
                        break;
                    const int v_ = qMin(v, slider->maximum);
                    int tickLength = (v_ == slider->minimum || v_ >= slider->maximum) ? 4 : 3;
                    int pos = QStyle::sliderPositionFromValue(slider->minimum, slider->maximum, v_,
                                                              available, slider->upsideDown);
                    pos += fudge;
                    if (isHorizontal) {
                        if (ticks & QSlider::TicksAbove) {
                            lines.append(QLineF(pos, tickOffset - 0.5,
                                                pos, tickOffset - tickLength - 0.5));
                        }

                        if (ticks & QSlider::TicksBelow) {
                            lines.append(QLineF(pos, tickOffset + thickness + 0.5,
                                                pos, tickOffset + thickness + tickLength + 0.5));
                        }
                    } else {
                        if (ticks & QSlider::TicksAbove) {
                            lines.append(QLineF(tickOffset - 0.5, pos,
                                                tickOffset - tickLength - 0.5, pos));
                        }

                        if (ticks & QSlider::TicksBelow) {
                            lines.append(QLineF(tickOffset + thickness + 0.5, pos,
                                                tickOffset + thickness + tickLength + 0.5, pos));
                        }
                    }
                    // in the case where maximum is max int
                    int nextInterval = v + interval;
                    if (nextInterval < v)
                        break;
                    v = nextInterval;
                }
                if (!lines.isEmpty()) {
                    QPainterStateGuard psg(painter);
                    painter->translate(slrect.topLeft());
                    painter->drawLines(lines.constData(), lines.size());
                }
            }
            if (sub & SC_SliderHandle) {
                const qreal outerRadius = qMin(8.0, (isHorizontal ? handleRect.height() / 2.0 : handleRect.width() / 2.0) - 1);
                float innerRadius = outerRadius * 0.43;

                if (option->styleObject) {
                    const QNumberStyleAnimation* animation = qobject_cast<QNumberStyleAnimation *>(this->animation(option->styleObject));
                    if (animation != nullptr) {
                        innerRadius = animation->currentValue();
                        option->styleObject->setProperty("_q_inner_radius", innerRadius);
                    } else {
                        bool isInsideHandle = option->activeSubControls == SC_SliderHandle;
                        innerRadius = outerRadius * sliderInnerRadius(state, isInsideHandle);
                    }
                }

                painter->setPen(Qt::NoPen);
                painter->setBrush(winUI3Color(controlFillSolid));
                painter->drawEllipse(handleCenter, outerRadius, outerRadius);
                painter->setBrush(calculateAccentColor(option));
                painter->drawEllipse(handleCenter, innerRadius, innerRadius);

                painter->setPen(winUI3Color(controlStrokeSecondary));
                painter->setBrush(Qt::NoBrush);
                painter->drawEllipse(handleCenter, outerRadius + 0.5, outerRadius + 0.5);
            }
            if (slider->state & State_HasFocus) {
                QStyleOptionFocusRect fropt;
                fropt.QStyleOption::operator=(*slider);
                fropt.rect = subElementRect(SE_SliderFocusRect, slider, widget);
                proxy()->drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
            }
        }
        break;
#endif
#if QT_CONFIG(combobox)
    case CC_ComboBox:
        if (const QStyleOptionComboBox *combobox = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            const auto frameRect = QRectF(option->rect).marginsRemoved(QMarginsF(1.5, 1.5, 1.5, 1.5));
            QStyleOption opt(*option);
            opt.state.setFlag(QStyle::State_On, false);
            drawRoundedRect(painter, frameRect, Qt::NoPen,
                            combobox->editable ? inputFillBrush(option, widget)
                                               : controlFillBrush(&opt, ControlType::Control));

            if (combobox->frame)
                drawLineEditFrame(painter, frameRect, combobox, combobox->editable, inUrlNavigator(widget));

            const bool hasFocus = state & State_HasFocus;

            if (sub & SC_ComboBoxArrow) {
                QRectF rect = proxy()->subControlRect(CC_ComboBox, option, SC_ComboBoxArrow, widget);
                painter->setFont(assetFont);
                painter->setPen(controlTextColor(option));
                drawGlyph(painter, rect, Icon::ChevronDownMed);
            }
            if (state & State_KeyboardFocusChange && hasFocus) {
                QStyleOptionFocusRect fropt;
                fropt.QStyleOption::operator=(*option);
                proxy()->drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
            }
        }
        break;
#endif // QT_CONFIG(combobox)
    case CC_ScrollBar:
        if (const QStyleOptionSlider *scrollbar = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            const bool vertical = scrollbar->orientation == Qt::Vertical;
            const bool horizontal = scrollbar->orientation == Qt::Horizontal;
            const bool isMouseOver = state & State_MouseOver;
            const bool isRtl = option->direction == Qt::RightToLeft;

            // hover factor t: 0 = idle (thin line), 1 = hovered (expanded track); animated between the two
            qreal t = isMouseOver ? 1.0 : 0.0;
            if (transitionsEnabled() && option->styleObject) {
                QObject *styleObject = option->styleObject;
                const State oldState = State(styleObject->property("_q_stylestate").toInt());
                if (oldState.testFlag(State_MouseOver) != isMouseOver) {
                    styleObject->setProperty("_q_stylestate", int(state));
                    constexpr int durationMS = 120;
                    qreal startValue = isMouseOver ? 0 : 1;
                    int curDurationMS = durationMS;
                    if (auto *running = qobject_cast<QNumberStyleAnimation *>(animation(styleObject))) {
                        startValue = running->currentValue();
                        curDurationMS = qMax(20, int(durationMS * (isMouseOver ? 1 - startValue : startValue)));
                        stopAnimation(styleObject);
                    }
                    auto *anim = new QNumberStyleAnimation(styleObject);
                    anim->setStartValue(startValue);
                    anim->setEndValue(isMouseOver ? 1 : 0);
                    anim->setDuration(curDurationMS);
                    startAnimation(anim);
                }
                if (auto *anim = qobject_cast<QNumberStyleAnimation *>(animation(styleObject)))
                    t = qBound<qreal>(0.0, anim->currentValue(), 1.0);
            }

            QCachedPainter cp(painter, QLatin1StringView("win11_scrollbar")
                                       % HexString<uint8_t>(colorSchemeIndex)
                                       % HexString<int>(scrollbar->minimum)
                                       % HexString<int>(scrollbar->maximum)
                                       % HexString<int>(scrollbar->sliderPosition)
                                       % HexString<int>(qRound(t * 24)),
                              scrollbar, scrollbar->rect.size());
            if (cp.needsPainting()) {
                if (t > 0) {
                    // expanded track: translucent overlay fading in, no border
                    QRectF rect = scrollbar->rect;
                    const QPointF center = rect.center();
                    if (vertical && rect.width() > 24)
                        rect.setWidth(rect.width() / 2);
                    else if (horizontal && rect.height() > 24)
                        rect.setHeight(rect.height() / 2);
                    rect.moveCenter(center);
                    QColor track = winUI3Color(subtleHighlightColor);
                    track.setAlphaF(track.alphaF() * t);
                    cp->setBrush(track);
                    cp->setPen(Qt::NoPen);
                    cp->drawRoundedRect(rect, topLevelRoundingRadius, topLevelRoundingRadius);
                }
                if (sub & SC_ScrollBarSlider) {
                    QRectF rect = proxy()->subControlRect(CC_ScrollBar, option, SC_ScrollBarSlider, widget);
                    const QPointF center = rect.center();
                    if (vertical)
                        rect.setWidth(1 + (rect.width() / 2 - 1) * t);
                    else
                        rect.setHeight(1 + (rect.height() / 2 - 1) * t);
                    rect.moveCenter(center);
                    cp->setBrush(Qt::gray);
                    cp->setPen(Qt::NoPen);
                    cp->drawRoundedRect(rect, secondLevelRoundingRadius, secondLevelRoundingRadius);
                }
                if (t > 0 && (sub & (SC_ScrollBarAddLine | SC_ScrollBarSubLine))) {
                    cp->setOpacity(t);
                    QFont f = QFont(assetFont);
                    f.setPointSize(6);
                    cp->setFont(f);
                    cp->setPen(Qt::gray);
                    if (sub & SC_ScrollBarAddLine) {
                        const QRectF rect = proxy()->subControlRect(CC_ScrollBar, option, SC_ScrollBarAddLine, widget);
                        drawGlyph(cp.painter(), rect, vertical ? Icon::CaretDownSolid8
                                                               : (isRtl ? Icon::CaretLeftSolid8 : Icon::CaretRightSolid8));
                    }
                    if (sub & SC_ScrollBarSubLine) {
                        const QRectF rect = proxy()->subControlRect(CC_ScrollBar, option, SC_ScrollBarSubLine, widget);
                        drawGlyph(cp.painter(), rect, vertical ? Icon::CaretUpSolid8
                                                               : (isRtl ? Icon::CaretRightSolid8 : Icon::CaretLeftSolid8));
                    }
                }
            }
        }
        break;
#if QT_CONFIG(toolbutton)
    case CC_ToolButton:
        if (const auto toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            auto prx = proxy();
            const bool isSplitButton =
                    toolbutton->features.testFlag(QStyleOptionToolButton::MenuButtonPopup);
            const auto fw = prx->pixelMetric(PM_DefaultFrameWidth, option, widget);
            const auto buttonRect = prx->subControlRect(control, toolbutton, SC_ToolButton, widget);
            const auto menuareaRect = isSplitButton
                    ? prx->subControlRect(control, toolbutton, SC_ToolButtonMenu, widget)
                    : QRect();

            State bflags = toolbutton->state;
            State mflags = toolbutton->state;
            if (toolbutton->activeSubControls.testFlag(SC_ToolButton)) {
                mflags &= ~(State_Sunken | State_MouseOver);
                if (bflags.testFlag(State_Sunken))
                    mflags |= State_Raised;
            }
            if (toolbutton->activeSubControls.testFlag(SC_ToolButtonMenu)) {
                bflags &= ~(State_Sunken | State_MouseOver);
                if (mflags.testFlag(State_Sunken))
                    bflags |= State_Raised;
            }

            QStyleOption tool = *toolbutton;
            if (toolbutton->subControls.testFlag(SC_ToolButton)) {
                QPainterStateGuard psg(painter);
                if (isSplitButton)
                    painter->setClipRect(buttonRect);
                tool.state = bflags;
                prx->drawPrimitive(PE_PanelButtonTool, &tool, painter, widget);
            }

            // focus outline only when the focus came from the keyboard, not after a mouse click
            if (toolbutton->state.testFlag(State_HasFocus) && toolbutton->state.testFlag(State_KeyboardFocusChange)) {
                QStyleOptionFocusRect fr;
                fr.QStyleOption::operator=(*toolbutton);
                prx->drawPrimitive(PE_FrameFocusRect, &fr, painter, widget);
            }
            QStyleOptionToolButton label = *toolbutton;
            label.state = bflags;
            label.rect = buttonRect.marginsRemoved(QMargins(fw, fw, fw, fw));

            // Only the split button's own menu area gets a chevron (it is a separate highlightable area);
            // the "has a menu" corner indicator on other tool buttons is intentionally not drawn.
            if (toolbutton->subControls.testFlag(SC_ToolButtonMenu)) {
                QPainterStateGuard psg(painter);
                painter->setClipRect(menuareaRect);
                tool.state = mflags;
                prx->drawPrimitive(PE_PanelButtonTool, &tool, painter, widget);

                QFont f = painter->font();
                f.setPixelSize(qMax(1, qRound(QFontInfo(f).pixelSize() * 0.9)));
                painter->setFont(f);
                painter->setPen(controlTextColor(option));
                const QRect textRect(menuareaRect.topLeft(), menuareaRect.size() - QSize(fw, 0));
                drawGlyph(painter, textRect, Icon::ChevronDownMed);
            }
            prx->drawControl(CE_ToolButtonLabel, &label, painter, widget);
        }
        break;
#endif // QT_CONFIG(toolbutton)
    default:
        QProxyStyle::drawComplexControl(control, option, painter, widget);
    }
}

void ElevenStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                    QPainter *painter,
                                    const QWidget *widget) const {
    const State state = option->state;
    QPainterStateGuard psg(painter);
    painter->setRenderHint(QPainter::Antialiasing);
    if (transitionsEnabled() && option->styleObject && (element == PE_IndicatorCheckBox || element == PE_IndicatorRadioButton)) {
        QObject *styleObject = option->styleObject; // Can be widget or qquickitem
        int oldState = styleObject->property("_q_stylestate").toInt();
        styleObject->setProperty("_q_stylestate", int(option->state));
        styleObject->setProperty("_q_stylerect", option->rect);
        bool doTransition = (((state & State_Sunken) != (oldState & State_Sunken)
                             || ((state & State_MouseOver) != (oldState & State_MouseOver))
                             || (state & State_On) != (oldState & State_On))
                             && state & State_Enabled);
        if (doTransition) {
            if (element == PE_IndicatorRadioButton) {
                QNumberStyleAnimation *t = new QNumberStyleAnimation(styleObject);
                t->setStartValue(styleObject->property("_q_inner_radius").toFloat());
                t->setEndValue(radioButtonInnerRadius(state));
                styleObject->setProperty("_q_end_radius", t->endValue());
                t->setStartTime(animationTime());
                t->setDuration(150);
                startAnimation(t);
            }
            else if (element == PE_IndicatorCheckBox) {
                if ((oldState & State_Off && state & State_On) || (oldState & State_NoChange && state & State_On)) {
                    QNumberStyleAnimation *t = new QNumberStyleAnimation(styleObject);
                    t->setStartValue(0.0f);
                    t->setEndValue(1.0f);
                    t->setStartTime(animationTime());
                    t->setDuration(150);
                    startAnimation(t);
                }
            }
        }
    }

    switch (element) {
    case PE_IndicatorArrowUp:
    case PE_IndicatorArrowDown:
    case PE_IndicatorArrowRight:
    case PE_IndicatorArrowLeft: {
        const QRect &r = option->rect;
        if (r.width() <= 1 || r.height() <= 1)
            break;
        Icon ico = Icon::Help;
        switch (element) {
        case PE_IndicatorArrowUp:
            ico = Icon::ChevronUpMed;
            break;
        case PE_IndicatorArrowDown:
            ico = Icon::ChevronDownMed;
            break;
        case PE_IndicatorArrowLeft:
            ico = Icon::ChevronLeftMed;
            break;
        case PE_IndicatorArrowRight:
            ico = Icon::ChevronRightMed;
            break;
        default:
            break;
        }
        QPainterStateGuard psg(painter);
        if (option->state.testFlag(State_Sunken)) {
            const auto bsx = proxy()->pixelMetric(PM_ButtonShiftHorizontal, option, widget);
            const auto bsy = proxy()->pixelMetric(PM_ButtonShiftVertical, option, widget);
            if (bsx != 0 || bsy != 0)
                painter->translate(bsx, bsy);
        }
        painter->setFont(assetFont);
        painter->setPen(option->palette.buttonText().color());
        painter->setBrush(option->palette.buttonText());
        drawGlyph(painter, option->rect, ico);
        break;
    }
    case PE_FrameFocusRect: {
        if (const QStyleOptionFocusRect *fropt = qstyleoption_cast<const QStyleOptionFocusRect *>(option)) {
            if (!(fropt->state & State_KeyboardFocusChange))
                break;
            // gray focus ring, never accent colored
            const QRectF focusRect = QRectF(option->rect).marginsRemoved(QMarginsF(1.5, 1.5, 1.5, 1.5));
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(colorSchemeIndex == 1 ? QColor(0xFF, 0xFF, 0xFF, 110) : QColor(0x00, 0x00, 0x00, 110), 1.5));
            painter->drawRoundedRect(focusRect, 4, 4);
        }
        break;
    }
    case PE_PanelTipLabel: {
        const auto rect = QRectF(option->rect).marginsRemoved(QMarginsF(0.5, 0.5, 0.5, 0.5));
        const auto pen = highContrastTheme ? option->palette.buttonText().color()
                                           : winUI3Color(frameColorLight);
        drawRoundedRect(painter, rect, pen, option->palette.toolTipBase());
        break;
    }
    case PE_FrameTabWidget:
    case PE_FrameTabBarBase:
        break; // no frame, no strip: the tab bar and the content share the window color
    case PE_FrameGroupBox:
        if (const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            const auto pen = highContrastTheme ? frame->palette.buttonText().color()
                                               : winUI3Color(frameColorStrong);
            if (frame->features & QStyleOptionFrame::Flat) {
                painter->setBrush(Qt::NoBrush);
                painter->setPen(pen);
                const QRect &fr = frame->rect;
                QPoint p1(fr.x(), fr.y() + 1);
                QPoint p2(fr.x() + fr.width(), p1.y());
                painter->drawLine(p1, p2);
            } else {
                const auto frameRect = QRectF(frame->rect).marginsRemoved(QMarginsF(1.5, 1.5, 1.5, 1.5));
                drawRoundedRect(painter, frameRect, pen, Qt::NoBrush);
            }
        }
        break;
    case PE_IndicatorHeaderArrow:
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            const auto indicator = header->sortIndicator;
            if (indicator != QStyleOptionHeader::None) {
                QPainterStateGuard psg(painter);
                QFont f(assetFont);
                f.setPointSize(6);
                painter->setFont(f);
                painter->setPen(header->palette.text().color());
                const auto ico = indicator == QStyleOptionHeader::SortUp ? Icon::ChevronDown
                                                                         : Icon::ChevronUp;
                drawGlyph(painter, option->rect, ico);
            }
        }
        break;
    case PE_IndicatorCheckBox: {
            const bool isOn = option->state & State_On;
            const bool isPartial = option->state & State_NoChange;

            // 16px box centered in the (18px) indicator rect; the stroke is inset by half its width so it is
            // drawn completely inside (item-clipped QML indicators cut the outer half off otherwise)
            QRectF rect(0, 0, 16, 16);
            rect.moveCenter(QRectF(option->rect).center());
            const QPointF center = rect.center();

            drawRoundedRect(painter, rect.adjusted(0.5, 0.5, -0.5, -0.5), borderPenControlAlt(option),
                            controlFillBrush(option, ControlType::ControlAlt));

            if (isOn) {
                QFont f(assetFont);
                f.setPixelSize(qRound(rect.height() * 0.62));
                painter->setFont(f);
                painter->setPen(controlTextColor(option, QPalette::Window));
                qreal clipWidth = 1.0;
                if (transitionsEnabled() && option->styleObject) {
                    QNumberStyleAnimation *animation = qobject_cast<QNumberStyleAnimation *>(
                            this->animation(option->styleObject));
                    if (animation)
                        clipWidth = animation->currentValue();
                }
                painter->setClipRect(QRectF(rect.x(), rect.y(), rect.width() * clipWidth, rect.height()));
                drawGlyph(painter, rect, Icon::AcceptMedium);
            } else if (isPartial) {
                QFont f(assetFont);
                f.setPixelSize(qRound(rect.height() * 0.5));
                painter->setFont(f);
                painter->setPen(controlTextColor(option, QPalette::Window));
                drawGlyph(painter, rect, Icon::Dash12);
            }
        }
        break;
    case PE_IndicatorBranch: {
            if (option->state & State_Children) {
                const bool isReverse = option->direction == Qt::RightToLeft;
                const bool isOpen = option->state & QStyle::State_Open;
                QFont f(assetFont);
                f.setPointSize(8);
                painter->setFont(f);
                painter->setPen(option->palette.color(isOpen ? QPalette::Active : QPalette::Disabled,
                                                      QPalette::WindowText));
                const auto ico = isOpen ? Icon::ChevronDownMed
                                        : (isReverse ? Icon::ChevronLeftMed
                                                     : Icon::ChevronRightMed);
                drawGlyph(painter, option->rect, ico);
            }
        }
        break;
    case PE_IndicatorRadioButton: {
            const bool isOn = option->state & State_On;
            qreal innerRadius = radioButtonInnerRadius(state);
            if (transitionsEnabled() && option->styleObject) {
                if (option->styleObject->property("_q_end_radius").isNull())
                    option->styleObject->setProperty("_q_end_radius", innerRadius);
                QNumberStyleAnimation *animation = qobject_cast<QNumberStyleAnimation *>(this->animation(option->styleObject));
                innerRadius = animation ? animation->currentValue() : option->styleObject->property("_q_end_radius").toFloat();
                option->styleObject->setProperty("_q_inner_radius", innerRadius);
            }

            const QRectF rect = option->rect;
            const QPointF center = rect.center();

            painter->setPen(borderPenControlAlt(option));
            painter->setBrush(controlFillBrush(option, ControlType::ControlAlt));
            if (isOn) {
                QPainterPath path;
                path.addEllipse(center, 7.5, 7.5);
                path.addEllipse(center, innerRadius, innerRadius);
                painter->drawPath(path);
                // Text On Accent/Primary
                painter->setBrush(option->palette.window().color());
                painter->drawEllipse(center, innerRadius, innerRadius);
            } else {
                painter->drawEllipse(center, 7.5, 7.5);
            }
        }
        break;
    case PE_PanelButtonTool:
    case PE_PanelButtonBevel:{
            const bool isEnabled = state & QStyle::State_Enabled;
            const bool isMouseOver = state & QStyle::State_MouseOver;
            const bool isRaised = state & QStyle::State_Raised;
            const bool flatTool = element == PE_PanelButtonTool && (state & State_AutoRaise);
            const int inset = flatTool ? 0 : 2; // flat tool buttons: highlight fills the whole button
            QRectF rect = option->rect.marginsRemoved(QMargins(inset, inset, inset, inset));
            const bool subtle = flatTool;
            painter->setPen(Qt::NoPen); // buttons have no border, only a fill
            if (element == PE_PanelButtonTool && isEnabled && (state & (State_On | State_NoChange)))
                painter->setBrush(colorSchemeIndex == 1 ? QColor(0xFF, 0xFF, 0xFF, 64)
                                                        : winUI3Color(fillControlSecondary)); // checked / menu open: gray, not accent
            else
                painter->setBrush(controlFillBrush(option, ControlType::Control));
            painter->drawRoundedRect(rect,
                                     secondLevelRoundingRadius, secondLevelRoundingRadius);

            if (false && isRaised && !subtle) { // raised bottom line removed together with the border
                const qreal sublineOffset = secondLevelRoundingRadius - 0.5;
                painter->setPen(WINUI3Colors[colorSchemeIndex][controlStrokeSecondary]);
                painter->drawLine(rect.bottomLeft() + QPointF(sublineOffset, 0.5), rect.bottomRight() + QPointF(-sublineOffset, 0.5));
            }
        }
        break;
    case PE_FrameDefaultButton:
        break; // no blue default-button frame
    case PE_PanelButtonCommand:
        // Called directly by custom widgets (e.g. Dolphin's breadcrumb buttons) instead of CE_PushButtonBevel;
        // without this case Breeze draws it, including its blue focus outline.
        if (const auto *btn = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            if (btn->features.testFlag(QStyleOptionButton::Flat)) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(state & (State_Sunken | State_On) ? winUI3Color(subtlePressedColor)
                                                                     : winUI3Color(subtleHighlightColor));
                painter->drawRoundedRect(QRectF(option->rect).marginsRemoved(QMarginsF(1, 1, 1, 1)),
                                         secondLevelRoundingRadius, secondLevelRoundingRadius);
            } else {
                proxy()->drawControl(CE_PushButtonBevel, option, painter, widget);
            }
        }
        break;
    case PE_FrameMenu:
        break;
    case PE_PanelMenu: {
        const QRect rect = option->rect.marginsRemoved(QMargins(2, 2, 2, 2));
        painter->setPen(highContrastTheme ? QPen(option->palette.windowText().color(), 2)
                                          : QPen(Qt::NoPen));
        painter->setBrush(menuFillColor(option));
        painter->drawRoundedRect(rect, topLevelRoundingRadius, topLevelRoundingRadius);
        break;
    }
    case PE_PanelLineEdit:
        if (const auto *panel = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            const bool isInSpinBox =
                    widget && qobject_cast<const QAbstractSpinBox *>(widget->parent()) != nullptr;
            const bool isInComboBox =
                    widget && qobject_cast<const QComboBox *>(widget->parent()) != nullptr;
            if (!isInSpinBox && !isInComboBox) {
                const auto frameRect =
                        QRectF(option->rect).marginsRemoved(QMarginsF(1.5, 1.5, 1.5, 1.5));
                drawRoundedRect(painter, frameRect, Qt::NoPen, inputFillBrush(option, widget));
                proxy()->drawPrimitive(PE_FrameLineEdit, panel, painter, widget); // always visible, even for frameless edits
            }
        }
        break;
    case PE_FrameLineEdit: {
        const auto frameRect = QRectF(option->rect).marginsRemoved(QMarginsF(1.5, 1.5, 1.5, 1.5));
        drawLineEditFrame(painter, frameRect, option, true, inUrlNavigator(widget));
        if (state & State_KeyboardFocusChange && state & State_HasFocus) {
            QStyleOptionFocusRect fropt;
            fropt.QStyleOption::operator=(*option);
            proxy()->drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
        }
        break;
    }
    case PE_Frame: {
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            const auto rect = QRectF(option->rect).marginsRemoved(QMarginsF(1.5, 1.5, 1.5, 1.5));
            if (qobject_cast<const QComboBoxPrivateContainer *>(widget)) {
                QPen pen;
                if (highContrastTheme)
                    pen = QPen(option->palette.windowText().color(), 2);
                else
                    pen = Qt::NoPen;
                drawRoundedRect(painter, rect, pen, menuFillColor(option));
            } else
                drawRoundedRect(painter, rect, Qt::NoPen, option->palette.brush(QPalette::Base));

            if (frame->frameShape == QFrame::NoFrame)
                break;

            const bool isEditable = qobject_cast<const QTextEdit *>(widget) != nullptr
                    || qobject_cast<const QPlainTextEdit *>(widget) != nullptr;
            drawLineEditFrame(painter, rect, option, isEditable);
        }
        break;
    }
    case PE_PanelItemViewItem:
        if (const QStyleOptionViewItem *vopt = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            // Custom delegates (e.g. Dolphin's places panel) only call this primitive and expect it to paint the
            // hover/selection highlight like Breeze does. Our own CE_ItemViewItem paints its highlight itself.
            if (!m_inItemViewItem && vopt->state.testAnyFlags(State_Selected | State_MouseOver)
                    && vopt->state.testFlag(State_Enabled)) {
                QPainterStateGuard psg(painter);
                painter->setRenderHint(QPainter::Antialiasing);
                const bool selected = vopt->state.testFlag(State_Selected);
                painter->setPen(Qt::NoPen);
                painter->setBrush(selected ? (colorSchemeIndex == 1 ? QColor(0xFF, 0xFF, 0xFF, 34)
                                                                    : winUI3Color(fillControlSecondary))
                                           : QBrush(winUI3Color(subtleHighlightColor)));
                // the places delegate draws its icon at a fixed rect.left()+4, so the highlight starts at the item
                // edge (4px of padding left of the icon) and the accent marker sits at the very left
                const bool rtl = option->direction == Qt::RightToLeft;
                const QRectF r = QRectF(vopt->rect).marginsRemoved(rtl ? QMarginsF(2, 2, 0, 2) : QMarginsF(0, 2, 2, 2));
                painter->drawRoundedRect(r, secondLevelRoundingRadius, secondLevelRoundingRadius);
                if (selected) {
                    const QColor col = accentColor();
                    painter->setBrush(col);
                    const bool isRtl = option->direction == Qt::RightToLeft;
                    const qreal xPos = isRtl ? vopt->rect.right() - 1.5 : vopt->rect.left();
                    const qreal yOfs = vopt->rect.height() / 4.;
                    painter->drawRoundedRect(QRectF(QPointF(xPos, vopt->rect.y() + yOfs),
                                                    QPointF(xPos + 1.5, vopt->rect.bottom() - yOfs)), 0.75, 0.75);
                }
            }
            if (vopt->backgroundBrush.style() != Qt::NoBrush) {
                QPainterStateGuard psg(painter);
                painter->setBrushOrigin(vopt->rect.topLeft());
                painter->fillRect(vopt->rect, vopt->backgroundBrush);
            }
        }
        break;
    case PE_PanelItemViewRow:
        if (const QStyleOptionViewItem *vopt = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            // this is only called from a QTreeView to paint
            //  - the tree branch decoration (incl. selected/hovered or not)
            //  - the (alternate) background of the item in always unselected state
            const QRect &rect = vopt->rect;
            const bool isRtl = option->direction == Qt::RightToLeft;
            if (rect.width() <= 0)
                break;

            if (vopt->features & QStyleOptionViewItem::Alternate) {
                QPalette::ColorGroup cg =
                        (widget ? widget->isEnabled() : (vopt->state & QStyle::State_Enabled))
                        ? QPalette::Normal
                        : QPalette::Disabled;
                if (cg == QPalette::Normal && !(vopt->state & QStyle::State_Active))
                    cg = QPalette::Inactive;
                painter->fillRect(rect, option->palette.brush(cg, QPalette::AlternateBase));
            }

            if (option->state & State_Selected && !highContrastTheme) {
                // keep in sync with CE_ItemViewItem QListView indicator painting
                const auto col = accentColor();
                painter->setBrush(col);
                painter->setPen(col);
                const auto xPos = isRtl ? rect.right() - 4.5f : rect.left() + 3.5f;
                const auto yOfs = rect.height() / 4.;
                QRectF r(QPointF(xPos, rect.y() + yOfs),
                         QPointF(xPos + 1, rect.y() + rect.height() - yOfs));
                painter->drawRoundedRect(r, 1, 1);
            }

            const bool isTreeDecoration = vopt->features.testFlag(
                    QStyleOptionViewItem::IsDecorationForRootColumn);
            if (isTreeDecoration && vopt->state.testAnyFlags(State_Selected | State_MouseOver) &&
                vopt->showDecorationSelected) {
                const bool onlyOne = vopt->viewItemPosition == QStyleOptionViewItem::OnlyOne ||
                                     vopt->viewItemPosition == QStyleOptionViewItem::Invalid;
                bool isFirst = vopt->viewItemPosition == QStyleOptionViewItem::Beginning;
                bool isLast = vopt->viewItemPosition == QStyleOptionViewItem::End;

                if (onlyOne)
                    isFirst = true;

                if (isRtl) {
                    isFirst = !isFirst;
                    isLast = !isLast;
                }

                painter->setBrush(itemRowHighlight(colorSchemeIndex, vopt->state.testFlag(State_Selected)));
                painter->setPen(Qt::NoPen);
                if (isFirst) {
                    QPainterStateGuard psg(painter);
                    painter->setClipRect(rect);
                    painter->drawRoundedRect(rect.marginsRemoved(QMargins(2, 2, -secondLevelRoundingRadius, 2)),
                                             secondLevelRoundingRadius, secondLevelRoundingRadius);
                } else if (isLast) {
                    QPainterStateGuard psg(painter);
                    painter->setClipRect(rect);
                    painter->drawRoundedRect(rect.marginsRemoved(QMargins(-secondLevelRoundingRadius, 2, 2, 2)),
                                             secondLevelRoundingRadius, secondLevelRoundingRadius);
                } else {
                    painter->drawRect(vopt->rect.marginsRemoved(QMargins(0, 2, 0, 2)));
                }
            }
        }
        break;
    case QStyle::PE_Widget: {
        if (widget && widget->palette().isBrushSet(QPalette::Active, widget->backgroundRole())) {
            const QBrush bg = widget->palette().brush(widget->backgroundRole());
            auto wp = QWidgetPrivate::get(widget);
            QPainterStateGuard psg(painter);
            wp->updateBrushOrigin(painter, bg);
            painter->fillRect(option->rect, bg);
        }
        break;
    }
    case QStyle::PE_FrameWindow:
        if (const auto *frm = qstyleoption_cast<const QStyleOptionFrame *>(option)) {

            QRectF rect= option->rect;
            int fwidth = int(frm->lineWidth + frm->midLineWidth);

            QRectF bottomLeftCorner = QRectF(rect.left() + 1.0,
                                             rect.bottom() - 1.0 - secondLevelRoundingRadius,
                                             secondLevelRoundingRadius,
                                             secondLevelRoundingRadius);
            QRectF bottomRightCorner = QRectF(rect.right() - 1.0  - secondLevelRoundingRadius,
                                              rect.bottom() - 1.0  - secondLevelRoundingRadius,
                                              secondLevelRoundingRadius,
                                              secondLevelRoundingRadius);

            //Draw Mask
            if (widget != nullptr) {
                QBitmap mask(widget->width(), widget->height());
                mask.clear();

                QPainter maskPainter(&mask);
                maskPainter.setRenderHint(QPainter::Antialiasing);
                maskPainter.setBrush(Qt::color1);
                maskPainter.setPen(Qt::NoPen);
                maskPainter.drawRoundedRect(option->rect,secondLevelRoundingRadius,secondLevelRoundingRadius);
                const_cast<QWidget*>(widget)->setMask(mask);
            }

            //Draw Window
            painter->setPen(QPen(frm->palette.base(), fwidth));
            painter->drawLine(QPointF(rect.left(), rect.top()),
                              QPointF(rect.left(), rect.bottom() - fwidth));
            painter->drawLine(QPointF(rect.left() + fwidth, rect.bottom()),
                              QPointF(rect.right() - fwidth, rect.bottom()));
            painter->drawLine(QPointF(rect.right(), rect.top()),
                              QPointF(rect.right(), rect.bottom() - fwidth));

            painter->setPen(WINUI3Colors[colorSchemeIndex][surfaceStroke]);
            painter->drawLine(QPointF(rect.left() + 0.5, rect.top() + 0.5),
                              QPointF(rect.left() + 0.5, rect.bottom() - 0.5 - secondLevelRoundingRadius));
            painter->drawLine(QPointF(rect.left() + 0.5 + secondLevelRoundingRadius, rect.bottom() - 0.5),
                              QPointF(rect.right() - 0.5 - secondLevelRoundingRadius, rect.bottom() - 0.5));
            painter->drawLine(QPointF(rect.right() - 0.5, rect.top() + 1.5),
                              QPointF(rect.right() - 0.5, rect.bottom() - 0.5 - secondLevelRoundingRadius));

            painter->setPen(Qt::NoPen);
            painter->setBrush(frm->palette.base());
            painter->drawPie(bottomRightCorner.marginsAdded(QMarginsF(2.5,2.5,0.0,0.0)),
                             270 * 16,90 * 16);
            painter->drawPie(bottomLeftCorner.marginsAdded(QMarginsF(0.0,2.5,2.5,0.0)),
                             -90 * 16,-90 * 16);

            painter->setPen(WINUI3Colors[colorSchemeIndex][surfaceStroke]);
            painter->setBrush(Qt::NoBrush);
            painter->drawArc(bottomRightCorner,
                             0 * 16,-90 * 16);
            painter->drawArc(bottomLeftCorner,
                             -90 * 16,-90 * 16);
        }
        break;
    default:
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
}

/*!
    \internal
*/
void ElevenStyle::drawControl(ControlElement element, const QStyleOption *option,
                                  QPainter *painter, const QWidget *widget) const
{
    if (element == kCapacityBarElement) { // free-space bar of the properties dialog: bar on top, text below
        if (const auto *bar = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            QPainterStateGuard psg(painter);
            painter->setRenderHint(QPainter::Antialiasing);
            const qreal barHeight = 8;
            const QRectF barRect(bar->rect.left() + 0.5, bar->rect.top() + 2, bar->rect.width() - 1, barHeight);
            painter->setPen(Qt::NoPen);
            painter->setBrush(colorSchemeIndex == 1 ? QColor(0xFF, 0xFF, 0xFF, 40) : QColor(0x00, 0x00, 0x00, 30));
            painter->drawRoundedRect(barRect, barHeight / 2, barHeight / 2);
            const qreal range = qMax(1, bar->maximum - bar->minimum);
            const qreal fraction = qBound<qreal>(0, (bar->progress - bar->minimum) / range, 1);
            if (fraction > 0) {
                QRectF fill = barRect;
                fill.setWidth(qMax(barHeight, barRect.width() * fraction));
                if (bar->direction == Qt::RightToLeft)
                    fill.moveRight(barRect.right());
                painter->setBrush(accentColor());
                painter->drawRoundedRect(fill, barHeight / 2, barHeight / 2);
            }
            if (bar->textVisible) {
                painter->setPen(bar->palette.color(QPalette::WindowText));
                const QRect textRect(bar->rect.left(), int(barRect.bottom()) + 2, bar->rect.width(),
                                     bar->fontMetrics.height() + 2); // full text height, do not clip descenders
                painter->drawText(textRect, Qt::AlignCenter | Qt::AlignTop, bar->text);
            }
        }
        return;
    }
    State flags = option->state;

    QPainterStateGuard psg(painter);
    painter->setRenderHint(QPainter::Antialiasing);
    switch (element) {
    case QStyle::CE_ComboBoxLabel:
#if QT_CONFIG(combobox)
        if (const QStyleOptionComboBox *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            painter->setPen(controlTextColor(option));
            if (cb->editable) {
                // Breeze paints a Base-colored block behind the icon of editable combo boxes (Dolphin's location
                // bar); draw just the icon, the line edit is a separate child widget.
                if (!cb->currentIcon.isNull()) {
                    QRect editRect = proxy()->subControlRect(CC_ComboBox, cb, SC_ComboBoxEditField, widget);
                    QRect iconRect(editRect);
                    iconRect.setWidth(cb->iconSize.width() + 4);
                    iconRect = alignedRect(cb->direction, Qt::AlignLeft | Qt::AlignVCenter, iconRect.size(), editRect);
                    const QIcon::Mode mode = cb->state & State_Enabled ? QIcon::Normal : QIcon::Disabled;
                    const QPixmap pm = cb->currentIcon.pixmap(cb->iconSize, painter->device()->devicePixelRatio(), mode);
                    proxy()->drawItemPixmap(painter, iconRect, Qt::AlignCenter, pm);
                }
                break;
            }
            QStyleOptionComboBox newOption = *cb;
            newOption.rect.adjust(4,0,-4,0);
            QProxyStyle::drawControl(element, &newOption, painter, widget);
        }
#endif // QT_CONFIG(combobox)
        break;
    case QStyle::CE_TabBarTabShape:
#if QT_CONFIG(tabbar)
        if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            // floating tabs: the selected one is a lighter rounded pill inside the (slightly darker) strip,
            // hovered tabs get a subtle highlight, the others are transparent
            const bool isEnabled = tab->state & QStyle::State_Enabled;
            const bool selected = tab->state & State_Selected;
            QColor fill(Qt::transparent);
            if (selected)
                fill = colorSchemeIndex == 1 ? QColor(0xFF, 0xFF, 0xFF, 30) : QColor(0xFF, 0xFF, 0xFF, 200);
            else if (isEnabled && tab->state & State_MouseOver)
                fill = winUI3Color(subtleHighlightColor);
            if (fill.alpha() > 0) {
                QPainterStateGuard psg(painter);
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setPen(Qt::NoPen);
                painter->setBrush(fill);
                painter->drawRoundedRect(QRectF(tab->rect).adjusted(3, 3, -3, -3), 6, 6);
            }
        }
#endif  // QT_CONFIG(tabbar)
        break;
    case CE_ToolButtonLabel:
#if QT_CONFIG(toolbutton)
        if (const QStyleOptionToolButton *toolbutton
            = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            QRect rect = toolbutton->rect;
            int shiftX = 0;
            int shiftY = 0;
            if (toolbutton->state & (State_Sunken | State_On)) {
                shiftX = proxy()->pixelMetric(PM_ButtonShiftHorizontal, toolbutton, widget);
                shiftY = proxy()->pixelMetric(PM_ButtonShiftVertical, toolbutton, widget);
            }
            // Arrow type always overrules and is always shown
            bool hasArrow = toolbutton->features & QStyleOptionToolButton::Arrow;
            if (((!hasArrow && toolbutton->icon.isNull()) && !toolbutton->text.isEmpty())
                || toolbutton->toolButtonStyle == Qt::ToolButtonTextOnly) {
                int alignment = Qt::AlignCenter | Qt::TextShowMnemonic;
                if (!proxy()->styleHint(SH_UnderlineShortcut, toolbutton, widget))
                    alignment |= Qt::TextHideMnemonic;
                rect.translate(shiftX, shiftY);
                painter->setFont(toolbutton->font);
                const QString text = toolButtonElideText(toolbutton, rect, alignment);
                // option->state has no State_Sunken here, windowsvistastyle/CC_ToolButton removes it
                painter->setPen(controlTextColor(option));
                proxy()->drawItemText(painter, rect, alignment, toolbutton->palette,
                                      toolbutton->state & State_Enabled, text);
            } else {
                QPixmap pm;
                QSize pmSize = toolbutton->iconSize;
                if (!toolbutton->icon.isNull()) {
                    QIcon::State state = toolbutton->state & State_On ? QIcon::On : QIcon::Off;
                    QIcon::Mode mode;
                    if (!(toolbutton->state & State_Enabled))
                        mode = QIcon::Disabled;
                    else if ((toolbutton->state & State_MouseOver) && (toolbutton->state & State_AutoRaise))
                        mode = QIcon::Active;
                    else
                        mode = QIcon::Normal;
                    pm = toolbutton->icon.pixmap(toolbutton->rect.size().boundedTo(toolbutton->iconSize), painter->device()->devicePixelRatio(),
                                                 mode, state);
                    pm = adaptIcon(pm, controlTextColor(option));
                    pmSize = pm.size() / pm.devicePixelRatio();
                }

                if (toolbutton->toolButtonStyle != Qt::ToolButtonIconOnly) {
                    painter->setFont(toolbutton->font);
                    QRect pr = rect,
                            tr = rect;
                    int alignment = Qt::TextShowMnemonic;
                    if (!proxy()->styleHint(SH_UnderlineShortcut, toolbutton, widget))
                        alignment |= Qt::TextHideMnemonic;

                    if (toolbutton->toolButtonStyle == Qt::ToolButtonTextUnderIcon) {
                        pr.setHeight(pmSize.height() + 4); //### 4 is currently hardcoded in QToolButton::sizeHint()
                        tr.adjust(0, pr.height() - 1, 0, -1);
                        pr.translate(shiftX, shiftY);
                        if (!hasArrow) {
                            proxy()->drawItemPixmap(painter, pr, Qt::AlignCenter, pm);
                        } else {
                            drawArrow(proxy(), toolbutton, pr, painter, widget);
                        }
                        alignment |= Qt::AlignCenter;
                    } else {
                        pr.setWidth(pmSize.width() + 4); //### 4 is currently hardcoded in QToolButton::sizeHint()
                        // centre icon + text as one group inside the button
                        const int textW = toolbutton->fontMetrics.size(Qt::TextShowMnemonic, toolbutton->text).width();
                        const int extra = qMax(0, (rect.width() - pr.width() - textW) / 2);
                        tr.setLeft(rect.left() + extra + pr.width());
                        pr.translate(extra, 0);
                        pr.translate(shiftX, shiftY);
                        if (!hasArrow) {
                            proxy()->drawItemPixmap(painter, QStyle::visualRect(toolbutton->direction, rect, pr), Qt::AlignCenter, pm);
                        } else {
                            drawArrow(proxy(), toolbutton, pr, painter, widget);
                        }
                        alignment |= Qt::AlignLeft | Qt::AlignVCenter;
                    }
                    tr.translate(shiftX, shiftY);
                    const QString text = toolButtonElideText(toolbutton, tr, alignment);
                    painter->setPen(controlTextColor(option));
                    proxy()->drawItemText(painter, QStyle::visualRect(toolbutton->direction, rect, tr), alignment, toolbutton->palette,
                                          toolbutton->state & State_Enabled, text);
                } else {
                    rect.translate(shiftX, shiftY);
                    if (hasArrow) {
                        drawArrow(proxy(), toolbutton, rect, painter, widget);
                    } else {
                        proxy()->drawItemPixmap(painter, rect, Qt::AlignCenter, pm);
                    }
                }
            }
        }
#endif  // QT_CONFIG(toolbutton)
        break;
    case QStyle::CE_ShapedFrame:
        if (widget && widget->inherits("QComboBoxListView"))
            break;
        if (widget && widget->inherits("QComboBoxPrivateContainer")) {
            // Breeze (our base) paints an outlined menu frame on these containers from an event filter before
            // this runs; replace whatever is there with our borderless rounded surface.
            QPainterStateGuard psg(painter);
            painter->setCompositionMode(QPainter::CompositionMode_Source);
            painter->fillRect(option->rect, Qt::transparent);
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(menuFillColor(option));
            painter->drawRoundedRect(QRectF(option->rect), topLevelRoundingRadius, topLevelRoundingRadius);
            break;
        }
        if (const QStyleOptionFrame *f = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            int frameShape  = f->frameShape;
            int frameShadow = QFrame::Plain;
            if (f->state & QStyle::State_Sunken)
                frameShadow = QFrame::Sunken;
            else if (f->state & QStyle::State_Raised)
                frameShadow = QFrame::Raised;

            int lw = f->lineWidth;
            int mlw = f->midLineWidth;

            switch (frameShape) {
            case QFrame::Box:
                if (frameShadow == QFrame::Plain)
                    qDrawPlainRoundedRect(painter, f->rect, secondLevelRoundingRadius, secondLevelRoundingRadius, highContrastTheme == true ? f->palette.buttonText().color() : WINUI3Colors[colorSchemeIndex][frameColorStrong], lw);
                else
                    qDrawShadeRect(painter, f->rect, f->palette, frameShadow == QFrame::Sunken, lw, mlw);
                break;
            case QFrame::Panel:
                if (frameShadow == QFrame::Plain)
                    qDrawPlainRoundedRect(painter, f->rect, secondLevelRoundingRadius, secondLevelRoundingRadius, highContrastTheme == true ? f->palette.buttonText().color() : WINUI3Colors[colorSchemeIndex][frameColorStrong], lw);
                else
                    qDrawShadePanel(painter, f->rect, f->palette, frameShadow == QFrame::Sunken, lw);
                break;
            default:
                QProxyStyle::drawControl(element, option, painter, widget);
            }
        }
        break;
#if QT_CONFIG(progressbar)
    case CE_ProgressBarGroove:
        if (const auto baropt = qstyleoption_cast<const QStyleOptionProgressBar*>(option)) {
            QRect rect = option->rect;
            QPointF center = rect.center();
            if (baropt->state & QStyle::State_Horizontal) {
                rect.setHeight(1);
                rect.moveTop(center.y());
            } else {
                rect.setWidth(1);
                rect.moveLeft(center.x());
            }
            painter->setPen(Qt::NoPen);
            painter->setBrush(Qt::gray);
            painter->drawRect(rect);
        }
        break;
    case CE_ProgressBarContents:
        if (const auto baropt = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            QPainterStateGuard psg(painter);
            QRectF rect = option->rect;
            painter->translate(rect.topLeft());
            rect.translate(-rect.topLeft());

            constexpr qreal progressBarThickness = 3;
            constexpr qreal progressBarHalfThickness = progressBarThickness / 2.0;

            const auto isIndeterminate = baropt->maximum == 0 && baropt->minimum == 0;
            const auto orientation =
                    (baropt->state & QStyle::State_Horizontal) ? Qt::Horizontal : Qt::Vertical;
            const auto inverted = baropt->invertedAppearance;
            const auto reverse = (baropt->direction == Qt::RightToLeft) ^ inverted;
            // If the orientation is vertical, we use a transform to rotate
            // the progress bar 90 degrees (counter)clockwise. This way we can use the
            // same rendering code for both orientations.
            if (orientation == Qt::Vertical) {
                rect = QRectF(rect.left(), rect.top(), rect.height(),
                              rect.width()); // flip width and height
                QTransform m;
                if (inverted) {
                    m.rotate(90);
                    m.translate(0, -rect.height() + 1);
                } else {
                    m.rotate(-90);
                    m.translate(-rect.width(), 0);
                }
                painter->setTransform(m, true);
            } else if (reverse) {
                QTransform m = QTransform::fromScale(-1, 1);
                m.translate(-rect.width(), 0);
                painter->setTransform(m, true);
            }
            const qreal offset = (int(rect.height()) % 2 == 0) ? 0.5f : 0.0f;

            if (isIndeterminate) {
#if QT_CONFIG(animation)
                auto anim = this->animation(option->styleObject);
                if (!anim) {
                    auto anim = new QStyleAnimation(option->styleObject);
                    anim->setFrameRate(QStyleAnimation::SixtyFps);
                    startAnimation(anim);
                }
                constexpr auto loopDurationMSec = 4000;
                const auto elapsedTime = std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now());
                const auto elapsed = elapsedTime.time_since_epoch().count();
                const auto handleCenter = (elapsed % loopDurationMSec) / float(loopDurationMSec);
                const auto isLongHandle = (elapsed / loopDurationMSec) % 2 == 0;
                const auto lengthFactor = (isLongHandle ? 33.0f : 25.0f) / 100.0f;
#else
                constexpr auto handleCenter = 0.5f;
                constexpr auto lengthFactor = 1;
#endif
                const auto begin = qMax(handleCenter * (1 + lengthFactor) - lengthFactor, 0.0f);
                const auto end = qMin(handleCenter * (1 + lengthFactor), 1.0f);
                const auto barBegin = begin * rect.width();
                const auto barEnd = end * rect.width();
                rect = QRectF(QPointF(rect.left() + barBegin, rect.top()),
                              QPointF(rect.left() + barEnd, rect.bottom()));
            } else {
#if QT_CONFIG(animation)
                stopAnimation(option->styleObject);
#endif
                const auto fillPercentage = (float(baropt->progress - baropt->minimum))
                        / (float(baropt->maximum - baropt->minimum));
                rect.setWidth(rect.width() * fillPercentage);
            }
            const QPointF center = rect.center();
            rect.setHeight(progressBarThickness);
            rect.moveTop(center.y() - progressBarHalfThickness - offset);
            drawRoundedRect(painter, rect, Qt::NoPen, QBrush(accentColor()));
        }
        break;
    case CE_ProgressBarLabel:
        if (const auto baropt = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            const bool vertical = !(baropt->state & QStyle::State_Horizontal);
            if (!vertical) {
                proxy()->drawItemText(painter, baropt->rect, Qt::AlignCenter | Qt::TextSingleLine,
                                      baropt->palette, baropt->state & State_Enabled, baropt->text,
                                      QPalette::Text);
            }
        }
        break;
#endif // QT_CONFIG(progressbar)
    case CE_PushButtonLabel:
        if (const QStyleOptionButton *btn = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            if (btn->features.testFlag(QStyleOptionButton::HasMenu)) {
                QStyleOptionButton btnCopy(*btn);
                btnCopy.rect = btn->rect.marginsRemoved(QMargins(contentHMargin, 0, contentHMargin, 0));
                // the bevel already draws our chevron; without this Breeze adds its own arrow next to it
                btnCopy.features &= ~QStyleOptionButton::HasMenu;
                btnCopy.rect.setWidth(btnCopy.rect.width() - proxy()->pixelMetric(PM_MenuButtonIndicator, btn, widget));
                btnCopy.palette.setBrush(QPalette::ButtonText, controlTextColor(option));
                QProxyStyle::drawControl(element, &btnCopy, painter, widget);
                break;
            }
            // icon and text are laid out as one group and centred in the button
            const QRect rect = btn->rect.marginsRemoved(QMargins(contentHMargin, 0, contentHMargin, 0));
            const bool hasIcon = !btn->icon.isNull();
            const bool hasText = !btn->text.isEmpty();
            const int gap = 2 * contentItemHMargin;
            const QSize iconSize = hasIcon ? btn->iconSize : QSize();
            const int iconW = hasIcon ? iconSize.width() : 0;
            const int textAvail = qMax(0, rect.width() - iconW - (hasIcon && hasText ? gap : 0));
            const QString shown = hasText ? btn->fontMetrics.elidedText(btn->text, Qt::ElideRight, textAvail,
                                                                       Qt::TextShowMnemonic)
                                          : QString();
            const int textW = hasText ? btn->fontMetrics.size(Qt::TextShowMnemonic, shown).width() : 0;
            const int total = iconW + (hasIcon && hasText ? gap : 0) + textW;
            int x = rect.left() + qMax(0, (rect.width() - total) / 2);
            if (btn->direction == Qt::RightToLeft)
                x = rect.right() + 1 - qMax(0, (rect.width() - total) / 2) - total;

            QPainterStateGuard psg(painter);
            int cursor = x;
            const auto place = [&](int w) {
                QRect r(cursor, rect.top(), w, rect.height());
                cursor += w;
                return btn->direction == Qt::RightToLeft ? r : r;
            };
            if (hasIcon) {
                const QIcon::Mode mode = !(btn->state & State_Enabled) ? QIcon::Disabled : QIcon::Normal;
                const QIcon::State st = (btn->state & State_On) ? QIcon::On : QIcon::Off;
                const QPixmap pm = adaptIcon(btn->icon.pixmap(iconSize, painter->device()->devicePixelRatio(), mode, st),
                                             controlTextColor(option));
                proxy()->drawItemPixmap(painter, place(iconW), Qt::AlignCenter, pm);
                if (hasText)
                    cursor += gap;
            }
            if (hasText) {
                int flags = Qt::AlignVCenter | Qt::AlignLeft | Qt::TextShowMnemonic;
                if (!proxy()->styleHint(SH_UnderlineShortcut, btn, widget))
                    flags |= Qt::TextHideMnemonic;
                painter->setFont(widget ? widget->font() : QGuiApplication::font());
                painter->setPen(controlTextColor(option));
                proxy()->drawItemText(painter, place(textW), flags, btn->palette,
                                      btn->state & State_Enabled, shown);
            }
        }
        break;
    case CE_PushButtonBevel:
        if (const QStyleOptionButton *btn = qstyleoption_cast<const QStyleOptionButton *>(option))  {
            using namespace StyleOptionHelper;

            QRectF rect = btn->rect.marginsRemoved(QMargins(2, 2, 2, 2));
            painter->setPen(Qt::NoPen);
            if (btn->features.testFlag(QStyleOptionButton::Flat)) {
                painter->setBrush(btn->palette.button());
                painter->drawRoundedRect(rect, secondLevelRoundingRadius, secondLevelRoundingRadius);
                if (flags & (State_Sunken | State_On)) {
                    painter->setBrush(WINUI3Colors[colorSchemeIndex][subtlePressedColor]);
                }
                else if (flags & State_MouseOver) {
                    painter->setBrush(WINUI3Colors[colorSchemeIndex][subtleHighlightColor]);
                }
                painter->drawRoundedRect(rect, secondLevelRoundingRadius, secondLevelRoundingRadius);
            } else if (widget && widget->inherits("PartWidget")) {
                // KDE Partition Manager's partition bar is a row of push buttons whose palette Button color is the
                // file system color; the neutral control fill would turn every partition gray.
                painter->setBrush(btn->palette.button());
                painter->drawRoundedRect(rect, secondLevelRoundingRadius, secondLevelRoundingRadius);
                if (flags & State_MouseOver) {
                    painter->setBrush(WINUI3Colors[colorSchemeIndex][subtleHighlightColor]);
                    painter->drawRoundedRect(rect, secondLevelRoundingRadius, secondLevelRoundingRadius);
                }
            } else {
                painter->setBrush(controlFillBrush(option, ControlType::Control));
                painter->drawRoundedRect(rect, secondLevelRoundingRadius, secondLevelRoundingRadius);

                // no border, the fill alone marks the button
            }
            if (btn->features.testFlag(QStyleOptionButton::HasMenu)) {
                QPainterStateGuard psg(painter);

                const bool isEnabled = !isDisabled(option);
                QRect textRect = btn->rect.marginsRemoved(QMargins(contentHMargin, 0, contentHMargin, 0));
                const auto indSize = proxy()->pixelMetric(PM_MenuButtonIndicator, btn, widget);
                const auto indRect =
                        QRect(btn->rect.right() - indSize - contentItemHMargin, textRect.top(),
                              indSize + contentItemHMargin, btn->rect.height());
                const auto vindRect = visualRect(btn->direction, btn->rect, indRect);
                textRect.setWidth(textRect.width() - indSize);

                int fontSize = painter->font().pointSize();
                QFont f(assetFont);
                f.setPointSize(qRound(fontSize * 0.9f)); // a little bit smaller
                painter->setFont(f);
                QColor penColor = option->palette.color(
                        isEnabled ? QPalette::Active : QPalette::Disabled, QPalette::Text);
                if (isEnabled)
                    penColor.setAlpha(percentToAlpha(60.63)); // fillColorTextSecondary
                painter->setPen(penColor);
                drawGlyph(painter, vindRect, Icon::ChevronDownMed);
            }
        }
        break;
    case CE_MenuBarItem:
        if (const auto *mbi = qstyleoption_cast<const QStyleOptionMenuItem *>(option))  {
            using namespace StyleOptionHelper;

            constexpr int hPadding = 11;
            constexpr int topPadding = 4;
            constexpr int bottomPadding = 6;
            QStyleOptionMenuItem newMbi = *mbi;

            newMbi.font.setPointSize(10);
            newMbi.palette.setColor(QPalette::ButtonText, controlTextColor(&newMbi));
            if (!isDisabled(&newMbi)) {
                QPen pen(Qt::NoPen);
                QBrush brush(Qt::NoBrush);
                if (highContrastTheme) {
                    pen = QPen(newMbi.palette.highlight().color(), 2);
                    brush = newMbi.palette.window();
                } else if (isPressed(&newMbi)) { // its menu is open
                    brush = colorSchemeIndex == 1 ? QColor(0xFF, 0xFF, 0xFF, 40) : QColor(0x00, 0x00, 0x00, 30);
                } else if (isHover(&newMbi) || newMbi.state.testFlag(State_Selected)) { // hover or keyboard focus
                    brush = winUI3Color(subtleHighlightColor);
                }
                if (pen != Qt::NoPen || brush != Qt::NoBrush) {
                    const QRect rect = mbi->rect.marginsRemoved(QMargins(5, 0, 5, 0));
                    drawRoundedRect(painter, rect, pen, brush);
                }
            }
            newMbi.rect.adjust(hPadding,topPadding,-hPadding,-bottomPadding);
            painter->setFont(newMbi.font);
            // Draw the label ourselves: Breeze's drawControl would paint its own (accent blue) highlight
            // behind the active item on top of the gray one above.
            int flags = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
            if (!proxy()->styleHint(SH_UnderlineShortcut, mbi, widget))
                flags |= Qt::TextHideMnemonic;
            proxy()->drawItemText(painter, newMbi.rect, flags, newMbi.palette, !isDisabled(&newMbi), newMbi.text,
                                  QPalette::ButtonText);
        }
        break;

#if QT_CONFIG(menu)
    case CE_MenuEmptyArea:
        break;

    case CE_MenuItem:
        if (const auto *menuitem = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            const auto visualMenuRect = [&](const QRect &rect) {
                return visualRect(option->direction, menuitem->rect, rect);
            };
            bool dis = !(menuitem->state & State_Enabled);
            bool checked = menuitem->checkType != QStyleOptionMenuItem::NotCheckable
                    ? menuitem->checked : false;
            bool act = menuitem->state & State_Selected;

            const QRect rect = menuitem->rect.marginsRemoved(QMargins(2,2,2,2));
            if (act && dis == false) {
                drawRoundedRect(painter, rect, Qt::NoPen, highContrastTheme ? menuitem->palette.brush(QPalette::Highlight)
                                                                            : QBrush(winUI3Color(subtleHighlightColor)));
            }
            if (menuitem->menuItemType == QStyleOptionMenuItem::Separator) {
                constexpr int yoff = 1;
                painter->setPen(highContrastTheme ? menuitem->palette.buttonText().color() : winUI3Color(dividerStrokeDefault));
                painter->drawLine(menuitem->rect.topLeft() + QPoint(0, yoff),
                                  menuitem->rect.topRight() + QPoint(0, yoff));
                break;
            }

            int xOffset = contentHMargin;
            // WinUI3 draws, in contrast to former windows styles, the checkmark and icon separately
            const auto checkMarkWidth = proxy()->pixelMetric(PM_IndicatorWidth, option, widget);
            if (menuitem->checkType != QStyleOptionMenuItem::NotCheckable) {
                // regular radio button / check box indicators, unchecked entries included
                const QRect vRect(visualMenuRect(QRect(rect.x() + xOffset, rect.y(),
                                                       checkMarkWidth, rect.height())));
                QStyleOption ind;
                ind.palette = menuitem->palette;
                ind.direction = option->direction;
                ind.fontMetrics = menuitem->fontMetrics;
                QRect r(0, 0, checkMarkWidth, checkMarkWidth);
                r.moveCenter(vRect.center());
                ind.rect = r;
                ind.state = (dis ? State() : State_Enabled) | (checked ? State_On : State_Off);
                proxy()->drawPrimitive(menuitem->checkType == QStyleOptionMenuItem::Exclusive
                                           ? PE_IndicatorRadioButton : PE_IndicatorCheckBox,
                                       &ind, painter, widget);
            }
            if (menuitem->menuHasCheckableItems)
                xOffset += checkMarkWidth + contentItemHMargin * 2; // same width as reserved in CT_MenuItem
            if (!menuitem->icon.isNull()) {
                // 4 is added to maxIconWidth in qmenu.cpp to PM_SmallIconSize
                QRect vRect(visualMenuRect(QRect(rect.x() + xOffset,
                                                 rect.y(),
                                                 menuitem->maxIconWidth - 4,
                                                 rect.height())));
                QIcon::Mode mode = dis ? QIcon::Disabled : QIcon::Normal;
                if (act && !dis)
                    mode = QIcon::Active;
                const auto size = proxy()->pixelMetric(PM_SmallIconSize, option, widget);
                QRect pmr(QPoint(0, 0), QSize(size, size));
                pmr.moveCenter(vRect.center());
                menuitem->icon.paint(painter, pmr, Qt::AlignCenter, mode,
                                     checked ? QIcon::On : QIcon::Off);
            }
            if (menuitem->maxIconWidth > 0)
                xOffset += menuitem->maxIconWidth - 4 + contentItemHMargin;

            QStringView s(menuitem->text);
            if (!s.isEmpty()) {                     // draw text
                QPoint tl(rect.left() + xOffset, rect.top());
                QPoint br(rect.right() - menuitem->reservedShortcutWidth - contentHMargin,
                          rect.bottom());
                QRect textRect(tl, br);
                QRect vRect(visualMenuRect(textRect));

                qsizetype t = s.indexOf(u'\t');
                int text_flags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
                if (!proxy()->styleHint(SH_UnderlineShortcut, menuitem, widget))
                    text_flags |= Qt::TextHideMnemonic;
                text_flags |= Qt::AlignLeft;
                // a submenu doesn't paint a possible shortcut in WinUI3
                if (t >= 0 && menuitem->menuItemType != QStyleOptionMenuItem::SubMenu) {
                    QRect shortcutRect(QPoint(textRect.right(), textRect.top()),
                                       QPoint(rect.right(), textRect.bottom()));
                    QRect vShortcutRect(visualMenuRect(shortcutRect));
                    QColor penColor;
                    if (highContrastTheme) {
                        penColor = menuitem->palette.color(act ? QPalette::HighlightedText
                                                               : QPalette::Text);
                    } else {
                        penColor = menuitem->palette.color(dis ? QPalette::Disabled
                                                               : QPalette::Active, QPalette::Text);
                        if (!dis)
                            penColor.setAlpha(percentToAlpha(60.63));   // fillColorTextSecondary
                    }
                    painter->setPen(penColor);
                    const QString textToDraw = s.mid(t + 1).toString();
                    painter->drawText(vShortcutRect, text_flags, textToDraw);
                    s = s.left(t);
                }
                QFont font = menuitem->font;
                if (menuitem->menuItemType == QStyleOptionMenuItem::DefaultItem)
                    font.setBold(true);
                painter->setFont(font);
                QColor penColor;
                if (highContrastTheme && act)
                    penColor = menuitem->palette.color(QPalette::HighlightedText);
                else
                    penColor = menuitem->palette.color(dis ? QPalette::Disabled
                                                           : QPalette::Current, QPalette::Text);
                painter->setPen(penColor);
                const QString textToDraw = s.left(t).toString();
                painter->drawText(vRect, text_flags, textToDraw);
            }
            if (menuitem->menuItemType == QStyleOptionMenuItem::SubMenu) {// draw sub menu arrow
                int fontSize = menuitem->font.pointSize();
                QFont f(assetFont);
                f.setPointSize(qRound(fontSize * 0.9f)); // a little bit smaller
                painter->setFont(f);
                const int yOfs = 0; // vector glyph is centred in the row, the font-baseline offset is not needed
                QPoint tl(rect.right() - 2 * 6 - contentItemHMargin,
                          rect.top() + yOfs);
                QRect submenuRect(tl, rect.bottomRight());
                QRect vSubMenuRect = visualMenuRect(submenuRect);
                painter->setPen(option->palette.text().color());
                const bool isReverse = option->direction == Qt::RightToLeft;
                const auto ico = isReverse ? Icon::ChevronLeftMed : Icon::ChevronRightMed;
                drawGlyph(painter, vSubMenuRect, ico);
            }
        }
        break;
#endif // QT_CONFIG(menu)
    case CE_MenuBarEmptyArea: {
        break;
    }
    case CE_HeaderEmptyArea:
        break;
    case CE_HeaderSection: {
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(header->palette.button());
            painter->drawRect(header->rect);

            painter->setPen(highContrastTheme == true ? header->palette.buttonText().color() : WINUI3Colors[colorSchemeIndex][frameColorLight]);
            painter->setBrush(Qt::NoBrush);

            if (header->position == QStyleOptionHeader::OnlyOneSection) {
                break;
            }
            else if (header->position == QStyleOptionHeader::Beginning) {
                painter->drawLine(QPointF(option->rect.topRight()) + QPointF(0.5,0.0),
                                  QPointF(option->rect.bottomRight()) + QPointF(0.5,0.0));
            }
            else if (header->position == QStyleOptionHeader::End) {
                painter->drawLine(QPointF(option->rect.topLeft()) - QPointF(0.5,0.0),
                                  QPointF(option->rect.bottomLeft()) - QPointF(0.5,0.0));
            } else {
                painter->drawLine(QPointF(option->rect.topRight()) + QPointF(0.5,0.0),
                                  QPointF(option->rect.bottomRight()) + QPointF(0.5,0.0));
                painter->drawLine(QPointF(option->rect.topLeft()) - QPointF(0.5,0.0),
                                  QPointF(option->rect.bottomLeft()) - QPointF(0.5,0.0));
            }
            painter->drawLine(QPointF(option->rect.bottomLeft()) + QPointF(0.0,0.5),
                              QPointF(option->rect.bottomRight()) + QPointF(0.0,0.5));
        }
        break;
    }
    case CE_ItemViewItem: {
        if (const QStyleOptionViewItem *vopt = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            const auto p = proxy();
            QRect checkRect = p->subElementRect(SE_ItemViewItemCheckIndicator, vopt, widget);
            QRect iconRect = p->subElementRect(SE_ItemViewItemDecoration, vopt, widget);
            QRect textRect = p->subElementRect(SE_ItemViewItemText, vopt, widget);

            // draw the background (highlight itself is painted below)
            m_inItemViewItem = true;
            proxy()->drawPrimitive(PE_PanelItemViewItem, option, painter, widget);
            m_inItemViewItem = false;

            const QRect &rect = vopt->rect;
            const bool isRtl = option->direction == Qt::RightToLeft;
            bool onlyOne = vopt->viewItemPosition == QStyleOptionViewItem::OnlyOne ||
                           vopt->viewItemPosition == QStyleOptionViewItem::Invalid;
            bool isFirst = vopt->viewItemPosition == QStyleOptionViewItem::Beginning;
            bool isLast = vopt->viewItemPosition == QStyleOptionViewItem::End;

            // the tree decoration already painted the left side of the rounded rect
            if (vopt->features.testFlag(QStyleOptionViewItem::IsDecoratedRootColumn) &&
                vopt->showDecorationSelected) {
                isFirst = false;
                if (onlyOne) {
                    onlyOne = false;
                    isLast = true;
                }
            }

            if (isRtl) {
                if (isFirst) {
                    isFirst = false;
                    isLast = true;
                } else if (isLast) {
                    isFirst = true;
                    isLast = false;
                }
            }
            const bool highlightCurrent = vopt->state.testAnyFlags(State_Selected | State_MouseOver);
            if (highlightCurrent) {
                if (highContrastTheme) {
                    painter->setBrush(vopt->palette.highlight());
                } else {
                    painter->setBrush(itemRowHighlight(colorSchemeIndex, vopt->state.testFlag(State_Selected)));
                }
            } else {
                painter->setBrush(vopt->backgroundBrush);
            }
            painter->setPen(Qt::NoPen);

            if (onlyOne) {
                painter->drawRoundedRect(rect.marginsRemoved(QMargins(2, 2, 2, 2)),
                                         secondLevelRoundingRadius, secondLevelRoundingRadius);
            } else if (isFirst) {
                QPainterStateGuard psg(painter);
                painter->setClipRect(rect);
                painter->drawRoundedRect(rect.marginsRemoved(QMargins(2, 2, -secondLevelRoundingRadius, 2)),
                                         secondLevelRoundingRadius, secondLevelRoundingRadius);
            } else if (isLast) {
                QPainterStateGuard psg(painter);
                painter->setClipRect(rect);
                painter->drawRoundedRect(rect.marginsRemoved(QMargins(-secondLevelRoundingRadius, 2, 2, 2)),
                                         secondLevelRoundingRadius, secondLevelRoundingRadius);
            } else {
                painter->drawRect(rect.marginsRemoved(QMargins(0, 2, 0, 2)));
            }

            // draw the check mark
            if (vopt->features & QStyleOptionViewItem::HasCheckIndicator) {
                QStyleOptionViewItem option(*vopt);
                option.rect = checkRect;
                option.state = option.state & ~QStyle::State_HasFocus;

                switch (vopt->checkState) {
                case Qt::Unchecked:
                    option.state |= QStyle::State_Off;
                    break;
                case Qt::PartiallyChecked:
                    option.state |= QStyle::State_NoChange;
                    break;
                case Qt::Checked:
                    option.state |= QStyle::State_On;
                    break;
                }
                proxy()->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &option, painter, widget);
            }

            // draw the icon
            if (iconRect.isValid()) {
                QIcon::Mode mode = QIcon::Normal;
                if (!(vopt->state & QStyle::State_Enabled))
                    mode = QIcon::Disabled;
                else if (vopt->state & QStyle::State_Selected)
                    mode = QIcon::Selected;
                QIcon::State state = vopt->state & QStyle::State_Open ? QIcon::On : QIcon::Off;
                vopt->icon.paint(painter, iconRect, vopt->decorationAlignment, mode, state);
            }

            painter->setPen(highlightCurrent && highContrastTheme ? vopt->palette.base().color()
                                                                  : vopt->palette.text().color());
            viewItemDrawText(painter, vopt, textRect);

            // paint a vertical marker for QListView
            if (vopt->state & State_Selected && !highContrastTheme) {
                if (const QListView *lv = qobject_cast<const QListView *>(widget);
                    lv && lv->viewMode() != QListView::IconMode) {
                    const auto col = accentColor();
                    painter->setBrush(col);
                    painter->setPen(col);
                    const auto xPos = isRtl ? rect.right() - 4.5f : rect.left() + 3.5f;
                    const auto yOfs = rect.height() / 4.;
                    QRectF r(QPointF(xPos, rect.y() + yOfs),
                             QPointF(xPos + 1, rect.y() + rect.height() - yOfs));
                    painter->drawRoundedRect(r, 1, 1);
                }
            }
        }
        break;
    }
    default:
        QProxyStyle::drawControl(element, option, painter, widget);
    }
}

int ElevenStyle::styleHint(StyleHint hint, const QStyleOption *opt,
              const QWidget *widget, QStyleHintReturn *returnData) const {
    if (int(hint) == kStyleCustomElementHint && widget && widget->objectName() == QLatin1String("CE_CapacityBar"))
        return int(kCapacityBarElement);
    switch (hint) {
    case QStyle::SH_Menu_AllowActiveAndDisabled:
        return 0;
    case QStyle::SH_UnderlineShortcut:
        // no underlined keyboard accelerators in menus (context menus, menu bar, tray menus); other widgets keep
        // whatever Breeze says
        if ((opt && opt->type == QStyleOption::SO_MenuItem) || qobject_cast<const QMenu *>(widget)
            || qobject_cast<const QMenuBar *>(widget))
            return 0;
        return QProxyStyle::styleHint(hint, opt, widget, returnData);
    case SH_GroupBox_TextLabelColor:
        if (opt!=nullptr && widget!=nullptr)
            return opt->palette.text().color().rgba();
        return 0;
    case QStyle::SH_ItemView_ShowDecorationSelected:
        return 1;
    case QStyle::SH_Slider_AbsoluteSetButtons:
        return Qt::LeftButton;
    case QStyle::SH_Slider_PageSetButtons:
        return 0;
    default:
        return QProxyStyle::styleHint(hint, opt, widget, returnData);
    }
}

QRect ElevenStyle::subElementRect(QStyle::SubElement element, const QStyleOption *option,
                     const QWidget *widget) const
{
    QRect ret;
    switch (element) {
    case QStyle::SE_RadioButtonIndicator:
    case QStyle::SE_CheckBoxIndicator: {
        ret = QProxyStyle::subElementRect(element, option, widget);
        const auto ofs =
                (option->direction == Qt::RightToLeft) ? -contentItemHMargin : +contentItemHMargin;
        ret.moveLeft(ret.left() + ofs);
        break;
    }
    case QStyle::SE_ComboBoxFocusRect:
    case QStyle::SE_CheckBoxFocusRect:
    case QStyle::SE_RadioButtonFocusRect:
    case QStyle::SE_PushButtonFocusRect:
        ret = option->rect;
        break;
    case QStyle::SE_LineEditContents:
        ret = option->rect.adjusted(4,0,-4,0);
        break;
    case SE_ItemViewItemCheckIndicator:
    case SE_ItemViewItemDecoration:
    case SE_ItemViewItemText: {
        ret = QProxyStyle::subElementRect(element, option, widget);
        if (!ret.isValid() || highContrastTheme)
            return ret;

        if (const QListView *lv = qobject_cast<const QListView *>(widget);
            lv && lv->viewMode() != QListView::IconMode) {
            const int xOfs = contentHMargin;
            const bool isRtl = option->direction == Qt::RightToLeft;
            if (isRtl) {
                ret.moveRight(ret.right() - xOfs);
                if (ret.left() < option->rect.left())
                    ret.setLeft(option->rect.left());
            } else {
                ret.moveLeft(ret.left() + xOfs);
                if (ret.right() > option->rect.right())
                    ret.setRight(option->rect.right());
            }
        }
        break;
    }
#if QT_CONFIG(progressbar)
    case SE_ProgressBarGroove:
    case SE_ProgressBarContents:
    case SE_ProgressBarLabel:
        if (const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            QStyleOptionProgressBar optCopy(*pb);
            // we only support label right from content
            optCopy.textAlignment = Qt::AlignRight;
            return QProxyStyle::subElementRect(element, &optCopy, widget);
        }
        break;
#endif // QT_CONFIG(progressbar)
    case QStyle::SE_HeaderLabel:
    case QStyle::SE_HeaderArrow:
        ret = QProxyStyle::subElementRect(element, option, widget);
        break;
    case SE_PushButtonContents: {
        int border = proxy()->pixelMetric(PM_DefaultFrameWidth, option, widget);
        ret = option->rect.marginsRemoved(QMargins(border, border, border, border));
        break;
    }
    default:
        ret = QProxyStyle::subElementRect(element, option, widget);
    }
    return ret;
}

/*!
 \internal
 */
QRect ElevenStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                                         SubControl subControl, const QWidget *widget) const
{
    QRect ret;

    switch (control) {
#if QT_CONFIG(spinbox)
    case CC_SpinBox:
        if (const QStyleOptionSpinBox *spinbox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            const bool hasButtons = spinbox->buttonSymbols != QAbstractSpinBox::NoButtons;
            const int fw = spinbox->frame
                    ? proxy()->pixelMetric(PM_SpinBoxFrameWidth, spinbox, widget)
                    : 0;
            const int buttonHeight = hasButtons
                    ? qMin(spinbox->rect.height() - 3 * fw, spinbox->fontMetrics.height() * 5 / 4)
                    : 0;
            const QSize buttonSize(buttonHeight * 6 / 5, buttonHeight);
            const int textFieldLength = spinbox->rect.width() - 2 * fw - 2 * buttonSize.width();
            const QPoint topLeft(spinbox->rect.topLeft() + QPoint(fw, fw));
            switch (subControl) {
            case SC_SpinBoxUp:
            case SC_SpinBoxDown: {
                if (!hasButtons)
                    return QRect();
                const int yOfs = ((spinbox->rect.height() - 2 * fw) - buttonSize.height()) / 2;
                ret = QRect(topLeft.x() + textFieldLength, topLeft.y() + yOfs, buttonSize.width(),
                            buttonSize.height());
                if (subControl == SC_SpinBoxDown)
                    ret.moveRight(ret.right() + buttonSize.width());
                break;
            }
            case SC_SpinBoxEditField:
                ret = QRect(topLeft,
                            spinbox->rect.bottomRight() - QPoint(fw + 2 * buttonSize.width(), fw));
                break;
            case SC_SpinBoxFrame:
                ret = spinbox->rect;
            default:
                break;
            }
            ret = visualRect(spinbox->direction, spinbox->rect, ret);
        }
        break;
#endif // Qt_NO_SPINBOX
    case CC_ScrollBar:
    {
        ret = QProxyStyle::subControlRect(control, option, subControl, widget);

        if (subControl == SC_ScrollBarAddLine || subControl == SC_ScrollBarSubLine) {
            if (const QStyleOptionSlider *scrollbar = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
                if (scrollbar->orientation == Qt::Vertical)
                    ret = ret.adjusted(2,2,-2,-3);
                else
                    ret = ret.adjusted(3,2,-2,-2);
            }
        }
        break;
    }
    case CC_ComboBox: {
        if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            const auto indicatorWidth =
                    proxy()->pixelMetric(PM_MenuButtonIndicator, option, widget);
            switch (subControl) {
            case SC_ComboBoxArrow: {
                const int fw =
                        cb->frame ? proxy()->pixelMetric(PM_ComboBoxFrameWidth, cb, widget) : 0;
                const int buttonHeight =
                        qMin(cb->rect.height() - 3 * fw, cb->fontMetrics.height() * 5 / 4);
                const QSize buttonSize(buttonHeight * 6 / 5, buttonHeight);
                const int textFieldLength = cb->rect.width() - 2 * fw - buttonSize.width();
                const QPoint topLeft(cb->rect.topLeft() + QPoint(fw, fw));
                const int yOfs = ((cb->rect.height() - 2 * fw) - buttonSize.height()) / 2;
                ret = QRect(topLeft.x() + textFieldLength, topLeft.y() + yOfs, buttonSize.width(),
                            buttonSize.height());
                ret = visualRect(option->direction, option->rect, ret);
                break;
            }
            case SC_ComboBoxEditField: {
                ret = option->rect;
                if (cb->frame) {
                    const int fw = proxy()->pixelMetric(PM_ComboBoxFrameWidth, cb, widget);
                    ret = ret.marginsRemoved(QMargins(fw, fw, fw, fw));
                }
                ret.setWidth(ret.width() - indicatorWidth - contentHMargin * 2);
                ret = visualRect(option->direction, option->rect, ret);
                break;
            }
            default:
                ret = QProxyStyle::subControlRect(control, option, subControl, widget);
                break;
            }
        }
        break;
    }
#if QT_CONFIG(groupbox)
    case CC_GroupBox: {
        ret = QProxyStyle::subControlRect(control, option, subControl, widget);
        switch (subControl) {
        case SC_GroupBoxCheckBox:
            ret.moveTop(1);
            break;
        default:
            break;
        }
        break;
    }
#endif // QT_CONFIG(groupbox)
    default:
        ret = QProxyStyle::subControlRect(control, option, subControl, widget);
    }
    return ret;
}

/*!
 \internal
 */
QSize ElevenStyle::sizeFromContents(ContentsType type, const QStyleOption *option,
                                           const QSize &size, const QWidget *widget) const
{
    QSize contentSize(size);

    if (type == CT_TabBarTab) {
        // taller tabs: the selected one is a floating pill with 3px of margin around it
        contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            const bool vertical = tab->shape == QTabBar::RoundedEast || tab->shape == QTabBar::RoundedWest
                                  || tab->shape == QTabBar::TriangularEast || tab->shape == QTabBar::TriangularWest;
            (vertical ? contentSize.rwidth() : contentSize.rheight()) += 10;
        }
        return contentSize;
    }

    if (type == CT_ToolButton) {
        contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (const auto *tb = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            const bool iconOnly = tb->toolButtonStyle == Qt::ToolButtonIconOnly || tb->text.isEmpty();
            const bool split = tb->features.testFlag(QStyleOptionToolButton::MenuButtonPopup);
            // "pure icon": the content is just the icon. Composite buttons (Prism's LabeledToolButton has its own
            // labels/large icon and reports a bigger size) keep the size the base style computed.
            const bool pureIcon = size.width() <= tb->iconSize.width() + 6 && size.height() <= tb->iconSize.height() + 6;
            if (iconOnly && !split && pureIcon) {
                // Icon-only buttons are exactly square and depend only on the icon size, so every icon
                // button (search, hamburger, gear, ...) gets the same highlight, with or without a menu.
                const int side = qMax(32, tb->iconSize.width() + 14);
                contentSize = QSize(side, side);
            } else if (!iconOnly && !split) {
                contentSize.rwidth() += 2 * contentHMargin; // inner padding for text buttons
            }
        }
        return contentSize;
    }

    switch (type) {

#if QT_CONFIG(menubar)
    case CT_MenuBarItem:
        if (!contentSize.isEmpty()) {
            constexpr int hMargin = 2 * 6;
            constexpr int hPadding = 2 * 11;
            constexpr int itemHeight = 32;
            contentSize.setWidth(contentSize.width() + hMargin + hPadding);
#if QT_CONFIG(tabwidget)
            if (widget->parent() && !qobject_cast<const QTabWidget *>(widget->parent()))
#endif
                contentSize.setHeight(itemHeight);
        }
        break;
#endif
#if QT_CONFIG(menu)
    case CT_MenuItem:
        if (const auto *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            int width = size.width();
            int height;
            if (menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
                width = 10;
                height = 3;
            } else {
                height = qMax(menuItem->fontMetrics.height() + 14, 30);
                if (!menuItem->icon.isNull()) {
                    int iconExtent = proxy()->pixelMetric(PM_SmallIconSize, option, widget);
                    height = qMax(height,
                                  menuItem->icon.actualSize(QSize(iconExtent, iconExtent)).height() + 4);
                }
            }
            if (menuItem->text.contains(u'\t'))
                width += contentItemHMargin; // the text width is already in
            if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu)
                width += 2 * 6 + contentItemHMargin;
            if (menuItem->menuItemType == QStyleOptionMenuItem::DefaultItem) {
                const QFontMetrics fm(menuItem->font);
                QFont fontBold = menuItem->font;
                fontBold.setBold(true);
                const QFontMetrics fmBold(fontBold);
                width += fmBold.horizontalAdvance(menuItem->text) - fm.horizontalAdvance(menuItem->text);
            }
            // in contrast to windowsvista, the checkmark and icon are drawn separately
            if (menuItem->menuHasCheckableItems) {
                const auto checkMarkWidth = proxy()->pixelMetric(PM_IndicatorWidth, option, widget);
                width += checkMarkWidth + contentItemHMargin * 2;
            }
            // we have an icon and it's already in the given size, only add margins
            // 4 is added in qmenu.cpp to PM_SmallIconSize
            if (menuItem->maxIconWidth > 0)
                width += contentItemHMargin * 2 + menuItem->maxIconWidth - 4;
            width += 2 * 2; // margins for rounded border
            width += 2 * contentHMargin;
            if (width < 100)    // minimum size
                width = 100;
            contentSize = QSize(width, height);
        }
        break;
#endif // QT_CONFIG(menu)
#if QT_CONFIG(spinbox)
    case CT_SpinBox: {
        if (const auto *spinBoxOpt = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            // Add button + frame widths
            const bool hasButtons = (spinBoxOpt->buttonSymbols != QAbstractSpinBox::NoButtons);
            const int margins = 8;
            const int buttonWidth = hasButtons ? 16 + contentItemHMargin : 0;
            const int frameWidth = spinBoxOpt->frame
                    ? proxy()->pixelMetric(PM_SpinBoxFrameWidth, option, widget)
                    : 0;

            contentSize += QSize(2 * buttonWidth + 2 * frameWidth + 2 * margins, 2 * frameWidth);
        }
        break;
    }
#endif
#if QT_CONFIG(combobox)
    case CT_ComboBox:
        if (const auto *comboBoxOpt = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);  // don't rely on QWindowsThemeData
            contentSize += QSize(0, 4); // for the lineedit frame
            if (comboBoxOpt->subControls & SC_ComboBoxArrow) {
                const auto w = proxy()->pixelMetric(PM_MenuButtonIndicator, option, widget);
                contentSize.rwidth() += w + contentItemHMargin;
            }
        }
        break;
#endif
    case CT_LineEdit: {
        if (qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            contentSize = QProxyStyle::sizeFromContents(type, option, size, widget); // don't rely on QWindowsThemeData
            contentSize += QSize(0, 4); // for the lineedit frame
        }
        break;
    }
    case CT_HeaderSection:
        // windows vista does not honor the indicator (as it was drawn above the text, not on the
        // side) so call QProxyStyle::styleHint directly to get the correct size hint
        contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);
        break;
    case CT_RadioButton:
    case CT_CheckBox:
        if (const auto *buttonOpt = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            const auto p = proxy();
            const bool isRadio = (type == CT_RadioButton);

            const int width = p->pixelMetric(
                    isRadio ? PM_ExclusiveIndicatorWidth : PM_IndicatorWidth, option, widget);
            const int height = p->pixelMetric(
                    isRadio ? PM_ExclusiveIndicatorHeight : PM_IndicatorHeight, option, widget);

            int margins = 2 * contentItemHMargin;
            if (!buttonOpt->icon.isNull() || !buttonOpt->text.isEmpty()) {
                margins += p->pixelMetric(isRadio ? PM_RadioButtonLabelSpacing
                                                  : PM_CheckBoxLabelSpacing,
                                          option, widget);
            }

            contentSize += QSize(width + margins, 4);
            contentSize.setHeight(qMax(size.height(), height + 2 * contentItemHMargin));
        }
        break;

        // the indicator needs 2px more in width when there is no text, not needed when
        // the style draws the text
        contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (size.width() == 0)
            contentSize.rwidth() += 2;
        break;
    case CT_PushButton: {
        contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (const auto *pb = qstyleoption_cast<const QStyleOptionButton *>(option);
            pb && pb->text.isEmpty() && !pb->icon.isNull() && pb->features.testFlag(QStyleOptionButton::Flat)) {
            const int side = qMax(32, pb->iconSize.width() + 14); // flat icon-only push button, same as tool buttons
            contentSize = QSize(side, side);
            break;
        }
        // we want our own horizontal spacing
        const int oldMargin = proxy()->pixelMetric(PM_ButtonMargin, option, widget);
        contentSize.rwidth() += 2 * contentHMargin - oldMargin;
        break;
    }
    case CT_ItemViewItem: {
        if (const auto *viewItemOpt = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            if (const QListView *lv = qobject_cast<const QListView *>(widget);
                lv && lv->viewMode() != QListView::IconMode) {
                if (lv->inherits("KFilePlacesView")) { // Dolphin places panel: only slightly taller than Breeze
                    contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);
                    contentSize.rheight() += 4;
                    break;
                }
                QStyleOptionViewItem vOpt(*viewItemOpt);
                // viewItemSize only takes PM_FocusFrameHMargin into account but no additional
                // margin, therefore adjust it here for a correct width during layouting when
                // WrapText is enabled
                vOpt.rect.setRight(vOpt.rect.right() - contentHMargin);
                contentSize = QProxyStyle::sizeFromContents(type, &vOpt, size, widget);
                contentSize.rwidth() += contentHMargin;
                contentSize.rheight() += 2 * contentHMargin;

            } else {
                contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);
            }
        }
        break;
    }
    default:
        contentSize = QProxyStyle::sizeFromContents(type, option, size, widget);
        break;
    }

    return contentSize;
}


/*!
 \internal
 */
int ElevenStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    int res = 0;

    switch (metric) {
    case QStyle::PM_IndicatorWidth:
    case QStyle::PM_IndicatorHeight:
    case QStyle::PM_ExclusiveIndicatorWidth:
    case QStyle::PM_ExclusiveIndicatorHeight:
        res = 18; // the box itself is 16px; 1px of margin per side keeps its stroke from being clipped by the item
        break;
    case PM_ToolBarItemSpacing: // gap between the (now filled) tool buttons
        res = 6;
        break;
    case PM_SliderThickness:        // full height of a slider
        if (auto opt = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            // hard-coded in qslider.cpp, but we need a little bit more
            constexpr auto TickSpace = 5;
            if (opt->tickPosition & QSlider::TicksAbove)
                res += 6 - TickSpace;
            if (opt->tickPosition & QSlider::TicksBelow)
                res += 6 - TickSpace;
        }
        Q_FALLTHROUGH();
    case PM_SliderControlThickness: // size of the control handle
    case PM_SliderLength:           // same because handle is a circle with r=8
        res += 2 * 8;
        break;
    case PM_RadioButtonLabelSpacing:
    case PM_CheckBoxLabelSpacing:
        res = 2 * contentItemHMargin;
        break;
    case QStyle::PM_TitleBarButtonIconSize:
        res = 16;
        break;
    case QStyle::PM_TitleBarButtonSize:
        res = 32;
        break;
    case QStyle::PM_ScrollBarExtent:
        res = 12;
        break;
    case QStyle::PM_SubMenuOverlap:
        res = -1;
        break;
    case PM_MenuButtonIndicator: {
        res = contentItemHMargin;
        if (widget) {
            const int fontSize = widget->font().pointSize();
            QFont f(assetFont);
            f.setPointSize(qRound(fontSize * 0.9f)); // a little bit smaller
            res += glyphAdvance(f);
        } else {
            res += 12;
        }
        break;
    }
    case PM_ComboBoxFrameWidth:
    case PM_SpinBoxFrameWidth:
    case PM_DefaultFrameWidth:
        res = 2;
        break;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        res = 0;
        break;
    case PM_TreeViewIndentation:
        res = 30;
        break;
    case PM_ProgressBarChunkWidth:
        res = 0;    // no chunks on windows11
        break;
    default:
        res = QProxyStyle::pixelMetric(metric, option, widget);
    }

    return res;
}

namespace {
// Delegate for combo box popup lists. Qt's own QComboBoxDelegate paints separators (insertSeparator) itself; the
// styled delegate we swap in does not, which left separators as empty (and hoverable) rows.
class ComboRowDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (isSeparator(index)) {
            QPainterStateGuard psg(p);
            QColor line = option.palette.color(QPalette::Text);
            line.setAlpha(46);
            p->setPen(line);
            const int y = option.rect.center().y();
            p->drawLine(option.rect.left() + 8, y, option.rect.right() - 8, y);
            return;
        }
        QStyledItemDelegate::paint(p, option, index);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (isSeparator(index))
            return QSize(1, 9);
        return QStyledItemDelegate::sizeHint(option, index);
    }

private:
    static bool isSeparator(const QModelIndex &index)
    {
        return index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("separator");
    }
};
}

namespace {
// setViewportMargins() is protected; expose it for the one widget class we pad.
struct ScrollAreaAccess : QAbstractScrollArea
{
    using QAbstractScrollArea::setViewportMargins;
};
}

void ElevenStyle::polish(QWidget* widget)
{
    // Prism Launcher's instance list and icon picker (both use its ListViewDelegate) paint the selection themselves with fillRect(palette Highlight) and a
    // Window-colored label box for unselected items, so the style can only change the palette it sees: a gray
    // translucent highlight instead of the solid accent, and a label box that blends into the list background.
    if (auto *view = qobject_cast<QAbstractItemView *>(widget);
        view && (view->inherits("InstanceView")
                 || (view->itemDelegate() && !qstrcmp(view->itemDelegate()->metaObject()->className(), "ListViewDelegate")))) {
        QPalette pal = view->palette();
        const bool dark = pal.window().color().lightness() < 128;
        pal.setColor(QPalette::Highlight, dark ? QColor(0xFF, 0xFF, 0xFF, 46) : QColor(0x00, 0x00, 0x00, 40));
        pal.setColor(QPalette::HighlightedText, pal.color(QPalette::Text));
        pal.setColor(QPalette::Window, pal.color(QPalette::Base));
        view->setPalette(pal);
        view->viewport()->setPalette(pal);
    }

    // Popup lists of editable combo boxes (Dolphin's location bar history) use Qt's plain QItemDelegate, which
    // paints palette-blue selections, a focus rect and a near-black Base background. Use the styled delegate so
    // the rows are drawn by CE_ItemViewItem (gray rounded highlight) and give the list the menu surface color.
    if (auto *lv = qobject_cast<QListView *>(widget);
        lv && !qstrcmp(lv->metaObject()->className(), "QComboBoxListView")) {
        if (lv->itemDelegate() && !qstrcmp(lv->itemDelegate()->metaObject()->className(), "QItemDelegate"))
            lv->setItemDelegate(new ComboRowDelegate(lv));
        QPalette pal = lv->palette();
        QColor surface = pal.window().color();
        if (surface.lightness() < 128)
            surface = surface.lighter(118);
        pal.setColor(QPalette::Base, surface);
        lv->setPalette(pal);
        lv->viewport()->setPalette(pal);
    }


    // Dolphin's places sidebar draws icons and highlights right at the panel edge; give it a small inner padding.
    if (auto *area = qobject_cast<QAbstractScrollArea *>(widget); area && area->inherits("KFilePlacesView"))
    {
        static_cast<ScrollAreaAccess *>(area)->setViewportMargins(6, 0, 6, 0);
        if (auto *list = qobject_cast<QListView *>(widget))
            list->setSpacing(0);
    }

#if QT_CONFIG(commandlinkbutton)
    if (!qobject_cast<QCommandLinkButton *>(widget))
#endif // QT_CONFIG(commandlinkbutton)
        QProxyStyle::polish(widget);

    const bool isScrollBar = qobject_cast<QScrollBar *>(widget);
    const auto comboBoxContainer = qobject_cast<const QComboBoxPrivateContainer *>(widget);
#if QT_CONFIG(menubar)
    if (qobject_cast<QMenuBar *>(widget)) {
        constexpr int itemHeight = 32;
        if (widget->maximumHeight() < itemHeight) {
            widget->setProperty("_q_original_menubar_maxheight", widget->maximumHeight());
            widget->setMaximumHeight(itemHeight);
        }
    }
#endif
    if (isScrollBar || qobject_cast<QMenu *>(widget) || comboBoxContainer) {
        bool wasCreated = widget->testAttribute(Qt::WA_WState_Created);
        bool layoutDirection = widget->testAttribute(Qt::WA_RightToLeft);
        widget->setAttribute(Qt::WA_OpaquePaintEvent,false);
        widget->setAttribute(Qt::WA_TranslucentBackground);
        if (!isScrollBar)
            widget->setWindowFlag(Qt::FramelessWindowHint);
        widget->setAttribute(Qt::WA_RightToLeft, layoutDirection);
        widget->setAttribute(Qt::WA_WState_Created, wasCreated);
        // No QGraphicsDropShadowEffect here: it doubled the popup (offset ghost copy) and costs a software
        // blur per repaint; the compositor draws the shadow for us.
    } else if (QComboBox* cb = qobject_cast<QComboBox*>(widget)) {
        if (cb->isEditable()) {
            QLineEdit *le = cb->lineEdit();
            le->setFrame(false);
        }
    } else if (const auto *scrollarea = qobject_cast<QAbstractScrollArea *>(widget);
               scrollarea
               && !qobject_cast<QGraphicsView *>(widget)
#if QT_CONFIG(mdiarea)
               && !qobject_cast<QMdiArea *>(widget)
#endif
        ) {
        // Text edits keep their opaque viewport: the frame is not painted behind it, so without the fill the
        // text underneath shows through (Dolphin's inline rename editor)
        if (scrollarea->frameShape() == QFrame::StyledPanel && !qobject_cast<QTextEdit *>(widget)
                && !qobject_cast<QPlainTextEdit *>(widget)) {
            const auto vp = scrollarea->viewport();
            const bool isAutoFillBackground = vp->autoFillBackground();
            const bool isStyledBackground = vp->testAttribute(Qt::WA_StyledBackground);
            vp->setProperty("_q_original_autofill_background", isAutoFillBackground);
            vp->setProperty("_q_original_styled_background", isStyledBackground);
            vp->setAutoFillBackground(false);
            vp->setAttribute(Qt::WA_StyledBackground, true);
        }
        // QTreeView & QListView are already set in the base windowsvista style
        if (auto table = qobject_cast<QTableView *>(widget))
            table->viewport()->setAttribute(Qt::WA_Hover, true);
    }
}

void ElevenStyle::unpolish(QWidget *widget)
{
    if (auto *area = qobject_cast<QAbstractScrollArea *>(widget); area && area->inherits("KFilePlacesView"))
        static_cast<ScrollAreaAccess *>(area)->setViewportMargins(0, 0, 0, 0);

#if QT_CONFIG(commandlinkbutton)
    if (!qobject_cast<QCommandLinkButton *>(widget))
#endif // QT_CONFIG(commandlinkbutton)
        QProxyStyle::unpolish(widget);

#if QT_CONFIG(menubar)
    if (qobject_cast<QMenuBar *>(widget) && !widget->property("_q_original_menubar_maxheight").isNull()) {
        widget->setMaximumHeight(widget->property("_q_original_menubar_maxheight").toInt());
        widget->setProperty("_q_original_menubar_maxheight", QVariant());
    }
#endif
    const auto comboBoxContainer = qobject_cast<const QComboBoxPrivateContainer *>(widget);
    if (comboBoxContainer) {
        widget->setAttribute(Qt::WA_OpaquePaintEvent, true);
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
        widget->setWindowFlag(Qt::FramelessWindowHint, false);
    }

    if (const auto *scrollarea = qobject_cast<QAbstractScrollArea *>(widget);
        scrollarea
#if QT_CONFIG(mdiarea)
        && !qobject_cast<QMdiArea *>(widget)
#endif
        ) {
        const auto vp = scrollarea->viewport();
        const auto wasAutoFillBackground = vp->property("_q_original_autofill_background").toBool();
        vp->setAutoFillBackground(wasAutoFillBackground);
        vp->setProperty("_q_original_autofill_background", QVariant());
        const auto origStyledBackground = vp->property("_q_original_styled_background").toBool();
        vp->setAttribute(Qt::WA_StyledBackground, origStyledBackground);
        vp->setProperty("_q_original_styled_background", QVariant());
    }
}

/*
The colors for Windows 11 are taken from the official WinUI3 Figma style at
https://www.figma.com/community/file/1159947337437047524
*/
QColor ElevenStyle::accentColor() const
{
    return QGuiApplication::palette().color(QPalette::Accent);
}

void ElevenStyle::polish(QPalette &result)
{
    QProxyStyle::polish(result);
    updateColorScheme();
}

QColor ElevenStyle::calculateAccentColor(const QStyleOption *option) const
{
    using namespace StyleOptionHelper;
    if (isDisabled(option))
        return winUI3Color(fillAccentDisabled);
    const auto alphaColor = isPressed(option) ? fillAccentTertiary
                                              : isHover(option) ? fillAccentSecondary
                                                                : fillAccentDefault;
    const auto alpha = winUI3Color(alphaColor);
    QColor col = accentColor();
    col.setAlpha(alpha.alpha());
    return col;
}

QPen ElevenStyle::borderPenControlAlt(const QStyleOption *option) const
{
    using namespace StyleOptionHelper;
    if (isChecked(option))
        return Qt::NoPen;   // same color as fill color, so no pen needed
    if (highContrastTheme)
        return option->palette.buttonText().color();
    if (isDisabled(option) || isPressed(option))
        return winUI3Color(frameColorStrongDisabled);
    return winUI3Color(frameColorStrong);
}

QBrush ElevenStyle::controlFillBrush(const QStyleOption *option, ControlType controlType) const
{
    using namespace StyleOptionHelper;
    static constexpr WINUI3Color colorEnums[2][4] = {
        // Light & Dark Control
        { fillControlDefault, fillControlSecondary,
          fillControlTertiary, fillControlDisabled },
        // Light & Dark Control Alt
        { fillControlAltSecondary, fillControlAltTertiary,
          fillControlAltQuarternary, fillControlAltDisabled },
    };

    // Qt's original returns the palette's Button brush here when the app set one. Plasma sets it for every
    // app, which made buttons follow the colour scheme's button colour instead of the WinUI translucent
    // gray fill.

    if (isChecked(option)) {
        // checked push/tool buttons are marked with a stronger gray fill, like every other highlight here;
        // check boxes and radio buttons (Control Alt) keep the accent color
        if (controlType == ControlType::Control)
            return isDisabled(option) ? QBrush(Qt::NoBrush)
                                      : QBrush(colorSchemeIndex == 1 ? QColor(0xFF, 0xFF, 0xFF, 64)
                                                                     : winUI3Color(fillControlSecondary));
        return calculateAccentColor(option);
    }

    const auto state = calcControlState(option);
    if (controlType == ControlType::Control && state == ControlState::Disabled)
        return Qt::NoBrush; // disabled buttons: no fill at all
    if (controlType == ControlType::Control && colorSchemeIndex == 1 && !highContrastTheme) {
        // Dark buttons: WinUI's 6 % white is only visible on Mica; on Plasma's flat backgrounds it vanishes.
        static constexpr int alpha[4] = { 15, 46, 10, 7 }; // normal, hover, pressed, disabled (of 255)
        return QColor(0xFF, 0xFF, 0xFF, alpha[int(state)]);
    }
    return winUI3Color(colorEnums[int(controlType)][int(state)]);
}

QBrush ElevenStyle::inputFillBrush(const QStyleOption *option, const QWidget *widget) const
{
    // slightly different states than in controlFillBrush
    using namespace StyleOptionHelper;
    // Dolphin's location bar in edit mode: the combo box sits next to widgets that are painted on the plain
    // window background, so a fill on the field alone makes the icon area look like a black block. Leave it
    // unfilled so the whole bar has one surface colour.
    for (const QWidget *w = widget; w; w = w->parentWidget())
        if (w->inherits("KUrlNavigator"))
            return Qt::NoBrush;
    // No palette override: Plasma sets every palette role, which turned all inputs into the scheme's own
    // Button/Base colour (near black on dark schemes) instead of a light overlay on the surrounding surface.
    if (colorSchemeIndex == 1 && !highContrastTheme) {
        // dark: translucent white overlays; focus is a gray highlight like hover (WinUI's darker "active" fill
        // would sink the field below its surroundings)
        if (isDisabled(option))
            return QColor(0xFF, 0xFF, 0xFF, 7);
        if (hasFocus(option) || isHover(option))
            return QColor(0xFF, 0xFF, 0xFF, 22);
        return QColor(0xFF, 0xFF, 0xFF, 10); // same shade as a button at rest
    }
    if (isDisabled(option))
        return winUI3Color(fillControlDisabled);
    if (hasFocus(option))
        return winUI3Color(fillControlInputActive);
    if (isHover(option))
        return winUI3Color(fillControlSecondary);
    return winUI3Color(fillControlDefault);
}

QColor ElevenStyle::controlTextColor(const QStyleOption *option, QPalette::ColorRole role) const
{
    using namespace StyleOptionHelper;
    static constexpr WINUI3Color colorEnums[2][4] = {
        // Control, unchecked
        { textPrimary, textPrimary, textSecondary, textDisabled },
        // Control, checked
        { textOnAccentPrimary, textOnAccentPrimary, textOnAccentSecondary, textOnAccentDisabled },
    };

    if (option->palette.isBrushSet(QPalette::Current, QPalette::ButtonText))
        return option->palette.buttonText().color();

    const int colorIndex = isChecked(option) ? 1 : 0;
    const auto state = calcControlState(option);
    const auto alpha = winUI3Color(colorEnums[colorIndex][int(state)]);
    QColor col = option->palette.color(role);
    col.setAlpha(alpha.alpha());
    return col;
}

void ElevenStyle::drawLineEditFrame(QPainter *p, const QRectF &rect, const QStyleOption *o, bool isEditable, bool forceFrame) const
{
    const bool isHovered = o->state & State_MouseOver;
    const auto frameCol = highContrastTheme
            ? o->palette.color(isHovered ? QPalette::Accent
                                         : QPalette::ButtonText)
            : (colorSchemeIndex == 1 ? QColor(0xFF, 0xFF, 0xFF, 0x30) : winUI3Color(frameColorLight)); // dark: clearer border
    if (highContrastTheme || forceFrame)
        drawRoundedRect(p, rect, frameCol, Qt::NoBrush); // no frame stroke otherwise (except the location bar)

    if (!isEditable || StyleOptionHelper::isDisabled(o))
        return;

    QPainterStateGuard psg(p);
    p->setClipRect(rect.marginsRemoved(QMarginsF(0, rect.height() - 0.5, 0, -1)));
    const bool hasFocus = o->state & State_HasFocus;
    const auto underlineCol = hasFocus
            ? o->palette.color(QPalette::Accent) // WinUI text box: accent underline when focused
            : colorSchemeIndex == 0 ? QColor(0x80, 0x80, 0x80)
                                    : QColor(0xa0, 0xa0, 0xa0);
    const auto penUnderline = QPen(underlineCol, hasFocus ? 2 : 1);
    drawRoundedRect(p, rect, penUnderline, Qt::NoBrush);
}

QColor ElevenStyle::winUI3Color(enum WINUI3Color col) const
{
    return WINUI3Colors[colorSchemeIndex][col];
}



// ---- helpers copied from QCommonStyle(Private), so we do not depend on its internals ----
static QSizeF viewItemTextLayout(QTextLayout &textLayout, int lineWidth, int maxHeight = -1, int *lastVisibleLine = nullptr)
{
    if (lastVisibleLine)
        *lastVisibleLine = -1;
    qreal height = 0;
    qreal widthUsed = 0;
    textLayout.beginLayout();
    int i = 0;
    while (true) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        widthUsed = qMax(widthUsed, line.naturalTextWidth());
        // we assume that the height of the next line is the same as the current one
        if (maxHeight > 0 && lastVisibleLine && height + line.height() > maxHeight) {
            const QTextLine nextLine = textLayout.createLine();
            *lastVisibleLine = nextLine.isValid() ? i : -1;
            break;
        }
        ++i;
    }
    textLayout.endLayout();
    return QSizeF(widthUsed, height);
}

static QString calculateElidedText(const QString &text, const QTextOption &textOption,
                                                 const QFont &font, const QRect &textRect, const Qt::Alignment valign,
                                                 Qt::TextElideMode textElideMode, int flags,
                                                 bool lastVisibleLineShouldBeElided, QPointF *paintStartPosition)
{
    QTextLayout textLayout(text, font);
    textLayout.setTextOption(textOption);

    // In AlignVCenter mode when more than one line is displayed and the height only allows
    // some of the lines it makes no sense to display those. From a users perspective it makes
    // more sense to see the start of the text instead something inbetween.
    const bool vAlignmentOptimization = paintStartPosition && valign.testFlag(Qt::AlignVCenter);

    int lastVisibleLine = -1;
    viewItemTextLayout(textLayout, textRect.width(), vAlignmentOptimization ? textRect.height() : -1, &lastVisibleLine);

    const QRectF boundingRect = textLayout.boundingRect();
    // don't care about LTR/RTL here, only need the height
    const QRect layoutRect = QStyle::alignedRect(Qt::LayoutDirectionAuto, valign,
                                                 boundingRect.size().toSize(), textRect);

    if (paintStartPosition)
        *paintStartPosition = QPointF(textRect.x(), layoutRect.top());

    QString ret;
    qreal height = 0;
    const int lineCount = textLayout.lineCount();
    for (int i = 0; i < lineCount; ++i) {
        const QTextLine line = textLayout.lineAt(i);
        height += line.height();

        // above visible rect
        if (height + layoutRect.top() <= textRect.top()) {
            if (paintStartPosition)
                paintStartPosition->ry() += line.height();
            continue;
        }

        const int start = line.textStart();
        const int length = line.textLength();
        const bool drawElided = line.naturalTextWidth() > textRect.width();
        bool elideLastVisibleLine = lastVisibleLine == i;
        if (!drawElided && i + 1 < lineCount && lastVisibleLineShouldBeElided) {
            const QTextLine nextLine = textLayout.lineAt(i + 1);
            const int nextHeight = height + nextLine.height() / 2;
            // elide when less than the next half line is visible
            if (nextHeight + layoutRect.top() > textRect.height() + textRect.top())
                elideLastVisibleLine = true;
        }

        QString text = textLayout.text().mid(start, length);
        if (drawElided || elideLastVisibleLine) {
            if (elideLastVisibleLine) {
                if (text.endsWith(QChar::LineSeparator))
                    text.chop(1);
                text += QChar(0x2026);
            }
            Q_DECL_UNINITIALIZED const QStackTextEngine engine(text, font);
            ret += engine.elidedText(textElideMode, textRect.width(), flags);

            // no newline for the last line (last visible or real)
            // sometimes drawElided is true but no eliding is done so the text ends
            // with QChar::LineSeparator - don't add another one. This happened with
            // arabic text in the testcase for QTBUG-72805
            if (i < lineCount - 1 &&
                !ret.endsWith(QChar::LineSeparator))
                ret += QChar::LineSeparator;
        } else {
            ret += text;
        }

        // below visible text, can stop
        if ((height + layoutRect.top() >= textRect.bottom()) ||
                (lastVisibleLine >= 0 && lastVisibleLine == i))
            break;
    }
    return ret;
}

void ElevenStyle::viewItemDrawText(QPainter *p, const QStyleOptionViewItem *option, const QRect &rect) const
{
    const QWidget *widget = option->widget;
    const int textMargin = proxy()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;

    QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0); // remove width padding
    const bool wrapText = option->features & QStyleOptionViewItem::WrapText;
    QTextOption textOption;
    textOption.setWrapMode(wrapText ? QTextOption::WordWrap : QTextOption::ManualWrap);
    textOption.setTextDirection(option->direction);
    textOption.setAlignment(QStyle::visualAlignment(option->direction, option->displayAlignment));

    QPointF paintPosition;
    const QString newText = calculateElidedText(option->text, textOption,
                                                option->font, textRect, option->displayAlignment,
                                                option->textElideMode, 0,
                                                true, &paintPosition);

    QTextLayout textLayout(newText, option->font);
    textLayout.setTextOption(textOption);
    viewItemTextLayout(textLayout, textRect.width());
    textLayout.draw(p, paintPosition);
}

QString ElevenStyle::toolButtonElideText(const QStyleOptionToolButton *option,
                                                 const QRect &textRect, int flags) const
{
    if (option->fontMetrics.horizontalAdvance(option->text) <= textRect.width())
        return option->text;

    QString text = option->text;
    text.replace(u'\n', QChar::LineSeparator);
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::ManualWrap);
    textOption.setTextDirection(option->direction);

    return calculateElidedText(text, textOption,
                               option->font, textRect, Qt::AlignTop,
                               Qt::ElideMiddle, flags,
                               false, nullptr);
}

// ---- animation registry (same behaviour as QCommonStylePrivate) ----
bool ElevenStyle::transitionsEnabled() const
{
    return proxy()->styleHint(SH_Widget_Animation_Duration, nullptr, nullptr) > 0;
}

QStyleAnimation *ElevenStyle::animation(const QObject *target) const
{
    return m_animations.value(target);
}

void ElevenStyle::startAnimation(QStyleAnimation *animation) const
{
    const QObject *target = animation->target();
    stopAnimation(target);
    QObject::connect(animation, &QStyleAnimation::destroyed, this, [this, target]() {
        m_animations.remove(target);
        if (auto *w = const_cast<QWidget *>(qobject_cast<const QWidget *>(target)))
            w->update();
    });
    m_animations.insert(target, animation);
    animation->start();
}

void ElevenStyle::stopAnimation(const QObject *target) const
{
    if (QStyleAnimation *a = m_animations.take(target)) {
        a->stop();
        delete a;
    }
}

QTime ElevenStyle::animationTime()
{
    return QTime::currentTime();
}

// ---- vector glyphs replacing the Segoe Fluent Icons font ----
static int glyphAdvance(const QFont &font)
{
    return qRound(QFontInfo(font).pixelSize() * 0.8);
}

// The glyph is drawn in a square of the painter font's pixel size, centred in rect,
// using the painter's current pen colour.
static void drawGlyph(QPainter *p, const QRectF &rect, Icon icon)
{
    const qreal em = QFontInfo(p->font()).pixelSize();
    const QPointF c = rect.center();
    const auto pt = [&](qreal x, qreal y) { return QPointF(c.x() + (x - 0.5) * em, c.y() + (y - 0.5) * em); };

    QPainterStateGuard guard(p);
    p->setRenderHint(QPainter::Antialiasing);
    const QColor color = p->pen().color();
    p->setPen(QPen(color, qMax<qreal>(1.0, em * 0.11), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);

    QPainterPath path;
    bool fill = false;
    switch (icon) {
    case Icon::CheckMark:
    case Icon::AcceptMedium:
        path.moveTo(pt(0.20, 0.54)); path.lineTo(pt(0.42, 0.74)); path.lineTo(pt(0.80, 0.28));
        break;
    case Icon::Dash12:
        path.moveTo(pt(0.25, 0.5)); path.lineTo(pt(0.75, 0.5));
        break;
    case Icon::ChevronDown: case Icon::ChevronDownMed: case Icon::ChevronDownSmall:
        path.moveTo(pt(0.24, 0.38)); path.lineTo(pt(0.5, 0.64)); path.lineTo(pt(0.76, 0.38));
        break;
    case Icon::ChevronUp: case Icon::ChevronUpMed: case Icon::ChevronUpSmall:
        path.moveTo(pt(0.24, 0.62)); path.lineTo(pt(0.5, 0.36)); path.lineTo(pt(0.76, 0.62));
        break;
    case Icon::ChevronLeftMed:
        path.moveTo(pt(0.62, 0.24)); path.lineTo(pt(0.36, 0.5)); path.lineTo(pt(0.62, 0.76));
        break;
    case Icon::ChevronRightMed:
        path.moveTo(pt(0.38, 0.24)); path.lineTo(pt(0.64, 0.5)); path.lineTo(pt(0.38, 0.76));
        break;
    case Icon::CaretDownSolid8:
        path.moveTo(pt(0.28, 0.4)); path.lineTo(pt(0.72, 0.4)); path.lineTo(pt(0.5, 0.65)); path.closeSubpath(); fill = true;
        break;
    case Icon::CaretUpSolid8:
        path.moveTo(pt(0.28, 0.6)); path.lineTo(pt(0.72, 0.6)); path.lineTo(pt(0.5, 0.35)); path.closeSubpath(); fill = true;
        break;
    case Icon::CaretLeftSolid8:
        path.moveTo(pt(0.6, 0.28)); path.lineTo(pt(0.6, 0.72)); path.lineTo(pt(0.35, 0.5)); path.closeSubpath(); fill = true;
        break;
    case Icon::CaretRightSolid8:
        path.moveTo(pt(0.4, 0.28)); path.lineTo(pt(0.4, 0.72)); path.lineTo(pt(0.65, 0.5)); path.closeSubpath(); fill = true;
        break;
    case Icon::Close:
    case Icon::Clear:
        path.moveTo(pt(0.28, 0.28)); path.lineTo(pt(0.72, 0.72));
        path.moveTo(pt(0.72, 0.28)); path.lineTo(pt(0.28, 0.72));
        break;
    case Icon::More:
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        for (qreal x : {0.24, 0.5, 0.76})
            p->drawEllipse(pt(x, 0.5), em * 0.06, em * 0.06);
        return;
    default:
        return;
    }
    if (fill) {
        p->setBrush(color);
        p->setPen(Qt::NoPen);
    }
    p->drawPath(path);
}

QT_END_NAMESPACE
