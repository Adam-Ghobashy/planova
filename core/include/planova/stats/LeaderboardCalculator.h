#pragma once

#include <planova/common/Dtos.h>

#include <vector>

namespace planova::s6 {

struct LeaderRow {
    Id memberId;
    int done = 0;
    int assigned = 0;
    int points = 0;
    double efficiency = 0.0;
    int rank = 0;
};

class LeaderboardCalculator {
public:

    static std::vector<LeaderRow> compute(const std::vector<TaskDto>& tasks,
                                          const std::vector<MemberDto>& members);
};

}