#pragma once
#include "planova/common/Event.h"
#include "planova/common/IProjection.h"
#include "planova/slicer/ScoringWeights.h"
#include <optional>

namespace planova::slicer {

class SlicerWeightsProjection : public IProjection {
public:
    void reset() override;
    void apply(const Event& event) override;
    std::optional<ScoringWeights> last() const { return last_; }

private:
    std::optional<ScoringWeights> last_ = std::nullopt;
};

}
