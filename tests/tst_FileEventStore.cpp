#include <QtTest>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include "planova/store/FileEventStore.h"

class tst_FileEventStore : public QObject
{
    Q_OBJECT

private slots:
    void appendThenReadAllReturnsSameEvents();
    void readAllSortsByTimeThenId();
    void sameFilesGiveSameList();
    void brokenFileIsReported();
    void leftoverTmpIsIgnored();
    void appendToReadOnlyDirThrows();
};

static Event makeEvent(const std::string& id, int64_t ts)
{
    Event e;
    e.id = id;
    e.ts = ts;
    e.author = "tester";
    e.type = "ProjectCreated";
    e.payload = QJsonObject{{"name", "Planova"}};
    return e;
}

void tst_FileEventStore::appendThenReadAllReturnsSameEvents()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));

    store.append(makeEvent("1000-aaaaaaaa", 1000));

    const auto events = store.readAll();
    QCOMPARE(events.size(), size_t(1));
    QCOMPARE(QString::fromStdString(events[0].id), QString("1000-aaaaaaaa"));
}

void tst_FileEventStore::readAllSortsByTimeThenId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));

    store.append(makeEvent("2000-bbbbbbbb", 2000));
    store.append(makeEvent("1000-aaaaaaaa", 1000));

    const auto events = store.readAll();
    QCOMPARE(events.size(), size_t(2));
    QCOMPARE(QString::fromStdString(events[0].id), QString("1000-aaaaaaaa"));
    QCOMPARE(QString::fromStdString(events[1].id), QString("2000-bbbbbbbb"));
}

void tst_FileEventStore::sameFilesGiveSameList()
{
    QTemporaryDir dirA;
    QTemporaryDir dirB;
    QVERIFY(dirA.isValid());
    QVERIFY(dirB.isValid());
    FileEventStore storeA(std::filesystem::path(dirA.path().toStdString()));
    FileEventStore storeB(std::filesystem::path(dirB.path().toStdString()));

    storeA.append(makeEvent("1000-aaaaaaaa", 1000));
    storeA.append(makeEvent("2000-bbbbbbbb", 2000));
    storeB.append(makeEvent("2000-bbbbbbbb", 2000));
    storeB.append(makeEvent("1000-aaaaaaaa", 1000));

    const auto a = storeA.readAll();
    const auto b = storeB.readAll();
    QCOMPARE(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i)
        QCOMPARE(QString::fromStdString(a[i].id), QString::fromStdString(b[i].id));
}

static void writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

void tst_FileEventStore::brokenFileIsReported()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));

    writeFile(dir.filePath("abc.json"), "{not json");
    store.append(makeEvent("1000-aaaaaaaa", 1000));

    QCOMPARE(store.readAll().size(), size_t(1));
    QCOMPARE(store.brokenFiles().size(), size_t(1));
    QCOMPARE(QString::fromStdString(store.brokenFiles()[0].filename().string()), QString("abc.json"));
}

void tst_FileEventStore::leftoverTmpIsIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileEventStore store(std::filesystem::path(dir.path().toStdString()));

    writeFile(dir.filePath("x.json.tmp"), "{not json");

    QCOMPARE(store.readAll().size(), size_t(0));
    QCOMPARE(store.brokenFiles().size(), size_t(0));
}

void tst_FileEventStore::appendToReadOnlyDirThrows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // The "directory" is really a regular file, so the folder cannot be created.
    writeFile(dir.filePath("notAFolder"), "x");
    const auto blocked = std::filesystem::path(dir.path().toStdString()) / "notAFolder" / "events";
    FileEventStore store(blocked);

    QVERIFY_THROWS_EXCEPTION(std::exception, store.append(makeEvent("1000-aaaaaaaa", 1000)));
    QVERIFY(!std::filesystem::exists(blocked));
}

QTEST_MAIN(tst_FileEventStore)
#include "tst_FileEventStore.moc"