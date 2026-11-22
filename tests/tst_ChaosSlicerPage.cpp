#include "TestAppSetup.h"
#include "planova/slicer/WeightedScoringStrategy.h"
#include "slicer/AssignmentTableModel.h"
#include "slicer/ChaosSlicerPage.h"
#include "slicer/PizzaWidget.h"
#include "slicer/WeightSlidersPanel.h"
#include "tests/fakes/FakeClock.h"
#include "tests/fakes/FakeEventStore.h"
#include "tests/fakes/FakeMemberQuery.h"
#include "tests/fakes/FakeTaskQuery.h"
#include "theme/ThemeManager.h"
#include <QCoreApplication>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTableView>
#include <QTest>

using namespace planova::slicer;

namespace {
struct Board {
    FakeTaskQuery tasks;
    FakeMemberQuery members;
    FakeEventStore store;
    EventLog log{store};
    FakeClock clock{1000};
    WeightedScoringStrategy strategy;
    SlicerService service{tasks, members, strategy, log, clock};

    void addAlice() {
        members.addMember(MemberDto{"1", "Alice", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
    }
    void addTask(const char* id, std::optional<Id> assignee = std::nullopt) {
        tasks.addTask(TaskDto{id, id, SkillCategory::Backend, Priority::Medium, 3, 4, TaskStatus::ToDo,
                              std::move(assignee), {}});
    }
};
}

// Like the real FileEventStore when the disk write fails.
class FailingStore : public IEventStore {
public:
    void append(const Event&) override { throw std::runtime_error("Cannot write event file"); }
    std::vector<Event> readAll() const override { return {}; }
};

class tst_ChaosSlicerPage : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Q_INIT_RESOURCE(planova); installAppFonts(); }  // the focus tests need the real qss
    void applyEmitsChangedCountAndStatus() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        b.addTask("t2");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        QSignalSpy spy(&page, &ChaosSlicerPage::assignmentsApplied);

