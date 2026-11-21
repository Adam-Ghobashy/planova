#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "planova/common/Event.h"
#include "planova/common/IProjection.h"
#include "planova/common/Queries.h"
#include "planova/pins/Pin.h"

namespace planova {

struct ConflictView {
    Id id;
    Id taskId;
    Id memberId;
    std::string reason;
    Id pinId;
    int64_t ts;
};

// Open and resolved conflicts. A ConflictRaised event carries all its own data, so the result
// does not depend on the PinCreated or TaskUnassigned events that were stored with it.
class ConflictProjection : public IProjection, public IConflictQuery {
public:
    void reset() override;
    void apply(const Event& event) override;

    bool hasOpenConflict(const Id& memberId) const override;
    std::vector<ConflictView> openConflicts() const;  // oldest first
    std::optional<ConflictView> byId(const Id& conflictId) const;
    bool hasConflict(const Id& conflictId) const;  // open or resolved
    bool isOpen(const Id& conflictId) const;

private:
    struct Record {
        ConflictView view;
        bool open = true;
    };

    std::map<Id, Record> conflicts_;
    std::vector<Id> creationOrder_;
    // A rebuild can apply ConflictResolved before its ConflictRaised (same millisecond).
    std::set<Id> resolvedEarly_;
};

}  // namespace planova
