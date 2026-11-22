#include "TestAppSetup.h"
#include "app/CoreBridge.h"
#include "app/DemoData.h"
#include "app/MainWindow.h"
#include "app/Page.h"
#include "app/Sidebar.h"
#include "board/PhaseBoardPage.h"
#include "board/TaskCardWidget.h"
#include "board/TaskColumnWidget.h"
#include "git/GitHubIntegrationPage.h"
#include "home/HomePage.h"
#include "widgets/StatCardWidget.h"
#include "pins/PinsPage.h"
#include "welcome/ProjectCardWidget.h"
#include "welcome/WelcomePage.h"
#include "widgets/ModalDialog.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QToolButton>
#include <cmath>
#include <filesystem>
#include <functional>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextDocument>
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

// Shortcuts only fire in the active window.
void showActive(QWidget& window)
{
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    static_cast<void>(QTest::qWaitForWindowActive(&window));   // activation is asynchronous
}

// Modal dialogs block, so the test code that fills one in runs from a timer inside the dialog's event loop.
// `drive` gets the top visible ModalDialog; if there is none, the test fails when the timer fires.
void driveTopDialog(std::function<void(ModalDialog*)> drive, int delayMs = 50)
{
    QTimer::singleShot(delayMs, [drive] {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            auto* dialog = qobject_cast<ModalDialog*>(widget);
            if (dialog && dialog->isVisible()) {
                drive(dialog);
                return;
            }
        }
        QFAIL("no modal dialog is open");
    });
}

