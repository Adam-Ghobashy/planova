#include "app/CoreBridge.h"
#include "board/PhaseBoardPage.h"
#include "board/TaskCardWidget.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include "widgets/ModalDialog.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <algorithm>

namespace {
// The decline dialog has a reason field, the follow-up confirm does not.
ModalDialog* visibleModal(bool withReasonField)
{
    const auto widgets = QApplication::topLevelWidgets();
    for (QWidget* w : widgets) {
        auto* dlg = qobject_cast<ModalDialog*>(w);
        if (dlg && dlg->isVisible() && (dlg->findChild<QPlainTextEdit*>() != nullptr) == withReasonField)
            return dlg;
    }
    return nullptr;
}

// exec() blocks, so the user's clicks come from timers that run inside the dialog's event loop.
// After 2 s the safety net rejects any open dialog, so a missing dialog fails the test instead of hanging.
QTimer& safetyNet()
{
    static QTimer* timer = new QTimer(qApp);
    timer->setSingleShot(true);
    return *timer;
}

void armSafetyNet()
{
    QTimer& timer = safetyNet();
    timer.disconnect();
    QObject::connect(&timer, &QTimer::timeout, [] {
        for (bool withField : {false, true}) {
            if (ModalDialog* dlg = visibleModal(withField)) {
                qWarning("safety net: closing a ModalDialog nobody closed");
                dlg->reject();
            }
        }
    });
    timer.start(2000);
}

void clickButton(ModalDialog* dlg, const char* name)
{
    auto* button = dlg->findChild<QPushButton*>(name);
    QVERIFY2(button, name);
    QTest::mouseClick(button, Qt::LeftButton);
}

// Types `reason` in the decline dialog and presses "Decline task"; then answers the follow-up confirm.
void scriptDecline(const QString& reason, const char* confirmButton)
{
    armSafetyNet();
    QTimer::singleShot(0, [reason, confirmButton] {
        ModalDialog* dlg = visibleModal(true);
        QVERIFY2(dlg, "no decline dialog");
        dlg->findChild<QPlainTextEdit*>()->setPlainText(reason);
        // The click below blocks until the confirm closes, so the answer is scheduled first.
        QTimer::singleShot(0, [confirmButton] {
            ModalDialog* confirm = visibleModal(false);
            QVERIFY2(confirm, "no confirm dialog");
            clickButton(confirm, confirmButton);
        });
        clickButton(dlg, "modalDangerButton");
    });
}

NewTask sampleTask(const char* title)
{
    NewTask t;
    t.title = title;
    t.skill = SkillCategory::Backend;
    t.priority = Priority::Medium;
    t.complexity = 2;
    t.estHours = 4;
    return t;
}

QString declineText() { return QString::fromUtf8("Decline task…"); }

QStringList actionTexts(const TaskCardWidget& card)
{
    QStringList texts;
    for (const QAction* a : card.actions()) texts << a->text();
    return texts;
}
}

