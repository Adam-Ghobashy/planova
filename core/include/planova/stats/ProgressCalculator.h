#pragma once

#include <planova/common/Dtos.h>

#include <vector>

namespace planova::s6 {

struct Progress {
    int tasksDone = 0;
    int tasksTotal = 0;
    int pointsDelivered = 0;
    int pointsTotal = 0;
};

class ProgressCalculator {
public:
    static Progress compute(const std::vector<TaskDto>& tasks);
};

}