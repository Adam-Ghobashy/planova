#include "BarChartWidget.h"

#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRectF>
#include <QSizePolicy>
#include <QtMath>

#include <cmath>

namespace
{
constexpr int kMarginY = 2;
constexpr int kRowPitch = 25;      // 16 px row + 9 px gap (reference)
constexpr int kRowHeight = 16;
constexpr int kBarHeight = 7;
constexpr int kGap = 12;
constexpr int kMinLabelWidth = 65;
constexpr int kMinValueWidth = 32;
constexpr int kMinHeight = 60;
constexpr int kValuePixelSize = 11;

// Reference --font-mono: JetBrains Mono, Consolas, monospace.
QFont monoFont()
{
    QFont f;
    f.setFamilies({QStringLiteral("JetBrains Mono"), QStringLiteral("Consolas"),
                   QStringLiteral("Menlo"), QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("monospace")});
    return f;
}
}


BarChartWidget::BarChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("barChartWidget");

    // Needed so QSS background/border paint on a custom QWidget.
    setAttribute(Qt::WA_StyledBackground, true);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Fallbacks only (palette roles, no literals). The .qss files override them.
    m_barColor = palette().color(QPalette::Highlight);
    m_trackColor = palette().color(QPalette::AlternateBase);
    m_textColor = palette().color(QPalette::WindowText);
    m_mutedColor = palette().color(QPalette::PlaceholderText);
    m_memberColors.fill(m_barColor);
}

void BarChartWidget::setBars(
    const std::vector<Bar>& newBars,
    const QString& newUnit
    )
{
    bars = newBars;
    unit = newUnit;

    updateGeometry();
    update();
}

void BarChartWidget::setBarColor(const QColor& color)
{
    m_barColor = color;
    update();
}

void BarChartWidget::setTrackColor(const QColor& color)
{
    m_trackColor = color;
    update();
}

void BarChartWidget::setTextColor(const QColor& color)
{
    m_textColor = color;
    update();
}

void BarChartWidget::setMutedColor(const QColor& color)
{
    m_mutedColor = color;
    update();
}

void BarChartWidget::setMinScale(double value)
{
    m_minScale = qMax(0.0, value);
    update();
}

void BarChartWidget::setMemberColor(int index, const QColor& color)
{
    if (index < 0 || index >= static_cast<int>(m_memberColors.size()))
    {
        return;
    }

    m_memberColors[static_cast<size_t>(index)] = color;
    update();
}

QColor BarChartWidget::colorFor(const Bar& bar) const
{
    if (bar.memberIndex >= 0
        && bar.memberIndex < static_cast<int>(m_memberColors.size()))
    {
        return m_memberColors[static_cast<size_t>(bar.memberIndex)];
    }

    return m_barColor;
}

QString BarChartWidget::valueText(double value) const
{
    // Whole numbers without decimals, otherwise one decimal. No exponent form.
    QString text = qFuzzyIsNull(value - std::round(value))
                       ? QString::number(static_cast<qlonglong>(std::llround(value)))
                       : QString::number(value, 'f', 1);

    // One-letter units hug the number ("12h"); longer units are spaced ("4 pts").
    if (!unit.isEmpty())
    {
        if (unit.size() > 1)
        {
            text += QLatin1Char(' ');
        }

        text += unit;
    }

    return text;
}

QSize BarChartWidget::sizeHint() const
{
    const int rows = static_cast<int>(bars.size());
    const int height =
        rows > 0 ? 2 * kMarginY + (rows - 1) * kRowPitch + kRowHeight : 0;

    return QSize(320, qMax(kMinHeight, height));
}

QSize BarChartWidget::minimumSizeHint() const
{
    return QSize(200, sizeHint().height());
}

void BarChartWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Names: widget font, semi-bold. Values: fixed-pitch, small.
    QFont labelFont = font();
    labelFont.setBold(true);

    QFont valueFont = monoFont();
    valueFont.setPixelSize(kValuePixelSize);

    const QFontMetrics labelMetrics(labelFont);
    const QFontMetrics valueMetrics(valueFont);

    if (bars.empty())
    {
        painter.setPen(m_mutedColor);
        painter.setFont(font());
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("No completed hours yet"));
        return;
    }

    // Column widths follow the content.
    int maxLabel = 0;
    int valueWidth = kMinValueWidth;
    double maxValue = 0.0;

    for (const Bar& bar : bars)
    {
        maxLabel = qMax(maxLabel, labelMetrics.horizontalAdvance(bar.label));
        valueWidth = qMax(valueWidth,
                          valueMetrics.horizontalAdvance(valueText(bar.value)));
        maxValue = qMax(maxValue, bar.value);
    }

    // Bars share one scale: at least m_minScale, or the largest value.
    maxValue = qMax(maxValue, m_minScale);

    const int labelWidth =
        qMin(qMax(maxLabel, kMinLabelWidth),
             qMax(kMinLabelWidth, static_cast<int>(width() * 0.35)));

    const int barX = labelWidth + kGap;
    const int barWidth = qMax(0, width() - barX - kGap - valueWidth);

    for (size_t i = 0; i < bars.size(); ++i)
    {
        const Bar& bar = bars[i];

        const int y = kMarginY + static_cast<int>(i) * kRowPitch;
        const qreal barY = y + (kRowHeight - kBarHeight) / 2.0;

        // Name
        painter.setFont(labelFont);
        painter.setPen(m_textColor);
        painter.drawText(
            QRectF(0, y, labelWidth, kRowHeight),
            Qt::AlignLeft | Qt::AlignVCenter,
            labelMetrics.elidedText(bar.label, Qt::ElideRight, labelWidth)
            );

        // Track
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_trackColor);
        painter.drawRoundedRect(QRectF(barX, barY, barWidth, kBarHeight),
                                kBarHeight / 2.0, kBarHeight / 2.0);

        // Fill (negative values are treated as 0)
        const double ratio =
            maxValue > 0.0 ? qBound(0.0, bar.value / maxValue, 1.0) : 0.0;

        // Never narrower than the corner diameter, so tiny values stay round.
        const double filledWidth =
            ratio > 0.0 ? qMax<double>(kBarHeight, barWidth * ratio) : 0.0;

        if (filledWidth > 0.0 && barWidth > 0)
        {
            painter.setBrush(colorFor(bar));
            painter.drawRoundedRect(
                QRectF(barX, barY, qMin<double>(filledWidth, barWidth), kBarHeight),
                kBarHeight / 2.0, kBarHeight / 2.0);
        }

        // Value
        painter.setFont(valueFont);
        painter.setPen(m_mutedColor);
        painter.drawText(
            QRectF(barX + barWidth + kGap, y, valueWidth, kRowHeight),
            Qt::AlignRight | Qt::AlignVCenter,
            valueText(bar.value)
            );
    }
}
