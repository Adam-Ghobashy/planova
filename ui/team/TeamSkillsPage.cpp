#include "team/TeamSkillsPage.h"
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSlider>
#include <QStyle>
#include "widgets/ModalDialog.h"
#include <exception>
#include <numeric>

using namespace std;

namespace planova::team {

namespace {

struct SkillMeta {
    SkillCategory category;
    const char* label;
};

constexpr SkillMeta kAllSkills[] = {
    {SkillCategory::Backend, "Backend"},
    {SkillCategory::Frontend, "Frontend"},
    {SkillCategory::Storage, "Storage"},
    {SkillCategory::UiUx, "UI / UX"},
    {SkillCategory::Integration, "Integration"},
    {SkillCategory::Testing, "Testing"},
    {SkillCategory::Documentation, "Documentation"}
};

const QStringList kKnownRoles = {
    "Backend Developer",
    "Frontend Developer",
    "Full Stack Developer",
    "UI / UX Designer",
    "Database Architect",
    "QA / Testing Engineer",
    "Integration & DevOps",
    "Technical Documentation",
    "Team & Systems Lead",
    "Other..."
};

QString initialsFor(const string& name) {
    QString qname = QString::fromStdString(name).trimmed();
    if (qname.isEmpty()) return "?";
    QStringList parts = qname.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        return (parts[0].left(1) + parts[1].left(1)).toUpper();
    }
    return qname.left(2).toUpper();
}

} // namespace

