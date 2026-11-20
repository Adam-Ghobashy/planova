#pragma once
// S5 contract between the Pins screen and the S5 backend.
// PinService implements IPinCommands, PinProjection implements IPinBoardQuery,
// ConflictService implements IConflictCommands. No Qt includes: core stays Qt::Core only.
#include <optional>
#include <string>
#include <vector>

#include "planova/common/Dtos.h"
#include "planova/common/Types.h"
#include "planova/pins/Pin.h"

namespace planova {

struct PinView {
    PinDto pin;                        // id, category, text, tags (lowercase, no '#'), authorId, ts
    std::optional<Id> taskId;
    std::vector<PinComment> comments;  // oldest first
};

struct IPinBoardQuery {
    virtual ~IPinBoardQuery() = default;
    virtual std::vector<PinView> openPins() const = 0;  // unresolved, undeleted, newest first
};

struct IPinCommands {
    virtual ~IPinCommands() = default;
    virtual Id create(PinCategory category, const std::string& text,
                      const std::vector<std::string>& tags, const Id& authorId,
                      std::optional<Id> taskId) = 0;
    virtual void comment(const Id& pinId, const Id& authorId, const std::string& text) = 0;
    virtual void resolve(const Id& pinId, const Id& by) = 0;
    virtual void edit(const Id& pinId, const Id& by, const std::string& text,
                      const std::vector<std::string>& tags, std::optional<Id> taskId) = 0;
    virtual void move(const Id& pinId, const Id& by, PinCategory to) = 0;
    virtual void remove(const Id& pinId, const Id& by) = 0;
};

struct IConflictCommands {
    virtual ~IConflictCommands() = default;
    virtual Id decline(const Id& taskId, const Id& memberId, const std::string& reason) = 0;
};

}  // namespace planova
