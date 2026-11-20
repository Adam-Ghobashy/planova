#include <planova/stats/StatsService.h>

namespace planova::s6 {

StatsService::StatsService(ITaskQuery& tasks, IMemberQuery& members)
    : tasks_(tasks)
    , members_(members)
{
}

Progress StatsService::summary() const
{
    return ProgressCalculator::compute(tasks_.all());
}

std::vector<LeaderRow> StatsService::leaderboard() const
{
    return LeaderboardCalculator::compute(tasks_.all(), members_.all());
}

}