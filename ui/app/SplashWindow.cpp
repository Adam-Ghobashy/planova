#include "app/SplashWindow.h"
#include "app/SplashTimeline.h"
#include "theme/ThemeManager.h"
#include "widgets/HandwritingPainter.h"

#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVariantAnimation>

namespace {
constexpr int kWidth = 520;
constexpr int kHeight = 300;
constexpr int kCornerRadius = 20;
constexpr int kBlurDivisor = 8;
constexpr double kBackdropOpacity = 0.5;
constexpr double kSheenEnd = 0.55;      // the sheen covers the top 55% of the card
constexpr int kHeroTop = 60;
constexpr int kHeroHeight = 110;
constexpr int kSubtitleTop = 190;

// Cheap blur: shrink and enlarge twice with smooth scaling.
QImage blurred(const QImage& source)
{
    QImage result = source;
    for (int pass = 0; pass < 2; ++pass) {
        const QSize full = result.size();
        const QSize small(qMax(1, full.width() / kBlurDivisor), qMax(1, full.height() / kBlurDivisor));
        result = result.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                       .scaled(full, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return result;
}
}

SplashWindow::SplashWindow(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);   // so a key press reaches keyPressEvent
    setFixedSize(kWidth, kHeight);

    if (const QScreen* screen = QGuiApplication::primaryScreen())
        move(screen->geometry().center() - rect().center());

    m_animation = new QVariantAnimation(this);
    m_animation->setStartValue(0);
    m_animation->setEndValue(SplashTimeline::kTotalMs);
    m_animation->setDuration(SplashTimeline::kTotalMs);
    m_animation->setEasingCurve(QEasingCurve::Linear);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        onTick(value.toInt());
    });
    connect(m_animation, &QVariantAnimation::finished, this, [this]() { emit finished(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() { update(); });
}

void SplashWindow::start()
{
    // The card is not on screen yet, so grabbing the screen at its position gives what lies behind it.
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QPoint topLeft = pos() - screen->geometry().topLeft();
        const QPixmap behind = screen->grabWindow(0, topLeft.x(), topLeft.y(), width(), height());
        if (!behind.isNull()) {
            m_backdrop = QPixmap::fromImage(blurred(behind.toImage()));
            m_backdrop.setDevicePixelRatio(behind.devicePixelRatio());
        }
    }

    onTick(0);
    show();
    activateWindow();
    setFocus();
    m_animation->start();
}

void SplashWindow::skipToEnd()
{
    if (m_animation->state() == QAbstractAnimation::Running && m_animation->currentTime() < SplashTimeline::kSkipMs)
        m_animation->setCurrentTime(SplashTimeline::kSkipMs);
}

void SplashWindow::onTick(int elapsedMs)
{
    m_elapsedMs = elapsedMs;
    setWindowOpacity(SplashTimeline::at(elapsedMs).windowOpacity);
    update();
}

void SplashWindow::mousePressEvent(QMouseEvent*)
{
    skipToEnd();
}

void SplashWindow::keyPressEvent(QKeyEvent*)
{
    skipToEnd();
}

void SplashWindow::paintEvent(QPaintEvent*)
{
    const ThemeManager& theme = ThemeManager::instance();
    const SplashFrame frame = SplashTimeline::at(m_elapsedMs);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(card, kCornerRadius, kCornerRadius);

    p.save();
    p.setClipPath(shape);
    // The desktop is only half visible over a solid base. Without the base a very dark (light theme) or very
    // light (dark theme) desktop would pull the subtitle below the 4.5:1 text contrast.
    p.fillPath(shape, theme.color(Token::Background));
    if (!m_backdrop.isNull()) {
        p.setOpacity(kBackdropOpacity);
        p.drawPixmap(rect(), m_backdrop);
        p.setOpacity(1.0);
    }
    p.fillPath(shape, theme.color(Token::SurfaceGlass));

    QColor sheenStart = theme.color(Token::GlassSheen);
    QColor sheenEnd = sheenStart;
    sheenEnd.setAlpha(0);
    QLinearGradient sheen(0, 0, 0, height() * kSheenEnd);
    sheen.setColorAt(0.0, sheenStart);
    sheen.setColorAt(1.0, sheenEnd);
    p.fillRect(QRectF(0, 0, width(), height() * kSheenEnd), sheen);
    p.restore();

    p.setPen(QPen(theme.color(Token::BorderStrong), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape);

    HandwritingPainter::paint(p, QRectF(0, kHeroTop, width(), kHeroHeight),
                              theme.font(FontRole::Script, 44), theme.color(Token::TextPrimary),
                              frame.wordReveal, frame.markReveal);

    QColor subtitleColor = theme.color(Token::TextSecondary);
    subtitleColor.setAlphaF(subtitleColor.alphaF() * frame.subtitleOpacity);
    p.setPen(subtitleColor);
    p.setFont(theme.font(FontRole::Body, 11));
    p.drawText(QRectF(0, kSubtitleTop + frame.subtitleRise, width(), 30), Qt::AlignHCenter | Qt::AlignTop,
               QStringLiteral("Plan the work \u00B7 Match the team \u00B7 Measure actual progress"));
}
