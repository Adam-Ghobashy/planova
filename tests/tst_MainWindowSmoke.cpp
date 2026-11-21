#include "app/CoreBridge.h"
#include "app/MainWindow.h"
#include "app/Page.h"
#include "board/PhaseBoardPage.h"
#include "pins/PinsPage.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
};

QTEST_MAIN(tst_MainWindowSmoke)
#include "tst_MainWindowSmoke.moc"
