#pragma once
#include "planova/slicer/SlicerTypes.h"
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>
#include <vector>

class QVariantAnimation;

namespace planova::slicer {

// Draws one pizza slice per member (angle = share of total complexity) and a clickable legend.
class PizzaWidget : public QWidget {
    Q_OBJECT
public:
    explicit PizzaWidget(QWidget* parent = nullptr);
    void setPortions(const std::vector<planova::slicer::Portion>& portions);
    void sliceAnimated();

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

private:
    bool hasAssignedSlices() const;
    void toggleSelection(int portionIndex);
    void drawEmptyState(QPainter& painter) const;
    void drawSlice(QPainter& painter, int index) const;
    void drawLegend(QPainter& painter);

    std::vector<planova::slicer::Portion> portions_;
    QString selectedMemberId_;
    std::vector<QRectF> legendRects_;  // filled while painting, used for clicks
    double animT_ = 1.0;               // 0..1 while the slice animation runs, 1 at rest
    QVariantAnimation* anim_ = nullptr;
};

} // namespace planova::slicer
