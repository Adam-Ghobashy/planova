#pragma once
#include <QPointer>
#include <QWidget>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "planova/common/Queries.h"
#include "planova/pins/PinBoardApi.h"

class QButtonGroup;
class QHBoxLayout;

namespace planova::s5 {

class PinCardWidget;
class PinColumnWidget;

// Pins and Collab screen. Reads through IPinBoardQuery, writes through IPinCommands.
// Never appends events itself. Call refresh() when the core reports a change.
class PinsPage : public QWidget {
    Q_OBJECT
public:
    PinsPage(IPinBoardQuery& query, IPinCommands& commands, IMemberQuery& members, ITaskQuery& tasks,
             QWidget* parent = nullptr);
    ~PinsPage() override;

public slots:
    void refresh();

signals:
    // Emitted after every pin command.
    void pinsChanged();

private:
    Id actorFor(const Id& pinId) const;  // who is recorded as "by": the pin author
    void scheduleRefresh();  // queued; cards can be deleted while their own handlers run
    void runCommand(const std::function<void()>& fn);
    void rebuildChips(const std::vector<PinView>& all);
    void addChip(const QString& text, const Id& memberId, int memberIndex);

    void onLeavePin();
    void onEdit(const Id& pinId);
    void onResolve(const Id& pinId);
    void onDelete(const Id& pinId);
    void onComment(const Id& pinId, const QString& text, const Id& authorId);
    void onDropped(const Id& pinId, PinCategory to);

    IPinBoardQuery& query_;
    IPinCommands& commands_;
    IMemberQuery& memberQuery_;
    ITaskQuery& taskQuery_;

    std::vector<MemberDto> members_;
    std::vector<TaskDto> tasks_;
    std::map<Id, PinView> viewById_;
    std::map<Id, QPointer<PinCardWidget>> cards_;
    std::map<Id, PinCategory> shown_;  // pins visible after the last refresh (for enter animation)
    std::set<Id> expanded_;            // pins whose comment panel is open
    Id filterMemberId_;                // empty = all
    Id lastAuthorId_;                  // last author picked in a dialog or comment box, used as default
    bool refreshQueued_ = false;
    // Cleared in the destructor: a card that is deleted mid-fade (page teardown) must not run its command.
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    QWidget* chipBar_ = nullptr;
    QHBoxLayout* chipLayout_ = nullptr;
    QButtonGroup* chipGroup_ = nullptr;
    std::vector<PinColumnWidget*> columns_;
};

}  // namespace planova::s5
