#include "planova/slicer/SlicerService.h"
#include "planova/slicer/WeightedScoringStrategy.h"
#include "tests/fakes/FakeClock.h"
#include "tests/fakes/FakeEventStore.h"
#include "tests/fakes/FakeMemberQuery.h"
#include "tests/fakes/FakeTaskQuery.h"
#include <QTest>
#include <stdexcept>

using namespace planova::slicer;

namespace {
struct Fixture {
    FakeTaskQuery tq;
    FakeMemberQuery mq;
    FakeEventStore store;
    EventLog log{store};
    FakeClock clock{1000};
    WeightedScoringStrategy strategy;
    SlicerService svc{tq, mq, strategy, log, clock};

    Fixture() {
        tq.addTask(TaskDto{"t1", "Task 1", SkillCategory::Backend, Priority::Medium, 3, 5,
                           TaskStatus::ToDo, std::nullopt, {}});
        mq.addMember(MemberDto{"1", "Alice", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
    }
};
}

class tst_SlicerService : public QObject {
    Q_OBJECT
private slots:
    void previewAppendsZeroEvents() {
        Fixture f;
        auto res = f.svc.preview(ScoringWeights{});
        QCOMPARE(int(res.size()), 1);
        QCOMPARE(int(f.store.events.size()), 0);
    }
    void invalidWeightsAreRejected() {
        Fixture f;
        QVERIFY_EXCEPTION_THROWN(f.svc.preview(ScoringWeights{10, 10, 10}), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(f.svc.apply({}, ScoringWeights{110, -10, 0}), std::invalid_argument);
        QCOMPARE(int(f.store.events.size()), 0);
    }
    void applyAppendsAssignedAndWeightsEvents() {
        Fixture f;
        ScoringWeights w;
        f.svc.apply(f.svc.preview(w), w);
        QCOMPARE(int(f.store.events.size()), 2);
        QCOMPARE(f.store.events[0].type, std::string("TaskAssigned"));
        QCOMPARE(f.store.events[0].author, std::string("chaos-slicer"));
        QCOMPARE(f.store.events[0].payload["by"].toString(), QString("chaos-slicer"));
        QCOMPARE(f.store.events[1].type, std::string("SlicerWeightsChanged"));
    }
    void reApplyIsIdempotent() {
        Fixture f;
        ScoringWeights w;
        auto preview = f.svc.preview(w);
        f.svc.apply(preview, w);
        int afterFirst = int(f.store.events.size());
        f.tq.setAssignee("t1", "1");  // the fake does not replay events, so update it by hand
        f.svc.apply(preview, w);
        QCOMPARE(int(f.store.events.size()), afterFirst);
    }
    void applyReturnsChangedCount() {
        Fixture f;
        ScoringWeights w;
        auto preview = f.svc.preview(w);
        QCOMPARE(f.svc.apply(preview, w), 1);
        f.tq.setAssignee("t1", "1");
        int afterFirst = int(f.store.events.size());
        QCOMPARE(f.svc.apply(preview, w), 0);
        QCOMPARE(int(f.store.events.size()), afterFirst);
    }
    void applySkipsUnknownMember() {
        Fixture f;
        Assignment bad;
        bad.taskId = "t1";
        bad.memberId = "nope";
        QCOMPARE(f.svc.apply({bad}, ScoringWeights{}), 0);
        for (const auto& e : f.store.events) QVERIFY(e.type != "TaskAssigned");
    }
    void weightsSurviveRestart() {
        Fixture f;
        ScoringWeights w;
        auto preview = f.svc.preview(w);
        f.svc.apply(preview, w);
        const int eventCount = int(f.store.events.size());

        // Simulated restart: new log and service over the same stored events.
        FakeTaskQuery tq2;
        FakeMemberQuery mq2;
        tq2.addTask(TaskDto{"t1", "Task 1", SkillCategory::Backend, Priority::Medium, 3, 5,
                            TaskStatus::ToDo, Id("1"), {}});
        mq2.addMember(MemberDto{"1", "Alice", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        EventLog log2{f.store};
        WeightedScoringStrategy strategy2;
        SlicerService svc2{tq2, mq2, strategy2, log2, f.clock};
        log2.rebuild();

        svc2.apply(preview, w);
        QCOMPARE(int(f.store.events.size()), eventCount);
    }
    void changedWeightsAppendOnlyWeightsEvent() {
        Fixture f;
        ScoringWeights w;
        auto preview = f.svc.preview(w);
        f.svc.apply(preview, w);
        f.tq.setAssignee("t1", "1");
        int afterFirst = int(f.store.events.size());
        f.svc.apply(preview, ScoringWeights{40, 40, 20});
        QCOMPARE(int(f.store.events.size()), afterFirst + 1);
        QCOMPARE(f.store.events.back().type, std::string("SlicerWeightsChanged"));
    }
    void portionsBoardVersusPreview() {
        Fixture f;
        QCOMPARE(f.svc.portions()[0].taskCount, 0);
        auto preview = f.svc.preview(ScoringWeights{});
        QCOMPARE(f.svc.portions(preview)[0].taskCount, 1);
    }
};

QTEST_APPLESS_MAIN(tst_SlicerService)
#include "tst_SlicerService.moc"