TeamSkillsPage::TeamSkillsPage(MemberService& service,
                               const MemberProjection& members,
                               planova::ConflictService& conflicts,
                               const planova::ConflictProjection& conflictView,
                               ITaskQuery& tasks,
                               CoreBridge& bridge,
                               QWidget* parent)
    : QWidget(parent), service_(service), members_(members), conflicts_(conflicts),
      conflictView_(conflictView), tasks_(tasks), bridge_(bridge) {
    setObjectName("TeamSkillsPage");

    setAutoFillBackground(true);

    // Styles come from team.qss, loaded once by ThemeManager.

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("slicerScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* contentWidget = new QWidget(scrollArea);
    contentWidget->setObjectName("slicerScrollContent");
    auto* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(24, 20, 24, 24);
    mainLayout->setSpacing(16);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 4);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);
    auto* pageTitle = new QLabel("Team & Skills Directory", contentWidget);
    pageTitle->setObjectName("pageTitle");
    QFont titleFont = pageTitle->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    pageTitle->setFont(titleFont);

    auto* pageSubtitle = new QLabel(
        "Manage team members, project understanding levels, 7-domain skill matrices, and velocity profiles.",
        contentWidget);
    pageSubtitle->setObjectName("cardSubtitle");
    titleLayout->addWidget(pageTitle);
    titleLayout->addWidget(pageSubtitle);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    statusLabel_ = new QLabel(contentWidget);
    statusLabel_->setObjectName("statusLabel");
    statusLabel_->hide();
    headerLayout->addWidget(statusLabel_);

    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    statusTimer_->setInterval(4000);
    connect(statusTimer_, &QTimer::timeout, statusLabel_, &QLabel::hide);

    mainLayout->addLayout(headerLayout);

    auto* summaryCard = new QFrame(contentWidget);
    summaryCard->setObjectName("card");
    auto* summaryLayout = new QHBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(20, 14, 20, 14);
    summaryLayout->setSpacing(24);

    auto makeMetric = [&](const QString& label, QLabel*& valueOut) {
        auto* col = new QVBoxLayout();
        col->setSpacing(2);
        auto* lbl = new QLabel(label, summaryCard);
        lbl->setObjectName("cardSubtitle");
        valueOut = new QLabel("0", summaryCard);
        valueOut->setObjectName("cardTitle");
        QFont vf = valueOut->font();
        vf.setPointSize(15);
        vf.setBold(true);
        valueOut->setFont(vf);
        col->addWidget(lbl);
        col->addWidget(valueOut);
        summaryLayout->addLayout(col, 1);
    };

    makeMetric("Total Members", totalMembersValue_);
    makeMetric("Team Lead", teamLeadValue_);
    makeMetric("Avg Understanding", avgUnderstandingValue_);
    makeMetric("Avg Velocity (EMA)", avgVelocityValue_);

    mainLayout->addWidget(summaryCard);
    mainLayout->addWidget(buildSettingsCard(contentWidget));

    auto* addCard = new QFrame(contentWidget);
    addCard->setObjectName("card");
    auto* addCardLayout = new QVBoxLayout(addCard);
    addCardLayout->setContentsMargins(20, 16, 20, 16);
    addCardLayout->setSpacing(12);

    auto* addTitle = new QLabel("Add Team Member", addCard);
    addTitle->setObjectName("cardTitle");
    auto* addSubtitle = new QLabel(
        "Register a new member into the event log to include them in Chaos Slicer and Velocity tracking.",
        addCard);
    addSubtitle->setObjectName("cardSubtitle");
    addCardLayout->addWidget(addTitle);
    addCardLayout->addWidget(addSubtitle);

    auto* formRow = new QHBoxLayout();
    formRow->setSpacing(12);

    nameInput_ = new QLineEdit(addCard);
    nameInput_->setObjectName("lightInput");
    nameInput_->setPlaceholderText("Member Name (e.g., Adam Ghobashy)");

    roleCombo_ = new QComboBox(addCard);
    roleCombo_->setObjectName("roleCombo");
    roleCombo_->addItems(kKnownRoles);

    customRoleInput_ = new QLineEdit(addCard);
    customRoleInput_->setObjectName("lightInput");
    customRoleInput_->setPlaceholderText("Write custom role...");
    customRoleInput_->hide();

    connect(roleCombo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (text == "Other..." || text == "Other") {
            customRoleInput_->show();
            customRoleInput_->setFocus();
        } else {
            customRoleInput_->hide();
        }
    });

    leadCheck_ = new QCheckBox("Team Lead", addCard);
    leadCheck_->setObjectName("leadCheck");

    auto* addBtn = new QPushButton("+ Add Member", addCard);
    addBtn->setObjectName("primaryButton");
    connect(addBtn, &QPushButton::clicked, this, &TeamSkillsPage::onAddMember);
    connect(nameInput_, &QLineEdit::returnPressed, this, &TeamSkillsPage::onAddMember);
    connect(customRoleInput_, &QLineEdit::returnPressed, this, &TeamSkillsPage::onAddMember);

    formRow->addWidget(nameInput_, 2);
    formRow->addWidget(roleCombo_, 2);
    formRow->addWidget(customRoleInput_, 2);
    formRow->addWidget(leadCheck_);
    formRow->addWidget(addBtn);
    addCardLayout->addLayout(formRow);

    mainLayout->addWidget(addCard);

    cardsLayout_ = new QVBoxLayout();
    cardsLayout_->setSpacing(14);
    mainLayout->addLayout(cardsLayout_);

    emptyLabel_ = new QLabel(
        "No team members yet. Add the first one with the '+ Add Member' button above.",
        contentWidget);
    emptyLabel_->setObjectName("emptyLabel");
    emptyLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(emptyLabel_);

    mainLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);

    connect(&bridge_, &CoreBridge::changed, this, [this](const QString&) {
        if (!updating_) refresh();
    });

    refresh();
}

void TeamSkillsPage::onAddMember() {
    const string name = nameInput_->text().trimmed().toStdString();
    if (name.empty()) {
        showStatus("Please enter a member name");
        return;
    }

    QString qRole = roleCombo_->currentText();
    if (qRole == "Other..." || qRole == "Other") {
        qRole = customRoleInput_->text().trimmed();
    }
    if (qRole.isEmpty()) {
        qRole = "Developer";
    }
    string role = qRole.toStdString();

    updating_ = true;
    try {
        service_.add(name, role, leadCheck_->isChecked());
    } catch (const exception& e) {
        updating_ = false;
        showStatus(QString("Error: %1").arg(QString::fromUtf8(e.what())));
        return;
    }
    updating_ = false;

    nameInput_->clear();
    roleCombo_->setCurrentIndex(0);
    customRoleInput_->clear();
    customRoleInput_->hide();
    leadCheck_->setChecked(false);
    refresh();
    showStatus(QString("Added %1 to the team").arg(QString::fromStdString(name)));
}

