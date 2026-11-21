#include "planova/pins/PinProjection.h"

#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

#include "planova/common/EventTypes.h"
#include "planova/pins/PinCodec.h"

namespace planova {

namespace {

std::string str(const QJsonObject& o, const char* key)
{
    return o.value(key).toString().toStdString();
}

std::optional<Id> optionalId(const QJsonObject& o, const char* key)
{
    const std::string value = str(o, key);
    if (value.empty())
        return std::nullopt;
    return value;
}

std::vector<std::string> tagsOf(const QJsonObject& o)
{
    std::vector<std::string> tags;
    for (const QJsonValue& v : o.value("tags").toArray())
        tags.push_back(v.toString().toStdString());
    return tags;
}

}  // namespace

void PinProjection::reset()
{
    pins_.clear();
    creationOrder_.clear();
}

PinProjection::PinRecord* PinProjection::find(const Id& pinId)
{
    auto it = pins_.find(pinId);
    return it == pins_.end() ? nullptr : &it->second;
}

void PinProjection::apply(const Event& e)
{
    const QJsonObject& p = e.payload;
    if (e.type == PinCreated) {
        const Id pinId = str(p, "pinId");
        const auto category = categoryFromString(str(p, "category"));
        if (pinId.empty() || !category || pins_.count(pinId))
            return;
        PinRecord record;
        record.view.pin = PinDto{pinId, *category, str(p, "text"), tagsOf(p), str(p, "authorId"), e.ts};
        record.view.taskId = optionalId(p, "taskId");
        pins_[pinId] = record;
        creationOrder_.push_back(pinId);
        return;
    }

    PinRecord* record = find(str(p, "pinId"));
    if (!record)
        return;  // unknown pin, or an event type this projection does not handle
    if (e.type == PinCommentAdded) {
        record->view.comments.push_back(
            PinComment{str(p, "commentId"), str(p, "authorId"), str(p, "text"), e.ts});
    } else if (e.type == PinResolved || e.type == PinRemoved) {
        record->open = false;
    } else if (e.type == PinEdited) {
        record->view.pin.text = str(p, "text");
        record->view.pin.tags = tagsOf(p);
        record->view.taskId = optionalId(p, "taskId");
    } else if (e.type == PinMoved) {
        if (const auto category = categoryFromString(str(p, "category")))
            record->view.pin.category = *category;
    }
}

std::vector<const PinProjection::PinRecord*> PinProjection::openRecordsNewestFirst() const
{
    std::vector<const PinRecord*> result;
    // Walk creation order backwards so equal timestamps still show the later pin first.
    for (auto it = creationOrder_.rbegin(); it != creationOrder_.rend(); ++it) {
        const PinRecord& record = pins_.at(*it);
        if (record.open)
            result.push_back(&record);
    }
    std::stable_sort(result.begin(), result.end(), [](const PinRecord* a, const PinRecord* b) {
        return a->view.pin.ts > b->view.pin.ts;
    });
    return result;
}

std::vector<PinView> PinProjection::openPins() const
{
    std::vector<PinView> result;
    for (const PinRecord* record : openRecordsNewestFirst())
        result.push_back(record->view);
    return result;
}

int PinProjection::openCount() const
{
    return static_cast<int>(std::count_if(pins_.begin(), pins_.end(),
                                          [](const auto& entry) { return entry.second.open; }));
}

std::vector<PinDto> PinProjection::latest(int n) const
{
    std::vector<PinDto> result;
    for (const PinRecord* record : openRecordsNewestFirst()) {
        if (static_cast<int>(result.size()) >= n)
            break;
        result.push_back(record->view.pin);
    }
    return result;
}

int PinProjection::pinCountForTask(const Id& taskId) const
{
    return static_cast<int>(std::count_if(pins_.begin(), pins_.end(), [&](const auto& entry) {
        return entry.second.open && entry.second.view.taskId == taskId;
    }));
}

bool PinProjection::isOpen(const Id& pinId) const
{
    auto it = pins_.find(pinId);
    return it != pins_.end() && it->second.open;
}

bool PinProjection::hasPin(const Id& pinId) const
{
    return pins_.count(pinId) > 0;
}

bool PinProjection::hasComment(const Id& commentId) const
{
    for (const auto& entry : pins_)
        for (const PinComment& c : entry.second.view.comments)
            if (c.id == commentId)
                return true;
    return false;
}

}  // namespace planova
