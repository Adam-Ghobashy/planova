#include "PhaseBoardPage.h"
#include <map>
#include <optional>
#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include "BoardUtil.h"
#include "NewTaskDialog.h"
#include "pins/StyledComboBox.h"
#include "widgets/EmptyHint.h"
#include "widgets/ModalDialog.h"
#include "TaskCardWidget.h"
#include "TaskColumnWidget.h"
#include "app/CoreBridge.h"
#include "planova/PlanovaCore.h"
#include "pins/DeclineDialog.h"

PhaseBoardPage::PhaseBoardPage(PlanovaCore& core, CoreBridge& bridge, QWidget* parent)
    : QWidget(parent), core_(core) {
    setObjectName("PhaseBoardPage");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 32, 36, 28);
    root->setSpacing(28);

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
    modelValue_ = new QLabel(tr("Agile"));
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

    memberFilter_ = new planova::s5::StyledComboBox;   // painted chevron, tinted by the theme
    memberFilter_->setObjectName("BoardMemberFilter");
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

    // Columns (index == static_cast<int>(TaskStatus)), horizontally scrollable like Figma
    const QStringList titles = {tr("To Do"), tr("In Progress"), tr("Review / QA"), tr("Done")};
    const QStringList tones  = {"todo", "progress", "review", "done"};

    auto* host = new QWidget;
    host->setObjectName("BoardColumnsHost");
    auto* cols = new QHBoxLayout(host);
    cols->setContentsMargins(0, 0, 0, 0);
    cols->setSpacing(16);
    for (int i = 0; i < 4; ++i) {
        columns_[i] = new TaskColumnWidget(titles[i], tones[i]);
        cols->addWidget(columns_[i], 1);
        connect(columns_[i], &TaskColumnWidget::advanceRequested, this, &PhaseBoardPage::onAdvance);
        // Queued: the dialog runs its own event loop and the board rebuilds its cards while it is open,
        // so the card that sent the signal must finish its slot first.
        connect(columns_[i], &TaskColumnWidget::declineRequested, this, [this](const Id& taskId) {
            QMetaObject::invokeMethod(this, [this, taskId] { declineTask(taskId); }, Qt::QueuedConnection);
        });
    }

    auto* scroll = new QScrollArea;
    scroll->setObjectName("BoardScroll");
    scroll->setWidgetResizable(true);
    scroll->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus and the area follows them; the area itself would be a Tab stop with no ring
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(host);
    host->setAutoFillBackground(false);
    emptyHint_ = makeEmptyHint(tr("No tasks yet. Use '+ New Task' to add the first one."));
    root->addWidget(emptyHint_);
    root->addWidget(scroll, 1);

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
    emptyHint_->setVisible(tasks.empty());

    QString seModel;
    const auto current = core_.currentProject();
    if (current) {
        for (const auto& project : core_.projects().list()) {
            if (project.id == *current)
                seModel = QString::fromStdString(project.seModel);
        }
    }
    modelValue_->setText(seModel);

    std::map<Id, std::size_t> memberIndex;
    for (std::size_t i = 0; i < members.size(); ++i) memberIndex[members[i].id] = i;

    const QString query = search_->text().trimmed();
    const QString memberId = memberFilter_->currentData().toString();

    // Every card is rebuilt below. If a card or its advance button has the keyboard focus, remember the task so the
    // focus can go back to the same task's new card (it may sit in another column now).
    Id focusedTask;
    bool advanceHadFocus = false;
    if (QWidget* focused = QApplication::focusWidget(); focused && isAncestorOf(focused)) {
        for (QWidget* w = focused; w && w != this; w = w->parentWidget()) {
            if (auto* card = qobject_cast<TaskCardWidget*>(w)) {
                focusedTask = card->taskId();
                advanceHadFocus = focused->objectName() == "AdvanceButton";
                break;
            }
        }
    }

    for (TaskColumnWidget* c : columns_) c->clear();
    int counts[4] = {0, 0, 0, 0};
    TaskCardWidget* focusedCard = nullptr;

    for (const TaskDto& t : tasks) {
        if (!query.isEmpty() &&
            !QString::fromStdString(t.title).contains(query, Qt::CaseInsensitive))
            continue;
        if (!memberId.isEmpty() &&
            (!t.assignee || QString::fromStdString(*t.assignee) != memberId))
            continue;

        QString name, initials;
        std::optional<Token> color;   // no colour: the neutral avatar
        if (t.assignee) {
            auto it = memberIndex.find(*t.assignee);
            if (it != memberIndex.end()) {
                const MemberDto& m = members[it->second];
                name = QString::fromStdString(m.name).section(' ', 0, 0);   // first name only
                initials = board::initialsOf(m.name);
                color = board::memberToken(it->second);
            } else {
                // A finished task keeps its assignee after that member left the team.
                name = tr("Former member");
                initials = QString::fromUtf16(u"–");
            }
        }

        const int col = static_cast<int>(t.status);
        auto* card = new TaskCardWidget(t, name, initials, color, core_.pinQuery().pinCountForTask(t.id));
        columns_[col]->addCard(card);
        ++counts[col];
        if (!focusedTask.empty() && t.id == focusedTask) focusedCard = card;
    }

    for (int i = 0; i < 4; ++i) columns_[i]->setCount(counts[i]);

    if (focusedCard) {
        // The advance button is gone once the task is Done, and the card itself takes focus only when it has an
        // assignee. Whatever of the two exists gets the focus back.
        QWidget* advance = focusedCard->findChild<QToolButton*>("AdvanceButton");
        QWidget* target = (advanceHadFocus && advance) ? advance : focusedCard;
        if (target->focusPolicy() == Qt::NoFocus) target = advance;
        if (target) target->setFocus(Qt::OtherFocusReason);
    }
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
        ModalDialog::info(this, tr("Cannot move task"), QString::fromUtf8(e.what()));
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
        ModalDialog::info(this, tr("Cannot create task"), QString::fromUtf8(e.what()));
    }
}

void PhaseBoardPage::declineTask(const Id& taskId) {
    try {
        const auto task = core_.taskQuery().byId(taskId);
        if (!task || !task->assignee) return;

        QString memberName = QString::fromStdString(*task->assignee);
        for (const MemberDto& m : core_.memberProjection().all()) {
            if (m.id == *task->assignee) memberName = QString::fromStdString(m.name);
        }

        planova::s5::DeclineDialog dlg(taskId, *task->assignee, QString::fromStdString(task->title),
                                       memberName, core_.conflicts(), this);
        connect(&dlg, &planova::s5::DeclineDialog::openSlicerRequested, this,
                &PhaseBoardPage::slicerRequested);
        dlg.exec();                                   // appends events; bridge triggers refresh()
    } catch (const std::exception& e) {
        ModalDialog::info(this, tr("Cannot decline task"), QString::fromUtf8(e.what()));
    }
}
