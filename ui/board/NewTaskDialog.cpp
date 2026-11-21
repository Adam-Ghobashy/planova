#include "NewTaskDialog.h"
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include "BoardUtil.h"
#include "pins/StyledComboBox.h"

namespace {
QVBoxLayout* field(const QString& text, QWidget* w) {
    auto* box = new QVBoxLayout;
    box->setSpacing(6);
    auto* label = new QLabel(text);
    label->setObjectName("modalFieldLabel");
    box->addWidget(label);
    box->addWidget(w);
    return box;
}

QComboBox* makeCombo() {
    return new planova::s5::StyledComboBox;
}
}

NewTaskDialog::NewTaskDialog(const std::vector<TaskDto>& existing,
                             const std::vector<MemberDto>& members, QWidget* parent)
    : ModalDialog(QCoreApplication::translate("NewTaskDialog", "Create New Task"), parent) {
    QVBoxLayout* root = contentLayout();

    // title
    title_ = new QLineEdit;
    title_->setPlaceholderText(tr("e.g. Implement Strategy Pattern"));
    root->addLayout(field(tr("Task Title"), title_));

    // component + required skill
    component_ = makeCombo();
    component_->addItems({tr("Layered Core Architecture"),
                          tr("Qt Desktop UI & Views"),
                          tr("Chaos Slicer Assignment Engine"),
                          tr("Git-Based Event Sync & Persistence"),
                          tr("Complexity & Velocity Metrics")});

    skill_ = makeCombo();
    const SkillCategory skillOrder[] = {
                                         SkillCategory::Backend, SkillCategory::Frontend, SkillCategory::UiUx,
                                         SkillCategory::Storage, SkillCategory::Integration, SkillCategory::Testing,
                                         SkillCategory::Documentation};
    for (SkillCategory s : skillOrder)
        skill_->addItem(board::skillLabel(s), static_cast<int>(s));

    auto* row1 = new QHBoxLayout;
    row1->setSpacing(16);
    row1->addLayout(field(tr("Component"), component_), 1);
    row1->addLayout(field(tr("Required Skill"), skill_), 1);
    root->addLayout(row1);

    // complexity + priority + assignee
    complexity_ = makeCombo();
    for (int i = 1; i <= 5; ++i) complexity_->addItem(tr("Level %1").arg(i), i);
    complexity_->setCurrentIndex(2);

    priority_ = makeCombo();
    for (int i = 0; i <= static_cast<int>(Priority::Critical); ++i)
        priority_->addItem(board::priorityLabel(static_cast<Priority>(i)), i);
    priority_->setCurrentIndex(static_cast<int>(Priority::Medium));

    assignee_ = makeCombo();
    assignee_->addItem(tr("Unassigned"), QString());
    for (const MemberDto& m : members)
        assignee_->addItem(QString::fromStdString(m.name), QString::fromStdString(m.id));

    auto* row2 = new QHBoxLayout;
    row2->setSpacing(16);
    row2->addLayout(field(tr("Complexity (1-5)"), complexity_), 1);
    row2->addLayout(field(tr("Priority"), priority_), 1);
    row2->addLayout(field(tr("Assignee"), assignee_), 1);
    root->addLayout(row2);

    // estimated hours (needed by the core)
    hours_ = new QSpinBox;
    hours_->setRange(1, 200);
    hours_->setValue(8);
    hours_->setSuffix(tr(" h"));
    hours_->setButtonSymbols(QAbstractSpinBox::NoButtons);

    auto* row3 = new QHBoxLayout;
    row3->setSpacing(16);
    row3->addLayout(field(tr("Estimated Hours"), hours_), 1);
    row3->addStretch(2);
    root->addLayout(row3);

    // dependencies (optional)
    // One check box per open task, in a small scrolling box.
    auto* depsBox = new QScrollArea;
    depsBox->setObjectName("modalDeps");
    depsBox->setWidgetResizable(true);
    depsBox->setMaximumHeight(130);
    depsBox->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    depsBox->viewport()->setObjectName("modalViewport");
    auto* depsHost = new QWidget;
    depsHost->setObjectName("modalDepsHost");
    auto* depsLayout = new QVBoxLayout(depsHost);
    depsLayout->setContentsMargins(8, 6, 8, 6);
    depsLayout->setSpacing(4);
    for (const TaskDto& t : existing) {
        if (t.status == TaskStatus::Done) continue;
        auto* check = new QCheckBox(QString::fromStdString(t.title).replace("&", "&&"));   // "&" alone would be a shortcut mark
        check->setProperty("taskId", QString::fromStdString(t.id));
        depsLayout->addWidget(check);
        depChecks_.push_back(check);
    }
    depsLayout->addStretch(1);
    depsBox->setWidget(depsHost);
    if (!depChecks_.empty())
        root->addLayout(field(tr("Depends on (optional)"), depsBox));
    else
        depsBox->hide();

    error_ = new QLabel;
    error_->setObjectName("modalError");
    error_->setWordWrap(true);
    error_->hide();
    root->addWidget(error_);

    addButton(tr("Cancel"), Role::Secondary);
    addButton(tr("Add Task"), Role::Primary);
}

void NewTaskDialog::accept() {
    if (title_->text().trimmed().isEmpty()) {
        error_->setText(tr("Please enter a task title."));
        error_->show();
        title_->setFocus();
        return;
    }
    ModalDialog::accept();
}

NewTask NewTaskDialog::taskData() const {
    NewTask n{};
    n.title = title_->text().trimmed().toStdString();
    n.skill = static_cast<SkillCategory>(skill_->currentData().toInt());
    n.priority = static_cast<Priority>(priority_->currentData().toInt());
    n.complexity = complexity_->currentData().toInt();
    n.estHours = hours_->value();
    for (const QCheckBox* check : depChecks_) {
        if (check->isChecked())
            n.deps.push_back(check->property("taskId").toString().toStdString());
    }
    return n;
}

std::optional<Id> NewTaskDialog::assignee() const {
    const QString id = assignee_->currentData().toString();
    if (id.isEmpty()) return std::nullopt;
    return id.toStdString();
}

QString NewTaskDialog::component() const {
    return component_->currentText();
}