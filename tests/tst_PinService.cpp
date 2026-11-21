#include <QtTest>
#include <stdexcept>
#include <vector>

#include "planova/common/EventTypes.h"
#include "planova/pins/PinProjection.h"
#include "planova/pins/PinService.h"
#include "planova/store/EventLog.h"
#include "tests/fakes/FakeClock.h"
#include "tests/fakes/FakeEventStore.h"

using namespace planova;

class tst_PinService : public QObject {
    Q_OBJECT

    FakeEventStore store;
    FakeClock clock{1000};
    EventLog* log = nullptr;
    PinProjection* proj = nullptr;
    PinService* svc = nullptr;

    Id makePin(const std::string& text)
    {
        return svc->create(PinCategory::HandoffNote, text, {}, "m1", std::nullopt);
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

    void emptyTextRejected()
    {
        QVERIFY_EXCEPTION_THROWN(makePin("   "), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(makePin(""), std::invalid_argument);
        QVERIFY(store.events.empty());
    }

    void tagsNormalised()
    {
        svc->create(PinCategory::HandoffNote, "note", {"#API", " db ", "api", ""}, "m1", std::nullopt);
        const auto pins = proj->openPins();
        QCOMPARE(pins.size(), size_t(1));
        const std::vector<std::string> expected = {"api", "db"};
        QCOMPARE(pins[0].pin.tags, expected);
    }

    void createAppendsOnePinCreated()
    {
        const Id id = svc->create(PinCategory::FeedbackBlocker, "Login is flaky", {"auth"}, "m1", Id("t1"));
        QCOMPARE(store.events.size(), size_t(1));
        const Event& e = store.events[0];
        QCOMPARE(e.type, std::string(PinCreated));
        QCOMPARE(e.payload.value("pinId").toString().toStdString(), id);
        QCOMPARE(e.payload.value("category").toString(), QString("FeedbackBlocker"));
        QCOMPARE(e.payload.value("text").toString(), QString("Login is flaky"));
        QCOMPARE(e.payload.value("authorId").toString(), QString("m1"));
        QCOMPARE(e.payload.value("taskId").toString(), QString("t1"));
        QVERIFY(id.rfind("pin-", 0) == 0);
    }

    void commentAddsToPin()
    {
        const Id id = makePin("note");
        svc->comment(id, "m2", "I agree");
        const auto pins = proj->openPins();
        QCOMPARE(pins[0].comments.size(), size_t(1));
        QCOMPARE(pins[0].comments[0].text, std::string("I agree"));
        QCOMPARE(pins[0].comments[0].authorId, Id("m2"));
        QVERIFY(pins[0].comments[0].id.rfind("comment-", 0) == 0);
    }

    void commentOnUnknownPinThrows()
    {
        QVERIFY_EXCEPTION_THROWN(svc->comment("pin-nope", "m1", "hello"), std::invalid_argument);
        QVERIFY(store.events.empty());
    }

    void emptyCommentRejected()
    {
        const Id id = makePin("note");
        const size_t before = store.events.size();
        QVERIFY_EXCEPTION_THROWN(svc->comment(id, "m1", "  "), std::invalid_argument);
        QCOMPARE(store.events.size(), before);
    }

    void resolveRemovesFromOpenCount()
    {
        const Id a = makePin("first");
        makePin("second");
        QCOMPARE(proj->openCount(), 2);
        svc->resolve(a, "m1");
        QCOMPARE(proj->openCount(), 1);
        QCOMPARE(store.events[0].type, std::string(PinCreated));
        QCOMPARE(store.events.size(), size_t(3));
    }

    void resolveTwiceThrows()
    {
        const Id id = makePin("note");
        svc->resolve(id, "m1");
        const size_t before = store.events.size();
        QVERIFY_EXCEPTION_THROWN(svc->resolve(id, "m1"), std::invalid_argument);
        QCOMPARE(store.events.size(), before);
    }

    void moveChangesCategory()
    {
        const Id id = makePin("note");
        svc->move(id, "m1", PinCategory::ReadyForTesting);
        QVERIFY(proj->openPins()[0].pin.category == PinCategory::ReadyForTesting);
    }

    void editReplacesTextAndTags()
    {
        const Id id = svc->create(PinCategory::HandoffNote, "old", {"a"}, "m1", Id("t1"));
        svc->edit(id, "m1", "new text", {"#B", "c"}, std::nullopt);
        const auto pins = proj->openPins();
        QCOMPARE(pins[0].pin.text, std::string("new text"));
        const std::vector<std::string> expected = {"b", "c"};
        QCOMPARE(pins[0].pin.tags, expected);
        QVERIFY(!pins[0].taskId.has_value());
    }

    void editWithEmptyTextRejected()
    {
        const Id id = makePin("note");
        const size_t before = store.events.size();
        QVERIFY_EXCEPTION_THROWN(svc->edit(id, "m1", " ", {}, std::nullopt), std::invalid_argument);
        QCOMPARE(store.events.size(), before);
    }

    void removeHidesPin()
    {
        const Id id = makePin("note");
        svc->remove(id, "m1");
        QVERIFY(proj->openPins().empty());
        QCOMPARE(proj->openCount(), 0);
        const size_t before = store.events.size();
        QVERIFY_EXCEPTION_THROWN(svc->remove(id, "m1"), std::invalid_argument);
        QCOMPARE(store.events.size(), before);
    }

    void identicalPinsGetDistinctIds()
    {
        const Id a = makePin("same");
        const Id b = makePin("same");   // same clock tick, same payload
        QVERIFY(a != b);
        QCOMPARE(proj->openCount(), 2);
    }
};

QTEST_APPLESS_MAIN(tst_PinService)
#include "tst_PinService.moc"
