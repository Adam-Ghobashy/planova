#include "app/EventText.h"
#include "planova/common/EventTypes.h"
#include <QStringList>
#include <QTest>

class tst_EventText : public QObject {
    Q_OBJECT
private slots:
    void everyEventTypeHasPlainText_data()
    {
        QTest::addColumn<QString>("eventType");
        // Every name in EventTypes.h, listed explicitly so a forgotten one shows up here.
        const char* names[] = {
            ProjectCreated, ProjectUpdated, ProjectDeleted, ProjectOpened,
            MemberAdded, MemberUpdated, MemberRemoved, SkillRated, SettingChanged,
            GitConnected, GitDisconnected, GitSynced,
            TaskCreated, TaskMoved, TaskAssigned, TaskUnassigned, DependencyAdded,
            SlicerWeightsChanged,
            PinCreated, PinCommentAdded, PinResolved, PinEdited, PinMoved, PinRemoved,
            ConflictRaised, ConflictResolved,
        };
        for (const char* name : names)
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void everyEventTypeHasPlainText()
    {
        QFETCH(QString, eventType);
        const QString text = friendlyEventText(eventType);
        QVERIFY(!text.isEmpty());
        QVERIFY(text != eventType);
        QVERIFY2(text != QStringLiteral("Changes saved"), "known event types need their own wording");
        QVERIFY2(!text.contains(QLatin1Char('_')), "no raw identifiers");
    }
    void knownExamples()
    {
        QCOMPARE(friendlyEventText("TaskMoved"), QStringLiteral("Task moved"));
        QCOMPARE(friendlyEventText("PinCreated"), QStringLiteral("Pin added"));
        QCOMPARE(friendlyEventText("MemberRemoved"), QStringLiteral("Member removed"));
        QCOMPARE(friendlyEventText("ProjectOpened"), QStringLiteral("Project opened"));
    }
    void unknownFallsBack()
    {
        QCOMPARE(friendlyEventText("SomethingNew"), QStringLiteral("Changes saved"));
        QCOMPARE(friendlyEventText(QString()), QStringLiteral("Changes saved"));
    }
};

QTEST_MAIN(tst_EventText)
#include "tst_EventText.moc"
