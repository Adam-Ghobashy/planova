#include "widgets/HandwritingPainter.h"
#include <QFontMetricsF>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>

namespace {
constexpr double kEdgeSoftness = 0.07;   // soft edge, as a share of the image size
constexpr double kMarkGap = 2.0;         // pixels between the word and the "!"

// Renders `text` as `ink` into a transparent image. The pixel ratio keeps it sharp on scaled screens.
QImage renderText(const QString& text, const QFont& font, const QColor& ink, const QSizeF& size, qreal pixelRatio)
{
    QImage image((size * pixelRatio).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(pixelRatio);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(ink);
    painter.drawText(QRectF(QPointF(0, 0), size), Qt::AlignCenter, text);
    return image;
}

// Hides the part of the image beyond the reveal edge. The edge starts before the image and ends after it,
// so reveal 0 hides everything and reveal 1 shows everything.
void applyReveal(QImage& image, const QSizeF& size, double reveal, Qt::Orientation direction)
{
    const double length = direction == Qt::Horizontal ? size.width() : size.height();
    const double edge = reveal * length * (1.0 + kEdgeSoftness) - kEdgeSoftness * length;
    const double fade = kEdgeSoftness * length;

    QLinearGradient mask;
    if (direction == Qt::Horizontal)
        mask = QLinearGradient(edge, 0, edge + fade, 0);
    else
        mask = QLinearGradient(0, edge, 0, edge + fade);
    mask.setColorAt(0.0, Qt::black);                  // only the alpha matters for DestinationIn
    mask.setColorAt(1.0, Qt::transparent);

    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.fillRect(QRectF(QPointF(0, 0), size), mask);
}
}

void HandwritingPainter::paint(QPainter& p, const QRectF& area, const QFont& font, const QColor& ink,
                               double wordReveal, double markReveal)
{
    const QFontMetricsF metrics(font);
    const QString word = QStringLiteral("Welcome");
    const QString mark = QStringLiteral("!");

    // Script glyphs swing outside their advance width, so each image gets some room around the text.
    const double pad = metrics.height() * 0.2;
    const QSizeF wordSize(metrics.horizontalAdvance(word) + 2 * pad, metrics.height() + 2 * pad);
    const QSizeF markSize(metrics.horizontalAdvance(mark) + 2 * pad, metrics.height() + 2 * pad);

    const double totalWidth = wordSize.width() + markSize.width() - 4 * pad + kMarkGap;
    const double left = area.center().x() - totalWidth / 2 - pad;
    const double top = area.center().y() - wordSize.height() / 2;
    const qreal ratio = p.device()->devicePixelRatioF();

    if (wordReveal > 0.0) {
        QImage image = renderText(word, font, ink, wordSize, ratio);
        applyReveal(image, wordSize, qBound(0.0, wordReveal, 1.0), Qt::Horizontal);
        p.drawImage(QPointF(left, top), image);
    }
    if (markReveal > 0.0) {
        QImage image = renderText(mark, font, ink, markSize, ratio);
        applyReveal(image, markSize, qBound(0.0, markReveal, 1.0), Qt::Vertical);
        const double markLeft = left + wordSize.width() - 2 * pad + kMarkGap;
        p.drawImage(QPointF(markLeft, top), image);
    }
}
