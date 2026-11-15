#include "planova/slicer/PizzaPortionCalculator.h"
#include <QTest>

using namespace planova::slicer;

namespace {
MemberDto member(const char* id, const char* name) { return MemberDto{id, name, "Dev", false, {}, 100, 1.0}; }
TaskDto task(const char* id, int complexity, TaskStatus s, std::optional<Id> assignee) {
    return TaskDto{id, id, SkillCategory::Backend, Priority::Low, complexity, 2, s, std::move(assignee), {}};
}
}

class tst_PizzaPortions : public QObject {
    Q_OBJECT
private slots:
    void equalSharesSumTo360() {
        auto p = PizzaPortionCalculator::calculate(
            {task("t1", 2, TaskStatus::ToDo, Id("1")), task("t2", 2, TaskStatus::Done, Id("2"))},
            {member("1", "Alice"), member("2", "Bob")});
        QCOMPARE(int(p.size()), 2);
        QCOMPARE(p[0].taskCount, 1);
        QCOMPARE(p[1].taskCount, 1);
        QCOMPARE(p[0].share, 0.5);
        QCOMPARE(p[1].share, 0.5);
        QCOMPARE(p[0].spanAngle, 180.0);
        QCOMPARE(p[1].startAngle, 180.0);
        QCOMPARE(p[0].spanAngle + p[1].spanAngle, 360.0);
    }
    void complexityWeightsTheSlice() {
        auto p = PizzaPortionCalculator::calculate(
            {task("t1", 3, TaskStatus::ToDo, Id("1")), task("t2", 1, TaskStatus::ToDo, Id("2"))},
            {member("1", "Alice"), member("2", "Bob")});
        QCOMPARE(p[0].share, 0.75);
        QCOMPARE(p[1].share, 0.25);
    }
    void anglesChainFromShares() {
        // complexity 2 + 3 + 5 = 10: shares 0.2 / 0.3 / 0.5 -> spans 72 / 108 / 180 degrees,
        // and each slice starts where the previous one ended.
        auto p = PizzaPortionCalculator::calculate(
            {task("t1", 2, TaskStatus::ToDo, Id("1")), task("t2", 3, TaskStatus::ToDo, Id("2")),
             task("t3", 5, TaskStatus::ToDo, Id("3"))},
            {member("1", "Alice"), member("2", "Bob"), member("3", "Cara")});
        QCOMPARE(p[0].spanAngle, 72.0);
        QCOMPARE(p[1].startAngle, 72.0);
        QCOMPARE(p[1].spanAngle, 108.0);
        QCOMPARE(p[2].startAngle, 180.0);
        QCOMPARE(p[2].spanAngle, 180.0);
        QCOMPARE(p[2].startAngle + p[2].spanAngle, 360.0);
    }
    void memberWithoutTasksGetsThinSlice() {
        auto p = PizzaPortionCalculator::calculate({task("t1", 4, TaskStatus::InProgress, Id("1"))},
                                                   {member("1", "Alice"), member("2", "Bob")});
        QCOMPARE(p[0].share, 1.0);
        QVERIFY(!p[0].thinSlice);
        QCOMPARE(p[1].share, 0.0);
        QVERIFY(p[1].thinSlice);
    }
    void emptyProjectIsAllThinSlices() {
        auto p = PizzaPortionCalculator::calculate({}, {member("1", "Alice")});
        QCOMPARE(int(p.size()), 1);
        QCOMPARE(p[0].share, 0.0);
        QVERIFY(p[0].thinSlice);
    }
};

QTEST_APPLESS_MAIN(tst_PizzaPortions)
#include "tst_PizzaPortions.moc"
