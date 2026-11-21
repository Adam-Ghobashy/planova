#include "PinsPage.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <exception>

#include "LeavePinDialog.h"
#include "widgets/ModalDialog.h"
#include "PinCardWidget.h"
#include "PinColumnWidget.h"
#include "PinUiHelpers.h"
#include "widgets/EmptyHint.h"

namespace planova::s5 {

namespace {
const PinCategory kColumnOrder[] = {PinCategory::HandoffNote, PinCategory::FeedbackBlocker,
                                    PinCategory::ReadyForTesting, PinCategory::ArchitectureDecision};
constexpr int kStaggerMs = 40;
constexpr int kMaxStagger = 8;
}  // namespace

PinsPage::PinsPage(IPinBoardQuery& query, IPinCommands& commands, IMemberQuery& members,
                   ITaskQuery& tasks, QWidget* parent)
    : QWidget(parent), query_(query), commands_(commands), memberQuery_(members), taskQuery_(tasks)
{
    setObjectName("PinsPage");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    // Header
    auto* header = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto* title = new QLabel(tr("Pins & Team Handoffs"), this);
    title->setObjectName("PinsTitle");
    auto* subtitle = new QLabel(tr("Context-rich notes for handoffs, blockers, testing and decisions."), this);
    subtitle->setObjectName("PinsSubtitle");
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles, 1);

    auto* leave = new QPushButton(tr("Leave a Pin"), this);
    leave->setObjectName("LeavePinButton");
    leave->setCursor(Qt::PointingHandCursor);
    header->addWidget(leave);
    root->addLayout(header);

    // Filter chips
    chipBar_ = new QWidget(this);
    chipBar_->setObjectName("PinChipBar");
    chipLayout_ = new QHBoxLayout(chipBar_);
    chipLayout_->setContentsMargins(0, 0, 0, 0);
    chipLayout_->setSpacing(8);
    chipGroup_ = new QButtonGroup(this);
    chipGroup_->setExclusive(true);
    root->addWidget(chipBar_);

    emptyHint_ = makeEmptyHint(tr("No pins yet. Use 'Leave a Pin' to add a handoff, blocker, test request or decision."), this);
    root->addWidget(emptyHint_);

    // Columns
    auto* cols = new QHBoxLayout;
    cols->setSpacing(14);
    for (PinCategory c : kColumnOrder) {
        auto* col = new PinColumnWidget(c, categoryTitle(c), this);
        connect(col, &PinColumnWidget::pinDropped, this, &PinsPage::onDropped);
        columns_.push_back(col);
        cols->addWidget(col, 1);
    }
    root->addLayout(cols, 1);

    connect(leave, &QPushButton::clicked, this, &PinsPage::onLeavePin);

    refresh();
}

PinsPage::~PinsPage() { *alive_ = false; }

Id PinsPage::actorFor(const Id& pinId) const
{
    const auto it = viewById_.find(pinId);
    return it != viewById_.end() ? it->second.pin.authorId : Id{};
}

void PinsPage::scheduleRefresh()
{
    if (refreshQueued_) return;
    refreshQueued_ = true;
    QTimer::singleShot(0, this, [this] {
        refreshQueued_ = false;
        refresh();
    });
}

void PinsPage::runCommand(const std::function<void()>& fn)
{
    try {
        fn();
    } catch (const std::exception& e) {
        ModalDialog::info(this, tr("Pins"), QString::fromUtf8(e.what()));
    }
    scheduleRefresh();
    emit pinsChanged();
}

void PinsPage::addChip(const QString& text, const Id& memberId, int memberIndex)
{
    auto* b = new QPushButton(text, chipBar_);
    b->setObjectName("PinFilterChip");
    b->setCheckable(true);
    b->setChecked(memberId == filterMemberId_);
    b->setCursor(Qt::PointingHandCursor);
    b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);  // keep chips at text width
    if (memberIndex > 0) {
        b->setProperty("dot", true);
        auto* dot = new QLabel(b);
        dot->setObjectName("ChipDot");
        dot->setProperty("member", QString::number(memberIndex));
        dot->setFixedSize(9, 9);
        dot->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* l = new QHBoxLayout(b);
        l->setContentsMargins(12, 0, 0, 0);
        l->addWidget(dot, 0, Qt::AlignVCenter);
        l->addStretch();
    }
    chipGroup_->addButton(b);
    connect(b, &QAbstractButton::clicked, this, [this, memberId] {
        filterMemberId_ = memberId;
        scheduleRefresh();
    });
    chipLayout_->addWidget(b);
}

