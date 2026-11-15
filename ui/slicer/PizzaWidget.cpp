#include "slicer/PizzaWidget.h"
#include "theme/ThemeManager.h"
#include <QEasingCurve>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace planova::slicer {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMargin = 20.0;
constexpr double kLegendReserve = 110.0;  // space kept under the pie for the legend and hint
constexpr double kRestOffset = 6.0;       // gap between slices once sliced
constexpr double kSpreadOffset = 10.0;    // extra gap at the peak of the spreading
constexpr double kThinSliceDegrees = 3.0;
constexpr double kMinBadgeSpan = 25.0;    // slices narrower than this get no initials badge
constexpr double kCutterRim = 1.1;        // the cutter's "rim" position, as a multiple of the radius

// Toppings scattered over the whole pizza; each belongs to whichever slice it lands in.
struct Topping { double angleDeg; double distance; double size; bool basil; };
constexpr Topping kToppings[] = {
    {10, 0.62, 8, false},  {35, 0.82, 4, true},   {52, 0.42, 6, false},  {78, 0.70, 8, false},
    {100, 0.86, 4, true},  {118, 0.52, 7, false}, {140, 0.78, 6, false}, {160, 0.36, 4, true},
    {178, 0.66, 8, false}, {200, 0.84, 5, true},  {222, 0.56, 7, false}, {245, 0.76, 8, false},
    {268, 0.44, 4, true},  {290, 0.70, 6, false}, {312, 0.84, 4, true},  {330, 0.50, 8, false},
    {348, 0.76, 7, false}, {22, 0.28, 5, false},
};

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

// The pizza disc (crust + cheese) and every topping, centred on `c`. The caller clips as needed.
void paintPizzaDisc(QPainter& painter, const QPointF& c, double r) {
    auto& tm = ThemeManager::instance();
    painter.setPen(QPen(tm.color(Token::PizzaEdge), 1.5));
    painter.setBrush(tm.color(Token::PizzaCrust));
    painter.drawEllipse(c, r, r);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tm.color(Token::PizzaCheese));
    painter.drawEllipse(c, r - 12, r - 12);
    const double scale = r / 180.0;
    for (const Topping& t : kToppings) {
        painter.setBrush(tm.color(t.basil ? Token::PizzaBasil : Token::PizzaSauce));
        painter.drawEllipse(pointOnCircle(c, r * t.distance, t.angleDeg), t.size * scale, t.size * scale);
    }
}

} // namespace

PizzaWidget::PizzaWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(280, 420);
    setMouseTracking(true);  // pointer cursor over clickable slices and legend pills

    anim_ = new QVariantAnimation(this);
    anim_->setEasingCurve(QEasingCurve::Linear);
    connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        frame_ = timeline_.at(v.toInt());
        update();
    });
    connect(anim_, &QVariantAnimation::finished, this, [this] {
        animating_ = false;
        sliced_ = true;
        update();
    });
}

void PizzaWidget::setPortions(const std::vector<planova::slicer::Portion>& portions) {
    portions_ = portions;
    if (animating_) setSliced(true);  // the knife positions changed under it: skip to the end
    update();
}

void PizzaWidget::setSliced(bool sliced) {
    anim_->stop();
    animating_ = false;
    sliced_ = sliced;
    update();
}

