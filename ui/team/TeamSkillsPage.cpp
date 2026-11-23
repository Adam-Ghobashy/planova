#include "team/TeamSkillsPage.h"
#include "planova/common/EventTypes.h"
#include <QApplication>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QScrollArea>
#include <QSlider>
#include <QStyle>
#include "theme/ThemeManager.h"
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
    {SkillCategory::UiUx, "UI/UX"},
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

// Table columns. Profile, Understanding and Assignment have fixed widths; Technical Skills takes the rest.
// Three skill cells (five 24 px steps each) fit the skills column down to the default 1280 px window.
constexpr int kProfileCol = 0, kSkillsCol = 1, kUnderstandingCol = 2, kAssignmentCol = 3;
constexpr int kProfileWidth = 205;
constexpr int kMinUnderstandingWidth = 140;
constexpr int kHeaderTitleSlack = 4;   // a few pixels of air after the header title
constexpr int kUnderstandingSaveDelayMs = 500;   // pause after the last key press before the value is saved
constexpr int kAssignmentWidth = 160;
constexpr int kColumnGap = 10;

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
    scrollArea->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus and the area follows them; the area itself would be a Tab stop with no ring
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
    pageTitle->setFont(ThemeManager::instance().textFont(TextStyle::PageTitle));

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

    // Keyboard and wheel steps on an Understanding slider are saved once the user pauses.
    understandingTimer_ = new QTimer(this);
    understandingTimer_->setSingleShot(true);
    understandingTimer_->setInterval(kUnderstandingSaveDelayMs);
    connect(understandingTimer_, &QTimer::timeout, this, &TeamSkillsPage::commitUnderstanding);

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
        // Big numbers keep their size (20 px) in the Display family.
        QFont vf = ThemeManager::instance().font(FontRole::Display, 20);
        vf.setWeight(QFont::DemiBold);
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
        "Add a new member to the team.",
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

    // One header row for the whole table (hidden while there are no members).
    tableHeader_ = buildTableHeader(contentWidget);
    auto* tableLayout = new QVBoxLayout();
    tableLayout->setSpacing(10);
    tableLayout->addWidget(tableHeader_);

    cardsLayout_ = new QVBoxLayout();
    cardsLayout_->setSpacing(14);
    tableLayout->addLayout(cardsLayout_);
    mainLayout->addLayout(tableLayout);

    emptyLabel_ = new QLabel(
        "No team members yet. Add the first one with the '+ Add Member' button above.",
        contentWidget);
    emptyLabel_->setObjectName("emptyLabel");
    emptyLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(emptyLabel_);

    mainLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);

    connect(&bridge_, &CoreBridge::changed, this, [this](const QString& eventType) {
        // A waiting value belongs to the project that was open when it was typed. refresh() would save it into
        // the newly opened project, so it is dropped first.
        if (eventType == QString::fromUtf8(ProjectOpened)) discardUnderstanding();
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
    button->setProperty("settingKey", settingKey);
    button->setProperty("switchTitle", title);
    showSwitch(button, members_.getSetting(settingKey.toStdString(), false));
    connect(button, &QPushButton::toggled, this, [this, button, settingKey](bool on) {
        showSwitch(button, on);
        try {
            service_.setSetting(settingKey.toStdString(), on);
        } catch (const exception& e) {
            showSwitch(button, !on);   // the setting was not saved, so the switch goes back
            showStatus(QString::fromUtf8(e.what()));
        }
    });
    switches_.push_back(button);
    return button;
}

void TeamSkillsPage::showSwitch(QPushButton* button, bool on) {
    // No toggled signal: showing a setting must not save it again.
    const QSignalBlocker blocker(button);
    button->setChecked(on);
    button->setText(QString("%1: %2").arg(button->property("switchTitle").toString(), on ? "On" : "Off"));
}

void TeamSkillsPage::syncSwitches() {
    // The settings belong to the open project, so the switches are read again after every project change.
    for (QPushButton* button : switches_) {
        const string key = button->property("settingKey").toString().toStdString();
        showSwitch(button, members_.getSetting(key, false));
    }
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

void TeamSkillsPage::applyColumnWidths(QGridLayout* grid) const {
    // The same widths are used by the header row and by every member row, so the columns line up.
    // Technical Skills takes whatever is left.
    grid->setColumnMinimumWidth(kProfileCol, kProfileWidth);
    grid->setColumnMinimumWidth(kUnderstandingCol, understandingWidth_);
    grid->setColumnMinimumWidth(kAssignmentCol, kAssignmentWidth);
    grid->setColumnStretch(kSkillsCol, 1);
}

QWidget* TeamSkillsPage::buildTableHeader(QWidget* parent) {
    auto* header = new QWidget(parent);
    header->setObjectName("teamTableHeader");
    auto* grid = new QGridLayout(header);
    // 1 px card border + 20 px card padding, so the titles start where the row content starts.
    grid->setContentsMargins(21, 0, 21, 0);
    grid->setHorizontalSpacing(kColumnGap);

    const char* titles[] = {"Role / Teammate", "Technical Skills", "Understanding & Velocity",
                            "Assignment / Clash"};
    QLabel* labels[4];
    for (int col = 0; col < 4; ++col) {
        labels[col] = new QLabel(titles[col], header);
        labels[col]->setObjectName("tableHeaderCell");
    }

    // The Understanding column is as wide as its title needs (never below the default). Polishing first gives the
    // label the font from team.qss, so the measured text is the text that is drawn.
    QLabel* understandingTitle = labels[kUnderstandingCol];
    understandingTitle->ensurePolished();
    const int titleWidth = understandingTitle->fontMetrics().horizontalAdvance(understandingTitle->text());
    understandingWidth_ = qMax(kMinUnderstandingWidth, titleWidth + kHeaderTitleSlack);
    applyColumnWidths(grid);

    const int fixedWidths[] = {kProfileWidth, 0, understandingWidth_, kAssignmentWidth};
    for (int col = 0; col < 4; ++col) {
        if (fixedWidths[col] > 0) labels[col]->setFixedWidth(fixedWidths[col]);
        grid->addWidget(labels[col], 0, col, Qt::AlignBottom);
    }
    return header;
}

QWidget* TeamSkillsPage::buildAssignmentColumn(const MemberDto& member, QWidget* parent) {
    auto* column = new QWidget(parent);
    column->setObjectName("assignmentColumn");
    column->setFixedWidth(kAssignmentWidth);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    QStringList titles;
    for (const TaskDto& task : tasks_.all()) {
        if (task.assignee && *task.assignee == member.id && task.status != TaskStatus::Done)
            titles << QString::fromStdString(task.title);
    }
    auto* current = new QLabel(titles.isEmpty() ? QString::fromUtf8("—") : titles.join("\n"), column);
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

QWidget* TeamSkillsPage::buildProfileColumn(const MemberDto& member, int colorIndex, QWidget* parent) {
    auto* column = new QWidget(parent);
    column->setObjectName("profileColumn");
    column->setFixedWidth(kProfileWidth);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto* top = new QHBoxLayout();   // avatar and identity; the buttons below get the full column width
    top->setSpacing(10);

    auto* avatar = new QLabel(initialsFor(member.name), column);
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setObjectName("memberAvatar");
    // team.qss maps memberColor 1..6 to the Member1..Member6 colours.
    avatar->setProperty("memberColor", colorIndex % 6 + 1);
    top->addWidget(avatar, 0, Qt::AlignTop);

    auto* info = new QVBoxLayout();
    info->setSpacing(4);

    auto* nameLbl = new QLabel(QString::fromStdString(member.name), column);
    nameLbl->setObjectName("cardTitle");
    nameLbl->setWordWrap(true);
    info->addWidget(nameLbl);
    if (member.isLead) {
        auto* leadBadge = new QLabel(QString::fromUtf8("Lead 👑"), column);
        leadBadge->setObjectName("leadBadge");
        info->addWidget(leadBadge, 0, Qt::AlignLeft);
    }

    auto* roleLbl = new QLabel(QString::fromStdString(member.role), column);
    roleLbl->setObjectName("cardSubtitle");
    roleLbl->setWordWrap(true);
    info->addWidget(roleLbl);

    // Edit Role and Remove, hidden while the role is being edited.
    auto* actions = new QWidget(column);
    actions->setObjectName("roleActions");
    auto* actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(6);
    auto* editRoleBtn = new QPushButton("✎ Edit Role", actions);
    editRoleBtn->setCursor(Qt::PointingHandCursor);
    editRoleBtn->setToolTip("Change this member's role");
    editRoleBtn->setObjectName("editRoleButton");
    editRoleBtn->setProperty("focusKey", QString::fromStdString(member.id) + ":editRole");
    editRoleBtn->setMinimumHeight(24);   // pointer target of at least 24 px
    auto* removeBtn = new QPushButton("✕ Remove", actions);
    removeBtn->setCursor(Qt::PointingHandCursor);
    removeBtn->setToolTip("Remove member from the team");
    removeBtn->setObjectName("removeMemberButton");
    removeBtn->setMinimumHeight(24);
    actionsLayout->addWidget(editRoleBtn);
    actionsLayout->addWidget(removeBtn);
    actionsLayout->addStretch();
    layout->addWidget(actions);

    // Inline role editor: a dropdown of known roles, a custom text input and Save / Cancel.
    auto* editBox = new QWidget(column);
    editBox->setObjectName("roleEditBox");
    auto* editLayout = new QVBoxLayout(editBox);
    editLayout->setContentsMargins(0, 0, 0, 0);
    editLayout->setSpacing(6);
    auto* editRoleCombo = new QComboBox(editBox);
    editRoleCombo->setObjectName("roleCombo");
    editRoleCombo->setAccessibleName("Role");
    editRoleCombo->addItems(kKnownRoles);
    editLayout->addWidget(editRoleCombo);
    auto* editCustomRoleInput = new QLineEdit(editBox);
    editCustomRoleInput->setObjectName("lightInput");
    editCustomRoleInput->setAccessibleName("Custom role");
    editCustomRoleInput->setPlaceholderText("Write custom role...");
    editCustomRoleInput->hide();
    editLayout->addWidget(editCustomRoleInput);
    auto* editButtons = new QHBoxLayout();
    editButtons->setSpacing(6);
    auto* saveRoleBtn = new QPushButton("Save", editBox);
    saveRoleBtn->setCursor(Qt::PointingHandCursor);
    saveRoleBtn->setObjectName("saveRoleButton");
    saveRoleBtn->setMinimumHeight(24);
    auto* cancelRoleBtn = new QPushButton("Cancel", editBox);
    cancelRoleBtn->setCursor(Qt::PointingHandCursor);
    cancelRoleBtn->setObjectName("cancelRoleButton");
    cancelRoleBtn->setMinimumHeight(24);
    editButtons->addWidget(saveRoleBtn);
    editButtons->addWidget(cancelRoleBtn);
    editButtons->addStretch();
    editLayout->addLayout(editButtons);
    editBox->hide();
    layout->addWidget(editBox);

    connect(editRoleCombo, &QComboBox::currentTextChanged, [editCustomRoleInput](const QString& text) {
        if (text == "Other..." || text == "Other") {
            editCustomRoleInput->show();
            editCustomRoleInput->setFocus();
            editCustomRoleInput->selectAll();
        } else {
            editCustomRoleInput->hide();
        }
    });

    connect(editRoleBtn, &QPushButton::clicked, this, [=]() {
        roleLbl->hide();
        actions->hide();

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

        editBox->show();
        if (editCustomRoleInput->isVisible()) {
            editCustomRoleInput->setFocus();
            editCustomRoleInput->selectAll();
        } else {
            editRoleCombo->setFocus();
        }
    });

    connect(cancelRoleBtn, &QPushButton::clicked, this, [=]() {
        editBox->hide();
        roleLbl->show();
        actions->show();
        editRoleBtn->setFocus();
        // An understanding commit was put off while a role editor was open. Another open editor still holds it off.
        if (pendingRefresh_ && !roleEditorOpen()) refresh();
    });

    auto doSaveRole = [this, id = member.id, name = member.name, editRoleCombo, editCustomRoleInput]() {
        QString newRole = editRoleCombo->currentText();
        if (newRole == "Other..." || newRole == "Other") {
            newRole = editCustomRoleInput->text().trimmed();
        }
        if (newRole.isEmpty()) {
            newRole = "Developer";
        }
        // Read the stored value, not the one from when the card was built: while this editor was open an
        // understanding change may have been saved without a rebuild.
        flushUnderstanding();   // a waiting slider value must not be saved later with the old role
        int und = 0;
        for (const MemberDto& m : members_.all())
            if (m.id == id) und = m.understanding;
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
            QString("This removes %1 from the team. Their open tasks go back to unassigned.")
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

    top->addLayout(info, 1);
    layout->insertLayout(0, top);
    return column;
}

QWidget* TeamSkillsPage::buildSkillsColumn(const MemberDto& member, QWidget* parent) {
    auto* column = new QWidget(parent);
    column->setObjectName("skillsColumn");
    auto* grid = new QGridLayout(column);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    for (int i = 0; i < 7; ++i) {
        const auto& meta = kAllSkills[i];
        int currentRating = 3;
        auto it = member.ratings.find(meta.category);
        if (it != member.ratings.end()) {
            currentRating = it->second;
        }

        auto* cellWidget = new QWidget(column);
        cellWidget->setObjectName("skillCell");
        // Label above, five 24 px steps below; three cells per row keep the column narrow enough for 1280 px.
        auto* cellLayout = new QVBoxLayout(cellWidget);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(2);
        auto* stepsLayout = new QHBoxLayout();
        stepsLayout->setSpacing(1);

        auto* skillLbl = new QLabel(meta.label, cellWidget);
        skillLbl->setObjectName("skillLabel");
        cellLayout->addWidget(skillLbl);
        cellLayout->addLayout(stepsLayout);

        for (int r = 1; r <= 5; ++r) {
            auto* rateBtn = new QPushButton(QString::number(r), cellWidget);
            rateBtn->setFixedSize(24, 24);
            rateBtn->setCursor(Qt::PointingHandCursor);
            // The step number is the text; the name says which skill and member it belongs to.
            rateBtn->setAccessibleName(QString("%1 rating %2 of 5 for %3")
                                           .arg(meta.label).arg(r).arg(QString::fromStdString(member.name)));
            const bool active = (r <= currentRating);
            rateBtn->setObjectName("ratingButton");
            // Member + skill + step: the five steps of one skill must not share a key.
            rateBtn->setProperty("focusKey", QString("%1:%2:%3")
                                                 .arg(QString::fromStdString(member.id))
                                                 .arg(static_cast<int>(meta.category))
                                                 .arg(r));
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
            stepsLayout->addWidget(rateBtn);
        }
        stepsLayout->addStretch();
        grid->addWidget(cellWidget, i / 3, i % 3);
    }
    return column;
}

QWidget* TeamSkillsPage::buildUnderstandingColumn(const MemberDto& member, QWidget* parent) {
    auto* column = new QWidget(parent);
    column->setObjectName("understandingColumn");
    column->setFixedWidth(understandingWidth_);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // Project understanding: editable, 0 - 100 %.
    auto* undHeader = new QHBoxLayout();
    auto* undTitle = new QLabel("Understanding", column);
    undTitle->setObjectName("meterLabel");
    auto* undVal = new QLabel(QString("%1%").arg(member.understanding), column);
    undVal->setObjectName("sliderValueLabel");
    undHeader->addWidget(undTitle);
    undHeader->addStretch();
    undHeader->addWidget(undVal);

    auto* undSlider = new QSlider(Qt::Horizontal, column);
    undSlider->setRange(0, 100);
    undSlider->setValue(member.understanding);
    undSlider->setAccessibleName(QString("Project understanding of %1").arg(QString::fromStdString(member.name)));

    // refresh() rebuilds the card and finds the slider again by its focus key to give it back the keyboard focus.
    undSlider->setObjectName("understandingSlider");
    undSlider->setProperty("focusKey", QString::fromStdString(member.id) + ":understanding");

    connect(undSlider, &QSlider::valueChanged, this,
            [this, undVal, id = member.id, role = member.role, undSlider](int v) {
                undVal->setText(QString("%1%").arg(v));
                // While the mouse drags, the release saves. Keys, wheel and a click on the groove arrive here.
                if (!undSlider->isSliderDown()) queueUnderstanding(id, role, v);
            });
    connect(undSlider, &QSlider::sliderReleased, this,
            [this, id = member.id, role = member.role, undSlider]() {
                queueUnderstanding(id, role, undSlider->value());
                commitUnderstanding();
            });
    auto* undBlock = new QVBoxLayout();
    undBlock->setSpacing(4);
    undBlock->addLayout(undHeader);
    undBlock->addWidget(undSlider);
    layout->addLayout(undBlock);

    // Velocity (EMA, 0.25x to 5x): read-only, two decimals, bar scaled to the 5x maximum.
    auto* velHeader = new QHBoxLayout();
    auto* velTitle = new QLabel("Velocity", column);
    velTitle->setObjectName("meterLabel");
    auto* velVal = new QLabel(QString("%1x").arg(member.velocity, 0, 'f', 2), column);
    velVal->setObjectName("velocityValue");
    velHeader->addWidget(velTitle);
    velHeader->addStretch();
    velHeader->addWidget(velVal);

    auto* velBar = new QProgressBar(column);
    velBar->setObjectName("velocityBar");
    velBar->setRange(0, 100);
    velBar->setValue(qBound(0, qRound(member.velocity / 5.0 * 100.0), 100));
    velBar->setTextVisible(false);
    velBar->setFixedHeight(6);
    velBar->setAccessibleName(QString("Velocity of %1").arg(QString::fromStdString(member.name)));
    velBar->setAccessibleDescription(velVal->text());
    auto* velBlock = new QVBoxLayout();
    velBlock->setSpacing(4);
    velBlock->addLayout(velHeader);
    velBlock->addWidget(velBar);
    layout->addLayout(velBlock);
    return column;
}

QFrame* TeamSkillsPage::buildMemberCard(const MemberDto& member, int colorIndex) {
    auto* card = new QFrame(this);
    card->setObjectName("card");
    auto* grid = new QGridLayout(card);
    grid->setContentsMargins(20, 18, 20, 18);
    grid->setHorizontalSpacing(kColumnGap);
    applyColumnWidths(grid);
    grid->addWidget(buildProfileColumn(member, colorIndex, card), 0, kProfileCol, Qt::AlignVCenter);
    grid->addWidget(buildSkillsColumn(member, card), 0, kSkillsCol, Qt::AlignVCenter);
    grid->addWidget(buildUnderstandingColumn(member, card), 0, kUnderstandingCol, Qt::AlignVCenter);
    grid->addWidget(buildAssignmentColumn(member, card), 0, kAssignmentCol, Qt::AlignVCenter);
    return card;
}

void TeamSkillsPage::refresh() {
    // A value still waiting for its pause is saved first, so a rebuild never throws a keyboard change away.
    flushUnderstanding();

    pendingRefresh_ = false;
    syncSwitches();

    // The cards are rebuilt, so the control the user is working with (slider, rating step or Edit Role button) is
    // remembered by its "focusKey" and focused again below.
    QString focusedKey;
    if (const QWidget* focused = QApplication::focusWidget(); focused && isAncestorOf(focused))
        focusedKey = focused->property("focusKey").toString();

    while (QLayoutItem* item = cardsLayout_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const vector<MemberDto> allMembers = members_.all();
    emptyLabel_->setVisible(allMembers.empty());
    tableHeader_->setVisible(!allMembers.empty());

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
        QFrame* card = buildMemberCard(m, static_cast<int>(i));
        cardsLayout_->addWidget(card);
        if (!focusedKey.isEmpty() && focusedKey.startsWith(QString::fromStdString(m.id) + ":")) {
            for (QWidget* w : card->findChildren<QWidget*>()) {
                if (w->property("focusKey").toString() == focusedKey) {
                    w->setFocus(Qt::OtherFocusReason);
                    break;
                }
            }
        }
    }

    const double count = static_cast<double>(allMembers.size());
    teamLeadValue_->setText(leadName);
    avgUnderstandingValue_->setText(
        QString("%1%").arg( qRound(sumUnderstanding / count) ));
    avgVelocityValue_->setText(
        QString::number(sumVelocity / count, 'f', 2));
}

TeamSkillsPage::~TeamSkillsPage() {
    flushUnderstanding();   // catches its own exceptions; the children are still alive here
}

void TeamSkillsPage::hideEvent(QHideEvent* event) {
    flushUnderstanding();
    QWidget::hideEvent(event);
}

void TeamSkillsPage::queueUnderstanding(const Id& memberId, const string& role, int value) {
    // The single waiting slot belongs to one member: save it before another member takes the slot.
    if (pending_.valid && pending_.memberId != memberId) flushUnderstanding();
    pending_ = {memberId, role, value, true};
    understandingTimer_->start();   // every step restarts the wait, so holding an arrow key saves once at the end
}

bool TeamSkillsPage::flushUnderstanding() {
    understandingTimer_->stop();
    if (!pending_.valid) return true;
    const PendingUnderstanding toSave = pending_;
    pending_.valid = false;

    updating_ = true;   // our own change must not trigger a second refresh through CoreBridge::changed
    try {
        service_.update(toSave.memberId, toSave.role, toSave.value);
    } catch (const exception& e) {
        updating_ = false;
        showStatus(QString::fromUtf8(e.what()));
        return false;
    }
    updating_ = false;
    return true;
}

void TeamSkillsPage::discardUnderstanding() {
    understandingTimer_->stop();
    pending_.valid = false;
}

void TeamSkillsPage::commitUnderstanding() {
    const bool saved = flushUnderstanding();
    // The rebuild would delete a role editor the user is typing in, so it waits until the editor closes.
    if (roleEditorOpen()) pendingRefresh_ = true;
    else refresh();
    if (saved) showStatus("Updated project understanding");
}

bool TeamSkillsPage::roleEditorOpen() const {
    for (int i = 0; i < cardsLayout_->count(); ++i) {
        const QWidget* card = cardsLayout_->itemAt(i)->widget();
        const QWidget* editor = card ? card->findChild<QWidget*>("roleEditBox") : nullptr;
        if (editor && !editor->isHidden()) return true;
    }
    return false;
}

void TeamSkillsPage::showStatus(const QString& text) {
    statusLabel_->setText(text);
    statusLabel_->setVisible(!text.isEmpty());
    statusTimer_->start();
}

} // namespace planova::team