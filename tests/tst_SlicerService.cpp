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

// A different strategy: gives every ToDo task to the last member. Proves SlicerService only
// depends on the IAssignmentStrategy interface.
class LastMemberStrategy : public IAssignmentStrategy {
public:
    std::vector<Assignment> assign(const std::vector<TaskDto>& tasks, const std::vector<MemberDto>& members,
                                   const ScoringWeights&) const override {
        std::vector<Assignment> result;
        for (const auto& t : tasks)
            if (t.status == TaskStatus::ToDo) result.push_back({t.id, members.back().id, 0, "Last member", false});
        return result;
    }
};

// Says that one member has an open decline conflict.
class FakeConflictQuery : public IConflictQuery {
public:
    explicit FakeConflictQuery(Id memberId) : member_(std::move(memberId)) {}
    bool hasOpenConflict(const Id& memberId) const override { return memberId == member_; }
private:
    Id member_;
};

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
    void applyWritesOneEventPerChangedTaskOnly() {
        FakeTaskQuery tq;
        FakeMemberQuery mq;
        FakeEventStore store;
        EventLog log{store};
        FakeClock clock{1000};
        WeightedScoringStrategy strategy;
        SlicerService svc{tq, mq, strategy, log, clock};
        mq.addMember(MemberDto{"1", "Alice", "Dev", false, {}, 100, 1.0});
        mq.addMember(MemberDto{"2", "Bob", "Dev", false, {}, 100, 1.0});
        auto todo = [](const char* id, std::optional<Id> assignee) {
            return TaskDto{id, id, SkillCategory::Backend, Priority::Medium, 3, 4, TaskStatus::ToDo,
                           std::move(assignee), {}};
        };
        tq.addTask(todo("t1", Id("1")));       // already with Alice: unchanged
        tq.addTask(todo("t2", std::nullopt));  // unassigned: changes
        tq.addTask(todo("t3", Id("1")));       // moves from Alice to Bob: changes

        const std::vector<Assignment> plan = {{"t1", "1", 80, "r", false}, {"t2", "2", 70, "r", false},
                                              {"t3", "2", 60, "r", false}};
        QCOMPARE(svc.apply(plan, ScoringWeights{}), 2);

        std::vector<std::pair<QString, QString>> assigned;  // (taskId, memberId)
        for (const auto& e : store.events)
            if (e.type == "TaskAssigned")
                assigned.push_back({e.payload["taskId"].toString(), e.payload["memberId"].toString()});
        QCOMPARE(int(assigned.size()), 2);
        QCOMPARE(assigned[0], (std::pair<QString, QString>{"t2", "2"}));
        QCOMPARE(assigned[1], (std::pair<QString, QString>{"t3", "2"}));
    }
    void strategyIsSwappable() {
        FakeTaskQuery tq;
        FakeMemberQuery mq;
        FakeEventStore store;
        EventLog log{store};
        FakeClock clock{1000};
        LastMemberStrategy strategy;
        SlicerService svc{tq, mq, strategy, log, clock};
        tq.addTask(TaskDto{"t1", "Task 1", SkillCategory::Backend, Priority::Medium, 3, 5,
                           TaskStatus::ToDo, std::nullopt, {}});
        mq.addMember(MemberDto{"1", "Alice", "Dev", false, {}, 100, 1.0});
        mq.addMember(MemberDto{"2", "Bob", "Dev", false, {}, 100, 1.0});

        ScoringWeights w;
        QCOMPARE(svc.apply(svc.preview(w), w), 1);
        QCOMPARE(store.events[0].payload["memberId"].toString(), QString("2"));
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
    void savedWeightsDefault() {
        Fixture f;
        const ScoringWeights w = f.svc.savedWeights();
        QCOMPARE(w.skill, 50);
        QCOMPARE(w.workload, 30);
        QCOMPARE(w.priority, 20);
    }
    void saveWeightsAppendsOnce() {
        Fixture f;
        f.svc.saveWeights(ScoringWeights{60, 20, 20});
        f.svc.saveWeights(ScoringWeights{60, 20, 20});  // unchanged: nothing more is written
        QCOMPARE(int(f.store.events.size()), 1);
        QCOMPARE(f.store.events[0].type, std::string("SlicerWeightsChanged"));
        QCOMPARE(f.svc.savedWeights().skill, 60);
    }
    void saveInvalidRejected() {
        Fixture f;
        QVERIFY_EXCEPTION_THROWN(f.svc.saveWeights(ScoringWeights{60, 30, 20}), std::invalid_argument);
        QCOMPARE(int(f.store.events.size()), 0);
    }
    void previewSkipsMembersWithOpenConflict() {
        FakeTaskQuery tq;
        FakeMemberQuery mq;
        FakeEventStore store;
        EventLog log{store};
        FakeClock clock{1000};
        WeightedScoringStrategy strategy;
        FakeConflictQuery conflicts{"1"};
        SlicerService svc{tq, mq, strategy, log, clock, &conflicts};
        tq.addTask(TaskDto{"t1", "Task 1", SkillCategory::Backend, Priority::Medium, 3, 5,
                           TaskStatus::ToDo, std::nullopt, {}});
        mq.addMember(MemberDto{"1", "Alice", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});
        mq.addMember(MemberDto{"2", "Bob", "Dev", false, {{SkillCategory::Backend, 2}}, 100, 1.0});

        const auto preview = svc.preview(ScoringWeights{});
        QCOMPARE(int(preview.size()), 1);
        QCOMPARE(preview[0].memberId, Id("2"));  // Alice is the better match but has an open conflict
    }
    void previewUsesEveryoneWhenAllHaveConflicts() {
        FakeTaskQuery tq;
        FakeMemberQuery mq;
        FakeEventStore store;
        EventLog log{store};
        FakeClock clock{1000};
        WeightedScoringStrategy strategy;
        FakeConflictQuery conflicts{"1"};
        SlicerService svc{tq, mq, strategy, log, clock, &conflicts};
        tq.addTask(TaskDto{"t1", "Task 1", SkillCategory::Backend, Priority::Medium, 3, 5,
                           TaskStatus::ToDo, std::nullopt, {}});
        mq.addMember(MemberDto{"1", "Alice", "Dev", false, {{SkillCategory::Backend, 5}}, 100, 1.0});

        const auto preview = svc.preview(ScoringWeights{});
        QCOMPARE(int(preview.size()), 1);  // a one-person team still gets its tasks
        QCOMPARE(preview[0].memberId, Id("1"));
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
