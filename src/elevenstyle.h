// Copyright (C) 2022 The Qt Company Ltd.       (original QWindows11Style, qtbase v6.10.2)
// Copyright (C) 2026 the eleven-kde authors   (see README.md)
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef ELEVENSTYLE_H
#define ELEVENSTYLE_H

#include <QtWidgets/QProxyStyle>
#include <QtCore/QHash>
#include <QtGui/QFont>
#include <QtGui/QIcon>

class QStyleAnimation;
class QStyleOptionToolButton;
class QStyleOptionViewItem;

enum WINUI3Color {
    subtleHighlightColor,             //Subtle highlight based on alpha used for hovered elements
    subtlePressedColor,               //Subtle highlight based on alpha used for pressed elements
    frameColorLight,                  //Color of frame around flyouts and controls except for Checkbox and Radiobutton
    frameColorStrong,                 //Color of frame around Checkbox and Radiobuttons (normal and hover)
    frameColorStrongDisabled,         //Color of frame around Checkbox and Radiobuttons (pressed and disabled)
    controlStrongFill,                //Color of controls with strong filling such as the right side of a slider
    controlStrokeSecondary,
    controlStrokePrimary,
    menuPanelFill,                    //Color of menu panel
    controlStrokeOnAccentSecondary,   //Color of frame around Buttons in accent color
    controlFillSolid,                 //Color for solid fill
    surfaceStroke,                    //Color of MDI window frames
    focusFrameInnerStroke,
    focusFrameOuterStroke,
    fillControlDefault,               // button default color (alpha)
    fillControlSecondary,             // button hover color (alpha)
    fillControlTertiary,              // button pressed color (alpha)
    fillControlDisabled,              // button disabled color (alpha)
    fillControlInputActive,           // input active
    fillControlAltSecondary,          // checkbox/RadioButton default color (alpha)
    fillControlAltTertiary,           // checkbox/RadioButton hover color (alpha)
    fillControlAltQuarternary,        // checkbox/RadioButton pressed color (alpha)
    fillControlAltDisabled,           // checkbox/RadioButton disabled color (alpha)
    fillAccentDefault,                // button default color (alpha)
    fillAccentSecondary,              // button hover color (alpha)
    fillAccentTertiary,               // button pressed color (alpha)
    fillAccentDisabled,               // button disabled color (alpha)
    textPrimary,                      // text of default/hovered control
    textSecondary,                    // text of pressed control
    textDisabled,                     // text of disabled control
    textOnAccentPrimary,              // text of default/hovered control on accent color
    textOnAccentSecondary,            // text of pressed control on accent color
    textOnAccentDisabled,             // text of disabled control on accent color
    dividerStrokeDefault,             // divider color (alpha)
};

class ElevenStyle : public QProxyStyle
{
    Q_OBJECT
    Q_CLASSINFO("X-KDE-CustomElements", "true") // lets KDE widgets ask us for custom elements (KCapacityBar)
public:
    ElevenStyle();
    ~ElevenStyle() override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                            QPainter *painter, const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget) const override;
    QRect subElementRect(QStyle::SubElement element, const QStyleOption *option,
                         const QWidget *widget = nullptr) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                         SubControl subControl, const QWidget *widget) const override;
    void drawControl(ControlElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget) const override;
    int styleHint(StyleHint hint, const QStyleOption *opt = nullptr,
                  const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;
    void polish(QWidget *widget) override;
    void polish(QPalette &pal) override;
    void unpolish(QWidget *widget) override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,
                           const QSize &size, const QWidget *widget) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;

private:
    QColor calculateAccentColor(const QStyleOption *option) const;
    QPen borderPenControlAlt(const QStyleOption *option) const;
    enum class ControlType { Control, ControlAlt };
    QBrush controlFillBrush(const QStyleOption *option, ControlType controlType) const;
    QBrush inputFillBrush(const QStyleOption *option, const QWidget *widget) const;
    QColor controlTextColor(const QStyleOption *option,
                            QPalette::ColorRole role = QPalette::ButtonText) const;
    void drawLineEditFrame(QPainter *p, const QRectF &rect, const QStyleOption *o, bool isEditable = true, bool forceFrame = false) const;
    inline QColor winUI3Color(enum WINUI3Color col) const;
    void updateColorScheme();
    QColor accentColor() const;

    // replacements for QWindowsStylePrivate / QCommonStylePrivate helpers
    bool transitionsEnabled() const;
    QStyleAnimation *animation(const QObject *target) const;
    void startAnimation(QStyleAnimation *animation) const;
    void stopAnimation(const QObject *target) const;
    static QTime animationTime();
    QString toolButtonElideText(const QStyleOptionToolButton *toolbutton, const QRect &textRect, int flags) const;
    void viewItemDrawText(QPainter *p, const QStyleOptionViewItem *option, const QRect &rect) const;

    Q_DISABLE_COPY_MOVE(ElevenStyle)

    bool highContrastTheme = false;
    mutable bool m_inItemViewItem = false; // CE_ItemViewItem is painting its own highlight
    int colorSchemeIndex = 1;
    QFont assetFont;
    mutable QHash<const QObject *, QStyleAnimation *> m_animations;
};

#endif // ELEVENSTYLE_H
