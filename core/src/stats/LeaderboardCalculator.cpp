#include <planova/stats/LeaderboardCalculator.h>

#include <algorithm>
#include <string>

namespace planova::s6 {

namespace {

struct Entry {
    LeaderRow row;
    std::string name;
};

} // namespace

std::vector<LeaderRow> LeaderboardCalculator::compute(const std::vector<TaskDto>& tasks,
                                                      const std::vector<MemberDto>& members)
{
    std::vector<Entry> entries;
    entries.reserve(members.size());

    for (const MemberDto& m : members) {
        Entry e;
        e.row.memberId = m.id;
        e.name = m.name;

        int deliveredHours = 0;
        for (const TaskDto& t : tasks) {
            if (!t.assignee || *t.assignee != m.id) {
                continue;
            }
            ++e.row.assigned;
            if (t.status == TaskStatus::Done) {
                ++e.row.done;
                e.row.points += t.complexity;
                deliveredHours += t.estHours;
            }
        }

        if (deliveredHours > 0) {
            e.row.efficiency = static_cast<double>(e.row.points) / deliveredHours;
        }
        entries.push_back(std::move(e));
    }

    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.row.points != b.row.points) return a.row.points > b.row.points;
        if (a.row.efficiency != b.row.efficiency) return a.row.efficiency > b.row.efficiency;
        if (a.name != b.name) return a.name < b.name;
        return a.row.memberId < b.row.memberId;
    });

    std::vector<LeaderRow> out;
    out.reserve(entries.size());
    int rank = 1;
    for (Entry& e : entries) {
        e.row.rank = rank++;
        out.push_back(std::move(e.row));
    }
    return out;
}

}