#pragma once
#include "planova/common/Dtos.h"
#include "planova/slicer/SlicerTypes.h"
#include <QAbstractTableModel>
#include <set>
#include <vector>

namespace planova::slicer {

class AssignmentTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column : int { TaskComplexity = 0, Skill, Priority, Assignee, Rationale, Status, ActionOverride, ColCount };

    enum CustomRoles : int {
        TaskTitleRole = Qt::UserRole + 1,
        TaskComplexityRole,
        RequiredSkillRole,
        PriorityRole,
        AssigneeNameRole,
        AssigneeInitialsRole,
        MatchPctRole,
        WaitingOnDepRole,
        RationaleRole,
        HighlightedRole,     // bool: the row belongs to the member selected on the pizza
        ManualOverrideRole   // bool: the assignee was changed by the user
    };

    explicit AssignmentTableModel(QObject* parent = nullptr);
    void setAssignments(const std::vector<Assignment>& assignments, const std::vector<TaskDto>& tasks,
                        const std::vector<MemberDto>& members);
    std::vector<Assignment> currentAssignments() const { return assignments_; }
    // Rows assigned to this member get HighlightedRole = true. An empty id clears the highlight.
    void setHighlightedMember(const Id& memberId);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

signals:
    // Emitted after the user overrides an assignee (not when assignments are set from a preview).
    void assignmentEdited();

private:
    const TaskDto* findTask(const Id& id) const;
    const MemberDto* findMember(const Id& id) const;
    std::vector<Assignment> assignments_;
    std::vector<TaskDto> tasks_;
    std::vector<MemberDto> members_;
    Id highlightedMember_;
    std::set<Id> manualTaskIds_;
};

} // namespace planova::slicer
