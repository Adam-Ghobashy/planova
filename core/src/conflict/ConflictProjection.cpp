#include "planova/conflict/ConflictProjection.h"

#include <QJsonObject>
#include <algorithm>

#include "planova/common/EventTypes.h"

namespace planova {

namespace {

std::string str(const QJsonObject& o, const char* key)
{
    return o.value(key).toString().toStdString();
}

}  // namespace

void ConflictProjection::reset()
{
    conflicts_.clear();
    creationOrder_.clear();
    resolvedEarly_.clear();
}

void ConflictProjection::apply(const Event& e)
{
    const QJsonObject& p = e.payload;
    const Id conflictId = str(p, "conflictId");
    if (conflictId.empty())
        return;

    if (e.type == ConflictRaised) {
        if (conflicts_.count(conflictId))
            return;
        Record record;
        record.view = ConflictView{conflictId,     str(p, "taskId"), str(p, "memberId"),
                                   str(p, "reason"), str(p, "pinId"),  e.ts};
        record.open = resolvedEarly_.erase(conflictId) == 0;
        conflicts_[conflictId] = record;
        creationOrder_.push_back(conflictId);
    } else if (e.type == ConflictResolved) {
        auto it = conflicts_.find(conflictId);
        if (it != conflicts_.end())
            it->second.open = false;
        else
            resolvedEarly_.insert(conflictId);
    }
}

bool ConflictProjection::hasOpenConflict(const Id& memberId) const
{
    return std::any_of(conflicts_.begin(), conflicts_.end(), [&](const auto& entry) {
        return entry.second.open && entry.second.view.memberId == memberId;
    });
}

std::vector<ConflictView> ConflictProjection::openConflicts() const
{
    std::vector<ConflictView> result;
    for (const Id& id : creationOrder_) {
        const Record& record = conflicts_.at(id);
        if (record.open)
            result.push_back(record.view);
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const ConflictView& a, const ConflictView& b) { return a.ts < b.ts; });
    return result;
}

std::optional<ConflictView> ConflictProjection::byId(const Id& conflictId) const
{
    auto it = conflicts_.find(conflictId);
    if (it == conflicts_.end())
        return std::nullopt;
    return it->second.view;
}

bool ConflictProjection::hasConflict(const Id& conflictId) const
{
    return conflicts_.count(conflictId) > 0;
}

bool ConflictProjection::isOpen(const Id& conflictId) const
{
    auto it = conflicts_.find(conflictId);
    return it != conflicts_.end() && it->second.open;
}

}  // namespace planova
