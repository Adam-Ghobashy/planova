#pragma once
#include <map>
#include <optional>
#include <vector>

#include "planova/common/Event.h"
#include "planova/common/IProjection.h"
#include "planova/common/Queries.h"
#include "planova/pins/Pin.h"
#include "planova/pins/PinBoardApi.h"

namespace planova {

// Holds every pin ever created; resolved and removed pins stay hidden but known.
class PinProjection : public IProjection, public IPinBoardQuery, public IPinQuery {
public:
    void reset() override;
    void apply(const Event& event) override;

    std::vector<PinView> openPins() const override;  // newest first
    int openCount() const override;
    std::vector<PinDto> latest(int n) const override;
    int pinCountForTask(const Id& taskId) const override;

    bool isOpen(const Id& pinId) const;
    bool hasPin(const Id& pinId) const;          // open or closed
    bool hasComment(const Id& commentId) const;  // on any pin

private:
    struct PinRecord {
        PinView view;
        bool open = true;
    };

    PinRecord* find(const Id& pinId);
    std::vector<const PinRecord*> openRecordsNewestFirst() const;

    std::map<Id, PinRecord> pins_;
    std::vector<Id> creationOrder_;
};

}  // namespace planova
