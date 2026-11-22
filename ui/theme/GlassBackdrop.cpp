#include "GlassBackdrop.h"
#include "ThemeManager.h"
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>

namespace {
// A round glow: the token colour in the centre, fading to fully transparent at the radius.
void paintGlow(QPainter& painter, const QRectF& area, QPointF centre, qreal radius, const QColor& colour)
{
    QColor transparent = colour;
    transparent.setAlpha(0);
    QRadialGradient glow(centre, radius);
    glow.setColorAt(0.0, colour);
    glow.setColorAt(1.0, transparent);
    painter.fillRect(area, glow);
}
}

GlassBackdrop::GlassBackdrop(QWidget* parent) : QWidget(parent)
{
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        rebuildCache();
        update();
    });
}

QPixmap GlassBackdrop::cachedBackdrop() const
{
    return cache_;
}

void GlassBackdrop::paintEvent(QPaintEvent*)
{
    if (cache_.isNull())
        rebuildCache();
    QPainter painter(this);
    painter.drawPixmap(0, 0, cache_);
}

void GlassBackdrop::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuildCache();
}

void GlassBackdrop::rebuildCache()
{
    if (size().isEmpty()) {
        cache_ = QPixmap();
        return;
    }
    // Draw at the screen's real pixel density so the gradient stays smooth on high-density screens.
    const qreal ratio = devicePixelRatioF();
    cache_ = QPixmap(size() * ratio);
    cache_.setDevicePixelRatio(ratio);

    const ThemeManager& theme = ThemeManager::instance();
    const QRectF area = rect();
    const qreal w = area.width();
    const qreal h = area.height();

    QPainter painter(&cache_);
    QLinearGradient base(0, 0, 0, h);
    base.setColorAt(0.0, theme.color(Token::BackdropTop));
    base.setColorAt(1.0, theme.color(Token::BackdropBottom));
    painter.fillRect(area, base);

    paintGlow(painter, area, QPointF(w * 0.15, h * 0.10), w * 0.60, theme.color(Token::GlowPrimary));
    paintGlow(painter, area, QPointF(w * 0.90, h * 0.95), w * 0.55, theme.color(Token::GlowSecondary));
}
