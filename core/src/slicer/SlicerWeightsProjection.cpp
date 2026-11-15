#include "planova/slicer/SlicerWeightsProjection.h"
#include "planova/slicer/SlicerNames.h"

namespace planova::slicer {

void SlicerWeightsProjection::reset() {
    last_.reset();
}

void SlicerWeightsProjection::apply(const Event& event) {
    if (event.type != SlicerWeightsChanged) return;
    ScoringWeights w;
    w.skill = event.payload["skill"].toInt();
    w.workload = event.payload["workload"].toInt();
    w.priority = event.payload["priority"].toInt();
    last_ = w;
}

} // namespace planova::slicer
