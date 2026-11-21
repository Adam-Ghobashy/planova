#include "welcome/StepperWidget.h"
#include "theme/ThemeManager.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kCircle = 32;     // circle diameter in px
constexpr int kLine = 3;        // connecting line thickness
constexpr int kLabelGap = 8;    // space between circle and label
constexpr int kLabelHeight = 18;
}

StepperWidget::StepperWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("StepperWidget");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(tr("Setup progress"));
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

void StepperWidget::setSteps(const QStringList& labels)
{
    labels_ = labels;
    update();
}

void StepperWidget::setCurrent(int index)
{
    current_ = index;
    setAccessibleDescription(index >= 0 && index < labels_.size()
                                 ? tr("Step %1 of %2: %3").arg(index + 1).arg(labels_.size()).arg(labels_[index])
                                 : QString());
    update();
}

QSize StepperWidget::sizeHint() const
{
    return QSize(labels_.size() * 120, kCircle + kLabelGap + kLabelHeight);
}

QSize StepperWidget::minimumSizeHint() const
{
    return QSize(labels_.size() * 80, kCircle + kLabelGap + kLabelHeight);
}

void StepperWidget::paintEvent(QPaintEvent*)
{
    if (labels_.isEmpty())
        return;
    const ThemeManager& theme = ThemeManager::instance();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int count = labels_.size();
    const double columnWidth = double(width()) / count;
    const auto centerX = [&](int i) { return columnWidth * i + columnWidth / 2; };
    const double circleY = kCircle / 2.0;

    // Track under every circle, accent fill from the first circle up to the current one.
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme.color(Token::Track));
    painter.drawRect(QRectF(centerX(0), circleY - kLine / 2.0, centerX(count - 1) - centerX(0), kLine));
    if (current_ > 0) {
        painter.setBrush(theme.color(Token::Accent));
        painter.drawRect(QRectF(centerX(0), circleY - kLine / 2.0, centerX(current_) - centerX(0), kLine));
    }

    QFont numberFont = theme.font(FontRole::Body, 10);
    numberFont.setBold(true);
    for (int i = 0; i < count; ++i) {
        const bool done = i < current_;
        const bool active = i == current_;
        const QRectF circle(centerX(i) - kCircle / 2.0 + 1, 1, kCircle - 2, kCircle - 2);

        QColor fill = theme.color(Token::Surface);
        QColor border = theme.color(Token::BorderStrong);
        QColor text = theme.color(Token::TextMuted);
        if (active) {
            fill = border = theme.color(Token::Accent);
            text = theme.color(Token::AccentText);
        } else if (done) {
            fill = theme.color(Token::SuccessSoft);
            border = theme.color(Token::StatusConnected);
            text = theme.color(Token::PositiveText);
        }
        painter.setBrush(fill);
        painter.setPen(QPen(border, 2));
        painter.drawEllipse(circle);

        if (done) {
            // A check mark, drawn as a path so it does not depend on a font having the glyph.
            const QPointF c = circle.center();
            QPainterPath check;
            check.moveTo(c.x() - 6, c.y() + 0.5);
            check.lineTo(c.x() - 2, c.y() + 4.5);
            check.lineTo(c.x() + 6, c.y() - 4);
            painter.setPen(QPen(text, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(check);
        } else {
            painter.setPen(text);
            painter.setFont(numberFont);
            painter.drawText(circle, Qt::AlignCenter, QString::number(i + 1));
        }

        // The current label is bold and accent coloured, so it is not marked by colour alone.
        QFont labelFont = theme.font(FontRole::Body, 9);
        labelFont.setBold(active);
        labelFont.setWeight(active ? QFont::Bold : QFont::DemiBold);
        painter.setFont(labelFont);
        painter.setPen(active ? theme.color(Token::Accent)
                              : done ? theme.color(Token::TextSecondary) : theme.color(Token::TextMuted));
        const QRectF labelRect(columnWidth * i + 2, kCircle + kLabelGap, columnWidth - 4, kLabelHeight);
        painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop,
                         QFontMetrics(labelFont).elidedText(labels_[i], Qt::ElideRight, int(labelRect.width())));
    }
}
