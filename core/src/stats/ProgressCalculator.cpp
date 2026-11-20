#include <planova/stats/ProgressCalculator.h>

namespace planova::s6 {

Progress ProgressCalculator::compute(const std::vector<TaskDto>& tasks)
{
    Progress p;
    p.tasksTotal = static_cast<int>(tasks.size());

    for (const TaskDto& t : tasks) {
        p.pointsTotal += t.complexity;
        if (t.status == TaskStatus::Done) {
            ++p.tasksDone;
            p.pointsDelivered += t.complexity;
        }
    }
    return p;
}

}