void PizzaWidget::sliceAnimated() {
    anim_->stop();
    const std::vector<double> angles = cutAngles();
    if (angles.empty()) {  // zero or one slice: nothing to cut
        animating_ = false;
        sliced_ = true;
        update();
        return;
    }
    timeline_ = SliceTimeline(angles);
    frame_ = timeline_.at(0);
    sliced_ = false;
    animating_ = true;
    anim_->setDuration(timeline_.totalMs());
    anim_->setStartValue(0);
    anim_->setEndValue(timeline_.totalMs());
    anim_->start();
    update();
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

std::vector<double> PizzaWidget::cutAngles() const {
    std::vector<double> angles;
    for (const Portion& p : portions_)
        if (!p.thinSlice) angles.push_back(p.startAngle);
    if (angles.size() < 2) angles.clear();  // a single slice is the whole pizza: no knife work
    return angles;
}

double PizzaWidget::spread() const {
    if (cutAngles().empty()) return 0.0;  // nothing to pull apart
    if (animating_) return frame_.spread;
    return sliced_ ? 1.0 : 0.0;
}

double PizzaWidget::cutProgress(int cut) const {
    if (animating_) return cut < static_cast<int>(frame_.cutProgress.size()) ? frame_.cutProgress[cut] : 0.0;
    return sliced_ ? 1.0 : 0.0;
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

int PizzaWidget::legendIndexAt(const QPointF& pos) const {
    for (size_t i = 0; i < legendRects_.size() && i < portions_.size(); ++i)
        if (legendRects_[i].contains(pos)) return static_cast<int>(i);
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
    const int legend = legendIndexAt(pos);
    if (legend >= 0) {
        toggleSelection(legend);
        return;
    }
    // Slices only exist once the pizza has been cut.
    const int slice = (sliced_ && !animating_) ? sliceAt(pos) : -1;
    if (slice >= 0) {
        toggleSelection(slice);
    } else if (!selectedMemberId_.isEmpty()) {
        selectedMemberId_.clear();
        update();
        emit memberSelected(QString());
    }
}

void PizzaWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    const bool overSlice = sliced_ && !animating_ && sliceAt(pos) >= 0;
    setCursor(legendIndexAt(pos) >= 0 || overSlice ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
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
    } else if (spread() <= 0.0) {
        drawWholePizza(painter);
        drawCutLines(painter, true);  // grooves drawn on the cheese as the knife goes
    } else {
        drawCutLines(painter, false);  // grooves show in the gaps between the slices
        for (size_t i = 0; i < portions_.size(); ++i) drawSlice(painter, static_cast<int>(i));
    }
    if (animating_ && frame_.wheelVisible) drawCutter(painter);

    const double legendBottom = drawLegend(painter);
    if (!sliced_ && !animating_ && hasAssignedSlices() && !cutAngles().empty()) {
        painter.setPen(tm.color(Token::TextMuted));
        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);
        painter.drawText(QRectF(0, legendBottom + 2, width(), 20), Qt::AlignCenter,
                         "Press \"Slice the Project\" to divide the work");
    }
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

void PizzaWidget::drawWholePizza(QPainter& painter) const {
    paintPizzaDisc(painter, pieCenter(), pieRadius());
}

void PizzaWidget::drawCutLines(QPainter& painter, bool onTopOfPizza) const {
    auto& tm = ThemeManager::instance();
    const std::vector<double> angles = cutAngles();
    const double r = pieRadius();
    painter.setPen(QPen(tm.color(onTopOfPizza ? Token::PizzaEdge : Token::Border), onTopOfPizza ? 2.5 : 3.0,
                        Qt::SolidLine, Qt::RoundCap));
    for (size_t i = 0; i < angles.size(); ++i) {
        const double progress = cutProgress(static_cast<int>(i));
        if (progress <= 0.0) continue;
        // The knife runs from the rim towards the centre.
        painter.drawLine(pointOnCircle(pieCenter(), r, angles[i]), pointOnCircle(pieCenter(), r * (1.0 - progress), angles[i]));
    }
}

void PizzaWidget::drawSlice(QPainter& painter, int index) const {
    const Portion& p = portions_[index];
    auto& tm = ThemeManager::instance();

    const double span = p.thinSlice ? kThinSliceDegrees : p.spanAngle;
    const double bisector = p.startAngle + span / 2.0;
    const double s = spread();
    const double offset = kRestOffset * s + kSpreadOffset * std::sin(kPi * s);
    const QPointF c = pieCenter() + pointOnCircle(QPointF(0, 0), offset, bisector);
    const double r = pieRadius();
    const bool selected = !selectedMemberId_.isEmpty() && selectedMemberId_ == QString::fromStdString(p.memberId);

    // The wedge: Qt angles run counter-clockwise from 3 o'clock.
    QPainterPath wedge;
    wedge.moveTo(c);
    wedge.arcTo(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), 90.0 - p.startAngle, -span);
    wedge.closeSubpath();

    painter.save();
    painter.setClipPath(wedge);
    paintPizzaDisc(painter, c, r);
    painter.restore();

    painter.setPen(QPen(tm.color(selected ? Token::TextPrimary : Token::PizzaEdge), selected ? 3.0 : 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(wedge);

    if (p.thinSlice || span < kMinBadgeSpan) return;

    // Initials badge in the member's colour (matches the legend dot); fades in as the slices spread.
    painter.save();
    painter.setOpacity(s);
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
    painter.restore();
}

void PizzaWidget::drawCutter(QPainter& painter) const {
    auto& tm = ThemeManager::instance();
    const double r = pieRadius();
    const QPointF pos = pointOnCircle(pieCenter(), r * kCutterRim * frame_.wheelDistance, frame_.wheelAngleDeg);
    const QPointF outward = pointOnCircle(QPointF(0, 0), 1.0, frame_.wheelAngleDeg);

    painter.save();
    painter.setOpacity(frame_.wheelLifted ? 0.75 : 1.0);

    // Handle, pointing away from the pizza
    painter.setPen(QPen(tm.color(Token::PizzaEdge), 6.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(pos, pos + outward * 38.0);

    // Wheel: it spins as it travels
    painter.translate(pos);
    painter.rotate(frame_.wheelDistance * 720.0 + frame_.wheelAngleDeg);
    painter.setPen(QPen(tm.color(Token::TextMuted), 2.0));
    painter.setBrush(tm.color(Token::Surface));
    painter.drawEllipse(QPointF(0, 0), 14, 14);
    painter.setPen(QPen(tm.color(Token::Border), 1.5));
    for (int spoke = 0; spoke < 6; ++spoke) {
        painter.drawLine(QPointF(0, 0), pointOnCircle(QPointF(0, 0), 12.0, spoke * 60.0));
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(tm.color(Token::TextMuted));
    painter.drawEllipse(QPointF(0, 0), 3, 3);
    painter.restore();
}

double PizzaWidget::drawLegend(QPainter& painter) {
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
    return y;
}

} // namespace planova::slicer
