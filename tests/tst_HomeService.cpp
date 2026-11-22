#include <QtTest>

#include <optional>
#include <string>
#include <vector>

#include <planova/common/Queries.h>
#include <planova/stats/HomeService.h>
#include <planova/stats/StatsService.h>

using namespace planova::s6;

namespace {

TaskDto makeTask(const std::string& id, int complexity, int hours, TaskStatus status,
                 const char* assignee = nullptr)
{
    TaskDto t;
    t.id = id;
    t.title = id;
    t.complexity = complexity;
    t.estHours = hours;
    t.status = status;
    if (assignee != nullptr) {
        t.assignee = std::string(assignee);
    }
    return t;
}

MemberDto makeMember(const std::string& id, const std::string& name)
{
    MemberDto m;
    m.id = id;
    m.name = name;
    return m;
}

PinDto makePin(const std::string& id)
{
    PinDto p;
    p.id = id;
    p.text = id;
    return p;
}

struct FakeTasks : ITaskQuery {
    std::vector<TaskDto> tasks;
    std::vector<TaskDto> all() const override { return tasks; }
    std::optional<TaskDto> byId(const Id& id) const override
    {
        for (const TaskDto& t : tasks) {
            if (t.id == id) {
                return t;
            }
        }
        return std::nullopt;
    }
};

struct FakeMembers : IMemberQuery {
    std::vector<MemberDto> members;
    std::vector<MemberDto> all() const override { return members; }
    std::optional<MemberDto> byId(const Id& id) const override
    {
        for (const MemberDto& m : members) {
            if (m.id == id) {
                return m;
            }
        }
        return std::nullopt;
    }
};

struct FakePins : IPinQuery {
    std::vector<PinDto> pins;
    bool ignoreLimit = false; // simulates a query that returns more than n
    int openCount() const override { return static_cast<int>(pins.size()); }
    int pinCountForTask(const Id&) const override { return 0; }
    std::vector<PinDto> latest(int n) const override
    {
        std::vector<PinDto> out;
        for (const PinDto& p : pins) {
            if (!ignoreLimit && static_cast<int>(out.size()) >= n) {
                break;
            }
            out.push_back(p);
        }
        return out;
    }
};

} // namespace

class tst_HomeService : public QObject {
    Q_OBJECT

private slots:
    void statusCounts()
    {
        FakeTasks tasks;
        FakeMembers members;
        FakePins pins;
        tasks.tasks = {
            makeTask("a", 1, 1, TaskStatus::ToDo),       makeTask("b", 1, 1, TaskStatus::ToDo),
            makeTask("c", 1, 1, TaskStatus::InProgress), makeTask("d", 1, 1, TaskStatus::Review),
            makeTask("e", 1, 1, TaskStatus::Done),       makeTask("f", 1, 1, TaskStatus::Done),
            makeTask("g", 1, 1, TaskStatus::Done),       makeTask("h", 1, 1, TaskStatus::ToDo),
        };
        const DashboardData d = HomeService(tasks, members, pins).dashboard();
        QCOMPARE(d.total, 8);
        QCOMPARE(d.completed, 3);
        QCOMPARE(d.inProgress, 1); // InProgress only, like the Board column
        QCOMPARE(d.review, 1);
        QCOMPARE(d.remaining, 3);  // ToDo only
    }

    void inProgressCountMatchesBoard()
    {
        FakeTasks tasks;
        FakeMembers members;
        FakePins pins;
        tasks.tasks = {
            makeTask("a", 1, 1, TaskStatus::InProgress), makeTask("b", 1, 1, TaskStatus::InProgress),
            makeTask("c", 1, 1, TaskStatus::Review),     makeTask("d", 1, 1, TaskStatus::Review),
        };
        const DashboardData d = HomeService(tasks, members, pins).dashboard();
        QCOMPARE(d.inProgress, 2);
        QCOMPARE(d.review, 2); // Review is counted on its own
    }

