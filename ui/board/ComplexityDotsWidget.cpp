#include "ComplexityDotsWidget.h"
#include <QPainter>
#include "theme/ThemeManager.h"
#include "theme/Tokens.h"

namespace {
constexpr int kDots = 5;
constexpr int kDiameter = 10;
constexpr int kGap = 5;
}

ComplexityDotsWidget::ComplexityDotsWidget(int level, QWidget* parent)
    : QWidget(parent), level_(qBound(0, level, kDots)) {
    setFixedSize(sizeHint());
    setToolTip(tr("Complexity %1/5").arg(level_));
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

QSize ComplexityDotsWidget::sizeHint() const {
    return QSize(kDots * kDiameter + (kDots - 1) * kGap, kDiameter + 2);
}

void ComplexityDotsWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    const ThemeManager& theme = ThemeManager::instance();
    // 1-2 green, 3 yellow, 4 orange, 5 purple (as in Figma)
    const Token filled = level_ <= 2 ? Token::Complexity2
                         : level_ == 3 ? Token::Complexity3
                         : level_ == 4 ? Token::Complexity4
                                       : Token::Accent;
    QColor empty = theme.color(Token::TextMuted);
    empty.setAlpha(55);

    for (int i = 0; i < kDots; ++i) {
        p.setBrush(i < level_ ? theme.color(filled) : empty);
        p.drawEllipse(i * (kDiameter + kGap), 1, kDiameter, kDiameter);
    }
}