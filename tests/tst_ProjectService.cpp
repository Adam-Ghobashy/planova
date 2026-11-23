#include <QtTest>
#include <QTemporaryDir>
#include <stdexcept>
#include "planova/common/EventTypes.h"
#include "planova/project/ProjectService.h"
#include "planova/store/FileEventStore.h"

namespace {

class FixedClock : public IClock {
public:
    int64_t nowMs() const override { return 1000; }
};

// Counts up on every call so later events replay after earlier ones.
class CountingClock : public IClock {
public:
    int64_t nowMs() const override { return next_ += 10; }

private:
    mutable int64_t next_ = 1000;
};

}

class tst_ProjectService : public QObject
{
    Q_OBJECT

private slots:
    void createThenList();
    void reopenRestoresProject();
    void createRejectsBlankName();
    void createRejectsUnknownModel();
    void createTrimsName();
    void createTrimsTeam();
    void updateReplacesFields();
    void updateRejectsUnknownId();
    void updateRejectsBlankName();
    void updateRejectsUnknownModel();
    void updateWithoutChangesAppendsNothing();
    void realUpdateAppendsExactlyOneEvent();
    void removeDropsProject();
    void removeRejectsUnknownId();
    void removeIsAuthoredByProjectTeam();
    void reopenKeepsEditAndDelete();
};

void tst_ProjectService::createThenList()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    FixedClock clock;
    ProjectService service(log, clock);

    service.create("Planova", "Team Alpha", "Agile", "idea");

    const auto projects = service.list();
    QCOMPARE(projects.size(), size_t(1));
    QCOMPARE(QString::fromStdString(projects[0].name), QString("Planova"));
}

void tst_ProjectService::reopenRestoresProject()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = std::filesystem::path(dir.path().toStdString());
    FixedClock clock;

    {
        FileEventStore store(path);
        EventLog log(store);
        ProjectService service(log, clock);
        service.create("Planova", "Team Alpha", "Agile", "idea");
    }

    FileEventStore store(path);
    EventLog log(store);
    ProjectService service(log, clock);
    log.rebuild();

    QCOMPARE(service.list().size(), size_t(1));
}

void tst_ProjectService::createRejectsBlankName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    FixedClock clock;
    ProjectService service(log, clock);

    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, service.create("   ", "team", "Agile", ""));
    QVERIFY(service.list().empty());
}

void tst_ProjectService::createRejectsUnknownModel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    FixedClock clock;
    ProjectService service(log, clock);

    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, service.create("App", "team", "Kanban", ""));
    QVERIFY(service.list().empty());
}

void tst_ProjectService::createTrimsName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    FixedClock clock;
    ProjectService service(log, clock);

    service.create("  App  ", "team", "Agile", "");

    const auto projects = service.list();
    QCOMPARE(projects.size(), size_t(1));
    QCOMPARE(projects[0].name, std::string("App"));
}

void tst_ProjectService::createTrimsTeam()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    FixedClock clock;
    ProjectService service(log, clock);

    service.create("A", "  Team  ", "Agile", "");

    const auto projects = service.list();
    QCOMPARE(projects.size(), size_t(1));
    QCOMPARE(projects[0].team, std::string("Team"));
}

void tst_ProjectService::updateReplacesFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    const Id id = service.create("Planova", "Team Alpha", "Agile", "idea");

    service.update(id, "  Orbit ", "Team Beta", "Waterfall");

    const auto projects = service.list();
    QCOMPARE(projects.size(), size_t(1));
    QCOMPARE(projects[0].id, id);
    QCOMPARE(projects[0].name, std::string("Orbit"));
    QCOMPARE(projects[0].team, std::string("Team Beta"));
    QCOMPARE(projects[0].seModel, std::string("Waterfall"));
}

void tst_ProjectService::updateRejectsUnknownId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    service.create("Planova", "Team Alpha", "Agile", "idea");
    const size_t before = store.readAll().size();

    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, service.update("missing", "Orbit", "Team", "Agile"));
    QCOMPARE(store.readAll().size(), before);
}

void tst_ProjectService::updateRejectsBlankName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    const Id id = service.create("Planova", "Team Alpha", "Agile", "idea");
    const size_t before = store.readAll().size();

    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, service.update(id, "   ", "Team Alpha", "Agile"));
    QCOMPARE(store.readAll().size(), before);
}

void tst_ProjectService::updateRejectsUnknownModel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    const Id id = service.create("Planova", "Team Alpha", "Agile", "idea");
    const size_t before = store.readAll().size();

    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, service.update(id, "Planova", "Team Alpha", "Kanban"));
    QCOMPARE(store.readAll().size(), before);
}

void tst_ProjectService::updateWithoutChangesAppendsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    const Id id = service.create("Planova", "Team Alpha", "Agile", "idea");
    const size_t before = store.readAll().size();

    service.update(id, "Planova ", "Team Alpha", "Agile");

    QCOMPARE(store.readAll().size(), before);
}

void tst_ProjectService::realUpdateAppendsExactlyOneEvent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    const Id id = service.create("Planova", "Team Alpha", "Agile", "idea");
    const size_t before = store.readAll().size();

    service.update(id, "Orbit", "Team Alpha", "Agile");

    QCOMPARE(store.readAll().size(), before + 1);
}

void tst_ProjectService::removeDropsProject()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    const Id first = service.create("First", "Team A", "Agile", "");
    const Id second = service.create("Second", "Team B", "Scrum", "");

    service.remove(first);

    const auto projects = service.list();
    QCOMPARE(projects.size(), size_t(1));
    QCOMPARE(projects[0].id, second);
    QVERIFY(!service.exists(first));
    QVERIFY(service.exists(second));
}

void tst_ProjectService::removeRejectsUnknownId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    service.create("Planova", "Team Alpha", "Agile", "idea");
    const size_t before = store.readAll().size();

    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, service.remove("missing"));
    QCOMPARE(store.readAll().size(), before);
}

void tst_ProjectService::removeIsAuthoredByProjectTeam()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));
    EventLog log(store);
    CountingClock clock;
    ProjectService service(log, clock);
    const Id id = service.create("Planova", "Team Alpha", "Agile", "idea");

    service.remove(id);

    bool found = false;
    for (const Event& event : store.readAll()) {
        if (event.type != ProjectDeleted)
            continue;
        found = true;
        QCOMPARE(event.author, std::string("Team Alpha"));
    }
    QVERIFY(found);
}

void tst_ProjectService::reopenKeepsEditAndDelete()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = std::filesystem::path(dir.path().toStdString());
    CountingClock clock;
    Id a;

    {
        FileEventStore store(path);
        EventLog log(store);
        ProjectService service(log, clock);
        a = service.create("A", "Team A", "Agile", "");
        const Id b = service.create("B", "Team B", "Scrum", "");
        service.update(a, "A2", "Team A", "Agile");
        service.remove(b);
    }

    FileEventStore store(path);
    EventLog log(store);
    ProjectService service(log, clock);
    log.rebuild();

    const auto projects = service.list();
    QCOMPARE(projects.size(), size_t(1));
    QCOMPARE(projects[0].id, a);
    QCOMPARE(projects[0].name, std::string("A2"));
}

QTEST_MAIN(tst_ProjectService)
#include "tst_ProjectService.moc"