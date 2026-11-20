#include <QtTest>
#include <QTemporaryDir>
#include <filesystem>
#include <string>
#include <vector>

#include "planova/PlanovaCore.h"

class tst_PlanovaCoreProjects : public QObject {
    Q_OBJECT
private slots:
    void projectListSurvivesOpenProject();
    void membersAreIsolatedPerProject();
    void reopenRestoresProjectData();
    void openProjectNotifiesBus();
    void creatingProjectNotifiesBus();
    void legacyRootEventsVisibleBeforeOpen();
};

namespace {
Id makeProject(PlanovaCore& core, const std::string& name)
{
    return core.projects().create(name, "team-" + name, "Agile/Scrum", "idea");
}

bool contains(const std::vector<std::string>& list, const std::string& value)
{
    for (const auto& item : list)
        if (item == value)
            return true;
    return false;
}
} // namespace

void tst_PlanovaCoreProjects::projectListSurvivesOpenProject()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    const Id a = makeProject(core, "A");
    core.openProject(a);

    QCOMPARE(core.projects().list().size(), size_t(1));
}

void tst_PlanovaCoreProjects::membersAreIsolatedPerProject()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    const Id a = makeProject(core, "A");
    const Id b = makeProject(core, "B");

    core.openProject(a);
    core.members().add("Ana", "Backend Developer", false);
    QCOMPARE(core.memberQuery().all().size(), size_t(1));

    core.openProject(b);
    QVERIFY(core.memberQuery().all().empty());

    core.openProject(a);
    QCOMPARE(core.memberQuery().all().size(), size_t(1));
}

void tst_PlanovaCoreProjects::reopenRestoresProjectData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();

    Id a;
    {
        PlanovaCore core(ws);
        a = makeProject(core, "A");
        core.openProject(a);
        core.members().add("Ana", "Backend Developer", false);
    }

    PlanovaCore again(ws);
    QCOMPARE(again.projects().list().size(), size_t(1));
    again.openProject(a);
    QCOMPARE(again.memberQuery().all().size(), size_t(1));
    QCOMPARE(again.memberQuery().all().front().name, std::string("Ana"));
}

void tst_PlanovaCoreProjects::openProjectNotifiesBus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const Id a = makeProject(core, "A");

    std::vector<std::string> seen;
    core.bus().subscribe([&seen](const std::string& type) { seen.push_back(type); });

    core.openProject(a);
    QVERIFY(contains(seen, "ProjectOpened"));
}

void tst_PlanovaCoreProjects::creatingProjectNotifiesBus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    std::vector<std::string> seen;
    core.bus().subscribe([&seen](const std::string& type) { seen.push_back(type); });

    makeProject(core, "A");
    QVERIFY(contains(seen, "ProjectCreated"));
}

void tst_PlanovaCoreProjects::legacyRootEventsVisibleBeforeOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();

    {
        PlanovaCore core(ws);
        core.members().add("Old", "Backend Developer", false);
    }

    PlanovaCore again(ws);
    QCOMPARE(again.memberQuery().all().size(), size_t(1));
    QCOMPARE(again.memberQuery().all().front().name, std::string("Old"));
}

QTEST_APPLESS_MAIN(tst_PlanovaCoreProjects)
#include "tst_PlanovaCoreProjects.moc"
