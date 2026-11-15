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

class PizzaWidget : public QWidget {
    Q_OBJECT
public:
    explicit PizzaWidget(QWidget* parent = nullptr);
    void setPortions(const std::vector<planova::slicer::Portion>& portions);

    void sliceAnimated();
    void setSliced(bool sliced);
    bool isSliced() const { return sliced_; }
    bool isAnimating() const { return animating_; }

    QPointF pieCenter() const;
    double pieRadius() const;
    int sliceAt(const QPointF& pos) const;
    double legendBottom() const;

signals:
    void memberSelected(const QString& memberId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct LegendLayout {
        std::vector<QRectF> rects;
        double bottom = 0.0;
    };
    LegendLayout layoutLegend(double top) const;
    double legendTop() const;
    double legendHeight() const;
    void updateMinimumHeight();
    bool hasAssignedSlices() const;
    bool hasCuts() const;
    std::vector<double> cutAngles() const;
    double spread() const;
    double cutProgress(int cut) const;
    int legendIndexAt(const QPointF& pos) const;
    void toggleSelection(int portionIndex);
    bool hasMember(const QString& memberId) const;

    void drawEmptyState(QPainter& painter) const;
    void drawWholePizza(QPainter& painter) const;
    void drawSlice(QPainter& painter, int index) const;
    void drawCutLines(QPainter& painter, bool onTopOfPizza) const;
    void drawCutter(QPainter& painter) const;
    double drawLegend(QPainter& painter) const;

    std::vector<planova::slicer::Portion> portions_;
    QString selectedMemberId_;
    bool sliced_ = false;
    bool animating_ = false;
    SliceTimeline timeline_ = SliceTimeline(std::vector<double>());
    SliceFrame frame_;
    QVariantAnimation* anim_ = nullptr;
};

}
