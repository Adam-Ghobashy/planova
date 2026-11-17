#pragma once

#include "app/CoreBridge.h"
#include "planova/common/Dtos.h"
#include "planova/common/Queries.h"
#include "planova/team/MemberService.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
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
                   IMemberQuery& members,
                   CoreBridge& bridge,
                   QWidget* parent = nullptr);

    void refresh();

private slots:
    void onAddMember();
    void onSeedDemoTeam();

private:
    QFrame* buildMemberCard(const MemberDto& member, int colorIndex);
    void showStatus(const QString& text);

    MemberService& service_;
    IMemberQuery& members_;
    CoreBridge& bridge_;

    QLabel* statusLabel_ = nullptr;
    QTimer* statusTimer_ = nullptr;

    QLineEdit* nameInput_ = nullptr;
    QComboBox* roleCombo_ = nullptr;
    QLineEdit* customRoleInput_ = nullptr;
    QCheckBox* leadCheck_ = nullptr;

    QLabel* totalMembersValue_ = nullptr;
    QLabel* avgUnderstandingValue_ = nullptr;
    QLabel* avgVelocityValue_ = nullptr;
    QLabel* teamLeadValue_ = nullptr;

    QVBoxLayout* cardsLayout_ = nullptr;
    QLabel* emptyLabel_ = nullptr;

    bool updating_ = false;
};

} // namespace planova::team