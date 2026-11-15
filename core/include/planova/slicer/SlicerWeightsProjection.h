#pragma once
#include "planova/common/Event.h"
#include "planova/common/IProjection.h"
#include "planova/slicer/ScoringWeights.h"
#include <optional>

namespace planova::slicer {

// Remembers the weights from the most recent SlicerWeightsChanged event,
// so the service knows what was last applied even after a restart.
class SlicerWeightsProjection : public IProjection {
public:
    void reset() override;
    void apply(const Event& event) override;
    std::optional<ScoringWeights> last() const { return last_; }

private:
    std::optional<ScoringWeights> last_;
};

} // namespace planova::slicer
