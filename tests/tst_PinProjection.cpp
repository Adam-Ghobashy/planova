#include <QtTest>
#include <vector>

#include "planova/common/EventTypes.h"
#include "planova/pins/PinProjection.h"
#include "planova/pins/PinService.h"
#include "planova/store/EventLog.h"
#include "tests/fakes/FakeClock.h"
#include "tests/fakes/FakeEventStore.h"

using namespace planova;

class tst_PinProjection : public QObject {
    Q_OBJECT

    FakeEventStore store;
    FakeClock clock{1000};
    EventLog* log = nullptr;
    PinProjection* proj = nullptr;
    PinService* svc = nullptr;

    Id makePin(const std::string& text, std::optional<Id> taskId = std::nullopt)
    {
        return svc->create(PinCategory::HandoffNote, text, {}, "m1", taskId);
    }

private slots:
    void init()
    {
        store.events.clear();
        clock.setTime(1000);
        log = new EventLog(store);
        proj = new PinProjection;
        log->add(*proj);
        svc = new PinService(*log, *proj, clock);
    }

    void cleanup()
    {
        delete svc;
        delete proj;
        delete log;
    }

    void newestFirst()
    {
        clock.setTime(1000);
        const Id first = makePin("first");
        clock.setTime(2000);
        const Id second = makePin("second");
        const auto pins = proj->openPins();
        QCOMPARE(pins.size(), size_t(2));
        QCOMPARE(pins[0].pin.id, second);
        QCOMPARE(pins[1].pin.id, first);
        QCOMPARE(pins[0].pin.ts, int64_t(2000));
    }

    void commentsOldestFirst()
    {
        const Id id = makePin("note");
        clock.setTime(1500);
        svc->comment(id, "m2", "one");
        clock.setTime(1600);
        svc->comment(id, "m3", "two");
        const auto pins = proj->openPins();
        QCOMPARE(pins[0].comments.size(), size_t(2));
        QCOMPARE(pins[0].comments[0].text, std::string("one"));
        QCOMPARE(pins[0].comments[1].text, std::string("two"));
        QCOMPARE(pins[0].comments[1].ts, int64_t(1600));
    }

    void latestRespectsN()
    {
        for (int i = 0; i < 5; ++i) {
            clock.setTime(1000 + i);
            makePin("pin " + std::to_string(i));
        }
        const auto latest = proj->latest(3);
        QCOMPARE(latest.size(), size_t(3));
        QCOMPARE(latest[0].text, std::string("pin 4"));
        QVERIFY(proj->latest(0).empty());
    }

    void pinCountForTask()
    {
        makePin("a", Id("t1"));
        const Id b = makePin("b", Id("t1"));
        const Id c = makePin("c", Id("t1"));
        makePin("d", Id("t2"));
        makePin("e");
        svc->resolve(b, "m1");
        svc->remove(c, "m1");
        QCOMPARE(proj->pinCountForTask("t1"), 1);
        QCOMPARE(proj->pinCountForTask("t2"), 1);
        QCOMPARE(proj->pinCountForTask("none"), 0);
    }

    void rebuildEqualsLive()
    {
        clock.setTime(1000);
        const Id a = makePin("a");
        clock.setTime(1100);
        const Id b = makePin("b", Id("t1"));
        clock.setTime(1200);
        svc->comment(a, "m2", "hello");
        clock.setTime(1300);
        svc->resolve(b, "m1");
        clock.setTime(1400);
        makePin("c");

        PinProjection fresh;
        for (const Event& e : store.readAll())
            fresh.apply(e);

        const auto live = proj->openPins();
        const auto rebuilt = fresh.openPins();
        QCOMPARE(rebuilt.size(), live.size());
        for (size_t i = 0; i < live.size(); ++i) {
            QCOMPARE(rebuilt[i].pin.id, live[i].pin.id);
            QCOMPARE(rebuilt[i].comments.size(), live[i].comments.size());
        }
        QCOMPARE(fresh.openCount(), proj->openCount());
        QVERIFY(!fresh.isOpen(b));
        QVERIFY(fresh.isOpen(a));
    }

    void unknownEventIgnored()
    {
        makePin("note");
        Event e;
        e.id = "9000-xx";
        e.ts = 9000;
        e.author = "m1";
        e.type = "SomethingElse";
        proj->apply(e);
        QCOMPARE(proj->openCount(), 1);
    }

    void eventsForUnknownPinIgnored()
    {
        Event e;
        e.id = "9000-xx";
        e.ts = 9000;
        e.author = "m1";
        e.type = PinResolved;
        e.payload["pinId"] = "pin-ghost";
        proj->apply(e);
        QCOMPARE(proj->openCount(), 0);
        QVERIFY(!proj->isOpen("pin-ghost"));
    }
};

QTEST_APPLESS_MAIN(tst_PinProjection)
#include "tst_PinProjection.moc"
