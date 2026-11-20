#include "PhaseBoardPage.h"
#include <map>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include "BoardUtil.h"
#include "NewTaskDialog.h"
#include "TaskCardWidget.h"
#include "TaskColumnWidget.h"
#include "app/CoreBridge.h"
#include "app/DemoData.h"
#include "planova/PlanovaCore.h"

PhaseBoardPage::PhaseBoardPage(PlanovaCore& core, CoreBridge& bridge, QWidget* parent)
    : QWidget(parent), core_(core) {
    setObjectName("PhaseBoardPage");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 32, 36, 28);
    root->setSpacing(28);

    // ---- header
    auto* header = new QHBoxLayout;
    header->setSpacing(14);

    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(6);
    auto* title = new QLabel(tr("Phase Board"));
    title->setObjectName("PageTitle");

    auto* meta = new QHBoxLayout;
    meta->setSpacing(6);
    auto* modelLabel = new QLabel(tr("SE Model:"));
    modelLabel->setObjectName("ModelLabel");
    modelValue_ = new QLabel(tr("Agile/Scrum"));
    modelValue_->setObjectName("ModelValue");
    taskCount_ = new QLabel;
    taskCount_->setObjectName("TaskCountLabel");
    meta->addWidget(modelLabel);
    meta->addWidget(modelValue_);
    meta->addSpacing(4);
    meta->addWidget(taskCount_);
    meta->addStretch(1);

    titleBox->addWidget(title);
    titleBox->addLayout(meta);

    search_ = new QLineEdit;
    search_->setObjectName("BoardSearch");
    search_->setPlaceholderText(tr("Search tasks..."));
    search_->setClearButtonEnabled(true);
    search_->setFixedWidth(280);

    memberFilter_ = new QComboBox;
    memberFilter_->setObjectName("BoardMemberFilter");
    memberFilter_->setView(new QListView(memberFilter_));
    memberFilter_->setMinimumWidth(190);

    newTask_ = new QPushButton(tr("+ New Task"));
    newTask_->setObjectName("BoardPrimary");
    newTask_->setCursor(Qt::PointingHandCursor);

    header->addLayout(titleBox);
    header->addStretch(1);
    header->addWidget(search_, 0, Qt::AlignVCenter);
    header->addWidget(memberFilter_, 0, Qt::AlignVCenter);
    header->addWidget(newTask_, 0, Qt::AlignVCenter);
    root->addLayout(header);

    // ---- columns (index == static_cast<int>(TaskStatus)), horizontally scrollable like Figma
    const QStringList titles = {tr("To Do"), tr("In Progress"), tr("Review / QA"), tr("Done")};
    const QStringList tones  = {"todo", "progress", "review", "done"};

    auto* host = new QWidget;
    host->setObjectName("BoardColumnsHost");
    auto* cols = new QHBoxLayout(host);
    cols->setContentsMargins(0, 0, 0, 0);
    cols->setSpacing(24);
    for (int i = 0; i < 4; ++i) {
        columns_[i] = new TaskColumnWidget(titles[i], tones[i]);
        cols->addWidget(columns_[i], 1);
        connect(columns_[i], &TaskColumnWidget::advanceRequested, this, &PhaseBoardPage::onAdvance);
    }

    auto* scroll = new QScrollArea;
    scroll->setObjectName("BoardScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(host);
    host->setAutoFillBackground(false);
    root->addWidget(scroll, 1);

    // ---- wiring
    connect(&bridge, &CoreBridge::changed, this, [this] { refresh(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(memberFilter_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(newTask_, &QPushButton::clicked, this, &PhaseBoardPage::onNewTask);

    refresh();
}

void PhaseBoardPage::rebuildMemberFilter(const std::vector<MemberDto>& members) {
    const QString current = memberFilter_->currentData().toString();
    QSignalBlocker blocker(memberFilter_);
    memberFilter_->clear();
    memberFilter_->addItem(tr("All Members"), QString());
    for (const MemberDto& m : members)
        memberFilter_->addItem(QString::fromStdString(m.name), QString::fromStdString(m.id));
    const int idx = memberFilter_->findData(current);
    memberFilter_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void PhaseBoardPage::refresh() {
    const std::vector<TaskDto> tasks = core_.taskQuery().all();
    const std::vector<MemberDto> members = core_.memberProjection().all();
    rebuildMemberFilter(members);

    const auto projects = core_.projects().list();
    if (!projects.empty() && !projects.front().seModel.empty())
        modelValue_->setText(QString::fromStdString(projects.front().seModel));

    std::map<Id, std::size_t> memberIndex;
    for (std::size_t i = 0; i < members.size(); ++i) memberIndex[members[i].id] = i;

    const QString query = search_->text().trimmed();
    const QString memberId = memberFilter_->currentData().toString();

    for (TaskColumnWidget* c : columns_) c->clear();
    int counts[4] = {0, 0, 0, 0};

    for (const TaskDto& t : tasks) {
        if (!query.isEmpty() &&
            !QString::fromStdString(t.title).contains(query, Qt::CaseInsensitive))
            continue;
        if (!memberId.isEmpty() &&
            (!t.assignee || QString::fromStdString(*t.assignee) != memberId))
            continue;

        QString name, initials;
        Token color = Token::Member1;
        if (t.assignee) {
            auto it = memberIndex.find(*t.assignee);
            if (it != memberIndex.end()) {
                const MemberDto& m = members[it->second];
                name = QString::fromStdString(m.name).section(' ', 0, 0);   // first name only
                initials = board::initialsOf(m.name);
                color = board::memberToken(it->second);
            } else {
                name = QString::fromStdString(*t.assignee);
                initials = "?";
            }
        }

        const int col = static_cast<int>(t.status);
        columns_[col]->addCard(
            new TaskCardWidget(t, name, initials, color, demo::pinCountFor(t.title)));
        ++counts[col];
    }

    for (int i = 0; i < 4; ++i) columns_[i]->setCount(counts[i]);
    taskCount_->setText(QString::fromUtf16(u"\u2022") +
                        tr("  %1 tasks").arg(static_cast<int>(tasks.size())));
}

void PhaseBoardPage::onAdvance(const Id& taskId) {
    const auto task = core_.taskQuery().byId(taskId);
    if (!task) return;

    TaskStatus to;
    if (!board::nextStatus(task->status, to)) return;

    try {
        core_.tasks().move(taskId, to);              // appends TaskMoved; bridge triggers refresh()
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Cannot move task"), QString::fromUtf8(e.what()));
    }
}

void PhaseBoardPage::onNewTask() {
    NewTaskDialog dlg(core_.taskQuery().all(), core_.memberProjection().all(), this);
    if (dlg.exec() != QDialog::Accepted) return;

    try {
        const Id id = core_.tasks().create(dlg.taskData());
        if (const auto member = dlg.assignee())
            core_.tasks().assign(id, *member, "board");
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Cannot create task"), QString::fromUtf8(e.what()));
    }
}