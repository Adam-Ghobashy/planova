#include <QtTest>

#include <algorithm>
#include <string>
#include <vector>

#include <planova/common/Dtos.h>
#include <planova/stats/LeaderboardCalculator.h>

using namespace planova::s6;

class tst_Leaderboard : public QObject {
    Q_OBJECT

private:
    static TaskDto task(const char* id, int complexity, int hours, TaskStatus status,
                        const char* assignee)
    {
        TaskDto t;
        t.id = id;
        t.title = id;
        t.complexity = complexity;
        t.estHours = hours;
        t.status = status;
        t.assignee = std::string(assignee);
        return t;
    }

    static MemberDto member(const char* id, const char* name)
    {
        MemberDto m;
        m.id = id;
        m.name = name;
        return m;
    }

private slots:
    void morePointsRanksHigher()
    {
        const auto rows = LeaderboardCalculator::compute(
            {task("a", 2, 2, TaskStatus::Done, "m1"), task("b", 4, 2, TaskStatus::Done, "m2")},
            {member("m1", "Ana"), member("m2", "Ben")});
        QCOMPARE(int(rows.size()), 2);
        QCOMPARE(rows[0].memberId, std::string("m2"));
        QCOMPARE(rows[0].rank, 1);
        QCOMPARE(rows[1].rank, 2);
    }

    void equalPointsBreakOnEfficiency()
    {
        // Both deliver 4 points; m1 needs 2h, m2 needs 4h.
        const auto rows = LeaderboardCalculator::compute(
            {task("a", 4, 4, TaskStatus::Done, "m2"), task("b", 4, 2, TaskStatus::Done, "m1")},
            {member("m1", "Zed"), member("m2", "Amy")});
        QCOMPARE(rows[0].memberId, std::string("m1"));
        QVERIFY(rows[0].efficiency > rows[1].efficiency);
    }

    void equalPointsAndEfficiencyBreakOnName()
    {
        const auto rows = LeaderboardCalculator::compute(
            {task("a", 3, 3, TaskStatus::Done, "m1"), task("b", 3, 3, TaskStatus::Done, "m2")},
            {member("m1", "Zoe"), member("m2", "Adam")});
        QCOMPARE(rows[0].memberId, std::string("m2")); // Adam before Zoe
    }

    void complexityOneCannotOutrankComplexityFive()
    {
        const auto rows = LeaderboardCalculator::compute(
            {task("a", 1, 1, TaskStatus::Done, "m1"), task("b", 5, 20, TaskStatus::Done, "m2")},
            {member("m1", "Quick"), member("m2", "Deep")});
        QCOMPARE(rows[0].memberId, std::string("m2")); // 5 points beats 1 despite lower pts/h
    }

    void nothingDoneMeansZeroEfficiency()
    {
        const auto rows = LeaderboardCalculator::compute(
            {task("a", 3, 4, TaskStatus::InProgress, "m1")}, {member("m1", "Ana")});
        QCOMPARE(rows[0].points, 0);
        QCOMPARE(rows[0].done, 0);
        QCOMPARE(rows[0].assigned, 1);
        QCOMPARE(rows[0].efficiency, 0.0);
    }

    void emptyProjectGivesNoRows()
    {
        QVERIFY(LeaderboardCalculator::compute({}, {}).empty());
    }

    void orderIsStableAcrossInputOrder()
    {
        std::vector<MemberDto> members = {member("m1", "Ana"), member("m2", "Ben"),
                                          member("m3", "Cy")};
        const std::vector<TaskDto> tasks = {task("a", 3, 3, TaskStatus::Done, "m1"),
                                            task("b", 3, 3, TaskStatus::Done, "m2"),
                                            task("c", 3, 3, TaskStatus::Done, "m3")};
        const auto first = LeaderboardCalculator::compute(tasks, members);
        std::reverse(members.begin(), members.end());
        const auto second = LeaderboardCalculator::compute(tasks, members);
        QCOMPARE(int(first.size()), int(second.size()));
        for (std::size_t i = 0; i < first.size(); ++i) {
            QCOMPARE(first[i].memberId, second[i].memberId);
        }
    }
};

QTEST_APPLESS_MAIN(tst_Leaderboard)
#include "tst_Leaderboard.moc"
