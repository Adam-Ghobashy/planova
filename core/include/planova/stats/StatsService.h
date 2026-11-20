#pragma once

#include <planova/common/Queries.h>
#include <planova/stats/LeaderboardCalculator.h>
#include <planova/stats/ProgressCalculator.h>

#include <vector>

namespace planova::s6 {

class StatsService {
public:
    StatsService(ITaskQuery& tasks, IMemberQuery& members);

    Progress summary() const;
    std::vector<LeaderRow> leaderboard() const;

private:
    ITaskQuery& tasks_;
    IMemberQuery& members_;
};

}