QPushButton* TeamSkillsPage::makeSwitch(const QString& objectName, const QString& title,
                                        const QString& settingKey, QWidget* parent) {
    // A checkable push button reports its checked state to assistive tech; "On"/"Off" in the text
    // means the state is not shown by colour alone.
    auto* button = new QPushButton(parent);
    button->setObjectName(objectName);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(32);
    button->setChecked(members_.getSetting(settingKey.toStdString(), false));
    auto showState = [button, title](bool on) {
        button->setText(QString("%1: %2").arg(title, on ? "On" : "Off"));
    };
    showState(button->isChecked());
    connect(button, &QPushButton::toggled, this, [this, showState, settingKey](bool on) {
        showState(on);
        try {
            service_.setSetting(settingKey.toStdString(), on);
        } catch (const exception& e) {
            showStatus(QString::fromUtf8(e.what()));
        }
    });
    return button;
}

QFrame* TeamSkillsPage::buildSettingsCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("card");
    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(20, 14, 20, 14);
    layout->setSpacing(24);

    auto addSwitch = [&](const QString& objectName, const QString& title, const QString& hint,
                         const QString& key) {
        auto* col = new QVBoxLayout();
        col->setSpacing(4);
        auto* button = makeSwitch(objectName, title, key, card);
        button->setAccessibleDescription(hint);
        auto* hintLabel = new QLabel(hint, card);
        hintLabel->setObjectName("cardSubtitle");
        col->addWidget(button, 0, Qt::AlignLeft);
        col->addWidget(hintLabel);
        layout->addLayout(col);
    };
    addSwitch("autoMatchSwitch", "Auto-Match Priority", "Match parameters to qualify capacity",
              "autoMatch");
    addSwitch("smartAssignSwitch", "Smart Assign", "Match task assignments and capacity triggers",
              "smartAssign");
    layout->addStretch();
    return card;
}

QWidget* TeamSkillsPage::buildAssignmentColumn(const MemberDto& member, QWidget* parent) {
    auto* column = new QWidget(parent);
    column->setObjectName("assignmentColumn");
    column->setFixedWidth(240);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* heading = new QLabel("Assignment / Clash", column);
    heading->setObjectName("cardSubtitle");
    layout->addWidget(heading);

    QStringList titles;
    for (const TaskDto& task : tasks_.all()) {
        if (task.assignee && *task.assignee == member.id && task.status != TaskStatus::Done)
            titles << QString::fromStdString(task.title);
    }
    auto* current = new QLabel(titles.isEmpty() ? QString::fromUtf8("—") : titles.join(", "), column);
    current->setObjectName("assignmentText");
    current->setWordWrap(true);
    layout->addWidget(current);

    bool badgeShown = false;
    for (const ConflictView& conflict : conflictView_.openConflicts()) {
        if (conflict.memberId != member.id) continue;
        if (!badgeShown) {
            auto* badge = new QLabel("Decline and Clash", column);
            badge->setObjectName("clashBadge");
            layout->addWidget(badge, 0, Qt::AlignLeft);
            badgeShown = true;
        }
        const auto task = tasks_.byId(conflict.taskId);
        const QString taskTitle = task ? QString::fromStdString(task->title) : QString("a task");
        auto* text = new QLabel(QString::fromUtf8("Task Conflict Detected: %1 — %2")
                                    .arg(taskTitle, QString::fromStdString(conflict.reason)), column);
        text->setObjectName("conflictText");
        text->setWordWrap(true);
        layout->addWidget(text);

        // "&&" shows one ampersand; a single one would become a keyboard mnemonic.
        auto* resolve = new QPushButton("Agree && Resolve", column);
        resolve->setObjectName("resolveConflict");
        resolve->setCursor(Qt::PointingHandCursor);
        resolve->setMinimumHeight(28);
        connect(resolve, &QPushButton::clicked, this,
                [this, conflictId = conflict.id, memberId = member.id]() { resolveConflict(conflictId, memberId); });
        layout->addWidget(resolve, 0, Qt::AlignLeft);
    }
    layout->addStretch();
    return column;
}

void TeamSkillsPage::resolveConflict(const Id& conflictId, const Id& memberId) {
    try {
        conflicts_.resolve(conflictId, memberId);
    } catch (const exception& e) {
        ModalDialog::info(this, "Could not resolve", QString::fromUtf8(e.what()));
        return;
    }
    showStatus("Conflict resolved");
}

