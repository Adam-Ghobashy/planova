#include "planova/slicer/WeightedScoringStrategy.h"
#include "slicer/AssignmentTableModel.h"
#include "slicer/ChaosSlicerPage.h"
#include "slicer/PizzaWidget.h"
#include "tests/fakes/FakeClock.h"
#include "tests/fakes/FakeEventStore.h"
#include "tests/fakes/FakeMemberQuery.h"
#include "tests/fakes/FakeTaskQuery.h"
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
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
        QVERIFY(pizza->isAnimating());  // already sliced, so new weights cut it again
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
