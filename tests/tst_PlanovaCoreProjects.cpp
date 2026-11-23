#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <filesystem>
#include <stdexcept>
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
    void legacyDataImportedOnFirstOpen();
    void newProjectStartsEmptyEvenWithLegacyRoot();
    void hasProjectDataReflectsFolder();
    void openUnknownProjectThrows();
    void damagedRootFileCountedOnce();
    void updateProjectNotifiesBus();
    void deleteOpenProjectClosesIt();
    void deleteOtherProjectKeepsOpenOne();
    void deleteUnknownProjectThrows();
    void deleteUnknownProjectMessageIsFriendly();
    void deleteClearsEventsWrittenWhileClosed();
    void deleteFailureKeepsProjectOpen();
    void deleteReportsFolderItCannotRemove();
    void restartAfterDeleteStartsClean();
    void deleteRejectsUnsafeIdAndKeepsWorkspace_data();
    void deleteRejectsUnsafeIdAndKeepsWorkspace();
    void deleteFailureAfterAppendDoesNotReopenDeletedProject();
};

namespace {
Id makeProject(PlanovaCore& core, const std::string& name)
{
    return core.projects().create(name, "team-" + name, "Agile", "idea");
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

void tst_PlanovaCoreProjects::legacyDataImportedOnFirstOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    // No project open yet, so this member lands in the workspace root.
    core.members().add("Old", "Backend Developer", false);
    const Id a = makeProject(core, "A"); // old style: no project folder

    core.openProject(a);
    QCOMPARE(core.memberQuery().all().size(), size_t(1));
    QCOMPARE(core.memberQuery().all().front().name, std::string("Old"));
}

void tst_PlanovaCoreProjects::newProjectStartsEmptyEvenWithLegacyRoot()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    core.members().add("Old", "Backend Developer", false);
    const Id id = core.createProject("A", "team", "Agile", "idea");

    core.openProject(id);
    QVERIFY(core.memberQuery().all().empty());
}

void tst_PlanovaCoreProjects::hasProjectDataReflectsFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    // Not created yet, so no folder for any id.
    QVERIFY(!core.hasProjectData("p1"));
    const Id id = core.createProject("A", "team", "Agile", "idea");
    QVERIFY(core.hasProjectData(id));
}

void tst_PlanovaCoreProjects::openUnknownProjectThrows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, core.openProject("bogus"));
    QVERIFY(!core.currentProject());
    QVERIFY(!std::filesystem::exists(std::filesystem::path(dir.path().toStdString()) / "bogus"));
}

// With no project open, the project-list store and the project store both read the workspace root, so a damaged
// file there is seen twice. The user must hear about it once.
void tst_PlanovaCoreProjects::damagedRootFileCountedOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        QFile bad(dir.filePath("damaged.json"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("{ not json");
    }
    PlanovaCore core(dir.path().toStdString());
    QVERIFY(!core.currentProject());

    const std::vector<std::string> broken = core.brokenEventFiles();
    QCOMPARE(broken.size(), size_t(1));
    QCOMPARE(QString::fromStdString(broken[0]), QString("damaged.json"));
}

void tst_PlanovaCoreProjects::updateProjectNotifiesBus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const Id a = makeProject(core, "A");

    std::vector<std::string> seen;
    core.bus().subscribe([&seen](const std::string& type) { seen.push_back(type); });

    core.updateProject(a, "Renamed", "New team", "Scrum");
    QVERIFY(contains(seen, "ProjectUpdated"));
    QCOMPARE(core.projects().list().front().name, std::string("Renamed"));
}

void tst_PlanovaCoreProjects::deleteOpenProjectClosesIt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();
    PlanovaCore core(ws);
    const Id a = makeProject(core, "A");
    core.openProject(a);
    core.members().add("Ana", "Backend Developer", false);

    std::vector<std::string> seen;
    core.bus().subscribe([&seen](const std::string& type) { seen.push_back(type); });

    QVERIFY(!core.deleteProject(a).has_value());
    QVERIFY(!core.currentProject());
    QVERIFY(core.memberQuery().all().empty());
    QVERIFY(core.projects().list().empty());
    QVERIFY(!std::filesystem::exists(ws / a));
    QVERIFY(contains(seen, "ProjectDeleted"));
}

