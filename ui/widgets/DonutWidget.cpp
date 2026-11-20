#include "DonutWidget.h"

#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QtMath>

namespace
{
constexpr qreal kMaxDiameter = 130.0;   // reference ring size
constexpr qreal kLegendGap = 24.0;      // ring -> legend
constexpr qreal kDotDiameter = 10.0;
constexpr qreal kLegendRowGap = 10.0;

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


DonutWidget::DonutWidget(QWidget* parent)
    : QWidget(parent),
    doneValue(0),
    activeValue(0),
    todoValue(0)
{
    setObjectName("donutWidget");

    // Needed so QSS background/border paint on a custom QWidget.
    setAttribute(Qt::WA_StyledBackground, true);

    // Fallbacks only (palette roles, no literals). home.qss overrides them.
    m_doneColor = palette().color(QPalette::Link);
    m_activeColor = palette().color(QPalette::Highlight);
    m_trackColor = palette().color(QPalette::AlternateBase);
    m_textColor = palette().color(QPalette::WindowText);
    m_mutedColor = palette().color(QPalette::PlaceholderText);
    m_secondaryColor = palette().color(QPalette::WindowText);
    m_outlineColor = palette().color(QPalette::Mid);
}

void DonutWidget::setValues(int done, int active, int todo)
{
    const int newDone = qMax(0, done);
    const int newActive = qMax(0, active);
    const int newTodo = qMax(0, todo);

    // Unchanged values: no repaint.
    if (newDone == doneValue && newActive == activeValue && newTodo == todoValue)
        return;

    doneValue = newDone;
    activeValue = newActive;
    todoValue = newTodo;

    update();
}

void DonutWidget::setDoneColor(const QColor& color)
{
    m_doneColor = color;
    update();
}

void DonutWidget::setActiveColor(const QColor& color)
{
    m_activeColor = color;
    update();
}

void DonutWidget::setTrackColor(const QColor& color)
{
    m_trackColor = color;
    update();
}

void DonutWidget::setTextColor(const QColor& color)
{
    m_textColor = color;
    update();
}

void DonutWidget::setMutedColor(const QColor& color)
{
    m_mutedColor = color;
    update();
}

void DonutWidget::setSecondaryColor(const QColor& color)
{
    m_secondaryColor = color;
    update();
}

void DonutWidget::setOutlineColor(const QColor& color)
{
    m_outlineColor = color;
    update();
}

QSize DonutWidget::sizeHint() const
{
    return QSize(320, 140);
}

QSize DonutWidget::minimumSizeHint() const
{
    return QSize(250, 120);
}

void DonutWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int total = doneValue + activeValue + todoValue;

    // Ring on the left (max 130 px, like the reference), legend to its right.
    const QRectF area = QRectF(rect());

    const qreal diameter =
        qMax<qreal>(0.0, qMin(kMaxDiameter, qMin(area.height(), area.width() * 0.5)));
    const qreal penWidth = diameter * 0.10;   // 12 / 120 in the reference SVG

    const QPointF center(area.left() + diameter / 2.0, area.center().y());

    const QRectF ring(center.x() - diameter / 2.0 + penWidth / 2.0,
                      center.y() - diameter / 2.0 + penWidth / 2.0,
                      diameter - penWidth,
                      diameter - penWidth);

