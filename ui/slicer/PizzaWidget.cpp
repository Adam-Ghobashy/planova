#include "slicer/PizzaWidget.h"
#include "theme/ThemeManager.h"
#include <QEasingCurve>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace planova::slicer {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMargin = 20.0;
constexpr double kLegendReserve = 90.0;  // space kept under the pie for the legend
constexpr double kRestOffset = 6.0;      // gap between slices when idle
constexpr double kSpreadOffset = 10.0;   // extra gap at the peak of the animation
constexpr double kThinSliceDegrees = 3.0;
constexpr double kMinBadgeSpan = 25.0;   // slices narrower than this get no initials or toppings

QString initialsOf(const std::string& name) {
    const QStringList parts = QString::fromStdString(name).trimmed().split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return {};
    if (parts.size() == 1) return parts[0].left(1).toUpper();
    return (parts.first().left(1) + parts.last().left(1)).toUpper();
}

QString firstNameOf(const std::string& name) {
    const QStringList parts = QString::fromStdString(name).trimmed().split(' ', Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.first();
}

// Member1..Member6 repeat for teams larger than six.
QColor memberColor(int index) {
    return ThemeManager::instance().color(static_cast<Token>(static_cast<int>(Token::Member1) + (index % 6)));
}

// A point at `distance` from `center` in direction `angleDeg` (clockwise from 12 o'clock).
QPointF pointOnCircle(const QPointF& center, double distance, double angleDeg) {
    const double rad = qDegreesToRadians(angleDeg);
    return QPointF(center.x() + distance * std::sin(rad), center.y() - distance * std::cos(rad));
}

} // namespace

PizzaWidget::PizzaWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(280, 400);

    anim_ = new QVariantAnimation(this);
    anim_->setDuration(700);
    anim_->setStartValue(0.0);
    anim_->setEndValue(1.0);
    anim_->setEasingCurve(QEasingCurve::InOutQuad);
    connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        animT_ = v.toDouble();
        update();
    });
}

void PizzaWidget::setPortions(const std::vector<planova::slicer::Portion>& portions) {
    portions_ = portions;
    update();
}

void PizzaWidget::sliceAnimated() {
    anim_->stop();
    anim_->start();
}

void PizzaWidget::setSelectedMember(const QString& memberId) {
    if (selectedMemberId_ == memberId) return;
    selectedMemberId_ = memberId;
    update();
}

double PizzaWidget::pieRadius() const {
    const double byWidth = (width() - 2 * kMargin) / 2.0;
    const double byHeight = (height() - kLegendReserve - 2 * kMargin) / 2.0;
    return std::max(40.0, std::min(byWidth, byHeight));
}

QPointF PizzaWidget::pieCenter() const {
    return QPointF(width() / 2.0, kMargin + pieRadius());
}

bool PizzaWidget::hasAssignedSlices() const {
    return std::any_of(portions_.begin(), portions_.end(), [](const Portion& p) { return !p.thinSlice; });
}

int PizzaWidget::sliceAt(const QPointF& pos) const {
    if (!hasAssignedSlices()) return -1;
    const double dx = pos.x() - pieCenter().x();
    const double dy = pos.y() - pieCenter().y();
    if (std::hypot(dx, dy) > pieRadius()) return -1;

    // Clockwise angle from 12 o'clock, 0..360
    double angle = qRadiansToDegrees(std::atan2(dx, -dy));
    if (angle < 0.0) angle += 360.0;

    for (size_t i = 0; i < portions_.size(); ++i) {
        const Portion& p = portions_[i];
        if (p.thinSlice) continue;
        if (angle >= p.startAngle && angle < p.startAngle + p.spanAngle) return static_cast<int>(i);
    }
    // The last slice owns the 360 degree edge.
    for (size_t i = portions_.size(); i-- > 0;) {
        if (!portions_[i].thinSlice) return static_cast<int>(i);
    }
    return -1;
}

void PizzaWidget::toggleSelection(int portionIndex) {
    const QString id = QString::fromStdString(portions_[portionIndex].memberId);
    selectedMemberId_ = (selectedMemberId_ == id) ? QString() : id;
    update();
    emit memberSelected(selectedMemberId_);
}

void PizzaWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF pos = event->position();
    for (size_t i = 0; i < legendRects_.size() && i < portions_.size(); ++i) {
        if (legendRects_[i].contains(pos)) {
            toggleSelection(static_cast<int>(i));
            return;
        }
    }
    const int slice = sliceAt(pos);
    if (slice >= 0) {
        toggleSelection(slice);
    } else if (!selectedMemberId_.isEmpty()) {
        selectedMemberId_.clear();
        update();
        emit memberSelected(QString());
    }
}

void PizzaWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    auto& tm = ThemeManager::instance();

    if (portions_.empty()) {
        painter.setPen(tm.color(Token::TextMuted));
        painter.drawText(rect(), Qt::AlignCenter, "No workload data");
        legendRects_.clear();
        return;
    }

    if (!hasAssignedSlices()) {
        drawEmptyState(painter);
    } else {
        // Cut lines first, so they show in the gaps between spread slices. They grow out of the
        // centre during the first part of the animation.
        const double cutLength = pieRadius() * std::min(1.0, animT_ * 2.5);
        painter.setPen(QPen(tm.color(Token::Border), 3.0, Qt::SolidLine, Qt::RoundCap));
        for (const Portion& p : portions_) {
            if (p.thinSlice) continue;
            painter.drawLine(pieCenter(), pointOnCircle(pieCenter(), cutLength, p.startAngle));
        }
        for (size_t i = 0; i < portions_.size(); ++i) drawSlice(painter, static_cast<int>(i));
    }
    drawLegend(painter);
}

