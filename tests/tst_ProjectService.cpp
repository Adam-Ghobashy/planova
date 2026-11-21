#include <QtTest>
#include <QTemporaryDir>
#include <stdexcept>
#include "planova/project/ProjectService.h"
#include "planova/store/FileEventStore.h"

namespace {

class FixedClock : public IClock {
public:
    int64_t nowMs() const override { return 1000; }
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

QTEST_MAIN(tst_ProjectService)
#include "tst_ProjectService.moc"