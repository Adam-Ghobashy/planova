#pragma once
#include "planova/slicer/ScoringWeights.h"
#include <QWidget>

class QLabel;
class QSlider;

namespace planova::slicer {

class WeightSlidersPanel : public QWidget {
    Q_OBJECT
public:
    explicit WeightSlidersPanel(QWidget* parent = nullptr);
    ScoringWeights weights() const { return weights_; }
signals:
    // Fires continuously while a slider moves (use for live labels).
    void weightsChanged(const planova::slicer::ScoringWeights& weights);
    // Fires once when a change is final: slider released, or a keyboard/wheel step.
    void weightsCommitted(const planova::slicer::ScoringWeights& weights);
private:
    void onSliderMoved(int index, int value);
    void syncWidgets();
    ScoringWeights weights_;
    QSlider* sliders_[3] = {nullptr, nullptr, nullptr};
    QLabel* valueLabels_[3] = {nullptr, nullptr, nullptr};
    QLabel* sumLabel_ = nullptr;
};

} // namespace planova::slicer
