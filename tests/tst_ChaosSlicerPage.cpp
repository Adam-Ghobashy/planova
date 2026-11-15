#include "planova/slicer/WeightedScoringStrategy.h"
#include "slicer/AssignmentTableModel.h"
#include "slicer/ChaosSlicerPage.h"
#include "slicer/PizzaWidget.h"
#include "slicer/WeightSlidersPanel.h"
#include "tests/fakes/FakeClock.h"
#include "tests/fakes/FakeEventStore.h"
#include "tests/fakes/FakeMemberQuery.h"
#include "tests/fakes/FakeTaskQuery.h"
#include <QLabel>
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
};

QTEST_MAIN(tst_ChaosSlicerPage)
#include "tst_ChaosSlicerPage.moc"