void tst_PlanovaCoreProjects::deleteOtherProjectKeepsOpenOne()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();
    PlanovaCore core(ws);
    const Id a = core.createProject("A", "team", "Agile", "idea");
    const Id b = core.createProject("B", "team", "Agile", "idea");
    core.openProject(a);
    core.members().add("Ana", "Backend Developer", false);

    QVERIFY(!core.deleteProject(b).has_value());
    QCOMPARE(core.currentProject().value_or(Id()), a);
    QCOMPARE(core.memberQuery().all().size(), size_t(1));
    QVERIFY(std::filesystem::exists(ws / a / "events"));
    QVERIFY(!std::filesystem::exists(ws / b));
}

void tst_PlanovaCoreProjects::deleteUnknownProjectThrows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const Id a = makeProject(core, "A");
    core.openProject(a);

    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, core.deleteProject("bogus"));
    QCOMPARE(core.currentProject().value_or(Id()), a);
}

void tst_PlanovaCoreProjects::deleteUnknownProjectMessageIsFriendly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    try {
        core.deleteProject("bogus");
        QFAIL("deleteProject should throw for an unknown id");
    } catch (const std::invalid_argument& error) {
        QCOMPARE(QString::fromStdString(error.what()), QString("This project no longer exists."));
    }
}

// Events written while no project was open land in <ws>/.closed. Deleting the open project must not let them
// reappear.
void tst_PlanovaCoreProjects::deleteClearsEventsWrittenWhileClosed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();
    PlanovaCore core(ws);
    const Id a = makeProject(core, "A");
    core.openProject(a);
    core.members().add("Ana", "Backend Developer", false);

    // Copy Ana's event file into .closed, as if it had been written while no project was open.
    const std::filesystem::path closed = ws / ".closed";
    std::filesystem::create_directories(closed);
    for (const auto& entry : std::filesystem::directory_iterator(ws / a / "events"))
        std::filesystem::copy_file(entry.path(), closed / entry.path().filename());

    QVERIFY(!core.deleteProject(a).has_value());
    QVERIFY(core.memberQuery().all().empty());
    QVERIFY(!std::filesystem::exists(closed));
}

// Makes the workspace folder a plain file, so appending the ProjectDeleted event fails.
void tst_PlanovaCoreProjects::deleteFailureKeepsProjectOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = std::filesystem::path(dir.path().toStdString()) / "ws";
    PlanovaCore core(ws);
    const Id a = makeProject(core, "A");
    core.openProject(a);
    core.members().add("Ana", "Backend Developer", false);

    std::filesystem::remove_all(ws);
    {
        QFile blocker(QString::fromStdString(ws.string()));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
    }

    QVERIFY_THROWS_EXCEPTION(std::exception, core.deleteProject(a));
    QCOMPARE(core.currentProject().value_or(Id()), a);
    QVERIFY(core.projects().exists(a));
}

// A file held open blocks removal of its folder on Windows.
void tst_PlanovaCoreProjects::deleteReportsFolderItCannotRemove()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();
    PlanovaCore core(ws);
    const Id a = core.createProject("A", "team", "Agile", "idea");
    core.openProject(a);

    const std::filesystem::path folder = ws / a;
    QFile held(QString::fromStdString((folder / "events" / "held.tmp").string()));
    QVERIFY(held.open(QIODevice::WriteOnly));

    const auto leftover = core.deleteProject(a);
    if (!leftover.has_value())
        QSKIP("folder removal did not fail on this platform");
    QCOMPARE(*leftover, folder);
    QVERIFY(core.projects().list().empty());
    held.close();
}

