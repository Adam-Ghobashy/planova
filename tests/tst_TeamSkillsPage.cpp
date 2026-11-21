#include "app/CoreBridge.h"
#include "planova/PlanovaCore.h"
#include "team/TeamSkillsPage.h"
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
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
    void cleanup() { ThemeManager::instance().apply(ThemeMode::Light); }  // a failed test must not leave Dark behind
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

    // The members form one table: a single header row above the member rows, not a heading inside every card.
    void oneColumnHeaderRowAndTwoDecimalVelocity()
    {
        Fixture f;
        f.core.members().add("Ana", "Backend Developer", false);
        f.core.members().add("Ben", "Frontend Developer", false);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));

        QStringList titles;
        for (const QLabel* l : page->findChildren<QLabel*>("tableHeaderCell"))
            titles << l->text();
        QCOMPARE(titles, (QStringList{"Role / Teammate", "Technical Skills", "Project Understanding & Velocity",
                                      "Assignment / Clash"}));
        for (const QLabel* l : page->findChildren<QLabel*>())
            QVERIFY2(l->text() != "Assignment / Clash" || l->objectName() == "tableHeaderCell", "heading repeated in a row");

        const auto values = page->findChildren<QLabel*>("velocityValue");
        QCOMPARE(values.size(), 2);
        for (const QLabel* v : values)
            QVERIFY2(QRegularExpression("^\\d+\\.\\d{2}x$").match(v->text()).hasMatch(), qPrintable(v->text()));
    }

    // Header cells and the row columns start at the same x: it is one table.
    void headerColumnsAlignWithRows()
    {
        Fixture f;
        f.core.members().add("Ana", "Backend Developer", false);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        const auto headers = page->findChildren<QLabel*>("tableHeaderCell");
        QCOMPARE(headers.size(), 4);
        auto* understanding = page->findChild<QWidget*>("understandingColumn");
        auto* assignment = page->findChild<QWidget*>("assignmentColumn");
        QVERIFY(understanding && assignment);
        QCOMPARE(headers[2]->mapTo(page.get(), QPoint(0, 0)).x(), understanding->mapTo(page.get(), QPoint(0, 0)).x());
        QCOMPARE(headers[3]->mapTo(page.get(), QPoint(0, 0)).x(), assignment->mapTo(page.get(), QPoint(0, 0)).x());
    }

    void noSeedButton()
    {
        Fixture f;
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        for (const QPushButton* b : page->findChildren<QPushButton*>())
            QVERIFY(b->text() != "Seed Demo Team");
    }

    // Dark Accent equals dark TextPrimary. A focused Accent-filled button must keep a Surface fill, TextPrimary text
    // and a TextPrimary ring, also while hovered or pressed (":checked:hover", ":hover" and ":pressed" rules must not win).
    void focusRingStaysReadable_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<bool>("switchButton");  // false: the Resolve button of a conflict
        QTest::addColumn<QString>("state");  // focused, hovered or pressed
        for (bool dark : {false, true}) {
            for (bool sw : {true, false}) {
                for (const char* state : {"focused", "hovered", "pressed"}) {
                    const QByteArray row = QByteArray(dark ? "dark " : "light ")
                                           + (sw ? "checked switch" : "resolve button")
                                           + " " + state;
                    QTest::newRow(row.constData()) << dark << sw << QString::fromLatin1(state);
                }
            }
        }
    }
    void focusRingStaysReadable()
    {
        QFETCH(bool, dark);
        QFETCH(bool, switchButton);
        QFETCH(QString, state);
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const Id task = f.core.tasks().create(sampleTask("Build API"));
        f.core.tasks().assign(task, ana, "board");
        f.core.conflicts().decline(task, ana, "Overloaded");

        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1200, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        auto* button = page->findChild<QPushButton*>(switchButton ? "autoMatchSwitch" : "resolveConflict");
        QVERIFY(button);
        if (switchButton) button->setChecked(true);

        ThemeManager::instance().apply(dark ? ThemeMode::Dark : ThemeMode::Light);
        QCoreApplication::processEvents();
        if (state == "hovered") {
            // No real pointer is available here, so mark the button hovered. If the style does not react to that,
            // the hover cannot be rendered in this environment and the row is skipped.
            const QImage before = button->grab().toImage();
            button->setAttribute(Qt::WA_UnderMouse, true);
            if (button->grab().toImage() == before) QSKIP("hover cannot be rendered in this environment");
        }
        if (state == "pressed") button->setDown(true);
        button->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        if (state == "hovered") button->setAttribute(Qt::WA_UnderMouse, true);  // an event loop run may reset it
        const bool focused = button->hasFocus();
        const QImage image = button->grab().toImage();
        const QColor text = ThemeManager::instance().color(Token::TextPrimary);
        const QColor surface = ThemeManager::instance().color(Token::Surface);
        ThemeManager::instance().apply(ThemeMode::Light);
        if (!focused) QSKIP("window not active in this environment, focus state cannot be rendered");

        const int y = image.height() / 2;
        const QRgb ring = image.pixel(1, y);   // x = 0 is the antialiased rounded edge
        const QRgb fill = image.pixel(4, y);   // inside the 2 px ring, left of the text
        QCOMPARE(QColor(ring).rgb(), text.rgb());
        QVERIFY2(ring != fill, "focus ring cannot be told from the button fill");
        QVERIFY2(fill == surface.rgb(), "a focused button must keep the Surface fill (hover or pressed fill won)");
        bool textFound = false;
        for (int py = 4; py < image.height() - 4 && !textFound; ++py)
            for (int px = 4; px < image.width() - 4; ++px)
                if (image.pixel(px, py) == text.rgb()) { textFound = true; break; }
        QVERIFY2(textFound, "button text is not drawn in TextPrimary");
        QVERIFY2(fill != text.rgb(), "button text cannot be told from the fill");
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
