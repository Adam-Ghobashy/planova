#include "slicer/AssignmentTableModel.h"
#include "theme/ThemeManager.h"
#include <QSignalSpy>
#include <QTest>

using namespace planova::slicer;

namespace {
std::vector<MemberDto> members() {
    return {MemberDto{"1", "Alice", "Dev", false, {}, 100, 1.0}, MemberDto{"2", "Bob", "Dev", false, {}, 100, 1.0}};
}
TaskDto task(bool blocked) {
    return TaskDto{"t1", "Auth API", SkillCategory::Backend, Priority::High, 3, 4, TaskStatus::ToDo,
                   std::nullopt, blocked ? std::vector<Id>{"dep"} : std::vector<Id>{}};
}
}

class tst_AssignmentTableModel : public QObject {
    Q_OBJECT
private slots:
    void countsAndText() {
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "Rationale text", false}}, {task(false)}, members());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.columnCount(), 7);
        QCOMPARE(model.data(model.index(0, 0)).toString(), QString("Auth API (3 pts)"));
        QCOMPARE(model.data(model.index(0, 1)).toString(), QString("Backend"));
        QCOMPARE(model.data(model.index(0, 2)).toString(), QString("High"));
        QCOMPARE(model.data(model.index(0, 4)).toString(), QString("Rationale text"));
        QCOMPARE(model.data(model.index(0, 5)).toString(), QString("Ready"));
    }
    void uiUxLabel() {
        TaskDto t = task(false);
        t.skill = SkillCategory::UiUx;
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "R", false}}, {t}, members());
        QCOMPARE(model.data(model.index(0, 1), AssignmentTableModel::RequiredSkillRole).toString(), QString("UI/UX"));
        QCOMPARE(model.data(model.index(0, 1)).toString(), QString("UI/UX"));
    }
    void titleTooltipIsFull() {
        TaskDto t = task(false);
        t.title = "Integrate Critical Path Algorithm with the Weighted Scoring Strategy";
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "R", false}}, {t}, members());
        QCOMPARE(model.data(model.index(0, 0), Qt::ToolTipRole).toString(), QString::fromStdString(t.title));
    }
    void waitingStatusUsesThemeTokens() {
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "R", true}}, {task(true)}, members());
        QModelIndex idx = model.index(0, 5);
        QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), QString("Waiting on Dep"));
        QCOMPARE(model.data(idx, Qt::BackgroundRole).value<QColor>(),
                 ThemeManager::instance().color(Token::WarningSoft));
        QCOMPARE(model.data(idx, Qt::ForegroundRole).value<QColor>(),
                 ThemeManager::instance().color(Token::TextPrimary));
    }
    void manualOverrideUpdatesAssignment() {
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "Initial", false}}, {task(false)}, members());
        QVERIFY(model.setData(model.index(0, 6), QString("2"), Qt::EditRole));
        auto result = model.currentAssignments();
        QCOMPARE(result[0].memberId, std::string("2"));
        QCOMPARE(result[0].matchPct, 0);
        QCOMPARE(QString::fromStdString(result[0].rationale), QString("Manual override"));
    }
    void highlightMarksOnlyThatMembersRows() {
        AssignmentTableModel model;
        TaskDto t2 = task(false);
        t2.id = "t2";
        model.setAssignments({Assignment{"t1", "1", 85, "R", false}, Assignment{"t2", "2", 70, "R", false}},
                             {task(false), t2}, members());
        model.setHighlightedMember("2");
        QVERIFY(!model.data(model.index(0, 0), AssignmentTableModel::HighlightedRole).toBool());
        QVERIFY(model.data(model.index(1, 0), AssignmentTableModel::HighlightedRole).toBool());
    }
    void clearingHighlightRemovesIt() {
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "R", false}}, {task(false)}, members());
        model.setHighlightedMember("1");
        QVERIFY(model.data(model.index(0, 0), AssignmentTableModel::HighlightedRole).toBool());
        model.setHighlightedMember("");
        QVERIFY(!model.data(model.index(0, 0), AssignmentTableModel::HighlightedRole).toBool());
    }
    void overrideSetsManualFlag() {
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "Initial", false}}, {task(false)}, members());
        QVERIFY(!model.data(model.index(0, 3), AssignmentTableModel::ManualOverrideRole).toBool());
        QVERIFY(model.setData(model.index(0, 6), QString("2"), Qt::EditRole));
        QVERIFY(model.data(model.index(0, 3), AssignmentTableModel::ManualOverrideRole).toBool());
        model.setAssignments({Assignment{"t1", "1", 85, "Initial", false}}, {task(false)}, members());
        QVERIFY(!model.data(model.index(0, 3), AssignmentTableModel::ManualOverrideRole).toBool());
    }
    void pickingTheSameMemberIsNotAnOverride() {
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "Initial", false}}, {task(false)}, members());
        QSignalSpy edited(&model, &AssignmentTableModel::assignmentEdited);

        QVERIFY(model.setData(model.index(0, 6), QString("1"), Qt::EditRole));  // same member again

        QCOMPARE(edited.count(), 0);
        QVERIFY(!model.data(model.index(0, 3), AssignmentTableModel::ManualOverrideRole).toBool());
        QCOMPARE(QString::fromStdString(model.currentAssignments()[0].rationale), QString("Initial"));
        QCOMPARE(model.currentAssignments()[0].matchPct, 85);
    }
    void unknownMemberIsRejected() {
        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "Initial", false}}, {task(false)}, members());
        QVERIFY(!model.setData(model.index(0, 6), QString("99"), Qt::EditRole));
        QCOMPARE(model.currentAssignments()[0].memberId, std::string("1"));
    }
};

QTEST_MAIN(tst_AssignmentTableModel)
#include "tst_AssignmentTableModel.moc"
