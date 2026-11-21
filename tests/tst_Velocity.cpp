#include <QtTest>
#include <QJsonObject>
#include <cmath>
#include <vector>

#include "planova/common/Clock.h"
#include "planova/common/EventTypes.h"
#include "planova/store/EventLog.h"
#include "planova/task/TaskProjection.h"
#include "planova/team/MemberProjection.h"
#include "planova/team/MemberService.h"
#include "planova/team/VelocityCalculator.h"

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

class tst_Velocity : public QObject
{
    Q_OBJECT

private slots:
    void emaBlendingFormula()
    {
        // Current velocity = 1.0, Estimated = 4.0h, Actual = 2.0h (ratio = 2.0)
        // Blended = 0.7 * 1.0 + 0.3 * 2.0 = 1.3
        double v = VelocityCalculator::update(1.0, 4.0, 2.0);
        QVERIFY(std::abs(v - 1.3) < 1e-6);
    }

    void clampToBounds()
    {
        // Finishing a 100h task in 1h -> ratio = 100.0 -> must clamp to 5.0
        double fast = VelocityCalculator::update(4.8, 100.0, 1.0);
        QCOMPARE(fast, 5.0);

        // Taking 100h on a 1h task -> ratio = 0.01 -> must clamp to 0.25
        double slow = VelocityCalculator::update(0.3, 1.0, 100.0);
        QCOMPARE(slow, 0.25);
    }

    void zeroOrNegativeHoursIgnored()
    {
        QCOMPARE(VelocityCalculator::update(1.2, 0.0, 5.0), 1.2);
        QCOMPARE(VelocityCalculator::update(1.2, 5.0, 0.0), 1.2);
        QCOMPARE(VelocityCalculator::update(1.2, -3.0, 2.0), 1.2);
    }

    void taskMovedUpdatesMemberVelocity()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);
        Id memberId = svc.add("Adam", "Backend Lead", true);

        QCOMPARE(proj.byId(memberId)->velocity, 1.0);

        appendEvent(log, "evt-assign", clock.nowMs(), TaskAssigned,
                    {{"taskId", "task_1"},
                     {"memberId", QString::fromStdString(memberId)},
                     {"by", "board"}});

        appendEvent(log, "evt-start", clock.nowMs(), TaskMoved,
                    {{"taskId", "task_1"}, {"from", "ToDo"},
                     {"to", "InProgress"}, {"estHours", 4}});

        // Two hours later the task is Done (estimated 4h, took 2h)
        clock.advance(2 * 3600000LL);
        appendEvent(log, "evt-done", clock.nowMs(), TaskMoved,
                    {{"taskId", "task_1"}, {"from", "Review"},
                     {"to", "Done"}, {"estHours", 4}});

        // Expected velocity: 0.7 * 1.0 + 0.3 * (4.0 / 2.0) = 1.3
        double updatedVel = proj.byId(memberId)->velocity;
        QVERIFY(std::abs(updatedVel - 1.3) < 1e-6);
    }

    void instantMoveIgnored()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);
        Id memberId = svc.add("Adam", "Backend Lead", true);

        appendEvent(log, "evt-assign", clock.nowMs(), TaskAssigned,
                    {{"taskId", "task_1"},
                     {"memberId", QString::fromStdString(memberId)},
                     {"by", "board"}});
        appendEvent(log, "evt-start", clock.nowMs(), TaskMoved,
                    {{"taskId", "task_1"}, {"from", "ToDo"},
                     {"to", "InProgress"}, {"estHours", 8}});

        // One second later: bookkeeping, not work. Without the guard the ratio would be huge.
        clock.advance(1000);
        appendEvent(log, "evt-done", clock.nowMs(), TaskMoved,
                    {{"taskId", "task_1"}, {"from", "Review"},
                     {"to", "Done"}, {"estHours", 8}});

        QCOMPARE(proj.byId(memberId)->velocity, 1.0);
    }

    void doneWithoutAssigneeChangesNothing()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        MemberProjection proj;
        log.add(proj);

        TaskProjection tasks;
        MemberService svc(log, clock, proj, tasks);
        svc.add("Adam", "Backend Lead", true);
        clock.advance(10);
        svc.add("Zeyad", "UI Designer", false);

        // Same moves as above, but nobody was ever assigned to task_1
        appendEvent(log, "evt-start", clock.nowMs(), TaskMoved,
                    {{"taskId", "task_1"}, {"from", "ToDo"},
                     {"to", "InProgress"}, {"estHours", 4}});
        clock.advance(2 * 3600000LL);
        QVERIFY_THROWS_NO_EXCEPTION(
            appendEvent(log, "evt-done", clock.nowMs(), TaskMoved,
                        {{"taskId", "task_1"}, {"from", "Review"},
                         {"to", "Done"}, {"estHours", 4}}));

        for (const MemberDto& m : proj.all()) {
            QCOMPARE(m.velocity, 1.0);
        }
    }

private:
    static void appendEvent(EventLog& log, const char* id, int64_t ts,
                            const char* type, const QJsonObject& payload)
    {
        Event e;
        e.id = id;
        e.ts = ts;
        e.author = "test";
        e.type = type;
        e.payload = payload;
        log.append(e);
    }
};

QTEST_MAIN(tst_Velocity)
#include "tst_Velocity.moc"