#include "TestAppSetup.h"
#include "app/CoreBridge.h"
#include "app/DemoData.h"
#include "app/MainWindow.h"
#include "app/Page.h"
#include "board/PhaseBoardPage.h"
#include "board/TaskCardWidget.h"
#include "board/TaskColumnWidget.h"
#include "git/GitHubIntegrationPage.h"
#include "home/HomePage.h"
#include "pins/PinsPage.h"
#include "welcome/WelcomePage.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include <QApplication>
#include <cmath>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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
}

class tst_MainWindowSmoke : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(planova);
        installAppFonts();
        QCoreApplication::setOrganizationName("PlanovaTest");
    }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    // The start popup: "Continue an old project" opens the Projects tab, "Not now" changes nothing.
    void startDialogRoutesToProjectsTab()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.show();
        auto* pages = window.findChild<QStackedWidget*>();
        QVERIFY(pages);

        // exec() blocks, so the click comes from a timer that runs inside the dialog event loop.
        auto clickInDialog = [](const QString& text) {
            QTimer::singleShot(100, [text] {
                const auto buttons = QApplication::activeModalWidget()->findChildren<QPushButton*>();
                for (auto* b : buttons)
                    if (b->text() == text) { b->click(); return; }
                QApplication::activeModalWidget()->close();
            });
        };
        clickInDialog("Not now");
        window.showStartDialog();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Welcome));

        clickInDialog("Continue an old project");
        window.showStartDialog();
        QVERIFY(window.findChild<WelcomePage*>()->mode() == WelcomePage::Mode::Projects);

        clickInDialog("Start new project");
        window.showStartDialog();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Setup));
    }

    // A new project with no data must render every page without crashing, in both themes.
    void emptyProjectAllPagesBothThemes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("Empty", "team", "Agile", "idea"));
        CoreBridge bridge(core);

        for (ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
            ThemeManager::instance().apply(mode);
            MainWindow window(core, bridge);
            auto* pages = window.findChild<QStackedWidget*>();
            QVERIFY(pages);
            for (int i = 0; i < pages->count(); ++i) {
                pages->setCurrentIndex(i);
                QApplication::processEvents();
                QVERIFY2(!pages->currentWidget()->grab().isNull(), qPrintable(QString("page %1").arg(i)));
            }
        }
    }

    // Welcome -> Setup page -> skip back, then a full run that opens Home.
    void setupFlowFromWelcome()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        QVERIFY(pages);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Welcome));

        window.findChild<QPushButton*>("startNewProject")->click();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Setup));
        window.findChild<QLineEdit*>("projectNameEdit")->setText("Half typed");
        window.findChild<QPushButton*>("skipButton")->click();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Welcome));
        QVERIFY(core.projects().list().empty());

        // A second visit starts empty again.
        window.findChild<QPushButton*>("startNewProject")->click();
        QVERIFY(window.findChild<QLineEdit*>("projectNameEdit")->text().isEmpty());
        window.findChild<QLineEdit*>("projectNameEdit")->setText("Portal");
        for (int i = 0; i < 2; ++i)
            window.findChild<QPushButton*>("nextButton")->click();
        window.findChild<QPushButton*>("addMemberButton")->click();
        window.findChild<QLineEdit*>("memberNameEdit_0")->setText("Ana");
        for (int i = 0; i < 3; ++i)
            window.findChild<QPushButton*>("nextButton")->click();
        window.findChild<QPushButton*>("createButton")->click();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Home));
        QVERIFY(core.currentProject().has_value());
    }

    void pinsPageIsReal()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        QVERIFY(pages);
        QVERIFY(qobject_cast<planova::s5::PinsPage*>(pages->widget(static_cast<int>(Page::Pins))));
    }

    void boardShowsRealPinCount()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        const Id member = core.members().add("Ana Lopez", "Backend Developer", false);
        const Id withPins = core.tasks().create(sampleTask("Has pins"));
        core.tasks().create(sampleTask("No pins"));
        core.pins().create(PinCategory::HandoffNote, "one", {}, member, withPins);
        core.pins().create(PinCategory::FeedbackBlocker, "two", {}, member, withPins);

        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* board = window.findChild<PhaseBoardPage*>();
        QVERIFY(board);

        const auto counts = board->findChildren<QLabel*>("PinCount");
        QCOMPARE(counts.size(), 1);                       // hidden (not built) for the task without pins
        QCOMPARE(counts.first()->text(), QString("2"));
        QCOMPARE(board->findChildren<QFrame*>("PinChip").size(), 1);

        // A later pin shows up through the bridge (old cards are deleted later, so flush that first).
        core.pins().create(PinCategory::HandoffNote, "three", {}, member, withPins);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(board->findChildren<QLabel*>("PinCount").first()->text(), QString("3"));
    }

    // Every card on the demo board is wide enough for its own content: no clipped title, chip or arrow.
    void boardCardsNotClipped()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        demo::seedIfEmpty(core);
        CoreBridge bridge(core);
        for (ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark}) {
            ThemeManager::instance().apply(mode);
            MainWindow window(core, bridge);
            window.resize(1440, 900);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::Board));
            QApplication::processEvents();

            int checked = 0;
            for (const TaskCardWidget* card : window.findChildren<TaskCardWidget*>()) {
                if (!card->isVisible()) continue;
                ++checked;
                QVERIFY2(card->width() >= card->sizeHint().width(),
                         qPrintable(QString("card %1 is %2 px wide but needs %3")
                                        .arg(card->accessibleName()).arg(card->width()).arg(card->sizeHint().width())));
            }
            QVERIFY(checked >= 9);   // the demo board has nine tasks

            // The card must also fit inside its column's scroll area, not just be wide enough for itself:
            // a card whose content is wider than the column pushes the column's content past the edge (cut off, no scrollbar).
            for (const QScrollArea* area : window.findChildren<QScrollArea*>("ColumnScroll"))
                QVERIFY2(area->widget()->width() <= area->viewport()->width(),
                         qPrintable(QString("column content is %1 px but the viewport is %2 px")
                                        .arg(area->widget()->width()).arg(area->viewport()->width())));
        }
    }

    // Columns never get narrower than 260 px. The board scrolls sideways only when 4 columns do not fit.
    void boardColumnsKeepMinimumWidth()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        demo::seedIfEmpty(core);
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::Board));
        auto* scroll = window.findChild<QScrollArea*>("BoardScroll");
        QVERIFY(scroll);
        const auto columns = window.findChildren<TaskColumnWidget*>();
        QCOMPARE(columns.size(), 4);

        window.resize(1440, 900);   // the common desktop size: Done is fully visible, no sideways scroll
        QApplication::processEvents();
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        QVERIFY(columns.last()->mapTo(scroll->viewport(), QPoint(columns.last()->width(), 0)).x()
                <= scroll->viewport()->width());

        window.resize(2000, 900);   // wide: all four columns fit, nothing to scroll
        QApplication::processEvents();
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        for (const TaskColumnWidget* c : columns) {
            QVERIFY(c->width() >= 260);
            QVERIFY(c->mapTo(scroll->viewport(), QPoint(c->width(), 0)).x() <= scroll->viewport()->width());
        }

        window.resize(1000, 900);   // narrow: the columns keep 260 px and the board scrolls
        QApplication::processEvents();
        QVERIFY(scroll->horizontalScrollBar()->maximum() > 0);
        for (const TaskColumnWidget* c : columns)
            QVERIFY(c->width() >= 260);
        // At the minimum column width the cards still fit inside the column (nothing cut off).
        for (const QScrollArea* area : window.findChildren<QScrollArea*>("ColumnScroll"))
            QVERIFY2(area->widget()->width() <= area->viewport()->width(),
                     qPrintable(QString("column content is %1 px but the viewport is %2 px")
                                    .arg(area->widget()->width()).arg(area->viewport()->width())));
    }

    // A focused card keeps its size and the position of its content (the ring takes the place of padding).
    void focusedCardDoesNotShift()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        demo::seedIfEmpty(core);
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::Board));
        QApplication::processEvents();

        TaskCardWidget* card = nullptr;
        for (TaskCardWidget* c : window.findChildren<TaskCardWidget*>()) {
            if (c->isVisible() && c->focusPolicy() == Qt::StrongFocus) { card = c; break; }
        }
        QVERIFY(card);
        QLabel* title = card->findChild<QLabel*>("TaskTitle");
        QVERIFY(title);
        const QSize sizeBefore = card->size();
        const QPoint titleBefore = title->mapTo(card, QPoint(0, 0));
        card->setFocus(Qt::TabFocusReason);
        QApplication::processEvents();
        QVERIFY(card->hasFocus());
        QCOMPARE(card->size(), sizeBefore);
        QCOMPARE(title->mapTo(card, QPoint(0, 0)), titleBefore);
    }

    // The pins panel previews at most four pins, each with author and category text.
    void homeShowsAtMostFourPins()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        const Id member = core.members().add("Ana Lopez", "Backend Developer", false);
        for (int i = 0; i < 6; ++i)
            core.pins().create(PinCategory::HandoffNote, "pin " + std::to_string(i), {}, member, {});
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::Home));
        QApplication::processEvents();

        auto* home = window.findChild<planova::s6::HomePage*>();
        QVERIFY(home);
        int notes = 0;
        for (const QWidget* w : home->findChildren<QWidget*>("stickyNoteWidget")) {
            if (w->isVisible()) ++notes;
        }
        QCOMPARE(notes, 4);
        auto* link = home->findChild<QPushButton*>("homePinsLink");
        QVERIFY(link);
        QVERIFY(link->text().contains("Open Pins"));
        // Targets are at least 24 px high (the link and the "..." button).
        QVERIFY2(link->height() >= 24, qPrintable(QString::number(link->height())));
        QVERIFY2(home->findChild<QPushButton*>("homePinsMore")->height() >= 24, "homePinsMore too small");
    }

    // "Open Pins & Collab" goes to the Pins page.
    void homePinsLinkOpensPins()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        pages->setCurrentIndex(static_cast<int>(Page::Home));
        window.findChild<QPushButton*>("homePinsLink")->click();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Pins));
    }

    // ---- GitHub page ----
    // The page shows the repository form; no repository is pre-filled.
    void repoFieldEmptyByDefaultWithPlaceholder()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* repo = window.findChild<QLineEdit*>("repoInput");
        QVERIFY(repo);
        QVERIFY(repo->text().isEmpty());
        QCOMPARE(repo->placeholderText(), QString("https://github.com/<owner>/<repo>.git"));
    }

    void connectDisabledWhenEmpty()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::GitHub));
        QApplication::processEvents();

        auto* repo = window.findChild<QLineEdit*>("repoInput");
        auto* connect = window.findChild<QPushButton*>("connectButton");
        auto* hint = window.findChild<QLabel*>("connectHint");
        QVERIFY(repo && connect && hint);
        QVERIFY(!connect->isEnabled());
        QVERIFY(hint->isVisible());          // the reason is written next to the field

        repo->setText("   ");                // blanks are still empty
        QVERIFY(!connect->isEnabled());
        repo->setText("https://github.com/acme/portal.git");
        QVERIFY(connect->isEnabled());
        QVERIFY(!hint->isVisible());
        repo->clear();
        QVERIFY(!connect->isEnabled());
    }

    void githubShowsSimulatedNotice()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::GitHub));
        QApplication::processEvents();
        auto* notice = window.findChild<QLabel*>("simulatedNotice");
        QVERIFY(notice);
        QVERIFY(notice->isVisible());
        QCOMPARE(notice->text(), QString("Simulated data: commits and issues are generated locally until a real GitHub provider exists."));
    }

    // Typed but unsaved text must not be overwritten when the theme changes, connected or not.
    void unsavedInputSurvivesThemeToggle()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* repo = window.findChild<QLineEdit*>("repoInput");
        auto* branch = window.findChild<QLineEdit*>("branchInput");
        QVERIFY(repo && branch);

        repo->setText("https://github.com/acme/typed.git");
        branch->setText("draft");
        ThemeManager::instance().apply(ThemeMode::Dark);
        QApplication::processEvents();
        QCOMPARE(repo->text(), QString("https://github.com/acme/typed.git"));
        QCOMPARE(branch->text(), QString("draft"));

        core.git().connect("https://github.com/acme/saved.git", "main");   // the saved state differs from what is typed next
        QApplication::processEvents();
        repo->setText("https://github.com/acme/typed-again.git");
        ThemeManager::instance().apply(ThemeMode::Light);
        QApplication::processEvents();
        QCOMPARE(repo->text(), QString("https://github.com/acme/typed-again.git"));
    }

    // ---- GitHub page: lists, sync button and typed text ----
    // Commits left, issues right when the window is wide; stacked when it is not, so neither table is squeezed.
    void commitsAndIssuesSideBySide()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        demo::seedIfEmpty(core);
        core.git().connect("https://github.com/example/planova.git", "main");
        core.git().simulateSync(core.memberProjection().all());
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.show();
        window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::GitHub));
        auto* page = window.findChild<planova::git::GitHubIntegrationPage*>();
        QVERIFY(page);
        auto range = [&](QWidget* w, bool horizontal) {
            const QPoint topLeft = w->mapTo(page, QPoint(0, 0));
            return horizontal ? QPair<int, int>(topLeft.x(), topLeft.x() + w->width())
                              : QPair<int, int>(topLeft.y(), topLeft.y() + w->height());
        };
        auto overlaps = [](QPair<int, int> a, QPair<int, int> b) { return a.first < b.second && b.first < a.second; };

        window.resize(1440, 900);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);   // layout requests chain through the scroll area, one event loop turn each
        const auto tables = page->findChildren<QTableWidget*>();   // creation order: commits, issues
        QCOMPARE(tables.size(), 2);
        QTableWidget* commits = tables[0];
        QTableWidget* issues = tables[1];
        QVERIFY(commits && issues);
        QVERIFY(commits->isVisible() && issues->isVisible());
        QVERIFY2(!overlaps(range(commits, true), range(issues, true)), "wide: the tables overlap horizontally");
        QVERIFY2(overlaps(range(commits, false), range(issues, false)), "wide: the tables are not side by side");
        QVERIFY(range(commits, true).first < range(issues, true).first);   // commits on the left
        QVERIFY2(std::abs(commits->width() - issues->width()) <= 2, "wide: columns are not equal");

        window.resize(900, 900);   // the window stops at its minimum width (1157), so the page is about 930 px wide
        QTest::qWait(50);   // layout requests chain through the scroll area, one event loop turn each
        QVERIFY2(overlaps(range(commits, true), range(issues, true)), "narrow: the tables should share one column");
        QVERIFY2(!overlaps(range(commits, false), range(issues, false)), "narrow: the tables should be stacked");
        QVERIFY2(range(commits, true).second <= page->width(), "narrow: the commits table clips");

        window.resize(1440, 900);   // and back again
        QTest::qWait(50);   // layout requests chain through the scroll area, one event loop turn each
        QVERIFY(!overlaps(range(commits, true), range(issues, true)));
    }

    // Each list says what to do next while it is empty, and the connected wording differs from the unconnected one.
    void emptyListsSayWhatToDoNext()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::GitHub));
        QApplication::processEvents();
        auto* page = window.findChild<planova::git::GitHubIntegrationPage*>();
        const auto hints = page->findChildren<QLabel*>("EmptyHint");   // creation order: commits, issues
        QCOMPARE(hints.size(), 2);
        QLabel* commitsHint = hints[0];
        QLabel* issuesHint = hints[1];
        QVERIFY(commitsHint && issuesHint);
        QVERIFY(commitsHint->isVisible() && issuesHint->isVisible());
        QVERIFY(!page->findChildren<QTableWidget*>().first()->isVisible());
        QVERIFY(commitsHint->text().contains("Connect a repository"));
        QVERIFY(issuesHint->text().contains("Connect a repository"));

        core.git().connect("https://github.com/example/planova.git", "main");
        QApplication::processEvents();
        QVERIFY(commitsHint->isVisible());
        QVERIFY(!commitsHint->text().contains("Connect a repository"));
        QVERIFY(commitsHint->text().contains("Simulate Git Sync"));
    }

    // A disabled Simulate Git Sync says why, to the pointer and to a screen reader.
    void syncButtonExplainsWhyDisabled()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* page = window.findChild<planova::git::GitHubIntegrationPage*>();
        auto* repo = window.findChild<QLineEdit*>("repoInput");
        QPushButton* sync = nullptr;
        for (auto* b : page->findChildren<QPushButton*>("primaryButton"))
            if (b->text() == "Simulate Git Sync") sync = b;
        QVERIFY(sync && repo);
        QVERIFY(!sync->isEnabled());
        QVERIFY(sync->toolTip().contains("Connect a repository first"));
        QVERIFY(sync->accessibleDescription().contains("Connect a repository first"));
        QVERIFY(window.findChild<QLabel*>("connectHint")->text().contains("Simulate Git Sync"));

        repo->setText("https://github.com/example/planova.git");
        QVERIFY(sync->isEnabled());
        QVERIFY(!sync->toolTip().contains("Connect a repository first"));
        QVERIFY(!sync->accessibleDescription().contains("Connect a repository first"));
    }

    // Repository and branch text the user typed but has not connected must survive any other change in the core.
    void typedTextSurvivesUnrelatedCoreChange()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* repo = window.findChild<QLineEdit*>("repoInput");
        auto* branch = window.findChild<QLineEdit*>("branchInput");
        QVERIFY(repo && branch);

        core.git().connect("https://github.com/acme/saved.git", "main");
        QApplication::processEvents();
        QCOMPARE(repo->text(), QString("https://github.com/acme/saved.git"));   // untouched fields follow the saved state

        repo->setText("https://github.com/acme/typed.git");
        branch->setText("draft");
        core.members().add("Ana", "Backend Developer", false);                   // not a git change
        QApplication::processEvents();
        QCOMPARE(repo->text(), QString("https://github.com/acme/typed.git"));
        QCOMPARE(branch->text(), QString("draft"));

        core.git().simulateSync(core.memberProjection().all());                  // a git change, same repository
        QApplication::processEvents();
        QCOMPARE(repo->text(), QString("https://github.com/acme/typed.git"));
        QCOMPARE(branch->text(), QString("draft"));

        core.git().connect("https://github.com/acme/other.git", "dev");          // the saved repository itself changed
        QApplication::processEvents();
        QCOMPARE(repo->text(), QString("https://github.com/acme/other.git"));
        QCOMPARE(branch->text(), QString("dev"));
    }

    // The sidebar card shows the saved repository only; without one it says so and sync is off.
    void sidebarGitCardUsesSavedRepoOnly()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* title = window.findChild<QLabel*>("SidebarGitTitle");
        auto* sync = window.findChild<QPushButton*>("SidebarSyncButton");
        QVERIFY(title && sync);
        QCOMPARE(title->text(), QString("No repository connected"));
        QVERIFY(!sync->isEnabled());

        core.git().connect("https://github.com/acme/portal.git", "main");
        QApplication::processEvents();
        QVERIFY(title->text() != QString("No repository connected"));
        QVERIFY(sync->isEnabled());
    }

    // ---- Pins page ----
    // An empty board says so once, not once per column plus once for the page.
    void pinsEmptyStateShowsOneHint()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QStackedWidget*>()->setCurrentIndex(static_cast<int>(Page::Pins));
        QApplication::processEvents();
        auto* pins = window.findChild<planova::s5::PinsPage*>();
        QVERIFY(pins);
        auto visibleHints = [pins] {
            int n = 0;
            for (const QLabel* l : pins->findChildren<QLabel*>())
                if (l->isVisible() && (l->objectName() == "EmptyHint" || l->objectName() == "PinColumnEmpty")) ++n;
            return n;
        };
        QCOMPARE(visibleHints(), 1);

        // With one pin, the three empty columns say "No pins here" and the page hint is gone.
        const Id member = core.members().add("Ana Lopez", "Backend Developer", false);
        core.pins().create(PinCategory::HandoffNote, "one", {}, member, {});
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
        QCOMPARE(visibleHints(), 3);
    }

    // No page copy mentions SQLite or a database, and no first-person titles remain.
    void noPrototypeCopyOnBoardHomeTracking()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        demo::seedIfEmpty(core);
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        for (Page page : {Page::Board, Page::Home, Page::Tracking}) {
            QWidget* w = pages->widget(static_cast<int>(page));
            for (const QLabel* l : w->findChildren<QLabel*>()) {
                const QString text = l->text();
                QVERIFY2(!text.contains("SQLite", Qt::CaseInsensitive) && !text.contains("database", Qt::CaseInsensitive)
                             && !text.contains("Adam's") && !text.contains("Oliver's"),
                         qPrintable(text));
            }
        }
    }
};

QTEST_MAIN(tst_MainWindowSmoke)
#include "tst_MainWindowSmoke.moc"