        page.findChild<QPushButton*>("applyButton")->click();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
        QCOMPARE(page.findChild<QLabel*>("statusLabel")->text(), QString("Applied 2 assignments"));
    }
    void applyWithNoChangeSaysZero() {
        Board b;
        b.addAlice();
        b.addTask("t1", Id("1"));  // already assigned to the member the slicer would pick
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        QSignalSpy spy(&page, &ChaosSlicerPage::assignmentsApplied);

        page.findChild<QPushButton*>("applyButton")->click();

        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(page.findChild<QLabel*>("statusLabel")->text(), QString("Applied 0 assignments"));
    }
    void recalculateSetsStatus() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        QVERIFY(page.findChild<QLabel*>("statusLabel")->text().isEmpty());

        page.findChild<QPushButton*>("recalculateButton")->click();

        QCOMPARE(page.findChild<QLabel*>("statusLabel")->text(), QString("Recalculated"));
    }
    void externalBoardChangeRefreshesTheTable() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* table = page.findChild<QTableView*>("assignmentsTable");
        QCOMPARE(table->model()->rowCount(), 1);

        b.addTask("t2");  // another part of the app adds a task
        page.refreshFromBoard();

        QCOMPARE(table->model()->rowCount(), 2);
    }
    void applyKeepsManualOverridesAndWritesThem() {
        Board b;
        b.addAlice();
        b.members.addMember(MemberDto{"2", "Bob", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        // Like the real app: every event on the bus makes the page refresh.
        b.log.bus().subscribe([&page](const std::string&) { page.refreshFromBoard(); });
        auto* table = page.findChild<QTableView*>("assignmentsTable");

        const QModelIndex row = table->model()->index(0, AssignmentTableModel::Assignee);
        const bool aliceHasIt = row.data(AssignmentTableModel::AssigneeNameRole).toString() == "Alice";
        const QString chosen = aliceHasIt ? "2" : "1";
        QVERIFY(table->model()->setData(table->model()->index(0, AssignmentTableModel::ActionOverride), chosen,
                                        Qt::EditRole));
        page.findChild<QPushButton*>("applyButton")->click();

        // The user's choice was written to the event log ...
        int assigned = 0;
        for (const auto& e : b.store.events) {
            if (e.type == "TaskAssigned") {
                ++assigned;
                QCOMPARE(e.payload["memberId"].toString(), chosen);
            }
        }
        QCOMPARE(assigned, 1);
        // ... and the refresh triggered by our own events did not overwrite it in the table.
        QCOMPARE(table->model()->index(0, AssignmentTableModel::ActionOverride).data(Qt::EditRole).toString(),
                 chosen);
    }
    void noHorizontalOverflowAtMinimumWidth() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        page.resize(900, 900);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        auto* area = page.findChild<QScrollArea*>("slicerScrollArea");
        QVERIFY(area);
        QVERIFY2(!area->horizontalScrollBar()->isVisible(), "the page scrolls sideways at 900 px");
    }
    void overrideSurvivesPinEvent() {
        Board b;
        b.addAlice();
        b.members.addMember(MemberDto{"2", "Bob", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* table = page.findChild<QTableView*>("assignmentsTable");
        const QModelIndex row = table->model()->index(0, AssignmentTableModel::Assignee);
        const bool aliceHasIt = row.data(AssignmentTableModel::AssigneeNameRole).toString() == "Alice";
        const QString chosen = aliceHasIt ? "2" : "1";
        QVERIFY(table->model()->setData(table->model()->index(0, AssignmentTableModel::ActionOverride), chosen,
                                        Qt::EditRole));

        page.refreshFromBoard("PinCreated");  // a pin has nothing to do with the slicer

        QCOMPARE(table->model()->index(0, AssignmentTableModel::ActionOverride).data(Qt::EditRole).toString(),
                 chosen);
        QVERIFY(page.findChild<QLabel*>("statusLabel")->text().isEmpty());
    }
    void taskEventStillRecalculates() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* table = page.findChild<QTableView*>("assignmentsTable");
        b.addTask("t2");

        page.refreshFromBoard("TaskCreated");

        QCOMPARE(table->model()->rowCount(), 2);
    }
    void dependencyEventRecalculates() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* table = page.findChild<QTableView*>("assignmentsTable");
        b.addTask("t2");

        page.refreshFromBoard("DependencyAdded");

        QCOMPARE(table->model()->rowCount(), 2);
    }
    void tableUpdatesAfterApply() {
        Board b;
        b.addAlice();
        b.members.addMember(MemberDto{"2", "Bob", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* table = page.findChild<QTableView*>("assignmentsTable");
        const QString pick = table->model()->index(0, AssignmentTableModel::Assignee)
                                 .data(AssignmentTableModel::AssigneeNameRole).toString();

        page.findChild<QPushButton*>("applyButton")->click();
        b.tasks.setAssignee("t1", pick == "Alice" ? "1" : "2");  // the board now holds the applied assignee
        page.refreshFromBoard("");

        QCOMPARE(table->model()->rowCount(), 1);
        QCOMPARE(table->model()->index(0, AssignmentTableModel::Assignee)
                     .data(AssignmentTableModel::AssigneeNameRole).toString(), pick);
    }
    void failedSaveIsShownNotThrown() {
        FakeTaskQuery tasks;
        FakeMemberQuery members;
        FailingStore store;
        EventLog log{store};
        FakeClock clock{1000};
        WeightedScoringStrategy strategy;
        SlicerService service{tasks, members, strategy, log, clock};
        members.addMember(MemberDto{"1", "Alice", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        tasks.addTask(TaskDto{"t1", "t1", SkillCategory::Backend, Priority::Medium, 3, 4, TaskStatus::ToDo,
                              std::nullopt, {}});
        ChaosSlicerPage page(service, tasks, members);
        QSignalSpy spy(&page, &ChaosSlicerPage::assignmentsApplied);

        page.findChild<QPushButton*>("applyButton")->click();  // must not throw out of the slot

        QCOMPARE(spy.count(), 0);
        QVERIFY(page.findChild<QLabel*>("statusLabel")->text().contains("Cannot write event file"));
    }
    void emptyBoardDoesNotCrash() {
        Board b;  // no members, no tasks
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        page.findChild<QPushButton*>("recalculateButton")->click();
        page.findChild<QPushButton*>("applyButton")->click();
        QCOMPARE(page.findChild<QLabel*>("statusLabel")->text(), QString("Applied 0 assignments"));
    }
    void pizzaStartsWholeAndRecalculateKeepsItWhole() {
        Board b;
        b.addAlice();
        b.members.addMember(MemberDto{"2", "Bob", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        b.addTask("t1");
        b.addTask("t2");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* pizza = page.findChild<PizzaWidget*>();
        QVERIFY(!pizza->isSliced());

        page.findChild<QPushButton*>("recalculateButton")->click();
        QVERIFY(!pizza->isSliced());
        QVERIFY(!pizza->isAnimating());
    }
    void sliceButtonStartsTheCutter() {
        Board b;
        b.addAlice();
        b.members.addMember(MemberDto{"2", "Bob", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        b.addTask("t1");
        b.addTask("t2");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* pizza = page.findChild<PizzaWidget*>();

        page.findChild<QPushButton*>("sliceButton")->click();
        QVERIFY(pizza->isAnimating());
        QTRY_VERIFY_WITH_TIMEOUT(pizza->isSliced(), 5000);

        page.findChild<QPushButton*>("recalculateButton")->click();
        QVERIFY(!pizza->isAnimating());  // the cutter only ever starts from the button
        QVERIFY(pizza->isSliced());      // same slices, so they stay cut
    }
    void changingWeightsNeverStartsTheCutter() {
        Board b;
        b.addAlice();
        b.members.addMember(MemberDto{"2", "Bob", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        b.addTask("t1");
        b.addTask("t2");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* pizza = page.findChild<PizzaWidget*>();
        auto sliders = page.findChild<WeightSlidersPanel*>()->findChildren<QSlider*>();
        QVERIFY(sliders.size() == 3);

        sliders[0]->setValue(80);  // a keyboard-style change commits straight away
        QVERIFY(!pizza->isAnimating());
        QVERIFY(!pizza->isSliced());  // still whole

        page.findChild<QPushButton*>("sliceButton")->click();
        QTRY_VERIFY_WITH_TIMEOUT(pizza->isSliced(), 5000);
        sliders[1]->setValue(10);
        QVERIFY(!pizza->isAnimating());
    }
    void overridingAnAssigneeMakesThePizzaWholeAgain() {
        Board b;
        b.addAlice();
        b.members.addMember(MemberDto{"2", "Bob", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        b.addTask("t1");
        b.addTask("t2");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* pizza = page.findChild<PizzaWidget*>();
        auto* table = page.findChild<QTableView*>("assignmentsTable");
        page.findChild<QPushButton*>("sliceButton")->click();
        QTRY_VERIFY_WITH_TIMEOUT(pizza->isSliced(), 5000);

        // Hand the first task to whoever does not have it, so the slices must change.
        const QModelIndex first = table->model()->index(0, AssignmentTableModel::Assignee);
        const bool aliceHasIt = first.data(AssignmentTableModel::AssigneeNameRole).toString() == "Alice";
        QVERIFY(table->model()->setData(table->model()->index(0, AssignmentTableModel::ActionOverride),
                                        aliceHasIt ? "2" : "1", Qt::EditRole));
        QVERIFY(!pizza->isSliced());
        QVERIFY(!pizza->isAnimating());
    }
    void pageStartsFromSavedWeights() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        b.service.saveWeights(ScoringWeights{60, 20, 20});
        const auto eventsBefore = b.store.events.size();

        ChaosSlicerPage page(b.service, b.tasks, b.members);

        QCOMPARE(page.findChild<WeightSlidersPanel*>()->weights().skill, 60);
        QCOMPARE(b.store.events.size(), eventsBefore);  // loading the weights writes nothing
    }
    void pageReloadsSavedWeightsWhenProjectOpens() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* sliders = page.findChild<WeightSlidersPanel*>();
        QCOMPARE(sliders->weights().skill, 50);

        b.service.saveWeights(ScoringWeights{20, 50, 30});  // like another project's saved weights
        page.refreshFromBoard("ProjectOpened");

        QCOMPARE(sliders->weights().skill, 20);
        QCOMPARE(sliders->weights().workload, 50);
    }
    void setWeightsEmitsNoSignal() {
        WeightSlidersPanel panel;
        QSignalSpy changed(&panel, &WeightSlidersPanel::weightsChanged);
        QSignalSpy committed(&panel, &WeightSlidersPanel::weightsCommitted);

        panel.setWeights(ScoringWeights{70, 20, 10});

        QCOMPARE(panel.weights().skill, 70);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(committed.count(), 0);
    }
    void pizzaSelectionHighlightsRows() {
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        auto* table = page.findChild<QTableView*>("assignmentsTable");
        QVERIFY(table != nullptr);
        QVERIFY(!table->model()->index(0, 0).data(AssignmentTableModel::HighlightedRole).toBool());

        emit page.findChild<PizzaWidget*>()->memberSelected("1");
        QVERIFY(table->model()->index(0, 0).data(AssignmentTableModel::HighlightedRole).toBool());

        emit page.findChild<PizzaWidget*>()->memberSelected("");
        QVERIFY(!table->model()->index(0, 0).data(AssignmentTableModel::HighlightedRole).toBool());
    }

    // Dark Accent equals dark TextPrimary, so a plain ring on an Accent fill is invisible there. A focused button
    // must draw the TextPrimary ring and a fill that differs from it, in both themes.
    void focusRingVisibleOnButtons_data() {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<QString>("button");
        for (bool dark : {false, true}) {
            for (const char* name : {"applyButton", "sliceButton", "recalculateButton"}) {
                const QByteArray row = QByteArray(dark ? "dark " : "light ") + name;
                QTest::newRow(row.constData()) << dark << QString::fromLatin1(name);
            }
        }
    }
    void focusRingVisibleOnButtons() {
        QFETCH(bool, dark);
        QFETCH(QString, button);
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        page.resize(1000, 900);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        auto* target = page.findChild<QPushButton*>(button);
        QVERIFY(target);

        ThemeManager::instance().apply(dark ? ThemeMode::Dark : ThemeMode::Light);
        QCoreApplication::processEvents();
        target->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        const bool focused = target->hasFocus();
        const QImage image = target->grab().toImage();
        const QColor ring = ThemeManager::instance().color(Token::TextPrimary);
        ThemeManager::instance().apply(ThemeMode::Light);
        if (!focused) QSKIP("window not active in this environment, focus state cannot be rendered");

        const int y = image.height() / 2;
        const QRgb ringPx = image.pixel(1, y);      // x = 0 is the antialiased rounded edge
        const QRgb fillPx = image.pixel(8, y);      // inside the padding, left of the text
        QCOMPARE(QColor(ringPx).rgb(), ring.rgb());
        QVERIFY2(ringPx != fillPx, "focus ring cannot be told from the button fill");
    }
    void focusRingVisibleOnSliderHandle_data() {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }
    void focusRingVisibleOnSliderHandle() {
        QFETCH(bool, dark);
        Board b;
        b.addAlice();
        b.addTask("t1");
        ChaosSlicerPage page(b.service, b.tasks, b.members);
        page.resize(1000, 900);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        auto* slider = page.findChild<QSlider*>();
        QVERIFY(slider);

        ThemeManager::instance().apply(dark ? ThemeMode::Dark : ThemeMode::Light);
        QCoreApplication::processEvents();
        page.findChild<QPushButton*>("applyButton")->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        const QImage plain = slider->grab().toImage();
        slider->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        const bool focused = slider->hasFocus();
        const QImage ringed = slider->grab().toImage();
        const QColor surface = ThemeManager::instance().color(Token::Surface);
        ThemeManager::instance().apply(ThemeMode::Light);
        if (!focused) QSKIP("window not active in this environment, focus state cannot be rendered");

        // The handle is where the two pictures differ.
        QRect changed;
        for (int y = 0; y < plain.height(); ++y)
            for (int x = 0; x < plain.width(); ++x)
                if (plain.pixel(x, y) != ringed.pixel(x, y)) changed |= QRect(x, y, 1, 1);
        QVERIFY2(!changed.isNull(), "focus does not change the slider at all");
        // The centre of the handle must not keep the Accent fill: it turns into a Surface disc inside the ring.
        QCOMPARE(ringed.pixelColor(changed.center()).rgb(), surface.rgb());
        QVERIFY(plain.pixel(changed.center()) != ringed.pixel(changed.center()));
    }
};

QTEST_MAIN(tst_ChaosSlicerPage)
#include "tst_ChaosSlicerPage.moc"
