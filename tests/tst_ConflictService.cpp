#include <QtTest>
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "planova/common/EventTypes.h"
#include "planova/conflict/ConflictProjection.h"
#include "planova/conflict/ConflictService.h"
#include "planova/pins/PinProjection.h"
#include "planova/store/EventLog.h"
#include "planova/task/TaskProjection.h"
#include "planova/task/TaskService.h"
#include "tests/fakes/FakeClock.h"
#include "tests/fakes/FakeEventStore.h"
#include "tests/fakes/FakeTaskQuery.h"

using namespace planova;

class tst_ConflictService : public QObject {
    Q_OBJECT

    FakeEventStore store;
    FakeClock clock{1000};
    FakeTaskQuery tasks;
    EventLog* log = nullptr;
    ConflictProjection* proj = nullptr;
    ConflictService* svc = nullptr;

    std::vector<std::string> storedTypes() const
    {
        std::vector<std::string> types;
        for (const Event& e : store.events)
            types.push_back(e.type);
        return types;
    }

private slots:
    void init()
    {
        store.events.clear();
        clock.setTime(1000);
        tasks.setTasks({});
        TaskDto t1{"t1", "Task", SkillCategory::Backend, Priority::Medium, 3, 2,
                   TaskStatus::ToDo, Id("m1"), {}};
        TaskDto t2 = t1;
        t2.id = "t2";
        t2.assignee = std::nullopt;
        tasks.addTask(t1);
        tasks.addTask(t2);
        log = new EventLog(store);
        proj = new ConflictProjection;
        log->add(*proj);
        svc = new ConflictService(*log, tasks, *proj, clock);
    }

    void cleanup()
    {
        delete svc;
        delete proj;
        delete log;
    }

    void declineAppendsExactlyThree()
    {
        svc->decline("t1", "m1", "Too busy");
        const std::vector<std::string> expected = {ConflictRaised, TaskUnassigned, PinCreated};
        QCOMPARE(storedTypes(), expected);
    }

    void declineMarksOpenConflict()
    {
        svc->decline("t1", "m1", "Too busy");
        QVERIFY(proj->hasOpenConflict("m1"));
        QVERIFY(!proj->hasOpenConflict("m2"));
        QCOMPARE(proj->openConflicts().size(), size_t(1));
    }

    void resolveClears()
    {
        const Id conflictId = svc->decline("t1", "m1", "Too busy");
        svc->resolve(conflictId, "lead");
        QVERIFY(!proj->hasOpenConflict("m1"));
        QVERIFY(proj->openConflicts().empty());
    }

    void resolveTwiceThrows()
    {
        const Id conflictId = svc->decline("t1", "m1", "Too busy");
        svc->resolve(conflictId, "lead");
        const size_t before = store.events.size();
        QVERIFY_EXCEPTION_THROWN(svc->resolve(conflictId, "lead"), std::invalid_argument);
        QCOMPARE(store.events.size(), before);
        QVERIFY_EXCEPTION_THROWN(svc->resolve("missing", "lead"), std::invalid_argument);
        QCOMPARE(store.events.size(), before);
    }

    void declineUnassignedRejected()
    {
        QVERIFY_EXCEPTION_THROWN(svc->decline("t2", "m1", "Too busy"), std::invalid_argument);
        QVERIFY(store.events.empty());
    }

    void declineOtherMembersTaskRejected()
    {
        QVERIFY_EXCEPTION_THROWN(svc->decline("t1", "m2", "Too busy"), std::invalid_argument);
        QVERIFY(store.events.empty());
    }

    void declineUnknownTaskRejected()
    {
        QVERIFY_EXCEPTION_THROWN(svc->decline("nope", "m1", "Too busy"), std::invalid_argument);
        QVERIFY(store.events.empty());
    }

    void blankReasonRejected()
    {
        QVERIFY_EXCEPTION_THROWN(svc->decline("t1", "m1", "  \n"), std::invalid_argument);
        QVERIFY(store.events.empty());
    }

    void sameMillisecondDeclinesGetDistinctIds()
    {
        const Id first = svc->decline("t1", "m1", "Too busy");
        const Id second = svc->decline("t1", "m1", "Too busy");
        QVERIFY(first != second);
        QCOMPARE(proj->openConflicts().size(), size_t(2));
    }

    void taskUnassignedClearsAssignee()
    {
        // Real projections on a fresh log, so the task really starts assigned to m1.
        FakeEventStore realStore;
        EventLog realLog(realStore);
        TaskProjection taskProj;
        ConflictProjection conflictProj;
        realLog.add(taskProj);
        realLog.add(conflictProj);
        TaskService taskSvc(realLog, taskProj, clock);
        ConflictService conflictSvc(realLog, taskProj, conflictProj, clock);

        const Id taskId = taskSvc.create(
            NewTask{"Build API", SkillCategory::Backend, Priority::Medium, 3, 2, {}});
        taskSvc.assign(taskId, "m1", "lead");
        QCOMPARE(taskProj.byId(taskId)->assignee, std::optional<Id>("m1"));

        conflictSvc.decline(taskId, "m1", "Too busy");
        QVERIFY(!taskProj.byId(taskId)->assignee.has_value());
    }

    // decline appends three events in one millisecond; a rebuild may apply them in any order.
    void declineEventsApplyInAnyOrder()
    {
        FakeEventStore realStore;
        EventLog realLog(realStore);
        TaskProjection taskProj;
        ConflictProjection conflictProj;
        realLog.add(taskProj);
        realLog.add(conflictProj);
        TaskService taskSvc(realLog, taskProj, clock);
        ConflictService conflictSvc(realLog, taskProj, conflictProj, clock);

        const Id taskId = taskSvc.create(
            NewTask{"Build API", SkillCategory::Backend, Priority::Medium, 3, 2, {}});
        taskSvc.assign(taskId, "m1", "lead");
        const size_t declineStart = realStore.events.size();
        conflictSvc.decline(taskId, "m1", "Too busy");
        QCOMPARE(realStore.events.size(), declineStart + 3);

        // Fresh projections: task events in order, then the three decline events reversed.
        TaskProjection freshTasks;
        ConflictProjection freshConflicts;
        PinProjection freshPins;
        for (size_t i = 0; i < declineStart; ++i)
            freshTasks.apply(realStore.events[i]);
        for (size_t i = realStore.events.size(); i > declineStart; --i) {
            const Event& e = realStore.events[i - 1];
            freshTasks.apply(e);
            freshConflicts.apply(e);
            freshPins.apply(e);
        }

        QVERIFY(freshConflicts.hasOpenConflict("m1"));
        QCOMPARE(freshConflicts.openConflicts().size(), size_t(1));
        QVERIFY(!freshTasks.byId(taskId)->assignee.has_value());
        QCOMPARE(freshPins.openPins().size(), size_t(1));
        QCOMPARE(freshPins.openPins()[0].pin.text, std::string("Declined: Too busy"));
    }

    void resolvedBeforeRaisedStillResolves()
    {
        svc->decline("t1", "m1", "Too busy");
        const Id conflictId = proj->openConflicts()[0].id;
        svc->resolve(conflictId, "lead");

        // Replay both events in the opposite order.
        ConflictProjection fresh;
        for (size_t i = store.events.size(); i > 0; --i)
            fresh.apply(store.events[i - 1]);
        QVERIFY(!fresh.hasOpenConflict("m1"));
        QVERIFY(fresh.openConflicts().empty());
    }
};

QTEST_APPLESS_MAIN(tst_ConflictService)
#include "tst_ConflictService.moc"
