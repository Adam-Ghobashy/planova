#pragma once
#include "planova/common/Dtos.h"
#include "planova/slicer/SlicerTypes.h"
#include <vector>

namespace planova::slicer {

class PizzaPortionCalculator {
public:
    static std::vector<Portion> calculate(const std::vector<TaskDto>& tasks,
                                          const std::vector<MemberDto>& members);
};

}
