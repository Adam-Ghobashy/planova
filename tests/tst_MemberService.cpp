#include <QtTest>
#include <QJsonObject>
#include <stdexcept>
#include <vector>

#include "planova/common/Clock.h"
#include "planova/common/EventTypes.h"
#include "planova/store/EventLog.h"
#include "planova/task/TaskProjection.h"
#include "planova/task/TaskService.h"
#include "planova/team/MemberProjection.h"
#include "planova/team/MemberService.h"
#include "planova/team/SkillCategory.h"

using namespace std;

namespace {

class MemoryStore : public IEventStore {
public:
    vector<Event> events;
    void append(const Event& e) override { events.push_back(e); }
    vector<Event> readAll() const override { return events; }
};

class FakeClock : public IClock {
public:
    explicit FakeClock(int64_t startMs = 1000) : now_(startMs) {}
    int64_t nowMs() const override { return now_; }
    void advance(int64_t deltaMs) { now_ += deltaMs; }
private:
    int64_t now_;
};

} // namespace

class tst_MemberService : public QObject
{
    Q_OBJECT

private slots:
    void addAndQueryMembers()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);

        Id id1 = svc.add("Adam", "Backend Lead", true);
        clock.advance(10);
        Id id2 = svc.add("Zeyad", "UI Designer", false);

        auto allMembers = proj.all();
        QCOMPARE(allMembers.size(), size_t(2));
        QCOMPARE(allMembers[0].id, id1);
        QCOMPARE(allMembers[0].name, string("Adam"));
        QCOMPARE(allMembers[0].role, string("Backend Lead"));
        QCOMPARE(allMembers[0].isLead, true);

        QCOMPARE(allMembers[1].id, id2);
        QCOMPARE(allMembers[1].name, string("Zeyad"));
        QCOMPARE(allMembers[1].isLead, false);

        auto found = proj.byId(id1);
        QVERIFY(found.has_value());
        QCOMPARE(found->name, string("Adam"));

        auto missing = proj.byId("non_existent_id");
        QVERIFY(!missing.has_value());
    }

    void emptyNameThrows()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);
        MemberProjection proj;
        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);

        QVERIFY_THROWS_EXCEPTION(invalid_argument, svc.add("", "Developer", false));
    }

    void updateRoleAndClampUnderstanding()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);
        Id id = svc.add("Adam", "Junior Dev", false);

        clock.advance(10);
        svc.update(id, "Senior Dev", 85);
        QCOMPARE(proj.byId(id)->role, string("Senior Dev"));
        QCOMPARE(proj.byId(id)->understanding, 85);

        // Clamp above 100 -> 100
        clock.advance(10);
        svc.update(id, "Senior Dev", 150);
        QCOMPARE(proj.byId(id)->understanding, 100);

        // Clamp below 0 -> 0
        clock.advance(10);
        svc.update(id, "Senior Dev", -25);
        QCOMPARE(proj.byId(id)->understanding, 0);
    }

    void rateSkillsAndValidateRange()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);
        Id id = svc.add("Adam", "Backend", true);

        clock.advance(10);
        svc.rate(id, SkillCategory::Backend, 5);
        clock.advance(10);
        svc.rate(id, SkillCategory::Frontend, 4);

        auto dto = proj.byId(id);
        QVERIFY(dto.has_value());
        QCOMPARE(dto->ratings.at(SkillCategory::Backend), 5);
        QCOMPARE(dto->ratings.at(SkillCategory::Frontend), 4);

        // Out-of-bounds ratings (0 or 6) must throw invalid_argument
        QVERIFY_THROWS_EXCEPTION(invalid_argument, svc.rate(id, SkillCategory::Backend, 0));
        QVERIFY_THROWS_EXCEPTION(invalid_argument, svc.rate(id, SkillCategory::Backend, 6));
    }

    void toggleSettingsAndRebuild()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);

        QCOMPARE(proj.getSetting("auto_assign", false), false);

        clock.advance(10);
        svc.setSetting("auto_assign", true);
        QCOMPARE(proj.getSetting("auto_assign", false), true);

        log.rebuild();
        QCOMPARE(proj.getSetting("auto_assign", false), true);
    }

    void unknownSkillCategoryIsIgnored()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);
        Id id = svc.add("Adam", "Backend", true);

        // A hand-edited or newer-build event with a skill name we do not know
        Event e;
        e.id = "evt-quantum";
        e.ts = clock.nowMs() + 10;
        e.author = id;
        e.type = SkillRated;
        e.payload = QJsonObject{{"memberId", QString::fromStdString(id)},
                                {"category", "Quantum"},
                                {"rating", 3}};
        store.append(e);

        QVERIFY_THROWS_NO_EXCEPTION(log.rebuild());
        QVERIFY(proj.byId(id)->ratings.empty());
    }

    void removeMemberAndRebuild()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);
        Id id1 = svc.add("Adam", "Lead", true);
        clock.advance(10);
        Id id2 = svc.add("Zeyad", "UI", false);
        QCOMPARE(proj.all().size(), size_t(2));

        clock.advance(10);
        svc.remove(id1);
        QCOMPARE(proj.all().size(), size_t(1));
        QCOMPARE(proj.byId(id1).has_value(), false);
        QCOMPARE(proj.all()[0].id, id2);

        // Out-of-bounds / empty ID must throw invalid_argument
        QVERIFY_THROWS_EXCEPTION(invalid_argument, svc.remove(""));

        log.rebuild();
        QCOMPARE(proj.all().size(), size_t(1));
        QCOMPARE(proj.byId(id1).has_value(), false);
    }

    void updateUnknownThrows()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);
        MemberProjection proj;
        log.add(proj);
        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);

        QVERIFY_THROWS_EXCEPTION(invalid_argument, svc.update("ghost", "Dev", 50));
        QCOMPARE(store.events.size(), size_t(0));
    }

    void removeUnknownThrows()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);
        MemberProjection proj;
        log.add(proj);
        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);

        QVERIFY_THROWS_EXCEPTION(invalid_argument, svc.remove("ghost"));
        QCOMPARE(store.events.size(), size_t(0));
    }

    void rateUnknownThrows()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);
        MemberProjection proj;
        log.add(proj);
        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);

        QVERIFY_THROWS_EXCEPTION(invalid_argument, svc.rate("ghost", SkillCategory::Backend, 3));
        QCOMPARE(store.events.size(), size_t(0));
    }

    void addBlankNameThrows()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);
        MemberProjection proj;
        log.add(proj);
        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);

        QVERIFY_THROWS_EXCEPTION(invalid_argument, svc.add("   ", "Dev", false));
        QCOMPARE(store.events.size(), size_t(0));
    }

    void addTrimsName()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);
        MemberProjection proj;
        log.add(proj);
        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);

        const Id id = svc.add(" Ana ", "Dev", false);
        QCOMPARE(proj.byId(id)->name, string("Ana"));
    }

    void removeUnassignsOpenTasks()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);
        MemberProjection proj;
        TaskProjection tasks;
        log.add(proj);
        log.add(tasks);
        MemberService svc(log, clock, proj, tasks);
        TaskService taskSvc(log, tasks, clock, &proj);

        const Id member = svc.add("Ana", "Dev", false);
        clock.advance(10);
        const Id open = taskSvc.create(NewTask{"Open", SkillCategory::Backend, Priority::Medium, 3, 2, {}});
        clock.advance(10);
        const Id done = taskSvc.create(NewTask{"Done", SkillCategory::Backend, Priority::Medium, 3, 2, {}});
        clock.advance(10);
        taskSvc.assign(open, member, "board");
        clock.advance(10);
        taskSvc.assign(done, member, "board");
        clock.advance(10);
        taskSvc.move(done, TaskStatus::InProgress);
        clock.advance(10);
        taskSvc.move(done, TaskStatus::Review);
        clock.advance(10);
        taskSvc.move(done, TaskStatus::Done);
        clock.advance(10);

        svc.remove(member);

        QVERIFY(!tasks.byId(open)->assignee.has_value());
        QCOMPARE(tasks.byId(done)->assignee.value(), member);
        const size_t n = store.events.size();
        QCOMPARE(store.events[n - 2].type, string(TaskUnassigned));
        QCOMPARE(store.events[n - 2].payload["taskId"].toString().toStdString(), open);
        QCOMPARE(store.events[n - 1].type, string(MemberRemoved));
    }
};

QTEST_MAIN(tst_MemberService)
#include "tst_MemberService.moc"