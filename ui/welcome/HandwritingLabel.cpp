#include "welcome/HandwritingLabel.h"
#include "app/SplashTimeline.h"
#include "theme/ThemeManager.h"
#include "widgets/HandwritingPainter.h"

#include <QPainter>
#include <QVariantAnimation>

namespace {
constexpr int kHeroPointSize = 60;
constexpr int kHeight = 120;
}

HandwritingLabel::HandwritingLabel(QWidget* parent)
    : QWidget(parent), animation_(new QVariantAnimation(this))
{
    setAccessibleName(QStringLiteral("Welcome!"));
    setFixedHeight(kHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    animation_->setStartValue(0);
    animation_->setEndValue(kPlayMs);
    animation_->setDuration(kPlayMs);
    connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        elapsedMs_ = value.toInt();
        update();
        emit frameChanged(elapsedMs_);
    });
    connect(animation_, &QVariantAnimation::finished, this, [this] { played_ = true; });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

QSize HandwritingLabel::sizeHint() const
{
    return QSize(480, kHeight);
}

void HandwritingLabel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (played_) {
        elapsedMs_ = kPlayMs;
        emit frameChanged(elapsedMs_);
    } else if (animation_->state() == QAbstractAnimation::Stopped) {
        animation_->start();
    }
}

void HandwritingLabel::paintEvent(QPaintEvent*)
{
    const SplashFrame frame = SplashTimeline::at(elapsedMs_);
    const ThemeManager& theme = ThemeManager::instance();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    HandwritingPainter::paint(p, rect(), theme.font(FontRole::Script, kHeroPointSize),
                              theme.color(Token::TextPrimary), frame.wordReveal, frame.markReveal);
}
