#pragma once

#include "app/CoreBridge.h"
#include "planova/common/Dtos.h"
#include "planova/common/Queries.h"
#include "planova/conflict/ConflictService.h"
#include "planova/team/MemberProjection.h"
#include "planova/team/MemberService.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace planova::team {

class TeamSkillsPage : public QWidget {
    Q_OBJECT
public:
    TeamSkillsPage(MemberService& service,
                   const MemberProjection& members,
                   planova::ConflictService& conflicts,
                   const planova::ConflictProjection& conflictView,
                   ITaskQuery& tasks,
                   CoreBridge& bridge,
                   QWidget* parent = nullptr);

    void refresh();

private slots:
    void onAddMember();

private:
    QFrame* buildMemberCard(const MemberDto& member, int colorIndex);
    QFrame* buildSettingsCard(QWidget* parent);
    QWidget* buildTableHeader(QWidget* parent);
    QWidget* buildProfileColumn(const MemberDto& member, int colorIndex, QWidget* parent);
    QWidget* buildSkillsColumn(const MemberDto& member, QWidget* parent);
    QWidget* buildUnderstandingColumn(const MemberDto& member, QWidget* parent);
    QWidget* buildAssignmentColumn(const MemberDto& member, QWidget* parent);
    static void applyColumnWidths(QGridLayout* grid);
    QPushButton* makeSwitch(const QString& objectName, const QString& title, const QString& settingKey,
                            QWidget* parent);
    void resolveConflict(const Id& conflictId, const Id& memberId);
    void showStatus(const QString& text);
    // Understanding slider: a keyboard or wheel change waits for a pause before it is saved, a mouse release saves now.
    void queueUnderstanding(const Id& memberId, const std::string& role, int value);
    bool flushUnderstanding();   // saves the waiting value, if any; false when the service rejected it
    void commitUnderstanding();  // flush, rebuild, confirm

    MemberService& service_;
    const MemberProjection& members_;
    planova::ConflictService& conflicts_;
    const planova::ConflictProjection& conflictView_;
    ITaskQuery& tasks_;
    CoreBridge& bridge_;

    QLabel* statusLabel_ = nullptr;
    QTimer* statusTimer_ = nullptr;
    QTimer* understandingTimer_ = nullptr;   // single shot, restarted by every slider step
    struct PendingUnderstanding { Id memberId; std::string role; int value = 0; bool valid = false; };
    PendingUnderstanding pending_;

    QLineEdit* nameInput_ = nullptr;
    QComboBox* roleCombo_ = nullptr;
    QLineEdit* customRoleInput_ = nullptr;
    QCheckBox* leadCheck_ = nullptr;

    QLabel* totalMembersValue_ = nullptr;
    QLabel* avgUnderstandingValue_ = nullptr;
    QLabel* avgVelocityValue_ = nullptr;
    QLabel* teamLeadValue_ = nullptr;

    QWidget* tableHeader_ = nullptr;
    QVBoxLayout* cardsLayout_ = nullptr;
    QLabel* emptyLabel_ = nullptr;

    bool updating_ = false;
};

} // namespace planova::team