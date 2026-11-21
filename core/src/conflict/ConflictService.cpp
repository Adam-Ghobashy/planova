#include "planova/conflict/ConflictService.h"

#include <QJsonArray>
#include <stdexcept>

#include "planova/common/EventTypes.h"
#include "planova/common/Ids.h"
#include "planova/pins/PinCodec.h"

namespace planova {

namespace {

QString q(const std::string& s)
{
    return QString::fromStdString(s);
}

}  // namespace

ConflictService::ConflictService(EventLog& log, ITaskQuery& tasks,
                                 const ConflictProjection& projection, IClock& clock)
    : log_(log), tasks_(tasks), projection_(projection), clock_(clock)
{
}

void ConflictService::append(int64_t ts, const std::string& type, const std::string& author,
                             const QJsonObject& payload)
{
    Event e;
    e.ts = ts;
    e.author = author;
    e.type = type;
    e.payload = payload;
    e.id = makeEventId(ts, author, payload);
    log_.append(e);
}

Id ConflictService::decline(const Id& taskId, const Id& memberId, const std::string& reason)
{
    const auto task = tasks_.byId(taskId);
    if (!task)
        throw std::invalid_argument("Task not found");
    if (task->status == TaskStatus::Done)
        throw std::invalid_argument("A finished task cannot be declined");
    if (!task->assignee || *task->assignee != memberId)
        throw std::invalid_argument("Task is not assigned to this member");
    const std::string cleanReason = trimmed(reason);
    if (cleanReason.empty())
        throw std::invalid_argument("Reason must not be empty");

    // Each event gets its own time; the first one also seeds the conflict id.
    const int64_t ts = clock_.nowMs();

    QJsonObject raised;
    raised["taskId"] = q(taskId);
    raised["memberId"] = q(memberId);
    raised["reason"] = q(cleanReason);

    // Two identical declines in the same millisecond would get the same id, so add a counter.
    const Id base = "conflict-" + makeEventId(ts, memberId, raised);
    Id conflictId = base;
    for (int n = 2; projection_.hasConflict(conflictId); ++n)
        conflictId = base + "-" + std::to_string(n);
    // The pin id is derived from the conflict id: unique, and no PinProjection needed here.
    const Id pinId = "pin-" + conflictId;
    raised["conflictId"] = q(conflictId);
    raised["pinId"] = q(pinId);

    QJsonObject unassigned;
    unassigned["taskId"] = q(taskId);
    unassigned["memberId"] = q(memberId);
    unassigned["reason"] = q(cleanReason);

    QJsonObject pin;
    pin["pinId"] = q(pinId);
    pin["category"] = q(categoryToString(PinCategory::FeedbackBlocker));
    pin["text"] = q("Declined: " + cleanReason);
    pin["tags"] = QJsonArray{QStringLiteral("conflict")};
    pin["authorId"] = q(memberId);
    pin["taskId"] = q(taskId);

    append(ts, ConflictRaised, memberId, raised);
    append(clock_.nowMs(), TaskUnassigned, memberId, unassigned);
    append(clock_.nowMs(), PinCreated, memberId, pin);
    return conflictId;
}

void ConflictService::resolve(const Id& conflictId, const Id& by)
{
    if (!projection_.isOpen(conflictId))
        throw std::invalid_argument("Conflict not found or already resolved");
    QJsonObject payload;
    payload["conflictId"] = q(conflictId);
    payload["by"] = q(by);
    append(clock_.nowMs(), ConflictResolved, by, payload);
}

}  // namespace planova
