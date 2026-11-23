#pragma once
#include <QColor>
#include <QFont>
#include <QRectF>

class QPainter;

// Draws "Welcome" followed by "!" as if it were being written: the word is revealed from left to right,
// the mark from top to bottom, both with a soft edge. Shared by the splash and the welcome page.
class HandwritingPainter {
public:
    // The text is centred in `area`. Reveals run from 0 (nothing) to 1 (fully written).
    static void paint(QPainter& p, const QRectF& area, const QFont& font, const QColor& ink,
                      double wordReveal, double markReveal);
};
