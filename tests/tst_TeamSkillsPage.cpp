#include "app/CoreBridge.h"
#include "planova/PlanovaCore.h"
#include "team/TeamSkillsPage.h"
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

namespace {
NewTask sampleTask(const char* title)
{
    NewTask t;
    t.title = title;
    t.skill = SkillCategory::Backend;
    t.priority = Priority::Medium;
    t.complexity = 2;
    t.estHours = 4;
    return t;
}

// A page needs a project to exist, so each test starts from an open "A" project.
struct Fixture {
    QTemporaryDir dir;
    PlanovaCore core{dir.path().toStdString()};
    CoreBridge bridge{core};

    Fixture() { core.openProject(core.createProject("A", "team", "Agile/Scrum", "idea")); }

    planova::team::TeamSkillsPage* makePage()
    {
        return new planova::team::TeamSkillsPage(core.members(), core.memberProjection(), core.conflicts(),
                                                 core.conflictProjection(), core.taskQuery(), bridge);
    }
};
} // namespace

class tst_TeamSkillsPage : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Q_INIT_RESOURCE(planova); }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    void switchesPersist()
    {
        Fixture f;
        std::unique_ptr<planova::team::TeamSkillsPage> first(f.makePage());
        auto* autoMatch = first->findChild<QPushButton*>("autoMatchSwitch");
        auto* smartAssign = first->findChild<QPushButton*>("smartAssignSwitch");
        QVERIFY(autoMatch && smartAssign);
        QVERIFY(autoMatch->isCheckable());
        QVERIFY(!autoMatch->isChecked());
        autoMatch->click();
        QVERIFY(f.core.memberProjection().getSetting("autoMatch"));
        QVERIFY(!f.core.memberProjection().getSetting("smartAssign"));

        std::unique_ptr<planova::team::TeamSkillsPage> second(f.makePage());
        QVERIFY(second->findChild<QPushButton*>("autoMatchSwitch")->isChecked());
        QVERIFY(!second->findChild<QPushButton*>("smartAssignSwitch")->isChecked());

        second->findChild<QPushButton*>("autoMatchSwitch")->click();
        QVERIFY(!f.core.memberProjection().getSetting("autoMatch"));
    }

    void clashBadgeShownForDecliner()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        f.core.members().add("Ben", "Frontend Developer", false);
        const Id task = f.core.tasks().create(sampleTask("Build API"));
        f.core.tasks().assign(task, ana, "board");

        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->show();
        QVERIFY(page->findChildren<QLabel*>("clashBadge").isEmpty());

        f.core.conflicts().decline(task, ana, "Overloaded");
        QTest::qWait(10); // bridge refresh, old rows are deleted later
        const auto badges = page->findChildren<QLabel*>("clashBadge");
        QCOMPARE(badges.size(), 1);
        QCOMPARE(badges[0]->text(), QString("Decline and Clash"));
        QVERIFY(badges[0]->isVisible());
        QCOMPARE(page->findChildren<QPushButton*>("resolveConflict").size(), 1);

        bool foundText = false;
        for (const QLabel* l : page->findChildren<QLabel*>("conflictText"))
            foundText = foundText || l->text().contains("Task Conflict Detected: Build API")
                                  && l->text().contains("Overloaded");
        QVERIFY(foundText);
    }

    void resolveHidesBadge()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const Id task = f.core.tasks().create(sampleTask("Build API"));
        f.core.tasks().assign(task, ana, "board");
        f.core.conflicts().decline(task, ana, "Overloaded");

        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->show();
        auto* resolve = page->findChild<QPushButton*>("resolveConflict");
        QVERIFY(resolve);
        QVERIFY(f.core.conflictQuery().hasOpenConflict(ana));
        QTest::mouseClick(resolve, Qt::LeftButton);
        QTest::qWait(10);
        QVERIFY(!f.core.conflictQuery().hasOpenConflict(ana));
        for (const QLabel* badge : page->findChildren<QLabel*>("clashBadge"))
            QVERIFY(!badge->isVisible());
        QVERIFY(page->findChildren<QPushButton*>("resolveConflict").isEmpty()
                || !page->findChild<QPushButton*>("resolveConflict")->isVisible());
    }

    void noSeedButton()
    {
        Fixture f;
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        for (const QPushButton* b : page->findChildren<QPushButton*>())
            QVERIFY(b->text() != "Seed Demo Team");
    }

    void emptyHintNamesRealButton()
    {
        Fixture f;
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        const auto* hint = page->findChild<QLabel*>("emptyLabel");
        QVERIFY(hint);
        QVERIFY(hint->text().contains("+ Add Member"));
        QVERIFY(!hint->text().contains("Add Team Member"));
    }
};

QTEST_MAIN(tst_TeamSkillsPage)
#include "tst_TeamSkillsPage.moc"
