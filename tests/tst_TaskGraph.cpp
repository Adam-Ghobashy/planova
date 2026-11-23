#include <QtTest>
#include "planova/task/TaskGraph.h"
class TstTaskGraph : public QObject {
    Q_OBJECT
private slots:
    void rejectsTwoNodeCycle() {
        TaskGraph g;
        g.addNode("a", 1); g.addNode("b", 1);
        QVERIFY(g.addDependency("b", "a"));      // b depends on a
        QVERIFY(!g.addDependency("a", "b"));     // a -> b -> a
    }
    void rejectsSelfDependency() {
        TaskGraph g;
        g.addNode("a", 1);
        QVERIFY(!g.addDependency("a", "a"));
    }
    void rejectsLongCycle() {
        TaskGraph g;
        for (auto id : {"a", "b", "c"}) g.addNode(id, 1);
        QVERIFY(g.addDependency("b", "a"));
        QVERIFY(g.addDependency("c", "b"));
        QVERIFY(!g.addDependency("a", "c"));
    }
    void duplicateEdgeIsIdempotent() {
        TaskGraph g;
        g.addNode("a", 1); g.addNode("b", 1);
        QVERIFY(g.addDependency("b", "a"));
        QVERIFY(g.addDependency("b", "a"));
        QCOMPARE(g.blockedBy("b", {}).size(), size_t(1));
    }
    void blockedByListsOnlyUnfinished() {
        TaskGraph g;
        for (auto id : {"b", "c", "d"}) g.addNode(id, 1);
        g.addDependency("d", "b");
        g.addDependency("d", "c");
        QCOMPARE(g.blockedBy("d", {}), (std::vector<Id>{"b", "c"}));
        QCOMPARE(g.blockedBy("d", {"b"}), (std::vector<Id>{"c"}));
        QVERIFY(g.blockedBy("d", {"b", "c"}).empty());
    }
    void criticalPathPicksLongerBranch() {
        // a(2) -> b(5) -> d(2)   total 9
        // a(2) -> c(1) -> d(2)   total 5
        TaskGraph g;
        g.addNode("a", 2); g.addNode("b", 5); g.addNode("c", 1); g.addNode("d", 2);
        g.addDependency("b", "a");
        g.addDependency("c", "a");
        g.addDependency("d", "b");
        g.addDependency("d", "c");
        QCOMPARE(g.criticalPath(), (std::vector<Id>{"a", "b", "d"}));
    }
    void criticalPathOfIndependentTasksIsTheLongest() {
        TaskGraph g;
        g.addNode("a", 2); g.addNode("b", 7); g.addNode("c", 3);
        QCOMPARE(g.criticalPath(), (std::vector<Id>{"b"}));
    }
    void criticalPathOfEmptyGraphIsEmpty() {
        TaskGraph g;
        QVERIFY(g.criticalPath().empty());
    }
    void addDependencyKeepsExistingHours() {
        TaskGraph g;
        g.addNode("a", 4); g.addNode("b", 6);
        g.addDependency("b", "a");
        QCOMPARE(g.criticalPath(), (std::vector<Id>{"a", "b"}));   // 4 + 6, not 0 + 0
    }
};
QTEST_APPLESS_MAIN(TstTaskGraph)
#include "tst_TaskGraph.moc"