class tst_BoardDecline : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Q_INIT_RESOURCE(planova); }
    void init() { ThemeManager::instance().apply(ThemeMode::Light); }
    void cleanup() { safetyNet().stop(); }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    void menuOnlyWhenAssigned()
    {
        TaskDto t;
        t.id = "t1";
        t.title = "Build API";
        t.complexity = 2;
        t.estHours = 4;

        TaskCardWidget unassigned(t, "", "", Token::Member1);
        QVERIFY(!actionTexts(unassigned).contains(declineText()));
        // No menu means nothing to do with the keyboard on the card itself, so it is not a (dead) Tab stop.
        QCOMPARE(unassigned.focusPolicy(), Qt::NoFocus);

        t.assignee = Id("m1");
        TaskCardWidget assigned(t, "Ana", "A", Token::Member1);
        QCOMPARE(assigned.focusPolicy(), Qt::StrongFocus);   // the Menu key opens "Decline task..."
        QVERIFY(actionTexts(assigned).contains(declineText()));
        std::vector<Id> requested;
        connect(&assigned, &TaskCardWidget::declineRequested, this, [&](const Id& id) { requested.push_back(id); });
        assigned.actions().first()->trigger();
        QCOMPARE(requested.size(), size_t(1));
        QCOMPARE(requested[0], Id("t1"));
    }

    // A screen reader hears the title and status, then who has it and how many pins it carries.
    void cardHasAccessibleNameAndDescription()
    {
        TaskDto t;
        t.id = "t1";
        t.title = "Build API";
        t.status = TaskStatus::Review;
        t.complexity = 2;
        t.estHours = 4;

        TaskCardWidget unassigned(t, "", "", Token::Member1);
        QVERIFY(unassigned.accessibleName().contains("Build API"));
        QVERIFY(unassigned.accessibleName().contains("Review / QA"));
        QVERIFY(unassigned.accessibleDescription().contains("Unassigned"));

        t.assignee = Id("m1");
        TaskCardWidget assigned(t, "Ana", "A", Token::Member1, 2);
        QVERIFY(assigned.accessibleDescription().contains("Ana"));
        QVERIFY(assigned.accessibleDescription().contains("2 pins"));
    }

    // Every card shows who has the task; an unassigned one says so in words.
    void unassignedCardSaysUnassigned()
    {
        TaskDto t;
        t.id = "t1";
        t.title = "Build API";
        t.complexity = 2;
        t.estHours = 4;
        TaskCardWidget card(t, "", "", Token::Member1);
        QStringList metas;
        for (const QLabel* l : card.findChildren<QLabel*>("CardMeta")) metas << l->text();
        QVERIFY(metas.contains("Unassigned"));
    }

    void declineFlowUnassigns()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile/Scrum", "idea"));
        CoreBridge bridge(core);
        const Id member = core.members().add("Ana", "Backend Developer", false);
        const Id task = core.tasks().create(sampleTask("Build API"));
        core.tasks().assign(task, member, "board");

        PhaseBoardPage page(core, bridge);
        page.show();
        QSignalSpy slicer(&page, &PhaseBoardPage::slicerRequested);

        scriptDecline("Overloaded", "modalSecondaryButton");   // Cancel on "Open Slicer"
        page.declineTask(task);

        QVERIFY(!core.taskQuery().byId(task)->assignee.has_value());
        QVERIFY(core.conflictQuery().hasOpenConflict(member));
        const auto pins = core.pinBoard().openPins();
        QCOMPARE(pins.size(), size_t(1));
        const auto& tags = pins[0].pin.tags;
        QVERIFY(std::find(tags.begin(), tags.end(), "conflict") != tags.end());
        QCOMPARE(slicer.count(), 0);

        // The board refreshed from the bridge: no card offers "Decline task" any more.
        QTest::qWait(10);                                      // old cards are deleted later
        QVERIFY(!page.findChildren<TaskCardWidget*>().isEmpty());
        for (const auto* card : page.findChildren<TaskCardWidget*>())
            QVERIFY(!actionTexts(*card).contains(declineText()));
    }

    void openSlicerEmitsSignal()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile/Scrum", "idea"));
        CoreBridge bridge(core);
        const Id member = core.members().add("Ana", "Backend Developer", false);
        const Id task = core.tasks().create(sampleTask("Build API"));
        core.tasks().assign(task, member, "board");

        PhaseBoardPage page(core, bridge);
        page.show();
        QSignalSpy slicer(&page, &PhaseBoardPage::slicerRequested);

        scriptDecline("Overloaded", "modalPrimaryButton");     // "Open Slicer"
        page.declineTask(task);
        QCOMPARE(slicer.count(), 1);
    }

    // The real path: card action -> column -> page (queued) -> dialog.
    void cardActionOpensDialog()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile/Scrum", "idea"));
        CoreBridge bridge(core);
        const Id member = core.members().add("Ana", "Backend Developer", false);
        const Id task = core.tasks().create(sampleTask("Build API"));
        core.tasks().assign(task, member, "board");

        PhaseBoardPage page(core, bridge);
        page.show();
        const auto cards = page.findChildren<TaskCardWidget*>();
        QCOMPARE(cards.size(), 1);

        bool opened = false;
        armSafetyNet();
        QTimer::singleShot(100, [&opened] {   // the page opens the dialog from a queued call
            ModalDialog* dlg = visibleModal(true);
            opened = dlg != nullptr;
            if (dlg) clickButton(dlg, "modalSecondaryButton");   // Cancel
        });
        cards.first()->actions().first()->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(opened, 1500);
        QCOMPARE(core.taskQuery().byId(task)->assignee, std::optional<Id>(member));
    }

    void blankReasonChangesNothing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PlanovaCore core(dir.path().toStdString());
        core.openProject(core.createProject("A", "team", "Agile/Scrum", "idea"));
        CoreBridge bridge(core);
        const Id member = core.members().add("Ana", "Backend Developer", false);
        const Id task = core.tasks().create(sampleTask("Build API"));
        core.tasks().assign(task, member, "board");

        PhaseBoardPage page(core, bridge);
        page.show();

        armSafetyNet();
        QTimer::singleShot(0, [] {
            ModalDialog* dlg = visibleModal(true);
            QVERIFY2(dlg, "no decline dialog");
            dlg->findChild<QPlainTextEdit*>()->setPlainText("   ");
            auto* ok = dlg->findChild<QPushButton*>("modalDangerButton");
            QVERIFY(ok && !ok->isEnabled());              // blank reason: the button stays disabled
            clickButton(dlg, "modalSecondaryButton");     // Cancel
        });
        page.declineTask(task);

        QCOMPARE(core.taskQuery().byId(task)->assignee, std::optional<Id>(member));
        QVERIFY(!core.conflictQuery().hasOpenConflict(member));
        QVERIFY(core.pinBoard().openPins().empty());
    }
};

QTEST_MAIN(tst_BoardDecline)
#include "tst_BoardDecline.moc"
