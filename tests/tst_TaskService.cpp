#include <QtTest>
#include <algorithm>
#include <stdexcept>
#include "planova/common/Clock.h"
#include "planova/common/IEventStore.h"
#include "planova/store/EventLog.h"
#include "planova/task/TaskProjection.h"
#include "planova/task/TaskService.h"
namespace {
struct MemStore : IEventStore {
    std::vector<Event> evs;
    void append(const Event& e) override { evs.push_back(e); }
    std::vector<Event> readAll() const override {
        auto v = evs;
        std::sort(v.begin(), v.end(), [](const Event& a, const Event& b) {
            return a.ts != b.ts ? a.ts < b.ts : a.id < b.id;
        });
        return v;
    }
};
struct TickClock : IClock {                       // every call moves forward -> replay order is stable
    mutable int64_t t = 1000;
    int64_t nowMs() const override { return ++t; }
};
NewTask nt(const std::string& title, std::vector<Id> deps = {}, int hours = 2) {
    return NewTask{title, SkillCategory::Backend, Priority::Medium, 3, hours, std::move(deps)};
}
}
class TstTaskService : public QObject {
    Q_OBJECT
    MemStore store;
    TickClock clock;
    EventLog* log = nullptr;
    TaskProjection* proj = nullptr;
    TaskService* svc = nullptr;
private slots:
    void init() {
        store.evs.clear();
        log = new EventLog(store);
        proj = new TaskProjection;
        log->add(*proj);
        svc = new TaskService(*log, *proj, clock);
    }
    void cleanup() { delete svc; delete proj; delete log; }
    void createAppendsOneEventAndShowsTask() {
        Id id = svc->create(nt("A"));
        QCOMPARE(store.evs.size(), size_t(1));
        auto t = proj->byId(id);
        QVERIFY(t.has_value());
        QVERIFY(t->status == TaskStatus::ToDo);
        QCOMPARE(t->title, std::string("A"));
    }
    void createTwiceWithSameInputGivesDifferentIds() {
        Id a = svc->create(nt("Same"));
        Id b = svc->create(nt("Same"));
        QVERIFY(a != b);
        QCOMPARE(proj->all().size(), size_t(2));
    }
    void invalidCreateThrowsAndAppendsNothing() {
        NewTask bad = nt("A");
        bad.complexity = 9;
        QVERIFY_EXCEPTION_THROWN(svc->create(bad), std::invalid_argument);
        QCOMPARE(store.evs.size(), size_t(0));
    }
    void unknownDependencyIsRejected() {
        QVERIFY_EXCEPTION_THROWN(svc->create(nt("A", {"ghost"})), std::invalid_argument);
        QCOMPARE(store.evs.size(), size_t(0));
    }
    void legalMoveAppendsEventWithStringStatuses() {
        Id id = svc->create(nt("A"));
        svc->move(id, TaskStatus::InProgress);
        QCOMPARE(store.evs.size(), size_t(2));
        QVERIFY(proj->byId(id)->status == TaskStatus::InProgress);
        const Event& e = store.evs.back();
        QCOMPARE(e.payload["from"].toString(), QString("ToDo"));
        QCOMPARE(e.payload["to"].toString(), QString("InProgress"));
        QCOMPARE(e.payload["estHours"].toInt(), 2);
    }
    void illegalMoveThrowsAndAppendsNothing() {
        Id id = svc->create(nt("A"));
        QVERIFY_EXCEPTION_THROWN(svc->move(id, TaskStatus::Done), std::invalid_argument);
        QCOMPARE(store.evs.size(), size_t(1));
    }
    void blockedTaskCannotStartUntilPrerequisiteIsDone() {
        Id a = svc->create(nt("A"));
        Id b = svc->create(nt("B", {a}));
        QVERIFY_EXCEPTION_THROWN(svc->move(b, TaskStatus::InProgress), std::invalid_argument);
        QCOMPARE(proj->byId(b)->blockedBy.size(), size_t(1));
        svc->move(a, TaskStatus::InProgress);
        svc->move(a, TaskStatus::Review);
        svc->move(a, TaskStatus::Done);
        QVERIFY(proj->byId(b)->blockedBy.empty());
        svc->move(b, TaskStatus::InProgress);                  // now allowed
        QVERIFY(proj->byId(b)->status == TaskStatus::InProgress);
    }
    void cycleIsRejectedAndAppendsNothing() {
        Id a = svc->create(nt("A"));
        Id b = svc->create(nt("B", {a}));
        const size_t before = store.evs.size();
        QVERIFY(!svc->addDependency(a, b));
        QCOMPARE(store.evs.size(), before);
    }
    void addDependencyAppendsEventOnce() {
        Id a = svc->create(nt("A"));
        Id b = svc->create(nt("B"));
        QVERIFY(svc->addDependency(b, a));
        const size_t after = store.evs.size();
        QVERIFY(svc->addDependency(b, a));                     // duplicate: true, no new event
        QCOMPARE(store.evs.size(), after);
        QCOMPARE(proj->byId(b)->blockedBy.size(), size_t(1));
    }
    void assignAndUnassign() {
        Id a = svc->create(nt("A"));
        svc->assign(a, "m1", "slicer");
        QCOMPARE(proj->byId(a)->assignee.value(), std::string("m1"));
        svc->unassign(a, "declined");
        QVERIFY(!proj->byId(a)->assignee.has_value());
    }
    void unknownTaskThrows() {
        QVERIFY_EXCEPTION_THROWN(svc->move("nope", TaskStatus::InProgress), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(svc->assign("nope", "m1", "x"), std::invalid_argument);
        QCOMPARE(store.evs.size(), size_t(0));
    }
    void rebuiltProjectionEqualsLiveState() {
        Id a = svc->create(nt("A"));
        Id b = svc->create(nt("B", {a}));
        svc->assign(a, "m1", "slicer");
        svc->move(a, TaskStatus::InProgress);
        svc->move(a, TaskStatus::Review);
        auto live = proj->all();
        log->rebuild();
        auto rebuilt = proj->all();
        QCOMPARE(rebuilt.size(), live.size());
        for (size_t i = 0; i < live.size(); ++i) {
            QCOMPARE(rebuilt[i].id, live[i].id);
            QVERIFY(rebuilt[i].status == live[i].status);
            QCOMPARE(rebuilt[i].assignee.has_value(), live[i].assignee.has_value());
            QCOMPARE(rebuilt[i].blockedBy, live[i].blockedBy);
        }
        QVERIFY(proj->byId(b).has_value());
    }
};
QTEST_APPLESS_MAIN(TstTaskService)
#include "tst_TaskService.moc"


