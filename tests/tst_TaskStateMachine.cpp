#include <QtTest>
#include "planova/task/TaskStateMachine.h"
class TstTaskStateMachine : public QObject {
    Q_OBJECT
    static bool expected(int f, int t) {
        static const int allowed[][2] = {{0, 1}, {1, 0}, {1, 2}, {2, 1}, {2, 3}};
        for (auto& p : allowed) if (p[0] == f && p[1] == t) return true;
        return false;
    }
private slots:
    void allPairs_data() {
        static const char* names[] = {"ToDo", "InProgress", "Review", "Done"};
        QTest::addColumn<int>("from");
        QTest::addColumn<int>("to");
        QTest::addColumn<bool>("allowed");
        for (int f = 0; f < 4; ++f)
            for (int t = 0; t < 4; ++t)
                QTest::newRow(qPrintable(QString("%1->%2").arg(names[f], names[t])))
                    << f << t << expected(f, t);
    }
    void allPairs() {
        QFETCH(int, from);
        QFETCH(int, to);
        QFETCH(bool, allowed);
        QCOMPARE(TaskStateMachine::canMove(static_cast<TaskStatus>(from),
                                           static_cast<TaskStatus>(to)), allowed);
    }
    void outOfRangeIsRejected() {
        QVERIFY(!TaskStateMachine::canMove(static_cast<TaskStatus>(7), TaskStatus::Done));
    }
};
QTEST_APPLESS_MAIN(TstTaskStateMachine)
#include "tst_TaskStateMachine.moc"