// Presses a key combination in the window. If the environment cannot activate the window, the shortcut that
// owns the key is triggered directly, so the test still checks what the shortcut does.
void pressShortcut(MainWindow& window, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    if (window.isActiveWindow()) {
        QTest::keyClick(&window, key, modifiers);
        return;
    }
    const QKeySequence wanted(QKeyCombination(modifiers, key));
    for (QShortcut* shortcut : window.findChildren<QShortcut*>()) {
        if (shortcut->key() == wanted) {
            emit shortcut->activated();
            return;
        }
    }
    QFAIL("no shortcut for this key");
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
    void init()
    {
        ThemeManager::instance().apply(ThemeMode::Light);
        QSettings().remove("ui/lastPage");   // the test organisation name keeps this away from the real settings
    }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    // ---- Keyboard shortcuts ----
    // Ctrl+0 is the Projects Hub, Ctrl+1..7 the sidebar pages in order, Ctrl+N a new project, Ctrl+Shift+L the theme.
    void shortcutsSwitchPages()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        showActive(window);

        pressShortcut(window, Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Board));
        const auto order = Sidebar::navOrder();
        for (int i = 0; i < order.size(); ++i) {
            pressShortcut(window, static_cast<Qt::Key>(Qt::Key_1 + i), Qt::ControlModifier);
            QCOMPARE(pages->currentIndex(), static_cast<int>(order[i]));
        }
        pressShortcut(window, Qt::Key_0, Qt::ControlModifier);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));
    }

    void shortcutThemeToggle()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        showActive(window);

        pressShortcut(window, Qt::Key_L, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(ThemeManager::instance().mode(), ThemeMode::Dark);
        pressShortcut(window, Qt::Key_L, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(ThemeManager::instance().mode(), ThemeMode::Light);
    }

    // Without an open project the page shortcuts do nothing (the sidebar items are disabled too).
    void pageShortcutsNeedAnOpenProject()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        showActive(window);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));

        pressShortcut(window, Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));
    }

    // Ctrl+N opens an empty Setup page, but never wipes a Setup page the user is already filling in.
    void newProjectShortcutKeepsHalfFilledSetup()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        showActive(window);

        pressShortcut(window, Qt::Key_N, Qt::ControlModifier);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Setup));
        window.findChild<QLineEdit*>("projectNameEdit")->setText("Half typed");
        pressShortcut(window, Qt::Key_N, Qt::ControlModifier);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Setup));
        QCOMPARE(window.findChild<QLineEdit*>("projectNameEdit")->text(), QString("Half typed"));

        // From another page the visit starts empty again.
        pressShortcut(window, Qt::Key_0, Qt::ControlModifier);
        pressShortcut(window, Qt::Key_N, Qt::ControlModifier);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Setup));
        QVERIFY(window.findChild<QLineEdit*>("projectNameEdit")->text().isEmpty());
    }

    // ---- Remember the last page ----
    // The page the user left a project on is where the next start opens, if a project is open then.
    void startsOnTheLastPage()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        {
            MainWindow window(core, bridge);
            showActive(window);
            pressShortcut(window, Qt::Key_2, Qt::ControlModifier);
            QCOMPARE(window.findChild<QStackedWidget*>()->currentIndex(), static_cast<int>(Page::Board));
        }
        MainWindow next(core, bridge);
        QCOMPARE(next.findChild<QStackedWidget*>()->currentIndex(), static_cast<int>(Page::Board));
    }

    // Setup and the Hub are never saved, so leaving through them keeps the last project page.
    void hubAndSetupAreNotRemembered()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        {
            MainWindow window(core, bridge);
            showActive(window);
            pressShortcut(window, Qt::Key_5, Qt::ControlModifier);   // Team
            pressShortcut(window, Qt::Key_0, Qt::ControlModifier);   // Hub
            pressShortcut(window, Qt::Key_N, Qt::ControlModifier);   // Setup
        }
        QCOMPARE(QSettings().value("ui/lastPage").toInt(), static_cast<int>(Page::Team));
    }

    void startsOnHubWithoutProjectEvenWithASavedPage()
    {
        QSettings().setValue("ui/lastPage", static_cast<int>(Page::Board));
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        QCOMPARE(window.findChild<QStackedWidget*>()->currentIndex(), static_cast<int>(Page::Hub));
    }

    // Setup, Hub, numbers outside the page list and a value that is not a number all fall back to Home.
    void badSavedPageFallsBackToHome()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        CoreBridge bridge(core);
        for (const QVariant& saved : {QVariant(static_cast<int>(Page::Setup)), QVariant(static_cast<int>(Page::Hub)),
                                      QVariant(99), QVariant(-1), QVariant("garbage")}) {
            QSettings().setValue("ui/lastPage", saved);
            MainWindow window(core, bridge);
            QVERIFY2(window.findChild<QStackedWidget*>()->currentIndex() == static_cast<int>(Page::Home),
                     qPrintable(saved.toString()));
        }
    }

    // The start popup: pick a project opens its Home, Start new project opens Setup, "Not now" changes nothing.
    void startDialogRoutesChoices()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.createProject("Alpha", "Team A", "Agile", "");
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
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));   // no project open: the hub is behind the popup
        clickInDialog("Not now");
        window.showStartDialog();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));

        clickInDialog(QString("Alpha  ·  Team A"));
        window.showStartDialog();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Home));
        QVERIFY(core.currentProject().has_value());

        clickInDialog("Start new project");
        window.showStartDialog();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Setup));
    }

    // ---- Edit and delete from the Home menu ----
    void editOpenProjectUpdatesEverywhere()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("Alpha", "Team A", "Agile", ""));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.show();
        auto* badge = window.findChild<QLabel*>("homeBadge");
        QVERIFY(badge);
        QVERIFY(badge->isVisibleTo(&window));   // Agile shows the Active Sprint badge

        auto* more = window.findChild<QToolButton*>("homeMoreButton");
        QVERIFY(more);
        QCOMPARE(more->accessibleName(), QString("More actions for Alpha"));
        driveTopDialog([](ModalDialog* dialog) {
            dialog->findChild<QLineEdit*>("editProjectName")->setText("Orbit");
            dialog->findChild<QComboBox*>()->setCurrentText("Waterfall");
            dialog->findChild<QPushButton*>("modalPrimaryButton")->click();
        });
        more->menu()->actions().at(0)->trigger();

        QCOMPARE(window.findChild<QLabel*>("homeTitle")->text(), QString("Orbit"));
        QVERIFY(!badge->isVisibleTo(&window));
        QVERIFY(window.findChild<QLabel*>("projectSwitcherName")->text().contains("Orbit"));
        QVERIFY(window.statusBar()->currentMessage().startsWith("Orbit"));
        QCOMPARE(more->accessibleName(), QString("More actions for Orbit"));
    }

    void deleteOpenProjectReturnsToHub()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("Alpha", "Team A", "Agile", ""));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        showActive(window);
        auto* pages = window.findChild<QStackedWidget*>();
        pressShortcut(window, Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Board));
        QVERIFY(QSettings().contains("ui/lastPage"));

        // The Hub card of the open project has the same menu.
        auto* more = window.findChild<WelcomePage*>()->findChild<QToolButton*>("projectMoreButton");
        QVERIFY(more);
        driveTopDialog([](ModalDialog* dialog) {
            dialog->findChild<QLineEdit*>("deleteProjectConfirm")->setText("Alpha");
            dialog->findChild<QPushButton*>("modalDangerButton")->click();
        });
        more->menu()->actions().at(1)->trigger();

        QVERIFY(!core.currentProject().has_value());
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));
        pressShortcut(window, Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));
        const auto navItems = window.findChild<Sidebar*>()->findChildren<QPushButton*>("navItem");
        QVERIFY(!navItems.isEmpty());
        for (auto* item : navItems)
            QVERIFY(!item->isEnabled());
        QVERIFY(!QSettings().contains("ui/lastPage"));
        // Cards are replaced with deleteLater(), so let the old ones go before looking at the Hub.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (auto* card : window.findChild<WelcomePage*>()->findChildren<ProjectCardWidget*>())
            QVERIFY(card->projectName() != "Alpha");
    }

    void deleteReportsLeftoverFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const std::filesystem::path workspace = dir.path().toStdString();
        PlanovaCore core(workspace);
        const Id id = core.createProject("Alpha", "Team A", "Agile", "");
        core.openProject(id);
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        window.show();

        const std::filesystem::path folder = workspace / id;
        QFile held(QString::fromStdString((folder / "events" / "held.tmp").string()));
        QVERIFY(held.open(QIODevice::WriteOnly));

        auto* more = window.findChild<QToolButton*>("homeMoreButton");
        QVERIFY(more);
        QString infoText;
        bool sawInfo = false;
        // First dialog: the confirmation. After it closes, the info dialog about the leftover folder follows.
        driveTopDialog([&](ModalDialog* dialog) {
            dialog->findChild<QLineEdit*>("deleteProjectConfirm")->setText("Alpha");
            dialog->findChild<QPushButton*>("modalDangerButton")->click();
            driveTopDialog([&](ModalDialog* info) {
                sawInfo = true;
                for (auto* label : info->findChildren<QLabel*>())
                    infoText += label->text() + "\n";
                info->accept();
            }, 100);
        });
        more->menu()->actions().at(1)->trigger();
        held.close();
        if (!sawInfo)
            QSKIP("folder removal did not fail on this platform");
        QVERIFY2(infoText.contains(QString::fromStdString(folder.string())) ||
                 infoText.contains(QDir::toNativeSeparators(QString::fromStdString(folder.string()))),
                 qPrintable(infoText));
        QVERIFY(!core.currentProject().has_value());
        QVERIFY(core.projects().list().empty());
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

    // Projects Hub -> Setup page -> skip back, then a full run that opens Home.
    void setupFlowFromHub()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        QVERIFY(pages);
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));

        window.findChild<QPushButton*>("createProject")->click();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Setup));
        window.findChild<QLineEdit*>("projectNameEdit")->setText("Half typed");
        window.findChild<QPushButton*>("skipButton")->click();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Hub));
        QVERIFY(core.projects().list().empty());

        // A second visit starts empty again.
        window.findChild<QPushButton*>("createProject")->click();
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

    // The In Progress number leaves out tasks in Review, so a caption under it counts them.
    void homeInProgressCardCountsReview()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        const Id task = core.tasks().create(sampleTask("Build API"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* home = window.findChild<planova::s6::HomePage*>();
        QVERIFY(home);
        auto inProgressCaption = [home]() -> QLabel* {
            for (StatCardWidget* card : home->findChildren<StatCardWidget*>()) {
                auto* title = card->findChild<QLabel*>("statCardTitle");
                if (title && title->text() == "In Progress") return card->findChild<QLabel*>("statCardCaption");
            }
            return nullptr;
        };
        QLabel* caption = inProgressCaption();
        QVERIFY2(!caption || caption->isHidden(), "no task in review: no caption");

        core.tasks().move(task, TaskStatus::InProgress);
        core.tasks().move(task, TaskStatus::Review);
        caption = inProgressCaption();
        QVERIFY(caption);
        QVERIFY(!caption->isHidden());
        QCOMPARE(caption->text(), QString("+1 in review"));
    }

    // A pin's text is the user's, so markup in it shows as typed in the Home tooltip.
    void homePinTooltipShowsMarkupLiterally()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile", "idea"));
        const Id member = core.members().add("Ana Lopez", "Backend Developer", false);
        core.pins().create(PinCategory::HandoffNote, "<b>bold</b> & co", {}, member, {});
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* home = window.findChild<planova::s6::HomePage*>();
        QVERIFY(home);
        QWidget* note = home->findChild<QWidget*>("stickyNoteWidget");
        QVERIFY(note);
        const QString tip = note->toolTip();
        QVERIFY2(tip.contains("&lt;b&gt;bold&lt;/b&gt; &amp; co"), qPrintable(tip));
        QVERIFY2(Qt::mightBeRichText(tip), "an escaped tip must be shown as rich text, or the entities show");
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
        auto* title = window.findChild<QLabel*>("SidebarGitText");
        auto* sync = window.findChild<QPushButton*>("SidebarSyncButton");
        QVERIFY(title && sync);
        QCOMPARE(title->text(), QString("No repository"));
        QVERIFY(!sync->isEnabled());
        QCOMPARE(sync->toolTip(), QString("Connect a repository first (GitHub page)"));

        core.git().connect("https://github.com/acme/portal.git", "main");
        QApplication::processEvents();
        QVERIFY(title->text() != QString("No repository"));
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

    // The status bar names the project and the last saved change in plain words.
    void statusBarUsesPlainWording()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        QCOMPARE(window.statusBar()->currentMessage(), QString("No project open  •  Git: Not connected"));

        core.openProject(core.createProject("Alpha", "team", "Agile", "idea"));
        emit bridge.changed(QString("TaskMoved"));
        QCOMPARE(window.statusBar()->currentMessage(),
                 QString("Alpha  •  Git: Not connected  •  Saved: Task moved"));
        const QString message = window.statusBar()->currentMessage();
        QVERIFY(!message.contains("C++17") && !message.contains("Workspace") && !message.contains("Team Members"));
    }

    // "Active Sprint" belongs to sprint-based models only.
    void sprintBadgeOnlyForAgileAndScrum()
    {
        for (const char* model : {"Agile", "Scrum", "Waterfall", "Spiral"}) {
            QTemporaryDir dir;
            PlanovaCore core(dir.path().toStdString());
            core.openProject(core.createProject("P", "team", model, "idea"));
            CoreBridge bridge(core);
            MainWindow window(core, bridge);
            auto* badge = window.findChild<QLabel*>("homeBadge");
            QVERIFY(badge);
            const bool sprintModel = QString(model) == "Agile" || QString(model) == "Scrum";
            QCOMPARE(!badge->isHidden(), sprintModel);
        }
    }
};

QTEST_MAIN(tst_MainWindowSmoke)
#include "tst_MainWindowSmoke.moc"