QFrame* TeamSkillsPage::buildMemberCard(const MemberDto& member, int colorIndex) {
    auto* card = new QFrame(this);
    card->setObjectName("card");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(14);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    auto* avatar = new QLabel(initialsFor(member.name), card);
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setObjectName("memberAvatar");
    // team.qss maps memberColor 1..6 to the Member1..Member6 colours.
    avatar->setProperty("memberColor", colorIndex % 6 + 1);
    topRow->addWidget(avatar);

    auto* infoCol = new QVBoxLayout();
    infoCol->setSpacing(2);

    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);
    auto* nameLbl = new QLabel(QString::fromStdString(member.name), card);
    nameLbl->setObjectName("cardTitle");
    nameRow->addWidget(nameLbl);

    if (member.isLead) {
        auto* leadBadge = new QLabel(QString::fromUtf8("Lead 👑"), card);
        leadBadge->setObjectName("leadBadge");
        nameRow->addWidget(leadBadge);
    }

    auto* velBadge = new QLabel(
        QString("Velocity: %1").arg(member.velocity, 0, 'f', 2), card);
    velBadge->setObjectName("velocityBadge");
    nameRow->addWidget(velBadge);
    nameRow->addStretch();

    auto* roleRow = new QHBoxLayout();
    roleRow->setSpacing(8);
    roleRow->setContentsMargins(0, 0, 0, 0);

    auto* roleLbl = new QLabel(QString::fromStdString(member.role), card);
    roleLbl->setObjectName("cardSubtitle");
    roleRow->addWidget(roleLbl);

    auto* editRoleBtn = new QPushButton("✎ Edit Role", card);
    editRoleBtn->setCursor(Qt::PointingHandCursor);
    editRoleBtn->setToolTip("Change this member's role");
    editRoleBtn->setObjectName("editRoleButton");
    roleRow->addWidget(editRoleBtn);

    auto* removeBtn = new QPushButton("✕ Remove", card);
    removeBtn->setCursor(Qt::PointingHandCursor);
    removeBtn->setToolTip("Remove member from the team");
    removeBtn->setObjectName("removeMemberButton");
    roleRow->addWidget(removeBtn);

    // Inline edit widgets: Dropdown of known roles + custom text input
    auto* editRoleCombo = new QComboBox(card);
    editRoleCombo->setObjectName("roleCombo");
    editRoleCombo->addItems(kKnownRoles);
    editRoleCombo->hide();
    roleRow->addWidget(editRoleCombo);

    auto* editCustomRoleInput = new QLineEdit(card);
    editCustomRoleInput->setObjectName("lightInput");
    editCustomRoleInput->setPlaceholderText("Write custom role...");
    editCustomRoleInput->setFixedWidth(180);
    editCustomRoleInput->hide();
    roleRow->addWidget(editCustomRoleInput);

    connect(editRoleCombo, &QComboBox::currentTextChanged, [editCustomRoleInput](const QString& text) {
        if (text == "Other..." || text == "Other") {
            editCustomRoleInput->show();
            editCustomRoleInput->setFocus();
            editCustomRoleInput->selectAll();
        } else {
            editCustomRoleInput->hide();
        }
    });

    auto* saveRoleBtn = new QPushButton("Save", card);
    saveRoleBtn->setCursor(Qt::PointingHandCursor);
    saveRoleBtn->setObjectName("saveRoleButton");
    saveRoleBtn->hide();
    roleRow->addWidget(saveRoleBtn);

    auto* cancelRoleBtn = new QPushButton("Cancel", card);
    cancelRoleBtn->setCursor(Qt::PointingHandCursor);
    cancelRoleBtn->setObjectName("cancelRoleButton");
    cancelRoleBtn->hide();
    roleRow->addWidget(cancelRoleBtn);

    roleRow->addStretch();

    connect(editRoleBtn, &QPushButton::clicked, this, [=]() {
        roleLbl->hide();
        editRoleBtn->hide();
        removeBtn->hide();

        const QString currentRole = QString::fromStdString(member.role);
        int idx = editRoleCombo->findText(currentRole, Qt::MatchFixedString);
        if (idx >= 0 && currentRole != "Other..." && currentRole != "Other") {
            editRoleCombo->setCurrentIndex(idx);
            editCustomRoleInput->hide();
        } else {
            int otherIdx = editRoleCombo->findText("Other...");
            if (otherIdx >= 0) editRoleCombo->setCurrentIndex(otherIdx);
            editCustomRoleInput->setText(currentRole);
            editCustomRoleInput->show();
        }

        editRoleCombo->show();
        saveRoleBtn->show();
        cancelRoleBtn->show();
        if (editCustomRoleInput->isVisible()) {
            editCustomRoleInput->setFocus();
            editCustomRoleInput->selectAll();
        } else {
            editRoleCombo->setFocus();
        }
    });

    connect(cancelRoleBtn, &QPushButton::clicked, this, [=]() {
        editRoleCombo->hide();
        editCustomRoleInput->hide();
        saveRoleBtn->hide();
        cancelRoleBtn->hide();
        roleLbl->show();
        editRoleBtn->show();
        removeBtn->show();
    });

    auto doSaveRole = [this, id = member.id, und = member.understanding, name = member.name, editRoleCombo, editCustomRoleInput]() {
        QString newRole = editRoleCombo->currentText();
        if (newRole == "Other..." || newRole == "Other") {
            newRole = editCustomRoleInput->text().trimmed();
        }
        if (newRole.isEmpty()) {
            newRole = "Developer";
        }
        updating_ = true;
        try {
            service_.update(id, newRole.toStdString(), und);
        } catch (const exception& e) {
            updating_ = false;
            showStatus(QString::fromUtf8(e.what()));
            return;
        }
        updating_ = false;
        refresh();
        showStatus(QString("Updated %1's role to %2")
                       .arg(QString::fromStdString(name))
                       .arg(newRole));
    };

    connect(saveRoleBtn, &QPushButton::clicked, this, doSaveRole);
    connect(editCustomRoleInput, &QLineEdit::returnPressed, this, doSaveRole);

    connect(removeBtn, &QPushButton::clicked, this, [this, id = member.id, name = QString::fromStdString(member.name)]() {
        const bool confirmed = ModalDialog::confirm(
            this,
            "Remove Team Member",
            QString("Are you sure you want to remove %1 from the team?\n\nThis action will record a MemberRemoved event in the event log.")
                .arg(name),
            "Remove", ModalDialog::Role::Danger);

        if (!confirmed) {
            return;
        }

        updating_ = true;
        try {
            service_.remove(id);
        } catch (const exception& e) {
            updating_ = false;
            showStatus(QString("Error removing member: %1").arg(QString::fromUtf8(e.what())));
            return;
        }
        updating_ = false;
        refresh();
        showStatus(QString("Removed %1 from the team").arg(name));
    });

    infoCol->addLayout(nameRow);
    infoCol->addLayout(roleRow);
    topRow->addLayout(infoCol, 1);
    // Natural height, centred: a tall clash column must not stretch the name row.
    topRow->setAlignment(infoCol, Qt::AlignVCenter);

    // Understanding Slider (0 - 100%)
    auto* undCol = new QVBoxLayout();
    undCol->setSpacing(4);
    auto* undHeader = new QHBoxLayout();
    auto* undTitle = new QLabel("Project Understanding", card);
    undTitle->setObjectName("cardSubtitle");
    auto* undVal = new QLabel(QString("%1%").arg(member.understanding), card);
    undVal->setObjectName("sliderValueLabel");
    undHeader->addWidget(undTitle);
    undHeader->addStretch();
    undHeader->addWidget(undVal);

    auto* undSlider = new QSlider(Qt::Horizontal, card);
    undSlider->setRange(0, 100);
    undSlider->setValue(member.understanding);
    undSlider->setFixedWidth(180);

    connect(undSlider, &QSlider::valueChanged, this, [undVal](int v) {
        undVal->setText(QString("%1%").arg(v));
    });
    connect(undSlider, &QSlider::sliderReleased, this,
            [this, id = member.id, role = member.role, undSlider]() {
                updating_ = true;
                try {
                    service_.update(id, role, undSlider->value());
                } catch (const exception& e) {
                    updating_ = false;
                    showStatus(QString::fromUtf8(e.what()));
                    return;
                }
                updating_ = false;
                refresh();
                showStatus("Updated project understanding");
            });

    undCol->addLayout(undHeader);
    undCol->addWidget(undSlider);
    topRow->addLayout(undCol);
    topRow->setAlignment(undCol, Qt::AlignVCenter);
    topRow->addWidget(buildAssignmentColumn(member, card), 0, Qt::AlignVCenter);

    cardLayout->addLayout(topRow);

    auto* divider = new QFrame(card);
    divider->setFrameShape(QFrame::HLine);
    divider->setObjectName("cardDivider");
    cardLayout->addWidget(divider);

    auto* skillsGrid = new QGridLayout();
    skillsGrid->setHorizontalSpacing(18);
    skillsGrid->setVerticalSpacing(8);

    for (int i = 0; i < 7; ++i) {
        const auto& meta = kAllSkills[i];
        int currentRating = 3;
        auto it = member.ratings.find(meta.category);
        if (it != member.ratings.end()) {
            currentRating = it->second;
        }

        const int row = i / 4;
        const int col = i % 4;

        auto* cellWidget = new QWidget(card);
        cellWidget->setObjectName("skillCell");
        auto* cellLayout = new QHBoxLayout(cellWidget);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(4);

        auto* skillLbl = new QLabel(meta.label, cellWidget);
        skillLbl->setFixedWidth(92);
        skillLbl->setObjectName("skillLabel");
        cellLayout->addWidget(skillLbl);

        for (int r = 1; r <= 5; ++r) {
            auto* rateBtn = new QPushButton(QString::number(r), cellWidget);
            rateBtn->setFixedSize(24, 24);
            rateBtn->setCursor(Qt::PointingHandCursor);
            const bool active = (r <= currentRating);
            rateBtn->setObjectName("ratingButton");
            rateBtn->setProperty("active", active);
            // Re-polish so the qss [active] selector is applied.
            rateBtn->style()->unpolish(rateBtn);
            rateBtn->style()->polish(rateBtn);

            connect(rateBtn, &QPushButton::clicked, this,
                    [this, id = member.id, cat = meta.category, r, label = QString(meta.label)]() {
                        updating_ = true;
                        try {
                            service_.rate(id, cat, r);
                        } catch (const exception& e) {
                            updating_ = false;
                            showStatus(QString::fromUtf8(e.what()));
                            return;
                        }
                        updating_ = false;
                        refresh();
                        showStatus(QString("Rated %1: %2/5").arg(label).arg(r));
                    });
            cellLayout->addWidget(rateBtn);
        }
        cellLayout->addStretch();
        skillsGrid->addWidget(cellWidget, row, col);
    }

    cardLayout->addLayout(skillsGrid);
    return card;
}

