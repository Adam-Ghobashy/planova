#pragma once
// Shared helper for the keyboard focus-ring tests. It renders a widget without and with keyboard focus and checks
// that the focused rendering has a visible ring: a run of at least two pixels on the left edge, on the vertical
// centre line, that differs from the widget fill by at least 3:1 (WCAG non-text contrast) and that the focus
// created. Used by tst_FocusRings, tst_WelcomePage and tst_SetupPage so the check exists once.
#include <QAbstractButton>
#include <QApplication>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QString>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace focusring {

inline double luminance(const QColor& c)
{
    auto channel = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}

inline double contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// The most frequent opaque colour of the image: the fill of a button or field (text and ring are minorities).
inline QColor dominantColor(const QImage& image)
{
    QHash<QRgb, int> counts;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)) == 255)
                ++counts[image.pixel(x, y)];
    QRgb best = 0;
    int bestCount = -1;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (it.value() > bestCount) {
            best = it.key();
            bestCount = it.value();
        }
    }
    return QColor::fromRgb(best);
}

inline bool nearlySame(const QColor& a, const QColor& b)
{
    return std::abs(a.red() - b.red()) < 6 && std::abs(a.green() - b.green()) < 6 && std::abs(a.blue() - b.blue()) < 6;
}

// Bounding box of the pixels that differ clearly from `fill`, ignoring an 8 px band at the edge (ring and rounded
// corners). For a button this is its text and icon: when the focus rule does not compensate the border width with
// padding, this box moves.
inline QRect contentBox(const QImage& image, const QColor& fill)
{
    QRect box;
    for (int y = 8; y < image.height() - 8; ++y) {
        for (int x = 8; x < image.width() - 8; ++x) {
            const QColor c = image.pixelColor(x, y);
            if (std::abs(c.red() - fill.red()) + std::abs(c.green() - fill.green()) + std::abs(c.blue() - fill.blue()) > 60)
                box = box.isNull() ? QRect(x, y, 1, 1) : box.united(QRect(x, y, 1, 1));
        }
    }
    return box;
}

struct Result {
    bool ok = false;
    QString why;      // empty when ok
    QColor ring;
    QColor fill;
    QColor outside;   // colour just outside the widget (invalid when not measured)
};

enum class Shape {
    Box,       // buttons, fields, cards: a border ring on the left edge
    Indicator, // check boxes and sliders: the indicator changes, the widget edge does not
};

// The widget as the user sees it: cropped out of its parent's rendering, so a transparent button shows the page
// colour behind it instead of empty pixels.
inline QImage render(QWidget* widget)
{
    QApplication::processEvents();
    QWidget* parent = widget->parentWidget();
    const QPixmap pixmap = parent ? parent->grab(widget->geometry()) : widget->grab();
    return pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
}

// Gives `widget` keyboard focus (Tab reason, so the style shows the ring) and checks the ring.
// Returns ok=false with `why` filled when the focus could not be set or the ring is missing.
// With checkOutside, the ring must also contrast 3:1 with the pixel two pixels left of the widget in the window
// rendering (when that pixel exists and is opaque). minFillContrast is 3.0 (WCAG non-text contrast); a caller may lower
// it for one documented case.
inline Result check(QWidget* widget, Shape shape = Shape::Box, bool checkOutside = false, double minFillContrast = 3.0)
{
    Result result;
    widget->clearFocus();
    const QImage off = render(widget);
    const QSize hintBefore = widget->sizeHint();
    widget->setFocus(Qt::TabFocusReason);
    if (!widget->hasFocus()) {
        result.why = "could not take focus";
        return result;
    }
    const QImage on = render(widget);
    if (on.size() != off.size() || widget->sizeHint() != hintBefore) {
        result.why = "focus changes the widget size";
        return result;
    }

    result.fill = dominantColor(on);
    const int h = on.height();

    // Buttons must not shift their content: the border grows, so the padding must shrink by the same amount.
    // (Text fields are skipped: the caret appears with focus.)
    if (shape == Shape::Box && qobject_cast<QAbstractButton*>(widget)) {
        const QRect before = contentBox(off, dominantColor(off));
        const QRect after = contentBox(on, result.fill);
        // Compare the centres (twice their value, in half pixels): the text colour changes with the focus look, and
        // that moves the anti-aliased edge pixels by one pixel without moving the text.
        const int shiftX = std::abs((before.x() * 2 + before.width()) - (after.x() * 2 + after.width()));
        const int shiftY = std::abs((before.y() * 2 + before.height()) - (after.y() * 2 + after.height()));
        if (shiftX > 1 || shiftY > 1) {
            result.why = QString("focus moves the content of the button (%1,%2 %3x%4 before, %5,%6 %7x%8 after): padding not compensated")
                             .arg(before.x()).arg(before.y()).arg(before.width()).arg(before.height())
                             .arg(after.x()).arg(after.y()).arg(after.width()).arg(after.height());
            return result;
        }
    }

    int changed = 0;
    for (int py = 0; py < h; ++py)
        for (int px = 0; px < on.width(); ++px)
            if (on.pixel(px, py) != off.pixel(px, py))
                ++changed;
    if (changed < 12) {
        result.why = "focused rendering is identical to the unfocused one";
        return result;
    }
    if (shape == Shape::Indicator) {
        result.ok = true; // the indicator (box or handle) changed; its colours are covered by the audit table
        return result;
    }

    // Box: scan the centre row on the left edge, x = 0..7, for a run of >= 2 equal opaque pixels.
    const int y = h / 2;
    const int xMax = std::min(8, on.width());
    int x = 0;
    while (x < xMax) {
        const QColor c = on.pixelColor(x, y);
        int run = 1;
        while (x + run < xMax && c.alpha() == 255 && nearlySame(c, on.pixelColor(x + run, y))) {
            ++run;
        }
        if (c.alpha() == 255 && run >= 2 && contrast(c, result.fill) >= minFillContrast) {
            result.ring = c;
            result.ok = true;
            break;
        }
        x += run;
    }
    if (!result.ok) {
        // The best candidate for the message: the colour at x = 1.
        result.ring = on.pixelColor(std::min(1, on.width() - 1), y);
        result.why = QString("no ring: edge colour %1 vs fill %2 (contrast %3, needs 3 against the focused fill)")
                         .arg(result.ring.name(), result.fill.name())
                         .arg(contrast(result.ring, result.fill), 0, 'f', 2);
        return result;
    }

    if (checkOutside) {
        // The window rendering includes opacity effects and parent fills, so the pixel is what the user sees.
        QWidget* window = widget->window();
        const QPoint origin = widget->mapTo(window, QPoint(0, 0));
        const int px = origin.x() - 2;
        const int py = origin.y() + y;
        if (px >= 0 && py >= 0 && px < window->width() && py < window->height()) {
            QApplication::processEvents();
            const QImage around = window->grab().toImage();
            result.outside = around.pixelColor(px, py);
            if (result.outside.alpha() == 255 && contrast(result.ring, result.outside) < 3.0) {
                result.ok = false;
                result.why = QString("ring %1 does not stand out from the surroundings %2 (contrast %3)")
                                 .arg(result.ring.name(), result.outside.name())
                                 .arg(contrast(result.ring, result.outside), 0, 'f', 2);
            }
        }
    }
    return result;
}

} // namespace focusring