void tst_PlanovaCoreProjects::restartAfterDeleteStartsClean()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = dir.path().toStdString();
    Id a;
    {
        PlanovaCore core(ws);
        a = core.createProject("A", "team", "Agile", "idea");
        core.openProject(a);
        core.members().add("Ana", "Backend Developer", false);
        QVERIFY(!core.deleteProject(a).has_value());
    }

    PlanovaCore again(ws);
    QVERIFY(again.projects().list().empty());
    QVERIFY(!again.hasProjectData(a));
}

// A damaged or hand-edited project file can carry an id that is not a folder name. Deleting that project must
// never remove the workspace (or anything outside it).
void tst_PlanovaCoreProjects::deleteRejectsUnsafeIdAndKeepsWorkspace_data()
{
    QTest::addColumn<QString>("badId");
    QTest::newRow("empty") << QString();
    QTest::newRow("dot") << QString(".");
    QTest::newRow("dot dot") << QString("..");
    QTest::newRow("nested") << QString("x/y");
    // Windows drops a trailing dot or space, so "a." would name the folder "a".
    QTest::newRow("three dots") << QString("...");
    QTest::newRow("space") << QString(" ");
    QTest::newRow("dot space") << QString(". ");
    QTest::newRow("trailing dot") << QString("a.");
    QTest::newRow("trailing space") << QString("a ");
    QTest::newRow("colon") << QString("a:b");
    QTest::newRow("star") << QString("a*b");
}

void tst_PlanovaCoreProjects::deleteRejectsUnsafeIdAndKeepsWorkspace()
{
    QFETCH(QString, badId);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path ws = std::filesystem::path(dir.path().toStdString()) / "ws";
    Id damaged;
    Id keep;
    {
        PlanovaCore core(ws);
        damaged = core.createProject("Damaged", "team", "Agile", "idea");
        keep = core.createProject("Keep", "team", "Agile", "idea");
        core.openProject(keep);
        core.members().add("Ana", "Backend Developer", false);
    }
    // Rewrite the damaged project's creation event so its id is unsafe.
    bool rewritten = false;
    for (const auto& entry : std::filesystem::directory_iterator(ws)) {
        if (entry.path().extension() != ".json")
            continue;
        QFile file(QString::fromStdString(entry.path().string()));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        file.close();
        const QByteArray from = "\"projectId\":\"" + QByteArray::fromStdString(damaged) + "\"";
        if (!data.contains(from) || !data.contains("ProjectCreated"))
            continue;
        data.replace(from, "\"projectId\":\"" + badId.toUtf8() + "\"");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(data);
        rewritten = true;
    }
    QVERIFY(rewritten);

    PlanovaCore core(ws);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, core.deleteProject(badId.toStdString()));
    QVERIFY(std::filesystem::exists(ws / keep / "events"));
    QVERIFY(std::filesystem::exists(ws));
    QVERIFY(core.hasProjectData(keep));
}

// A bus subscriber that throws runs after ProjectDeleted is already stored, so the project is gone for good.
// The open project must not be restored then.
void tst_PlanovaCoreProjects::deleteFailureAfterAppendDoesNotReopenDeletedProject()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(std::filesystem::path(dir.path().toStdString()));
    const Id a = makeProject(core, "A");
    core.openProject(a);
    core.bus().subscribe([](const std::string& type) {
        if (type == "ProjectDeleted")
            throw std::runtime_error("subscriber failed");
    });

    const auto folder = std::filesystem::path(dir.path().toStdString()) / a;
    QVERIFY(std::filesystem::exists(folder));

    QVERIFY_THROWS_EXCEPTION(std::runtime_error, core.deleteProject(a));
    QVERIFY(!core.projects().exists(a));
    QVERIFY(!core.currentProject().has_value());
    // The project is gone from the list, so its folder must be gone too (no orphaned data).
    QVERIFY(!std::filesystem::exists(folder));
}

QTEST_APPLESS_MAIN(tst_PlanovaCoreProjects)
#include "tst_PlanovaCoreProjects.moc"