    // Track (full circle)
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(m_trackColor, penWidth, Qt::SolidLine, Qt::FlatCap));
    painter.drawEllipse(ring);

    // Segments. Angles are 1/16 degree, flat caps, span > 0 only, so a zero
    // value never draws a dot. The remainder of the ring is the To Do share.
    if (total > 0)
    {
        const int fullCircle = 360 * 16;

        const int doneAngle =
            qRound(static_cast<double>(doneValue) / total * fullCircle);

        int activeAngle =
            qRound(static_cast<double>(activeValue) / total * fullCircle);

        if (doneAngle + activeAngle > fullCircle)
        {
            activeAngle = fullCircle - doneAngle;
        }

        int startAngle = 90 * 16; // 12 o'clock, clockwise via negative span

        if (doneAngle > 0)
        {
            painter.setPen(
                QPen(m_doneColor, penWidth, Qt::SolidLine, Qt::FlatCap));
            painter.drawArc(ring, startAngle, -doneAngle);
        }

        startAngle -= doneAngle;

        if (activeAngle > 0)
        {
            painter.setPen(
                QPen(m_activeColor, penWidth, Qt::SolidLine, Qt::FlatCap));
            painter.drawArc(ring, startAngle, -activeAngle);
        }
    }

    // Centre text: completion percentage and the "Tasks" caption.
    const int percentage = total > 0
                               ? qRound(static_cast<double>(doneValue) / total * 100)
                               : 0;

    QFont percentageFont = monoFont();
    percentageFont.setPixelSize(qMax(12, qRound(diameter * 0.19)));
    percentageFont.setBold(true);

    QFont captionFont = font();
    captionFont.setPixelSize(qMax(8, qRound(diameter * 0.077)));

    const QFontMetrics percentageMetrics(percentageFont);
    const QFontMetrics captionMetrics(captionFont);

    const qreal innerWidth = qMax<qreal>(0.0, diameter - 2.0 * penWidth);
    const qreal blockHeight = percentageMetrics.height() + captionMetrics.height();
    const qreal blockTop = center.y() - blockHeight / 2.0;

    painter.setPen(m_textColor);
    painter.setFont(percentageFont);
    painter.drawText(QRectF(center.x() - innerWidth / 2.0, blockTop,
                            innerWidth, percentageMetrics.height()),
                     Qt::AlignCenter,
                     QString("%1%").arg(percentage));

    painter.setPen(m_mutedColor);
    painter.setFont(captionFont);
    painter.drawText(QRectF(center.x() - innerWidth / 2.0,
                            blockTop + percentageMetrics.height(),
                            innerWidth, captionMetrics.height()),
                     Qt::AlignCenter,
                     QStringLiteral("Tasks"));

    // Legend: "<dot> Label  <bold count>", vertically centred on the ring.
    const qreal legendX = area.left() + diameter + kLegendGap;
    const qreal legendWidth = area.right() - legendX;

    if (legendWidth < 40.0)
    {
        return;
    }

    QFont legendFont = font();
    QFont countFont = legendFont;
    countFont.setBold(true);

    const QFontMetrics legendMetrics(legendFont);
    const QFontMetrics countMetrics(countFont);

    const qreal rowHeight = qMax<qreal>(legendMetrics.height(), kDotDiameter);
    const qreal legendHeight = rowHeight * 3.0 + kLegendRowGap * 2.0;
    const qreal legendTop = center.y() - legendHeight / 2.0;

    auto drawLegendItem = [&](int index,
                              const QString& label,
                              int value,
                              const QColor& color)
    {
        const qreal y = legendTop + index * (rowHeight + kLegendRowGap);
        const qreal midY = y + rowHeight / 2.0;

        // Dot with a thin outline so pale colours stay visible on white.
        painter.setPen(QPen(m_outlineColor, 1.0));
        painter.setBrush(color);
        painter.drawEllipse(QPointF(legendX + kDotDiameter / 2.0, midY),
                            kDotDiameter / 2.0 - 0.5,
                            kDotDiameter / 2.0 - 0.5);

        const qreal textX = legendX + kDotDiameter + 8.0;
        const qreal labelWidth = legendMetrics.horizontalAdvance(label);
        const qreal countWidth =
            countMetrics.horizontalAdvance(QString::number(value));
        const qreal available = legendWidth - (textX - legendX);

        const QString shownLabel = legendMetrics.elidedText(
            label, Qt::ElideRight,
            qMax(0, static_cast<int>(available - countWidth - 6.0)));

        painter.setFont(legendFont);
        painter.setPen(m_secondaryColor);
        painter.drawText(QRectF(textX, y, labelWidth + 2.0, rowHeight),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         shownLabel);

        painter.setFont(countFont);
        painter.setPen(m_textColor);
        painter.drawText(QRectF(textX + labelWidth + 6.0, y,
                                countWidth + 2.0, rowHeight),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QString::number(value));
    };

    drawLegendItem(0, QStringLiteral("Done"), doneValue, m_doneColor);
    drawLegendItem(1, QStringLiteral("Active"), activeValue, m_activeColor);
    drawLegendItem(2, QStringLiteral("To do"), todoValue, m_trackColor);
}