    void contributionHoursSumDoneTasksPerMember()
    {
        FakeTasks tasks;
        FakeMembers members;
        FakePins pins;
        members.members = {makeMember("m1", "Ana"), makeMember("m2", "Ben")};
        tasks.tasks = {
            makeTask("a", 2, 4, TaskStatus::Done, "m1"),
            makeTask("b", 2, 6, TaskStatus::Done, "m1"),
            makeTask("c", 2, 9, TaskStatus::InProgress, "m1"), // not counted
            makeTask("d", 2, 3, TaskStatus::Done, "m2"),
            makeTask("e", 2, 5, TaskStatus::Done),             // unassigned
        };
        const DashboardData d = HomeService(tasks, members, pins).dashboard();
        QCOMPARE(int(d.contributionHours.size()), 2);
        QCOMPARE(d.contributionHours[0].first, std::string("m1"));
        QCOMPARE(d.contributionHours[0].second, 10);
        QCOMPARE(d.contributionHours[1].first, std::string("m2"));
        QCOMPARE(d.contributionHours[1].second, 3);
    }

    void pinsPreviewIsCappedAtFour()
    {
        FakeTasks tasks;
        FakeMembers members;
        FakePins pins;
        for (int i = 0; i < 6; ++i) {
            pins.pins.push_back(makePin("p" + std::to_string(i)));
        }
        const DashboardData d = HomeService(tasks, members, pins).dashboard();
        QCOMPARE(int(d.pins.size()), 4);
        QCOMPARE(d.pins[0].id, std::string("p0"));
    }

    void queryReturningTooManyPinsIsStillCapped()
    {
        FakeTasks tasks;
        FakeMembers members;
        FakePins pins;
        pins.ignoreLimit = true;
        for (int i = 0; i < 9; ++i) {
            pins.pins.push_back(makePin("p" + std::to_string(i)));
        }
        QCOMPARE(int(HomeService(tasks, members, pins).dashboard().pins.size()), 4);
    }

    void homeAndTrackingAgree()
    {
        FakeTasks tasks;
        FakeMembers members;
        FakePins pins;
        members.members = {makeMember("m1", "Ana")};
        tasks.tasks = {
            makeTask("a", 3, 2, TaskStatus::Done, "m1"),
            makeTask("b", 2, 2, TaskStatus::Review, "m1"),
            makeTask("c", 1, 2, TaskStatus::ToDo),
        };
        const DashboardData d = HomeService(tasks, members, pins).dashboard();
        StatsService stats(tasks, members);
        const Progress p = stats.summary();
        QCOMPARE(d.total, p.tasksTotal);
        QCOMPARE(d.completed, p.tasksDone);
        QCOMPARE(stats.leaderboard()[0].done, d.completed);
    }

    void emptyProjectIsSafe()
    {
        FakeTasks tasks;
        FakeMembers members;
        FakePins pins;
        const DashboardData d = HomeService(tasks, members, pins).dashboard();
        QCOMPARE(d.total, 0);
        QCOMPARE(d.completed, 0);
        QCOMPARE(d.inProgress, 0);
        QCOMPARE(d.remaining, 0);
        QVERIFY(d.contributionHours.empty());
        QVERIFY(d.pins.empty());
    }

    void membersWithNothingDoneHaveZeroHours()
    {
        FakeTasks tasks;
        FakeMembers members;
        FakePins pins;
        members.members = {makeMember("m1", "Ana"), makeMember("m2", "Ben")};
        tasks.tasks = {makeTask("a", 3, 4, TaskStatus::ToDo, "m1")};
        const DashboardData d = HomeService(tasks, members, pins).dashboard();
        QCOMPARE(d.contributionHours[0].second, 0);
        QCOMPARE(d.contributionHours[1].second, 0);
    }
};

QTEST_APPLESS_MAIN(tst_HomeService)
#include "tst_HomeService.moc"