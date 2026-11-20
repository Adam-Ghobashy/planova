#include <QtTest>
#include <QTemporaryDir>
#include <filesystem>

#include "planova/PlanovaCore.h"

class tst_PlanovaCoreTasks : public QObject {
    Q_OBJECT
private slots:
    void createdTaskSurvivesReopen();
};

void tst_PlanovaCoreTasks::createdTaskSurvivesReopen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();

    {
        PlanovaCore core(ws);
        core.openProject("p1");

        NewTask t;
        t.title = "Design API";
        t.skill = SkillCategory::Backend;
        t.priority = Priority::High;
        t.complexity = 3;
        t.estHours = 8;

        core.tasks().create(t);
        QCOMPARE(core.taskQuery().all().size(), size_t(1));
    }

    // simulate an app restart: a brand new core on the same folder
    PlanovaCore again(ws);
    again.openProject("p1");
    QCOMPARE(again.taskQuery().all().size(), size_t(1));
    QCOMPARE(again.taskQuery().all().front().title, std::string("Design API"));
}

QTEST_APPLESS_MAIN(tst_PlanovaCoreTasks)
#include "tst_PlanovaCoreTasks.moc"
