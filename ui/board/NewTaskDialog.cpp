#include "NewTaskDialog.h"
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include "BoardUtil.h"

namespace {
QVBoxLayout* field(const QString& text, QWidget* w) {
    auto* box = new QVBoxLayout;
    box->setSpacing(8);
    auto* label = new QLabel(text);
    label->setObjectName("FieldLabel");
    box->addWidget(label);
    box->addWidget(w);
    return box;
}

QComboBox* makeCombo() {
    auto* c = new QComboBox;
    c->setView(new QListView(c));      // lets the stylesheet control item height / look
    return c;
}
}

NewTaskDialog::NewTaskDialog(const std::vector<TaskDto>& existing,
                             const std::vector<MemberDto>& members, QWidget* parent)
    : QDialog(parent) {
    setObjectName("NewTaskDialog");
    setWindowTitle(tr("Create New Task"));
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(780, 540);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame;
    card->setObjectName("DialogCard");
    outer->addWidget(card);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    // ---------- header
    auto* header = new QFrame;
    header->setObjectName("DialogHeader");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(36, 26, 28, 22);
    auto* heading = new QLabel(tr("Create New Task"));
    heading->setObjectName("DialogTitle");
    auto* close = new QToolButton;
    close->setObjectName("DialogClose");
    close->setText(QString::fromUtf16(u"\u00D7"));
    close->setCursor(Qt::PointingHandCursor);
    hl->addWidget(heading);
    hl->addStretch(1);
    hl->addWidget(close);
    cardLayout->addWidget(header);

    // ---------- scrollable body
    auto* scroll = new QScrollArea;
    scroll->setObjectName("DialogScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setObjectName("DialogViewport");
    scroll->viewport()->setAutoFillBackground(false);

    auto* bodyWidget = new QWidget;
    bodyWidget->setObjectName("DialogBody");
    auto* root = new QVBoxLayout(bodyWidget);
    root->setContentsMargins(36, 24, 36, 24);
    root->setSpacing(22);

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
    row1->setSpacing(20);
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
    row2->setSpacing(20);
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
    row3->setSpacing(20);
    row3->addLayout(field(tr("Estimated Hours"), hours_), 1);
    row3->addStretch(2);
    root->addLayout(row3);

    // dependencies (optional)
    deps_ = new QListWidget;
    deps_->setObjectName("DepsList");
    deps_->setMaximumHeight(130);
    for (const TaskDto& t : existing) {
        if (t.status == TaskStatus::Done) continue;
        auto* item = new QListWidgetItem(QString::fromStdString(t.title), deps_);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, QString::fromStdString(t.id));
    }
    if (deps_->count() > 0)
        root->addLayout(field(tr("Depends on (optional)"), deps_));
    else
        deps_->hide();

    error_ = new QLabel;
    error_->setObjectName("FormError");
    error_->setWordWrap(true);
    error_->hide();
    root->addWidget(error_);
    root->addStretch(1);

    scroll->setWidget(bodyWidget);
    bodyWidget->setAutoFillBackground(false);
    cardLayout->addWidget(scroll, 1);

    // ---------- footer
    auto* footer = new QFrame;
    footer->setObjectName("DialogFooter");
    auto* fl = new QHBoxLayout(footer);
    fl->setContentsMargins(36, 22, 36, 22);
    fl->setSpacing(14);
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setObjectName("BoardSecondary");
    cancel->setCursor(Qt::PointingHandCursor);
    auto* add = new QPushButton(tr("Add Task"));
    add->setObjectName("BoardPrimary");
    add->setCursor(Qt::PointingHandCursor);
    add->setDefault(true);
    fl->addStretch(1);
    fl->addWidget(cancel);
    fl->addWidget(add);
    cardLayout->addWidget(footer);

    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(add, &QPushButton::clicked, this, &NewTaskDialog::validateAndAccept);
}

void NewTaskDialog::validateAndAccept() {
    if (title_->text().trimmed().isEmpty()) {
        error_->setText(tr("Please enter a task title."));
        error_->show();
        title_->setFocus();
        return;
    }
    accept();
}

NewTask NewTaskDialog::taskData() const {
    NewTask n{};
    n.title = title_->text().trimmed().toStdString();
    n.skill = static_cast<SkillCategory>(skill_->currentData().toInt());
    n.priority = static_cast<Priority>(priority_->currentData().toInt());
    n.complexity = complexity_->currentData().toInt();
    n.estHours = hours_->value();
    for (int i = 0; i < deps_->count(); ++i) {
        QListWidgetItem* item = deps_->item(i);
        if (item->checkState() == Qt::Checked)
            n.deps.push_back(item->data(Qt::UserRole).toString().toStdString());
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