#pragma once
#include "planova/slicer/SlicerTypes.h"
#include "slicer/SliceTimeline.h"
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>
#include <vector>

class QVariantAnimation;

namespace planova::slicer {

// A whole pizza that a cutter slices on request: one slice per member, angle = share of the
// total complexity. Starts whole; sliceAnimated() runs the cutter, then the slices stay apart.
class PizzaWidget : public QWidget {
    Q_OBJECT
public:
    explicit PizzaWidget(QWidget* parent = nullptr);
    void setPortions(const std::vector<planova::slicer::Portion>& portions);

    void sliceAnimated();                 // whole pizza -> cutter animation -> sliced
    void setSliced(bool sliced);          // jump straight to sliced / whole, no animation
    bool isSliced() const { return sliced_; }
    bool isAnimating() const { return animating_; }

    // Pie geometry (before any animation offset). Public so tests and hit-testing share it.
    QPointF pieCenter() const;
    double pieRadius() const;
    // Index into the portions of the slice under `pos`, or -1 (outside, empty slice, empty pizza).
    int sliceAt(const QPointF& pos) const;
    void setSelectedMember(const QString& memberId);  // empty string clears the selection

signals:
    // Emitted when the user clicks a slice or legend pill. An empty id means the selection was cleared.
    void memberSelected(const QString& memberId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    bool hasAssignedSlices() const;
    bool hasCuts() const;                    // two or more slices: there is something to cut
    std::vector<double> cutAngles() const;   // where the knife goes: the start of every slice, if there are 2+
    double spread() const;                   // 0 = together .. 1 = settled apart
    double cutProgress(int cut) const;       // 0 .. 1 for the cut with this index
    int legendIndexAt(const QPointF& pos) const;
    void toggleSelection(int portionIndex);

    void drawEmptyState(QPainter& painter) const;
    void drawWholePizza(QPainter& painter) const;
    void drawSlice(QPainter& painter, int index) const;
    void drawCutLines(QPainter& painter, bool onTopOfPizza) const;
    void drawCutter(QPainter& painter) const;
    double drawLegend(QPainter& painter);    // returns the bottom edge of the legend

    std::vector<planova::slicer::Portion> portions_;
    QString selectedMemberId_;
    std::vector<QRectF> legendRects_;  // filled while painting, used for clicks
    bool sliced_ = false;
    bool animating_ = false;
    SliceTimeline timeline_ = SliceTimeline(std::vector<double>());
    SliceFrame frame_;
    QVariantAnimation* anim_ = nullptr;
};

} // namespace planova::slicer
