#pragma once
#include <QJsonObject>
#include <string>
#include <vector>

#include "planova/common/Clock.h"
#include "planova/pins/PinBoardApi.h"
#include "planova/pins/PinProjection.h"
#include "planova/store/EventLog.h"

namespace planova {

// Validates first, then appends exactly one event. Throws std::invalid_argument and
// appends nothing when the input is bad or the pin is not open.
class PinService : public IPinCommands {
public:
    PinService(EventLog& log, const PinProjection& projection, IClock& clock);

    Id create(PinCategory category, const std::string& text, const std::vector<std::string>& tags,
              const Id& authorId, std::optional<Id> taskId) override;
    void comment(const Id& pinId, const Id& authorId, const std::string& text) override;
    void resolve(const Id& pinId, const Id& by) override;
    void edit(const Id& pinId, const Id& by, const std::string& text,
              const std::vector<std::string>& tags, std::optional<Id> taskId) override;
    void move(const Id& pinId, const Id& by, PinCategory to) override;
    void remove(const Id& pinId, const Id& by) override;

private:
    void requireOpen(const Id& pinId) const;
    void append(int64_t ts, const std::string& type, const std::string& author, const QJsonObject& payload);

    EventLog& log_;
    const PinProjection& projection_;
    IClock& clock_;
};

}  // namespace planova