void PinsPage::rebuildChips(const std::vector<PinView>& all)
{
    while (QLayoutItem* item = chipLayout_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            chipGroup_->removeButton(qobject_cast<QAbstractButton*>(w));
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    addChip(tr("All (%1)").arg(all.size()), Id{}, 0);
    for (std::size_t i = 0; i < members_.size(); ++i) {
        const auto& m = members_[i];
        const auto n = std::count_if(all.begin(), all.end(),
                                     [&](const PinView& v) { return v.pin.authorId == m.id; });
        addChip(QStringLiteral("%1 (%2)").arg(firstNameOf(QString::fromStdString(m.name))).arg(n), m.id,
                static_cast<int>(i % 6) + 1);
    }
    chipLayout_->addStretch();
}

void PinsPage::refresh()
{
    members_ = memberQuery_.all();
    tasks_ = taskQuery_.all();
    const std::vector<PinView> all = query_.openPins();

    rebuildChips(all);
    emptyHint_->setVisible(all.empty());

    std::map<Id, QString> names;
    std::map<Id, int> indexOf;
    for (std::size_t i = 0; i < members_.size(); ++i) {
        names[members_[i].id] = QString::fromStdString(members_[i].name);
        indexOf[members_[i].id] = static_cast<int>(i % 6) + 1;
    }
    std::map<Id, QString> taskTitles;
    for (const auto& t : tasks_) taskTitles[t.id] = QString::fromStdString(t.title);

    viewById_.clear();
    for (const auto& v : all) viewById_[v.pin.id] = v;
    cards_.clear();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::map<Id, PinCategory> nowShown;
    int staggerIdx = 0;

    for (auto* col : columns_) {
        const int scroll = col->scrollValue();
        std::vector<PinCardWidget*> cards;
        for (const auto& v : all) {
            if (v.pin.category != col->category()) continue;
            if (!filterMemberId_.empty() && v.pin.authorId != filterMemberId_) continue;

            const auto idxIt = indexOf.find(v.pin.authorId);
            const QString taskTitle = v.taskId && taskTitles.count(*v.taskId) ? taskTitles[*v.taskId] : QString();
            auto* card = new PinCardWidget(v, names, idxIt != indexOf.end() ? idxIt->second : 1, taskTitle, now);
            connect(card, &PinCardWidget::resolveRequested, this, &PinsPage::onResolve);
            connect(card, &PinCardWidget::editRequested, this, &PinsPage::onEdit);
            connect(card, &PinCardWidget::deleteRequested, this, &PinsPage::onDelete);
            connect(card, &PinCardWidget::commentSubmitted, this, &PinsPage::onComment);
            connect(card, &PinCardWidget::commentsToggled, this, [this](const Id& id, bool open) {
                if (open) expanded_.insert(id);
                else expanded_.erase(id);
            });
            card->setCommentAuthor(lastAuthorId_);
            if (expanded_.count(v.pin.id)) card->setCommentsExpanded(true);

            const auto seen = shown_.find(v.pin.id);
            const bool isNew = seen == shown_.end() || seen->second != v.pin.category;
            if (isNew) card->animateIn(std::min(staggerIdx++, kMaxStagger) * kStaggerMs);

            cards_[v.pin.id] = card;
            nowShown[v.pin.id] = v.pin.category;
            cards.push_back(card);
        }
        col->setCards(cards);
        // The scroll range is only valid after the layout ran.
        QTimer::singleShot(0, col, [col, scroll] { col->setScrollValue(scroll); });
    }
    shown_ = std::move(nowShown);

    // Forget expanded state of pins that no longer exist.
    for (auto it = expanded_.begin(); it != expanded_.end();)
        it = viewById_.count(*it) ? std::next(it) : expanded_.erase(it);
}

void PinsPage::onLeavePin()
{
    LeavePinDialog dlg(tasks_, members_, lastAuthorId_, this);
    if (dlg.exec() != QDialog::Accepted) return;
    lastAuthorId_ = dlg.authorId();
    runCommand([&] {
        commands_.create(dlg.category(), dlg.text().toStdString(), dlg.tags(), dlg.authorId(), dlg.taskId());
    });
}

void PinsPage::onEdit(const Id& pinId)
{
    const auto it = viewById_.find(pinId);
    if (it == viewById_.end()) return;
    const PinView before = it->second;
    LeavePinDialog dlg(before, tasks_, members_, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const Id by = before.pin.authorId;
    runCommand([&] {
        const std::string text = dlg.text().toStdString();
        const auto tags = dlg.tags();
        const auto task = dlg.taskId();
        if (text != before.pin.text || tags != before.pin.tags || task != before.taskId)
            commands_.edit(pinId, by, text, tags, task);
        // Two calls, not atomic: if move throws, the edit stays applied. The refresh shows the real state.
        if (dlg.category() != before.pin.category) commands_.move(pinId, by, dlg.category());
    });
}

void PinsPage::onResolve(const Id& pinId)
{
    const Id by = actorFor(pinId);
    auto act = [this, alive = alive_, pinId, by] {
        if (*alive) runCommand([&] { commands_.resolve(pinId, by); });
    };
    const auto it = cards_.find(pinId);
    if (it != cards_.end() && it->second) it->second->animateOut(act);
    else act();
}

void PinsPage::onDelete(const Id& pinId)
{
    if (!ModalDialog::confirm(this, tr("Delete pin"), tr("Delete this pin? This cannot be undone."),
                              tr("Delete"), ModalDialog::Role::Danger))
        return;
    const Id by = actorFor(pinId);
    auto act = [this, alive = alive_, pinId, by] {
        if (*alive) runCommand([&] { commands_.remove(pinId, by); });
    };
    const auto it = cards_.find(pinId);
    if (it != cards_.end() && it->second) it->second->animateOut(act);
    else act();
}

void PinsPage::onComment(const Id& pinId, const QString& text, const Id& authorId)
{
    lastAuthorId_ = authorId;
    runCommand([&] { commands_.comment(pinId, authorId, text.toStdString()); });
}

void PinsPage::onDropped(const Id& pinId, PinCategory to)
{
    const auto it = viewById_.find(pinId);
    if (it == viewById_.end() || it->second.pin.category == to) return;
    const Id by = it->second.pin.authorId;
    runCommand([&] { commands_.move(pinId, by, to); });
}

}  // namespace planova::s5
