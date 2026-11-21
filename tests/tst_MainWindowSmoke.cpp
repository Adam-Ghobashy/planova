#include "app/CoreBridge.h"
#include "app/DemoData.h"
#include "app/MainWindow.h"
#include "app/Page.h"
#include "board/PhaseBoardPage.h"
#include "board/TaskCardWidget.h"
#include "board/TaskColumnWidget.h"
#include "home/HomePage.h"
#include "pins/PinsPage.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
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
}

class tst_MainWindowSmoke : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(planova);
        QCoreApplication::setOrganizationName("PlanovaTest");
    }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    // A new project with no data must render every page without crashing, in both themes.
    void emptyProjectAllPagesBothThemes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("Empty", "team", "Agile/Scrum", "idea"));
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
        core.openProject(core.createProject("P", "team", "Agile/Scrum", "idea"));
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
        core.openProject(core.createProject("P", "team", "Agile/Scrum", "idea"));
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
        core.openProject(core.createProject("P", "team", "Agile/Scrum", "idea"));
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
        }
    }

    // Columns never get narrower than 300 px. The board scrolls sideways only when 4 columns do not fit.
    void boardColumnsKeepMinimumWidth()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile/Scrum", "idea"));
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

        window.resize(2000, 900);   // wide: all four columns fit, nothing to scroll
        QApplication::processEvents();
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        for (const TaskColumnWidget* c : columns) {
            QVERIFY(c->width() >= 300);
            QVERIFY(c->mapTo(scroll->viewport(), QPoint(c->width(), 0)).x() <= scroll->viewport()->width());
        }

        window.resize(1000, 900);   // narrow: the columns keep 300 px and the board scrolls
        QApplication::processEvents();
        QVERIFY(scroll->horizontalScrollBar()->maximum() > 0);
        for (const TaskColumnWidget* c : columns)
            QVERIFY(c->width() >= 300);
    }

    // A focused card keeps its size and the position of its content (the ring takes the place of padding).
    void focusedCardDoesNotShift()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile/Scrum", "idea"));
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
        core.openProject(core.createProject("P", "team", "Agile/Scrum", "idea"));
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
        core.openProject(core.createProject("P", "team", "Agile/Scrum", "idea"));
        CoreBridge bridge(core);
        MainWindow window(core, bridge);
        auto* pages = window.findChild<QStackedWidget*>();
        pages->setCurrentIndex(static_cast<int>(Page::Home));
        window.findChild<QPushButton*>("homePinsLink")->click();
        QCOMPARE(pages->currentIndex(), static_cast<int>(Page::Pins));
    }

    // No page copy mentions SQLite or a database, and no first-person titles remain.
    void noPrototypeCopyOnBoardHomeTracking()
    {
        QTemporaryDir dir;
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("P", "team", "Agile/Scrum", "idea"));
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
