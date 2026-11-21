#pragma once
#include "planova/common/Clock.h"
#include "planova/common/Queries.h"
#include "planova/slicer/IAssignmentStrategy.h"
#include "planova/slicer/ScoringWeights.h"
#include "planova/slicer/SlicerTypes.h"
#include "planova/slicer/SlicerWeightsProjection.h"
#include "planova/store/EventLog.h"
#include <vector>

namespace planova::slicer {

class SlicerService {
public:
    static constexpr const char* SLICER_AUTHOR = "chaos-slicer";

    SlicerService(ITaskQuery& tasks, IMemberQuery& members, IAssignmentStrategy& strategy,
                  EventLog& log, IClock& clock);
    SlicerService(const SlicerService&) = delete;
    SlicerService& operator=(const SlicerService&) = delete;

    std::vector<Assignment> preview(const ScoringWeights& weights) const;
    int apply(const std::vector<Assignment>& assignments, const ScoringWeights& weights);
    std::vector<Portion> portions() const;
    std::vector<Portion> portions(const std::vector<Assignment>& preview) const;

    // The weights last written to the log, or the 50/30/20 default if none were saved yet.
    ScoringWeights savedWeights() const;
    // Throws std::invalid_argument for bad weights. Writes nothing when they equal the saved ones.
    void saveWeights(const ScoringWeights& weights);

private:
    void appendWeightsEvent(const ScoringWeights& weights, int64_t now);
    ITaskQuery& tasks_;
    IMemberQuery& members_;
    IAssignmentStrategy& strategy_;
    EventLog& log_;
    IClock& clock_;
    SlicerWeightsProjection weights_;
};

}
