#include <QtTest>

#include <string>
#include <vector>

#include <planova/common/Dtos.h>
#include <planova/stats/ProgressCalculator.h>

using namespace planova::s6;

class tst_ProgressCalculator : public QObject {
    Q_OBJECT

private:
    static TaskDto task(const std::string& id, int complexity, TaskStatus status)
    {
        TaskDto t;
        t.id = id;
        t.title = id;
        t.complexity = complexity;
        t.estHours = 2;
        t.status = status;
        return t;
    }

private slots:
    void nineTasksThreeDone()
    {
        std::vector<TaskDto> tasks;
        for (int i = 0; i < 9; ++i) {
            tasks.push_back(task("t" + std::to_string(i), 1,
                                 i < 3 ? TaskStatus::Done : TaskStatus::ToDo));
        }
        const Progress p = ProgressCalculator::compute(tasks);
        QCOMPARE(p.tasksDone, 3);
        QCOMPARE(p.tasksTotal, 9);
    }

    void pointsUseComplexity()
    {
        const Progress p = ProgressCalculator::compute({task("a", 1, TaskStatus::Done),
                                                        task("b", 2, TaskStatus::InProgress),
                                                        task("c", 5, TaskStatus::Done)});
        QCOMPARE(p.pointsDelivered, 6);
        QCOMPARE(p.pointsTotal, 8);
        QCOMPARE(p.tasksDone, 2);
        QCOMPARE(p.tasksTotal, 3);
    }

    void emptyProjectIsAllZero()
    {
        const Progress p = ProgressCalculator::compute({});
        QCOMPARE(p.tasksDone, 0);
        QCOMPARE(p.tasksTotal, 0);
        QCOMPARE(p.pointsDelivered, 0);
        QCOMPARE(p.pointsTotal, 0);
    }
};

QTEST_APPLESS_MAIN(tst_ProgressCalculator)
#include "tst_ProgressCalculator.moc"