void TeamSkillsPage::refresh() {
    while (QLayoutItem* item = cardsLayout_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const vector<MemberDto> allMembers = members_.all();
    emptyLabel_->setVisible(allMembers.empty());

    totalMembersValue_->setText(QString::number(allMembers.size()));

    if (allMembers.empty()) {
        teamLeadValue_->setText("Unassigned");
        avgUnderstandingValue_->setText("0%");
        avgVelocityValue_->setText("1.00");
        return;
    }

    QString leadName = "Unassigned";
    double sumUnderstanding = 0.0;
    double sumVelocity = 0.0;

    for (size_t i = 0; i < allMembers.size(); ++i) {
        const auto& m = allMembers[i];
        if (m.isLead) {
            leadName = QString::fromStdString(m.name);
        }
        sumUnderstanding += m.understanding;
        sumVelocity += m.velocity;
        cardsLayout_->addWidget(buildMemberCard(m, static_cast<int>(i)));
    }

    const double count = static_cast<double>(allMembers.size());
    teamLeadValue_->setText(leadName);
    avgUnderstandingValue_->setText(
        QString("%1%").arg( qRound(sumUnderstanding / count) ));
    avgVelocityValue_->setText(
        QString::number(sumVelocity / count, 'f', 2));
}

void TeamSkillsPage::showStatus(const QString& text) {
    statusLabel_->setText(text);
    statusLabel_->setVisible(!text.isEmpty());
    statusTimer_->start();
}

} // namespace planova::team