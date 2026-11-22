#include "TestAppSetup.h"
#include "app/CoreBridge.h"
#include "planova/PlanovaCore.h"
#include "team/TeamSkillsPage.h"
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>

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

    Fixture() { core.openProject(core.createProject("A", "team", "Agile", "idea")); }

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
    void initTestCase() { Q_INIT_RESOURCE(planova); installAppFonts(); }
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
        page->activateWindow();
        static_cast<void>(QTest::qWaitForWindowActive(page.get()));   // activation is asynchronous; without the wait the focus row was sometimes skipped
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

    // ---- Understanding slider: keyboard changes are saved too ----
    // The slider of one member, found by its accessible name.
    static QSlider* understandingSlider(QWidget* page, const QString& name)
    {
        for (QSlider* s : page->findChildren<QSlider*>())
            if (s->accessibleName() == QString("Project understanding of %1").arg(name) && s->isVisible())
                return s;
        return nullptr;
    }
    static int storedUnderstanding(Fixture& f, const Id& id)
    {
        for (const auto& m : f.core.memberProjection().all())
            if (m.id == id) return m.understanding;
        return -1;
    }

    // The new project must not receive events from the page: count the files of its event log.
    static int eventFileCount(Fixture& f, const Id& project)
    {
        const std::filesystem::path events = std::filesystem::path(f.dir.path().toStdString()) / project / "events";
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(events))
            if (entry.is_regular_file()) ++count;
        return count;
    }

    void keyboardChangeIsSaved()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const int before = storedUnderstanding(f, ana);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        QSlider* slider = understandingSlider(page.get(), "Ana");
        QVERIFY(slider);
        slider->setFocus(Qt::TabFocusReason);
        QTest::keyClick(slider, Qt::Key_Right);
        QTest::keyClick(slider, Qt::Key_Right);
        QCOMPARE(slider->value(), before + 2);
        QCOMPARE(storedUnderstanding(f, ana), before);   // nothing saved while the keys are still being pressed
        QTest::qWait(900);                               // past the debounce
        QCOMPARE(storedUnderstanding(f, ana), before + 2);
    }

    void focusSurvivesSave()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        f.core.members().add("Ben", "Frontend Developer", false);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        page->activateWindow();
        const bool active = QTest::qWaitForWindowActive(page.get());   // activation is asynchronous
        QSlider* old = understandingSlider(page.get(), "Ana");
        QVERIFY(old);
        old->setFocus(Qt::TabFocusReason);
        if (!active || !old->hasFocus()) QSKIP("window not active in this environment, focus cannot be checked");
        QTest::keyClick(old, Qt::Key_PageUp);
        QTest::qWait(900);
        QTest::qWait(50);   // the old card is deleted on the next event loop turn
        // A window that lost activation in the meantime (another window, the user) has no keyboard focus to check.
        if (!page->isActiveWindow()) QSKIP("window lost activation during the test, focus cannot be checked");
        QSlider* now = understandingSlider(page.get(), "Ana");
        QVERIFY(now);
        QVERIFY2(now != old, "the card was not rebuilt, the test proves nothing");
        QVERIFY2(now->hasFocus(), "the keyboard user lost focus after the save");
        QCOMPARE(now->value(), storedUnderstanding(f, ana));
        // and the user can keep going: a second save works and focus is still on the member's slider
        const int saved = storedUnderstanding(f, ana);
        QTest::keyClick(now, Qt::Key_Right);
        QTest::qWait(900);
        QTest::qWait(50);
        QCOMPARE(storedUnderstanding(f, ana), saved + 1);
        if (!page->isActiveWindow()) QSKIP("window lost activation during the test, focus cannot be checked");
        QSlider* last = understandingSlider(page.get(), "Ana");
        QVERIFY(last && last->hasFocus());
    }

    void mouseReleaseStillSaves()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        QSlider* slider = understandingSlider(page.get(), "Ana");
        QVERIFY(slider);
        const int before = slider->value();
        const int span = slider->width() - 20;
        const int startX = 10 + span * before / 100;
        const int endX = 10 + span * 90 / 100;
        const int y = slider->height() / 2;
        QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, QPoint(startX, y));
        QTest::mouseMove(slider, QPoint((startX + endX) / 2, y));
        QTest::mouseMove(slider, QPoint(endX, y));
        QVERIFY(slider->isSliderDown());
        QCOMPARE(storedUnderstanding(f, ana), before);   // not saved mid-drag
        const int dragged = slider->value();
        QVERIFY(dragged != before);
        QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier, QPoint(endX, y));
        QCOMPARE(storedUnderstanding(f, ana), dragged);  // saved at once, no debounce wait
    }

    void pendingValueIsFlushedWhenAnotherMemberChanges()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const Id ben = f.core.members().add("Ben", "Frontend Developer", false);
        const int anaBefore = storedUnderstanding(f, ana);
        const int benBefore = storedUnderstanding(f, ben);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        QSlider* anaSlider = understandingSlider(page.get(), "Ana");
        QSlider* benSlider = understandingSlider(page.get(), "Ben");
        QVERIFY(anaSlider && benSlider);
        QTest::keyClick(anaSlider, Qt::Key_Right);
        QTest::keyClick(anaSlider, Qt::Key_Right);
        QTest::keyClick(benSlider, Qt::Key_Right);   // within the debounce, for another member
        QCOMPARE(storedUnderstanding(f, ana), anaBefore + 2);   // Ana was saved when Ben changed
        QCOMPARE(storedUnderstanding(f, ben), benBefore);
        QTest::qWait(900);
        QCOMPARE(storedUnderstanding(f, ana), anaBefore + 2);
        QCOMPARE(storedUnderstanding(f, ben), benBefore + 1);
    }

    void pendingValueIsFlushedOnHide()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const int before = storedUnderstanding(f, ana);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        QSlider* slider = understandingSlider(page.get(), "Ana");
        QVERIFY(slider);
        QTest::keyClick(slider, Qt::Key_Right);
        QCOMPARE(storedUnderstanding(f, ana), before);
        page->hide();
        QCOMPARE(storedUnderstanding(f, ana), before + 1);
    }

    void pendingValueIsFlushedOnDestruction()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const int before = storedUnderstanding(f, ana);
        {
            std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
            page->resize(1440, 900);
            page->show();
            QVERIFY(QTest::qWaitForWindowExposed(page.get()));
            QSlider* slider = understandingSlider(page.get(), "Ana");
            QVERIFY(slider);
            QTest::keyClick(slider, Qt::Key_Right);
        }
        QCOMPARE(storedUnderstanding(f, ana), before + 1);
    }

    void pendingValueIsNotWrittenToAnotherProject()
    {
        Fixture f;
        const Id projectA = *f.core.currentProject();
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const int anaBefore = storedUnderstanding(f, ana);
        const Id projectB = f.core.createProject("B", "team", "Agile", "idea");
        f.core.openProject(projectB);
        const Id zed = f.core.members().add("Zed", "Backend Developer", false);
        const int zedBefore = storedUnderstanding(f, zed);
        const int filesBefore = eventFileCount(f, projectB);
        f.core.openProject(projectA);

        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        QSlider* slider = understandingSlider(page.get(), "Ana");
        QVERIFY(slider);
        QTest::keyClick(slider, Qt::Key_Right);   // pending, not saved yet

        f.core.openProject(projectB);
        QTest::qWait(900);                        // the debounce would have fired by now
        QCOMPARE(storedUnderstanding(f, zed), zedBefore);
        QCOMPARE(eventFileCount(f, projectB), filesBefore);   // nothing was appended to the new project
        QCOMPARE(f.core.memberProjection().all().size(), size_t(1));
        const auto* status = page->findChild<QLabel*>("statusLabel");
        QVERIFY(status && !status->isVisible());   // no "member not found" error

        f.core.openProject(projectA);
        QCOMPARE(storedUnderstanding(f, ana), anaBefore);   // the discarded value was never written
    }


    // ---- Switches follow the open project; the role editor and the keyboard focus survive a rebuild ----
    static QPushButton* ratingButton(QWidget* page, const QString& label, int rating, const QString& member)
    {
        const QString name = QString("%1 rating %2 of 5 for %3").arg(label).arg(rating).arg(member);
        for (QPushButton* b : page->findChildren<QPushButton*>("ratingButton"))
            if (b->accessibleName() == name) return b;
        return nullptr;
    }

    void togglesFollowProject_data()
    {
        QTest::addColumn<QString>("switchName");
        QTest::addColumn<QString>("settingKey");
        QTest::addColumn<QString>("title");
        QTest::newRow("auto match") << "autoMatchSwitch" << "autoMatch" << "Auto-Match Priority";
        QTest::newRow("smart assign") << "smartAssignSwitch" << "smartAssign" << "Smart Assign";
    }
    void togglesFollowProject()
    {
        QFETCH(QString, switchName);
        QFETCH(QString, settingKey);
        QFETCH(QString, title);
        Fixture f;
        const Id projectA = *f.core.currentProject();
        f.core.members().setSetting(settingKey.toStdString(), true);   // on in A
        const Id projectB = f.core.createProject("B", "team", "Agile", "idea");   // off in B

        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        auto* button = page->findChild<QPushButton*>(switchName);
        QVERIFY(button);
        QVERIFY(button->isChecked());
        QCOMPARE(button->text(), title + ": On");

        f.core.openProject(projectB);
        const int filesBefore = eventFileCount(f, projectB);
        page->refresh();
        QVERIFY2(!button->isChecked(), "the switch still shows the setting of the previous project");
        QCOMPARE(button->text(), title + ": Off");
        QCOMPARE(eventFileCount(f, projectB), filesBefore);   // showing a setting must not save one

        f.core.openProject(projectA);
        page->refresh();
        QVERIFY(button->isChecked());
        QCOMPARE(button->text(), title + ": On");
    }

    void roleEditorSurvivesSliderCommit()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const int before = storedUnderstanding(f, ana);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));

        auto* editButton = page->findChild<QPushButton*>("editRoleButton");
        QVERIFY(editButton);
        editButton->click();
        QPointer<QWidget> editor = page->findChild<QWidget*>("roleEditBox");
        QVERIFY(editor && editor->isVisible());

        QSlider* slider = understandingSlider(page.get(), "Ana");
        QVERIFY(slider);
        QTest::keyClick(slider, Qt::Key_Right);
        QTest::qWait(600);   // past the 500 ms debounce: the value is committed
        QTest::qWait(50);    // a rebuild would delete the old card on this turn
        QCOMPARE(storedUnderstanding(f, ana), before + 1);   // the change was saved
        QVERIFY2(editor, "the role editor was deleted by the understanding commit");
        QVERIFY(editor->isVisible());

        // Closing the editor catches up with the rebuild that was put off.
        auto* cancel = editor->findChild<QPushButton*>("cancelRoleButton");
        QVERIFY(cancel);
        cancel->click();
        QTest::qWait(50);
        QSlider* rebuilt = understandingSlider(page.get(), "Ana");
        QVERIFY(rebuilt);
        QVERIFY2(rebuilt != slider, "the cards were not rebuilt after the editor closed");
        QCOMPARE(rebuilt->value(), before + 1);
        QVERIFY(!editor);   // the old card is gone
    }

    // Two role editors are open and a rebuild waits. Closing one must not rebuild the cards (that would delete the
    // other editor); closing the last one does.
    void cancelKeepsTheOtherRoleEditor()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const Id ben = f.core.members().add("Ben", "Frontend Developer", false);
        const int before = storedUnderstanding(f, ana);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));

        for (const Id& id : {ana, ben}) {
            for (QPushButton* button : page->findChildren<QPushButton*>("editRoleButton")) {
                if (button->property("focusKey").toString() == QString::fromStdString(id) + ":editRole")
                    button->click();
            }
        }
        const QList<QWidget*> editors = page->findChildren<QWidget*>("roleEditBox");
        QCOMPARE(editors.size(), 2);
        QPointer<QWidget> anaEditor = editors[0];   // cards are built in member order
        QPointer<QWidget> benEditor = editors[1];
        QVERIFY(anaEditor->isVisible() && benEditor->isVisible());

        QSlider* slider = understandingSlider(page.get(), "Ana");
        QVERIFY(slider);
        QTest::keyClick(slider, Qt::Key_Right);
        QTest::qWait(600);   // past the debounce: committed, the rebuild waits for the editors
        QCOMPARE(storedUnderstanding(f, ana), before + 1);

        anaEditor->findChild<QPushButton*>("cancelRoleButton")->click();
        QTest::qWait(50);    // a rebuild would delete the old cards on this turn
        QVERIFY2(benEditor, "closing one role editor deleted the other one");
        QVERIFY(benEditor->isVisible());

        benEditor->findChild<QPushButton*>("cancelRoleButton")->click();
        QTest::qWait(50);
        QVERIFY2(!benEditor, "the put-off rebuild did not run after the last editor closed");
        QCOMPARE(understandingSlider(page.get(), "Ana")->value(), before + 1);
    }

    // The role editor stays open while the understanding is committed; saving the role afterwards must keep that value.
    void roleSaveKeepsCommittedUnderstanding()
    {
        Fixture f;
        const Id ana = f.core.members().add("Ana", "Backend Developer", false);
        const int before = storedUnderstanding(f, ana);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));

        page->findChild<QPushButton*>("editRoleButton")->click();
        QSlider* slider = understandingSlider(page.get(), "Ana");
        QVERIFY(slider);
        QTest::keyClick(slider, Qt::Key_Right);
        QTest::qWait(600);
        QCOMPARE(storedUnderstanding(f, ana), before + 1);

        QComboBox* editCombo = page->findChild<QWidget*>("roleEditBox")->findChild<QComboBox*>("roleCombo");
        QVERIFY(editCombo);
        editCombo->setCurrentText("Frontend Developer");
        page->findChild<QPushButton*>("saveRoleButton")->click();
        QTest::qWait(50);
        for (const auto& m : f.core.memberProjection().all()) {
            if (m.id != ana) continue;
            QCOMPARE(m.role, std::string("Frontend Developer"));
            QCOMPARE(m.understanding, before + 1);
        }
    }

    void skillButtonKeepsFocus()
    {
        Fixture f;
        f.core.members().add("Ana", "Backend Developer", false);
        std::unique_ptr<planova::team::TeamSkillsPage> page(f.makePage());
        page->resize(1440, 900);
        page->show();
        page->activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        const bool active = QTest::qWaitForWindowActive(page.get());

        QPushButton* old = ratingButton(page.get(), "Frontend", 4, "Ana");
        QVERIFY(old);
        old->setFocus(Qt::TabFocusReason);
        if (!active && !old->hasFocus()) QSKIP("window cannot be activated in this environment, focus cannot be checked");
        QVERIFY(old->hasFocus());
        old->click();   // rates and rebuilds the cards
        QTest::qWait(50);

        QPushButton* now = ratingButton(page.get(), "Frontend", 4, "Ana");
        QVERIFY(now);
        QVERIFY2(now != old, "the card was not rebuilt, the test proves nothing");
        if (!page->isActiveWindow()) QSKIP("window lost activation during the test, focus cannot be checked");
        QVERIFY2(now->hasFocus(), "the keyboard user lost focus after rating");
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(now));
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
