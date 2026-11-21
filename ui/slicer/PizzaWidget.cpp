#include "slicer/PizzaWidget.h"
#include "slicer/PizzaText.h"
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
constexpr double kLegendGap = 22.0;
constexpr double kHintHeight = 26.0;
constexpr double kMaxRadius = 160.0;
constexpr double kMinRadius = 40.0;
constexpr double kPillHeight = 26.0;
constexpr double kPillSpacing = 10.0;
constexpr double kRestOffset = 6.0;
constexpr double kSpreadOffset = 10.0;
constexpr double kThinSliceDegrees = 3.0;
constexpr double kMinBadgeSpan = 25.0;
constexpr double kCutterRim = 1.1;

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

QColor memberColor(int index) {
    return ThemeManager::instance().color(static_cast<Token>(static_cast<int>(Token::Member1) + (index % 6)));
}

QPointF pointOnCircle(const QPointF& center, double distance, double angleDeg) {
    const double rad = qDegreesToRadians(angleDeg);
    return QPointF(center.x() + distance * std::sin(rad), center.y() - distance * std::cos(rad));
}

bool samePortions(const std::vector<Portion>& a, const std::vector<Portion>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].memberId != b[i].memberId || a[i].taskCount != b[i].taskCount ||
            a[i].thinSlice != b[i].thinSlice || std::abs(a[i].startAngle - b[i].startAngle) > 1e-6 ||
            std::abs(a[i].spanAngle - b[i].spanAngle) > 1e-6) {
            return false;
        }
    }
    return true;
}

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

}

PizzaWidget::PizzaWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });

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
    if (!samePortions(portions_, portions)) setSliced(false);
    portions_ = portions;
    if (!selectedMemberId_.isEmpty() && !hasMember(selectedMemberId_)) {
        selectedMemberId_.clear();
        emit memberSelected(QString());
    }
    updateGeometry();
    update();
}

