#include "slicer/AssigneeDelegate.h"
#include "slicer/AssignmentTableModel.h"
#include <QComboBox>
#include <QTableView>
#include <QTest>

using namespace planova::slicer;

class tst_AssigneeDelegate : public QObject {
    Q_OBJECT
private slots:
    void choosingAMemberCommitsTheOverride() {
        std::vector<MemberDto> members = {MemberDto{"1", "Alice", "Dev", false, {}, 100, 1.0},
                                          MemberDto{"2", "Bob", "Dev", false, {}, 100, 1.0}};
        TaskDto task{"t1", "Auth API", SkillCategory::Backend, Priority::High, 3, 4, TaskStatus::ToDo,
                     std::nullopt, {}};

        AssignmentTableModel model;
        model.setAssignments({Assignment{"t1", "1", 85, "Initial", false}}, {task}, members);

        QTableView view;
        view.setModel(&model);
        view.setItemDelegateForColumn(AssignmentTableModel::ActionOverride, new AssigneeDelegate(members, &view));
        view.show();

        view.edit(model.index(0, AssignmentTableModel::ActionOverride));
        auto* combo = view.findChild<QComboBox*>("overrideCombo");
        QVERIFY(combo != nullptr);

        combo->setCurrentIndex(1);
        emit combo->activated(1);

        QCOMPARE(model.currentAssignments()[0].memberId, std::string("2"));
        QCOMPARE(QString::fromStdString(model.currentAssignments()[0].rationale), QString("Manual override"));
    }
};

QTEST_MAIN(tst_AssigneeDelegate)
#include "tst_AssigneeDelegate.moc"