void PizzaWidget::drawEmptyState(QPainter& painter) const {
    auto& tm = ThemeManager::instance();
    const QPointF c = pieCenter();
    const double r = pieRadius();
    painter.setPen(QPen(tm.color(Token::Border), 2.0));
    painter.setBrush(tm.color(Token::SurfaceAlt));
    painter.drawEllipse(c, r, r);
    painter.setPen(tm.color(Token::TextMuted));
    painter.drawText(QRectF(c.x() - r, c.y() - 20, 2 * r, 40), Qt::AlignCenter, "No assigned tasks");
}

void PizzaWidget::drawSlice(QPainter& painter, int index) const {
    const Portion& p = portions_[index];
    auto& tm = ThemeManager::instance();

    const double span = p.thinSlice ? kThinSliceDegrees : p.spanAngle;
    const double bisector = p.startAngle + span / 2.0;
    const double offset = kRestOffset + kSpreadOffset * std::sin(kPi * animT_);
    const QPointF c = pieCenter() + (pointOnCircle(QPointF(0, 0), offset, bisector));
    const double r = pieRadius();
    const bool selected = !selectedMemberId_.isEmpty() && selectedMemberId_ == QString::fromStdString(p.memberId);

    // Qt angles are counter-clockwise from 3 o'clock in 1/16 degree.
    const int qtStart = static_cast<int>(std::round((90.0 - p.startAngle) * 16.0));
    const int qtSpan = static_cast<int>(std::round(-span * 16.0));

    // Crust, then the cheese surface inset from the rim.
    const QRectF outer(c.x() - r, c.y() - r, 2 * r, 2 * r);
    painter.setPen(QPen(tm.color(selected ? Token::TextPrimary : Token::PizzaEdge), selected ? 3.0 : 1.5));
    painter.setBrush(tm.color(Token::PizzaCrust));
    painter.drawPie(outer, qtStart, qtSpan);
    if (p.thinSlice) return;

    painter.setPen(Qt::NoPen);
    painter.setBrush(tm.color(Token::PizzaCheese));
    painter.drawPie(outer.adjusted(12, 12, -12, -12), qtStart, qtSpan);

    if (span < kMinBadgeSpan) return;

    // Toppings
    painter.setBrush(tm.color(Token::PizzaSauce));
    painter.drawEllipse(pointOnCircle(c, r * 0.62, p.startAngle + span * 0.35), 8, 8);
    painter.drawEllipse(pointOnCircle(c, r * 0.74, p.startAngle + span * 0.72), 6, 6);
    painter.setBrush(tm.color(Token::PizzaBasil));
    painter.drawEllipse(pointOnCircle(c, r * 0.80, p.startAngle + span * 0.52), 4, 4);

    // Initials badge in the member's colour (matches the legend dot)
    const QPointF badge = pointOnCircle(c, r * 0.35, bisector);
    painter.setPen(QPen(tm.color(Token::Surface), 2.0));
    painter.setBrush(memberColor(index));
    painter.drawEllipse(badge, 14, 14);
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(9);
    painter.setFont(font);
    painter.setPen(tm.color(Token::AccentText));
    painter.drawText(QRectF(badge.x() - 14, badge.y() - 14, 28, 28), Qt::AlignCenter, initialsOf(p.memberName));
}

void PizzaWidget::drawLegend(QPainter& painter) {
    auto& tm = ThemeManager::instance();
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(false);
    painter.setFont(font);
    const QFontMetrics fm(font);

    constexpr double pillHeight = 26.0;
    constexpr double spacing = 10.0;
    const double maxRowWidth = width() - 2 * kMargin;

    struct Pill { QString text; double width; };
    std::vector<Pill> pills;
    for (const Portion& p : portions_) {
        const QString text = QString("%1  %2 tasks").arg(firstNameOf(p.memberName)).arg(p.taskCount);
        pills.push_back({text, fm.horizontalAdvance(text) + 34.0});
    }

    // Pack pills into centred rows.
    legendRects_.assign(pills.size(), QRectF());
    double y = pieCenter().y() + pieRadius() + kRestOffset + kSpreadOffset + 22.0;
    size_t rowStart = 0;
    while (rowStart < pills.size()) {
        size_t rowEnd = rowStart;
        double rowWidth = 0.0;
        while (rowEnd < pills.size() &&
               (rowEnd == rowStart || rowWidth + spacing + pills[rowEnd].width <= maxRowWidth)) {
            rowWidth += (rowEnd == rowStart ? 0.0 : spacing) + pills[rowEnd].width;
            ++rowEnd;
        }
        double x = (width() - rowWidth) / 2.0;
        for (size_t i = rowStart; i < rowEnd; ++i) {
            legendRects_[i] = QRectF(x, y, pills[i].width, pillHeight);
            x += pills[i].width + spacing;
        }
        y += pillHeight + 8.0;
        rowStart = rowEnd;
    }

    for (size_t i = 0; i < pills.size(); ++i) {
        const QRectF r = legendRects_[i];
        const bool selected = !selectedMemberId_.isEmpty() &&
                              selectedMemberId_ == QString::fromStdString(portions_[i].memberId);
        painter.setPen(QPen(tm.color(selected ? Token::Accent : Token::Border), 1.0));
        painter.setBrush(tm.color(selected ? Token::AccentSoft : Token::Surface));
        painter.drawRoundedRect(r, pillHeight / 2, pillHeight / 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(memberColor(static_cast<int>(i)));
        painter.drawEllipse(QPointF(r.left() + 13, r.center().y()), 4, 4);
        painter.setPen(tm.color(Token::TextPrimary));
        painter.drawText(QRectF(r.left() + 24, r.top(), r.width() - 28, r.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, pills[i].text);
    }
}

} // namespace planova::slicer
