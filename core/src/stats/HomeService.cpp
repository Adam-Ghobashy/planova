#include <planova/stats/HomeService.h>

namespace planova::s6 {

HomeService::HomeService(ITaskQuery& tasks, IMemberQuery& members, IPinQuery& pins)
    : tasks_(tasks)
    , members_(members)
    , pins_(pins)
{
}

DashboardData HomeService::dashboard() const
{
    DashboardData d;

    const std::vector<TaskDto> tasks = tasks_.all();
    d.total = static_cast<int>(tasks.size());

    for (const TaskDto& t : tasks) {
        switch (t.status) {
        case TaskStatus::Done:
            ++d.completed;
            break;
        case TaskStatus::InProgress:
        case TaskStatus::Review:
            ++d.inProgress;
            break;
        case TaskStatus::ToDo:
            ++d.remaining;
            break;
        }
    }

    for (const MemberDto& m : members_.all()) {
        int hours = 0;
        for (const TaskDto& t : tasks) {
            if (t.status == TaskStatus::Done && t.assignee && *t.assignee == m.id) {
                hours += t.estHours;
            }
        }
        d.contributionHours.emplace_back(m.id, hours);
    }

    d.pins = pins_.latest(kMaxPins);
    if (static_cast<int>(d.pins.size()) > kMaxPins) {
        d.pins.resize(static_cast<std::size_t>(kMaxPins)); // query may ignore n
    }

    return d;
}

}