bool PizzaWidget::hasMember(const QString& memberId) const {
    return std::any_of(portions_.begin(), portions_.end(),
                       [&](const Portion& p) { return QString::fromStdString(p.memberId) == memberId; });
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
    if (angles.empty()) {
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

double PizzaWidget::legendHeight() const {
    return layoutLegend(0.0).bottom;
}

double PizzaWidget::legendTop() const {
    return pieCenter().y() + pieRadius() + kRestOffset + kSpreadOffset + kLegendGap;
}

double PizzaWidget::legendBottom() const {
    return legendTop() + legendHeight();
}

// Space needed below the pie: slice offsets, the gap to the legend, the legend and the hint line.
double PizzaWidget::belowPieHeight() const {
    return kRestOffset + kSpreadOffset + kLegendGap + legendHeight() + kHintHeight;
}

// The radius the content wants: at most 160 px, smaller only when the widget is narrow.
double PizzaWidget::idealRadius() const {
    return (std::max)(kMinRadius, (std::min)(kMaxRadius, (width() - 2 * kMargin) / 2.0));
}

double PizzaWidget::pieRadius() const {
    const double byHeight = (height() - belowPieHeight() - kMargin) / 2.0;
    return (std::max)(kMinRadius, (std::min)(idealRadius(), byHeight));
}

// The height follows the content (pie plus legend plus hint), so there is no dead space under the legend.
QSize PizzaWidget::sizeHint() const {
    const double height = kMargin + 2 * idealRadius() + belowPieHeight();
    return QSize(2 * static_cast<int>(kMargin + kMaxRadius), static_cast<int>(std::ceil(height)));
}

QSize PizzaWidget::minimumSizeHint() const {
    return QSize(280, sizeHint().height());
}

void PizzaWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateGeometry();  // the legend wraps differently at a new width, so the wanted height changes
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
    if (angles.size() < 2) angles.clear();
    return angles;
}

bool PizzaWidget::hasCuts() const {
    return std::count_if(portions_.begin(), portions_.end(), [](const Portion& p) { return !p.thinSlice; }) >= 2;
}

double PizzaWidget::spread() const {
    if (!hasCuts()) return 0.0;
    if (animating_) return frame_.spread;
    return sliced_ ? 1.0 : 0.0;
}

double PizzaWidget::cutProgress(int cut) const {
    if (animating_) return cut < static_cast<int>(frame_.cutProgress.size()) ? frame_.cutProgress[cut] : 0.0;
    return sliced_ ? 1.0 : 0.0;
}

int PizzaWidget::sliceAt(const QPointF& pos) const {
    if (!hasAssignedSlices()) return -1;
    const double r = pieRadius();
    const double gap = kRestOffset * spread();

    for (size_t i = 0; i < portions_.size(); ++i) {
        const Portion& p = portions_[i];
        if (p.thinSlice) continue;
        const QPointF c = pieCenter() + pointOnCircle(QPointF(0, 0), gap, p.startAngle + p.spanAngle / 2.0);
        const double dx = pos.x() - c.x();
        const double dy = pos.y() - c.y();
        if (std::hypot(dx, dy) > r) continue;
        double angle = qRadiansToDegrees(std::atan2(dx, -dy));
        if (angle < 0.0) angle += 360.0;
        if (angle >= p.startAngle && angle < p.startAngle + p.spanAngle) return static_cast<int>(i);
    }

    if (gap == 0.0 && std::hypot(pos.x() - pieCenter().x(), pos.y() - pieCenter().y()) <= r) {
        for (size_t i = portions_.size(); i-- > 0;) {
            if (!portions_[i].thinSlice) return static_cast<int>(i);
        }
    }
    return -1;
}

int PizzaWidget::legendIndexAt(const QPointF& pos) const {
    const std::vector<QRectF> rects = layoutLegend(legendTop()).rects;
    for (size_t i = 0; i < rects.size(); ++i)
        if (rects[i].contains(pos)) return static_cast<int>(i);
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

void PizzaWidget::paintEvent(QPaintEvent* ) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    auto& tm = ThemeManager::instance();

    if (portions_.empty()) {
        painter.setPen(tm.color(Token::TextMuted));
        painter.drawText(rect(), Qt::AlignCenter, "No workload data");

        return;
    }

    if (!hasAssignedSlices()) {
        drawEmptyState(painter);
    } else if (spread() <= 0.0) {
        drawWholePizza(painter);
        drawCutLines(painter, true);
    } else {
        drawCutLines(painter, false);
        for (size_t i = 0; i < portions_.size(); ++i) drawSlice(painter, static_cast<int>(i));
    }
    if (animating_ && frame_.wheelVisible) drawCutter(painter);

    const double legendEnd = drawLegend(painter);
    if (!sliced_ && !animating_ && hasCuts()) {
        painter.setPen(tm.color(Token::TextMuted));
        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);
        painter.drawText(QRectF(0, legendEnd + 2, width(), 20), Qt::AlignCenter,
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

    painter.setPen(QPen(tm.color(Token::PizzaEdge), 6.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(pos, pos + outward * 38.0);

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

PizzaWidget::LegendLayout PizzaWidget::layoutLegend(double top) const {
    QFont font = this->font();
    font.setPointSize(9);
    font.setBold(false);
    const QFontMetrics fm(font);
    const double maxRowWidth = width() - 2 * kMargin;

    std::vector<double> widths;
    for (const Portion& p : portions_) widths.push_back(fm.horizontalAdvance(legendText(p.memberName, p.taskCount)) + 34.0);

    LegendLayout layout;
    layout.rects.assign(portions_.size(), QRectF());
    double y = top;
    size_t rowStart = 0;
    while (rowStart < widths.size()) {
        size_t rowEnd = rowStart;
        double rowWidth = 0.0;
        while (rowEnd < widths.size() &&
               (rowEnd == rowStart || rowWidth + kPillSpacing + widths[rowEnd] <= maxRowWidth)) {
            rowWidth += (rowEnd == rowStart ? 0.0 : kPillSpacing) + widths[rowEnd];
            ++rowEnd;
        }
        double x = (width() - rowWidth) / 2.0;
        for (size_t i = rowStart; i < rowEnd; ++i) {
            layout.rects[i] = QRectF(x, y, widths[i], kPillHeight);
            x += widths[i] + kPillSpacing;
        }
        y += kPillHeight + 8.0;
        rowStart = rowEnd;
    }
    layout.bottom = y;
    return layout;
}

double PizzaWidget::drawLegend(QPainter& painter) const {
    auto& tm = ThemeManager::instance();
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(false);
    painter.setFont(font);

    const LegendLayout layout = layoutLegend(legendTop());
    for (size_t i = 0; i < layout.rects.size(); ++i) {
        const QRectF r = layout.rects[i];
        const bool selected = !selectedMemberId_.isEmpty() &&
                              selectedMemberId_ == QString::fromStdString(portions_[i].memberId);
        painter.setPen(QPen(tm.color(selected ? Token::Accent : Token::Border), 1.0));
        painter.setBrush(tm.color(selected ? Token::AccentSoft : Token::Surface));
        painter.drawRoundedRect(r, kPillHeight / 2, kPillHeight / 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(memberColor(static_cast<int>(i)));
        painter.drawEllipse(QPointF(r.left() + 13, r.center().y()), 4, 4);
        painter.setPen(tm.color(Token::TextPrimary));
        painter.drawText(QRectF(r.left() + 24, r.top(), r.width() - 28, r.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, legendText(portions_[i].memberName, portions_[i].taskCount));
    }
    return layout.bottom;
}

}
