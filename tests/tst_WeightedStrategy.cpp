#include "planova/slicer/WeightedScoringStrategy.h"
#include <QTest>

using namespace planova::slicer;

namespace {
MemberDto member(const char* id, const char* name, std::map<SkillCategory, int> ratings, double velocity = 1.0) {
    return MemberDto{id, name, "Dev", false, std::move(ratings), 100, velocity};
}
TaskDto task(const char* id, Priority p, int complexity, int hours, TaskStatus s = TaskStatus::ToDo,
             std::optional<Id> assignee = std::nullopt, std::vector<Id> deps = {}) {
    return TaskDto{id, id, SkillCategory::Backend, p, complexity, hours, s, std::move(assignee), std::move(deps)};
}
}

class tst_WeightedStrategy : public QObject {
    Q_OBJECT
private slots:
    void skillDominanceWhenWorkloadZero() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("t1", Priority::Medium, 3, 5)},
                                {member("1", "Alice", {{SkillCategory::Backend, 5}}),
                                 member("2", "Bob", {{SkillCategory::Backend, 2}})},
                                ScoringWeights{100, 0, 0});
        QCOMPARE(int(res.size()), 1);
        QCOMPARE(res[0].memberId, std::string("1"));
        QCOMPARE(res[0].matchPct, 100);
        QCOMPARE(QString::fromStdString(res[0].rationale),
                 QString("High Backend proficiency (5/5) with balanced load (0h)."));
    }

    void workloadLevelingSplitsEqualTasks() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("t1", Priority::Medium, 3, 4), task("t2", Priority::Medium, 3, 4)},
                                {member("1", "Alice", {{SkillCategory::Backend, 5}}),
                                 member("2", "Bob", {{SkillCategory::Backend, 5}})},
                                ScoringWeights{0, 100, 0});
        QCOMPARE(int(res.size()), 2);
        QCOMPARE(res[0].memberId, std::string("1"));  // tie -> Alice
        QCOMPARE(res[1].memberId, std::string("2"));  // Alice now loaded -> Bob
    }

    void startingLoadFromInProgressAndReview() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("e1", Priority::Medium, 3, 8, TaskStatus::InProgress, Id("1")),
                                 task("e2", Priority::Medium, 3, 4, TaskStatus::Review, Id("1")),
                                 task("t1", Priority::Medium, 3, 2)},
                                {member("1", "Alice", {{SkillCategory::Backend, 5}}),
                                 member("2", "Bob", {{SkillCategory::Backend, 5}})},
                                ScoringWeights{50, 50, 0});
        QCOMPARE(int(res.size()), 1);
        QCOMPARE(res[0].memberId, std::string("2"));  // Alice starts with 12h
    }

    void eligibilityRatedOnlyWhenSomeRated() {
        // Weights {0,100,0}: without the rule both score equal and Alice (unrated) would win by name.
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("t1", Priority::Medium, 3, 5)},
                                {member("1", "Alice", {}), member("2", "Bob", {{SkillCategory::Backend, 1}})},
                                ScoringWeights{0, 100, 0});
        QCOMPARE(res[0].memberId, std::string("2"));
    }

    void ratedMemberWinsEvenWhenLoaded() {
        // Without the rule, loaded Bob would lose to idle Alice.
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("e1", Priority::Medium, 3, 50, TaskStatus::InProgress, Id("2")),
                                 task("t1", Priority::Medium, 3, 5)},
                                {member("1", "Alice", {}), member("2", "Bob", {{SkillCategory::Backend, 1}})},
                                ScoringWeights{10, 80, 10});
        QCOMPARE(res[0].memberId, std::string("2"));
    }

    void allUnratedFallback() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("t1", Priority::Medium, 3, 5)},
                                {member("1", "Alice", {}), member("2", "Bob", {})},
                                ScoringWeights{50, 30, 20});
        QCOMPARE(res[0].memberId, std::string("1"));
        QCOMPARE(QString::fromStdString(res[0].rationale),
                 QString("No rated member for Backend; assigned by workload."));
    }

    void pnScalingForHighVsLow() {
        // The fast member is alphabetically LAST, so a broken Pn cannot pass by name tie-break.
        WeightedScoringStrategy strat;
        std::vector<MemberDto> ms = {member("1", "Adam", {{SkillCategory::Backend, 3}}, 1.0),
                                     member("2", "Zoe", {{SkillCategory::Backend, 3}}, 5.0)};
        auto high = strat.assign({task("tH", Priority::High, 3, 2)}, ms, ScoringWeights{0, 0, 100});
        QCOMPARE(high[0].memberId, std::string("2"));  // velocity decides
        auto low = strat.assign({task("tL", Priority::Low, 3, 2)}, ms, ScoringWeights{0, 0, 100});
        QCOMPARE(low[0].memberId, std::string("1"));   // Pn = 1 for both -> name decides
    }

    void tieBreakByName() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("t1", Priority::Medium, 3, 2)},
                                {member("2", "Zoe", {{SkillCategory::Backend, 4}}),
                                 member("1", "Adam", {{SkillCategory::Backend, 4}})},
                                ScoringWeights{50, 30, 20});
        QCOMPARE(res[0].memberId, std::string("1"));
    }

    void nonToDoIgnored() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("tD", Priority::High, 3, 2, TaskStatus::Done, Id("1"))},
                                {member("1", "Alice", {{SkillCategory::Backend, 5}})}, ScoringWeights{});
        QCOMPARE(int(res.size()), 0);
    }

    void waitingOnDepFlag() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("dep", Priority::Medium, 1, 2, TaskStatus::InProgress, Id("1")),
                                 task("t1", Priority::Medium, 3, 5, TaskStatus::ToDo, std::nullopt, {"dep"})},
                                {member("1", "Alice", {{SkillCategory::Backend, 5}})}, ScoringWeights{});
        QCOMPARE(int(res.size()), 1);
        QVERIFY(res[0].waitingOnDep);
    }

    void scoreEquationHandComputed() {
        // score = 0.5*S + 0.3*L + 0.2*Pn  (S = rating/5, L = 1 - hours/maxHours, Pn = velocity/5 for High)
        // Ann: S = 4/5 = 0.8, L = 1 - 0/10 = 1.0, Pn = 3/5 = 0.6  ->  0.40 + 0.30 + 0.12 = 0.82 -> 82
        // Ben: S = 5/5 = 1.0, L = 1 - 10/10 = 0,  Pn = 1/5 = 0.2  ->  0.50 + 0.00 + 0.04 = 0.54 -> 54
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("busy", Priority::Medium, 3, 10, TaskStatus::InProgress, Id("2")),
                                 task("t1", Priority::High, 3, 4), task("t2", Priority::Low, 2, 2)},
                                {member("1", "Ann", {{SkillCategory::Backend, 4}}, 3.0),
                                 member("2", "Ben", {{SkillCategory::Backend, 5}}, 1.0)},
                                ScoringWeights{50, 30, 20});
        QCOMPARE(int(res.size()), 2);
        QCOMPARE(res[0].taskId, std::string("t1"));  // High is sliced before Low
        QCOMPARE(res[0].memberId, std::string("1"));
        QCOMPARE(res[0].matchPct, 82);
        // Ann now holds 4h (Ben 10h, max 10). For the Low task Pn = 1 for everyone:
        // Ann: 0.5*0.8 + 0.3*(1 - 4/10) + 0.2*1 = 0.40 + 0.18 + 0.20 = 0.78 -> 78  (Ben: 0.50 + 0 + 0.20 = 0.70)
        QCOMPARE(res[1].memberId, std::string("1"));
        QCOMPARE(res[1].matchPct, 78);
    }

    void rationaleLevelGood() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("t1", Priority::Medium, 3, 5)},
                                {member("1", "Alice", {{SkillCategory::Backend, 3}})}, ScoringWeights{});
        QCOMPARE(QString::fromStdString(res[0].rationale),
                 QString("Good Backend proficiency (3/5) with balanced load (0h)."));
    }

    void rationaleNamesUiUxAsUiUx() {
        WeightedScoringStrategy strat;
        TaskDto t = task("t1", Priority::Medium, 3, 5);
        t.skill = SkillCategory::UiUx;
        auto res = strat.assign({t}, {member("1", "Alice", {{SkillCategory::UiUx, 4}})}, ScoringWeights{});
        QVERIFY(QString::fromStdString(res[0].rationale).contains("UI/UX proficiency"));
    }

    void rationaleLevelBasic() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("t1", Priority::Medium, 3, 5)},
                                {member("1", "Alice", {{SkillCategory::Backend, 1}})}, ScoringWeights{});
        QVERIFY(QString::fromStdString(res[0].rationale).startsWith("Basic Backend proficiency (1/5)"));
    }

    void rationaleAboveAverageLoad() {
        // Only Alice is rated, so she gets both tasks. Before the second one she holds 5h
        // while the team average is 2.5h.
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("t1", Priority::Medium, 3, 5), task("t2", Priority::Medium, 3, 5)},
                                {member("1", "Alice", {{SkillCategory::Backend, 5}}), member("2", "Bob", {})},
                                ScoringWeights{});
        QCOMPARE(int(res.size()), 2);
        QCOMPARE(QString::fromStdString(res[1].rationale),
                 QString("High Backend proficiency (5/5) with above-average load (5h)."));
    }

    void finishedDependencyDoesNotWait() {
        WeightedScoringStrategy strat;
        auto res = strat.assign({task("dep", Priority::Medium, 1, 2, TaskStatus::Done, Id("1")),
                                 task("t1", Priority::Medium, 3, 5, TaskStatus::ToDo, std::nullopt, {"dep"})},
                                {member("1", "Alice", {{SkillCategory::Backend, 5}})}, ScoringWeights{});
        QCOMPARE(int(res.size()), 1);
        QVERIFY(!res[0].waitingOnDep);
    }

    void inputOrderDoesNotChangeTheResult() {
        WeightedScoringStrategy strat;
        std::vector<TaskDto> ts = {task("a", Priority::High, 4, 6), task("b", Priority::Medium, 3, 4),
                                   task("c", Priority::Medium, 3, 4), task("d", Priority::Low, 2, 2)};
        std::vector<MemberDto> ms = {member("1", "Alice", {{SkillCategory::Backend, 4}}, 2.0),
                                     member("2", "Bob", {{SkillCategory::Backend, 4}}, 2.0),
                                     member("3", "Cara", {{SkillCategory::Backend, 2}}, 3.0)};
        auto forward = strat.assign(ts, ms, ScoringWeights{});

        std::reverse(ts.begin(), ts.end());
        std::reverse(ms.begin(), ms.end());
        auto backward = strat.assign(ts, ms, ScoringWeights{});

        QCOMPARE(int(forward.size()), int(backward.size()));
        for (size_t i = 0; i < forward.size(); ++i) {
            QCOMPARE(forward[i].taskId, backward[i].taskId);
            QCOMPARE(forward[i].memberId, backward[i].memberId);
            QCOMPARE(forward[i].matchPct, backward[i].matchPct);
        }
    }

    void determinism() {
        WeightedScoringStrategy strat;
        std::vector<MemberDto> ms = {member("1", "Alice", {{SkillCategory::Backend, 4}}, 2.0),
                                     member("2", "Bob", {{SkillCategory::Backend, 3}}, 3.0)};
        std::vector<TaskDto> ts = {task("t1", Priority::High, 3, 4), task("t2", Priority::High, 3, 4)};
        auto a = strat.assign(ts, ms, ScoringWeights{});
        auto b = strat.assign(ts, ms, ScoringWeights{});
        QCOMPARE(int(a.size()), int(b.size()));
        for (size_t i = 0; i < a.size(); ++i) {
            QCOMPARE(a[i].taskId, b[i].taskId);
            QCOMPARE(a[i].memberId, b[i].memberId);
            QCOMPARE(a[i].matchPct, b[i].matchPct);
        }
    }

    void matchPctBounds() {
        WeightedScoringStrategy strat;
        // Perfect: rating 5/5, no load, velocity 5.0 -> score 1.0 -> 100
        auto top = strat.assign({task("t1", Priority::High, 3, 4)},
                                {member("1", "Alice", {{SkillCategory::Backend, 5}}, 5.0)}, ScoringWeights{});
        QCOMPARE(top[0].matchPct, 100);
        // Worst: unrated, loaded (L = 0), velocity 0 -> score 0 -> 0
        auto worst = strat.assign({task("e1", Priority::Medium, 5, 100, TaskStatus::InProgress, Id("2")),
                                   task("t1", Priority::High, 3, 4)},
                                  {member("2", "Bob", {}, 0.0)}, ScoringWeights{});
        QCOMPARE(worst[0].matchPct, 0);
    }
};

QTEST_APPLESS_MAIN(tst_WeightedStrategy)
#include "tst_WeightedStrategy.moc"
