#include "slicer/WeightSlidersPanel.h"
#include "slicer/WeightRebalance.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

namespace planova::slicer {

WeightSlidersPanel::WeightSlidersPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("WeightSlidersPanel");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(14);

    auto* headerLayout = new QHBoxLayout();
    auto* cardTitle = new QLabel("Assignment Strategy Weights", this);
    cardTitle->setObjectName("cardTitle");
    headerLayout->addWidget(cardTitle);
    headerLayout->addStretch();

    sumLabel_ = new QLabel("Sum: 100%", this);
    sumLabel_->setObjectName("sumLabel");
    headerLayout->addWidget(sumLabel_);
    mainLayout->addLayout(headerLayout);

    auto* slidersRow = new QHBoxLayout();
    slidersRow->setSpacing(24);

    const char* names[3] = {"Skill Match (α)", "Workload Leveling (β)", "Priority Weight (γ)"};
    for (int i = 0; i < 3; ++i) {
        auto* itemLayout = new QVBoxLayout();
        itemLayout->setSpacing(6);

        auto* labelRow = new QHBoxLayout();
        auto* nameLabel = new QLabel(names[i], this);
        nameLabel->setObjectName("sliderNameLabel");
        labelRow->addWidget(nameLabel);
        labelRow->addStretch();
        valueLabels_[i] = new QLabel(this);
        valueLabels_[i]->setObjectName("sliderValueLabel");
        labelRow->addWidget(valueLabels_[i]);
        itemLayout->addLayout(labelRow);

        sliders_[i] = new QSlider(Qt::Horizontal, this);
        sliders_[i]->setRange(0, 100);
        sliders_[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        itemLayout->addWidget(sliders_[i]);

        connect(sliders_[i], &QSlider::valueChanged, this, [this, i](int v) {
            onSliderMoved(i, v);
        });
        connect(sliders_[i], &QSlider::sliderReleased, this, [this] {
            emit weightsCommitted(weights_);
        });

        slidersRow->addLayout(itemLayout, 1);
    }

    mainLayout->addLayout(slidersRow);
    syncWidgets();
}

void WeightSlidersPanel::onSliderMoved(int index, int value) {
    weights_ = rebalance(weights_, index, value);
    syncWidgets();
    emit weightsChanged(weights_);
    if (!sliders_[index]->isSliderDown()) emit weightsCommitted(weights_);
}

void WeightSlidersPanel::syncWidgets() {
    const int values[3] = {weights_.skill, weights_.workload, weights_.priority};
    for (int i = 0; i < 3; ++i) {
        QSignalBlocker blocker(sliders_[i]);
        sliders_[i]->setValue(values[i]);
        valueLabels_[i]->setText(QString::number(values[i]) + "%");
    }
    sumLabel_->setText(QString("Sum: %1%").arg(weights_.skill + weights_.workload + weights_.priority));
}

}
