#include <QtTest>
#include <QTemporaryDir>
#include <filesystem>

#include "planova/PlanovaCore.h"

namespace {

int countEventFiles(const std::filesystem::path& dir)
{
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".json") {
            ++count;
        }
    }
    return count;
}

}

class tst_PlanovaCoreSlicer : public QObject {
    Q_OBJECT
private slots:
    void applyShowsAssigneeOnTaskQuery();
    void reapplyAfterRestartAppendsNothing();
};

// Two members, Ana is the better Backend developer, and one Backend task.
static Id setUpTeamAndTask(PlanovaCore& core)
{
    const Id ana = core.members().add("Ana", "Developer", false);
    const Id ben = core.members().add("Ben", "Developer", false);
    core.members().rate(ana, SkillCategory::Backend, 5);
    core.members().rate(ben, SkillCategory::Backend, 2);

    NewTask t;
    t.title = "Build API";
    t.skill = SkillCategory::Backend;
    t.priority = Priority::High;
    t.complexity = 3;
    t.estHours = 8;
    core.tasks().create(t);
    return ana;
}

void tst_PlanovaCoreSlicer::applyShowsAssigneeOnTaskQuery()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlanovaCore core(std::filesystem::path(dir.path().toStdString()));
    core.openProject(core.createProject("A", "team", "Agile", ""));
    const Id anaId = setUpTeamAndTask(core);

    const auto preview = core.slicer().preview(planova::slicer::ScoringWeights{});
    QCOMPARE(core.slicer().apply(preview, planova::slicer::ScoringWeights{}), 1);
    QCOMPARE(core.taskQuery().all().front().assignee.value(), anaId);
}

void tst_PlanovaCoreSlicer::reapplyAfterRestartAppendsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws(dir.path().toStdString());

    Id anaId;
    Id projectId;
    {
        PlanovaCore core(ws);
        projectId = core.createProject("A", "team", "Agile", "");
        core.openProject(projectId);
        anaId = setUpTeamAndTask(core);
        const auto preview = core.slicer().preview(planova::slicer::ScoringWeights{});
        QCOMPARE(core.slicer().apply(preview, planova::slicer::ScoringWeights{}), 1);
    }

    // simulate an app restart: a brand new core on the same folder
    PlanovaCore again(ws);
    again.openProject(projectId);
    const auto eventsDir = ws / projectId / "events";
    const int before = countEventFiles(eventsDir);

    const auto preview = again.slicer().preview(planova::slicer::ScoringWeights{});
    QVERIFY(!preview.empty()); // replay restored the team and task
    QCOMPARE(again.taskQuery().all().front().assignee.value(), anaId);
    QCOMPARE(again.slicer().apply(preview, planova::slicer::ScoringWeights{}), 0);
    QCOMPARE(countEventFiles(eventsDir), before);
}

QTEST_APPLESS_MAIN(tst_PlanovaCoreSlicer)
#include "tst_PlanovaCoreSlicer.moc"
