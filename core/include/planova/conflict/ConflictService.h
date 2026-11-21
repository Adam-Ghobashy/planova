#pragma once
#include <QJsonObject>
#include <string>

#include "planova/common/Clock.h"
#include "planova/conflict/ConflictProjection.h"
#include "planova/pins/PinBoardApi.h"
#include "planova/store/EventLog.h"

namespace planova {

// Validates first, then appends. Throws std::invalid_argument and appends nothing on bad input.
class ConflictService : public IConflictCommands {
public:
    ConflictService(EventLog& log, ITaskQuery& tasks, const ConflictProjection& projection,
                    IClock& clock);

    // Appends ConflictRaised, TaskUnassigned and a FeedbackBlocker PinCreated. Returns the conflict id.
    Id decline(const Id& taskId, const Id& memberId, const std::string& reason) override;
    void resolve(const Id& conflictId, const Id& by);

private:
    void append(int64_t ts, const std::string& type, const std::string& author,
                const QJsonObject& payload);

    EventLog& log_;
    ITaskQuery& tasks_;
    const ConflictProjection& projection_;
    IClock& clock_;
};

}  // namespace planova
