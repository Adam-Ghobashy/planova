#include <QtTest>
#include <QTemporaryDir>
#include <filesystem>
#include <stdexcept>

#include "planova/PlanovaCore.h"
#include "planova/project/ProjectSetup.h"

namespace {

// A valid setup with two rated members; each test breaks one thing.
ProjectSetup validSetup()
{
    ProjectSetup setup;
    setup.name = "Library Portal";
    setup.team = "ECU Team";
    setup.seModel = "Scrum";
    setup.idea = "Book loans";

    SetupMember ana;
    ana.name = "Ana";
    ana.role = "Backend Developer";
    ana.isLead = true;
    ana.ratings = {{SkillCategory::Backend, 5}, {SkillCategory::Testing, 2}};
    SetupMember ben;
    ben.name = "Ben";
    ben.role = "UI / UX Designer";
    ben.ratings = {{SkillCategory::UiUx, 4}};
    setup.members = {ana, ben};

    setup.weights = {60, 20, 20};
    return setup;
}

// Number of entries in the workspace folder: project folders and event files.
int entryCount(const QTemporaryDir& dir)
{
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir.path().toStdString())) {
        Q_UNUSED(entry);
        ++count;
    }
    return count;
}

} // namespace

class tst_PlanovaCoreSetup : public QObject {
    Q_OBJECT
private slots:
    void setupWritesEverything();
    void gitStaysDisconnectedWithoutUrl();
    void createBlockedWithoutMember();
    void blankNameBlocked();
    void invalidWeightsBlocked();
    void unknownModelBlocked();
    void badRatingBlocked();
    void blankMemberNameBlocked();
};

void tst_PlanovaCoreSetup::setupWritesEverything()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    ProjectSetup setup = validSetup();
    setup.repoUrl = "https://github.com/team/portal.git";
    setup.branch = "develop";
    const Id id = core.setupProject(setup);

    QCOMPARE(core.projects().list().size(), size_t(1));
    QCOMPARE(core.projects().list().front().id, id);
    QCOMPARE(core.projects().list().front().seModel, std::string("Scrum"));
    QVERIFY(core.currentProject().has_value());
    QCOMPARE(*core.currentProject(), id);

    const auto members = core.memberQuery().all();
    QCOMPARE(members.size(), size_t(2));
    for (const auto& member : members) {
        if (member.name == "Ana") {
            QVERIFY(member.isLead);
            QCOMPARE(member.ratings.at(SkillCategory::Backend), 5);
            QCOMPARE(member.ratings.at(SkillCategory::Testing), 2);
        } else {
            QCOMPARE(member.name, std::string("Ben"));
            QCOMPARE(member.ratings.at(SkillCategory::UiUx), 4);
        }
    }

    const planova::slicer::ScoringWeights saved = core.slicer().savedWeights();
    QVERIFY(saved == (planova::slicer::ScoringWeights{60, 20, 20}));

    QVERIFY(core.gitProjection().isConnected());
    QCOMPARE(core.gitProjection().repoUrl(), std::string("https://github.com/team/portal.git"));
    QCOMPARE(core.gitProjection().branch(), std::string("develop"));
}

void tst_PlanovaCoreSetup::gitStaysDisconnectedWithoutUrl()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());

    core.setupProject(validSetup());
    QVERIFY(!core.gitProjection().isConnected());
}

void tst_PlanovaCoreSetup::createBlockedWithoutMember()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const int before = entryCount(dir);

    ProjectSetup setup = validSetup();
    setup.members.clear();
    QVERIFY_EXCEPTION_THROWN(core.setupProject(setup), std::invalid_argument);

    QVERIFY(core.projects().list().empty());
    QVERIFY(!core.currentProject().has_value());
    QCOMPARE(entryCount(dir), before); // no project folder and no event file
}

void tst_PlanovaCoreSetup::blankNameBlocked()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const int before = entryCount(dir);

    ProjectSetup setup = validSetup();
    setup.name = "   ";
    QVERIFY_EXCEPTION_THROWN(core.setupProject(setup), std::invalid_argument);
    QVERIFY(core.projects().list().empty());
    QCOMPARE(entryCount(dir), before);
}

void tst_PlanovaCoreSetup::invalidWeightsBlocked()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const int before = entryCount(dir);

    ProjectSetup setup = validSetup();
    setup.weights = {60, 30, 20}; // adds up to 110
    QVERIFY_EXCEPTION_THROWN(core.setupProject(setup), std::invalid_argument);
    QVERIFY(core.projects().list().empty());
    QCOMPARE(entryCount(dir), before);
}

void tst_PlanovaCoreSetup::unknownModelBlocked()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const int before = entryCount(dir);

    ProjectSetup setup = validSetup();
    setup.seModel = "Hybrid";
    QVERIFY_EXCEPTION_THROWN(core.setupProject(setup), std::invalid_argument);
    QVERIFY(core.projects().list().empty());
    QCOMPARE(entryCount(dir), before);
}

void tst_PlanovaCoreSetup::badRatingBlocked()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const int before = entryCount(dir);

    ProjectSetup setup = validSetup();
    setup.members.back().ratings[SkillCategory::Backend] = 6;
    QVERIFY_EXCEPTION_THROWN(core.setupProject(setup), std::invalid_argument);
    QVERIFY(core.projects().list().empty());
    QCOMPARE(entryCount(dir), before);
}

void tst_PlanovaCoreSetup::blankMemberNameBlocked()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    const int before = entryCount(dir);

    ProjectSetup setup = validSetup();
    setup.members.back().name = "  ";
    QVERIFY_EXCEPTION_THROWN(core.setupProject(setup), std::invalid_argument);
    QVERIFY(core.projects().list().empty());
    QCOMPARE(entryCount(dir), before);
}

QTEST_APPLESS_MAIN(tst_PlanovaCoreSetup)
#include "tst_PlanovaCoreSetup.moc"
