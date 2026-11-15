#pragma once
#include "planova/slicer/ScoringWeights.h"

namespace planova::slicer {

ScoringWeights rebalance(ScoringWeights current, int changedIndex, int newValue);

}
