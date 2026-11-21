#include <QtTest>
#include <QTemporaryDir>
#include <filesystem>

#include "planova/PlanovaCore.h"

class tst_PlanovaCorePins : public QObject {
    Q_OBJECT
private slots:
    void pinSurvivesReopen();
    void declineThroughCore();
    void projectsAreIsolated();
};

void tst_PlanovaCorePins::pinSurvivesReopen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();
    Id project;

    {
        PlanovaCore core(ws);
        project = core.createProject("A", "team", "Agile/Scrum", "idea");
        core.openProject(project);
        const Id member = core.members().add("Ana", "Backend Developer", false);
        core.pins().create(PinCategory::HandoffNote, "Hello", {"api"}, member, std::nullopt);
        QCOMPARE(core.pinQuery().openCount(), 1);
    }

    PlanovaCore again(ws);
    again.openProject(project);
    QCOMPARE(again.pinQuery().openCount(), 1);
    QCOMPARE(again.pinBoard().openPins().size(), size_t(1));
}

void tst_PlanovaCorePins::declineThroughCore()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    core.openProject(core.createProject("A", "team", "Agile/Scrum", "idea"));

    const Id member = core.members().add("Ana", "Backend Developer", false);
    NewTask t;
    t.title = "Build API";
    t.skill = SkillCategory::Backend;
    t.priority = Priority::High;
    t.complexity = 3;
    t.estHours = 8;
    const Id task = core.tasks().create(t);
    core.tasks().assign(task, member, "board");

    core.conflicts().decline(task, member, "Too busy");

    QVERIFY(!core.taskQuery().byId(task)->assignee.has_value());
    QVERIFY(core.conflictQuery().hasOpenConflict(member));
    QCOMPARE(core.home().dashboard().pins.size(), size_t(1));
}

void tst_PlanovaCorePins::projectsAreIsolated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const Id a = core.createProject("A", "team-a", "Agile/Scrum", "idea");
    const Id b = core.createProject("B", "team-b", "Agile/Scrum", "idea");

    core.openProject(a);
    const Id member = core.members().add("Ana", "Backend Developer", false);
    core.pins().create(PinCategory::HandoffNote, "Only in A", {}, member, std::nullopt);
    QCOMPARE(core.pinQuery().openCount(), 1);

    core.openProject(b);
    QCOMPARE(core.pinQuery().openCount(), 0);
    QVERIFY(core.pinBoard().openPins().empty());
}

QTEST_APPLESS_MAIN(tst_PlanovaCorePins)
#include "tst_PlanovaCorePins.moc"
