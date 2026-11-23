#include <QtTest>
#include <QTemporaryDir>
#include <filesystem>

#include "planova/PlanovaCore.h"

class tst_PlanovaCoreTasks : public QObject {
    Q_OBJECT
private slots:
    void createdTaskSurvivesReopen();
    void velocityIgnoresInstantTaskFlow();
};

void tst_PlanovaCoreTasks::velocityIgnoresInstantTaskFlow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(std::filesystem::path(dir.path().toStdString()));
    core.openProject(core.createProject("A", "team", "Agile", ""));

    const Id member = core.members().add("Ana", "Developer", false);
    NewTask t;
    t.title = "Build API";
    t.skill = SkillCategory::Backend;
    t.priority = Priority::High;
    t.complexity = 3;
    t.estHours = 8;
    const Id task = core.tasks().create(t);

    core.tasks().assign(task, member, "board");
    core.tasks().move(task, TaskStatus::InProgress);
    core.tasks().move(task, TaskStatus::Review);
    core.tasks().move(task, TaskStatus::Done);

    // The whole flow takes milliseconds: that is bookkeeping, so velocity stays at its start value.
    QCOMPARE(core.memberQuery().byId(member)->velocity, 1.0);
}

void tst_PlanovaCoreTasks::createdTaskSurvivesReopen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();
    Id projectId;

    {
        PlanovaCore core(ws);
        projectId = core.createProject("A", "team", "Agile", "");
        core.openProject(projectId);

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
    again.openProject(projectId);
    QCOMPARE(again.taskQuery().all().size(), size_t(1));
    QCOMPARE(again.taskQuery().all().front().title, std::string("Design API"));
}

QTEST_APPLESS_MAIN(tst_PlanovaCoreTasks)
#include "tst_PlanovaCoreTasks.moc"
