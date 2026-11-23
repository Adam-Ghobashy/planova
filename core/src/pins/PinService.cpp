#include "planova/pins/PinService.h"

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

std::string requireText(const std::string& text)
{
    const std::string clean = trimmed(text);
    if (clean.empty())
        throw std::invalid_argument("Text must not be empty");
    return clean;
}

QJsonArray toJson(const std::vector<std::string>& tags)
{
    QJsonArray array;
    for (const std::string& t : tags)
        array.append(q(t));
    return array;
}

}  // namespace

PinService::PinService(EventLog& log, const PinProjection& projection, IClock& clock)
    : log_(log), projection_(projection), clock_(clock)
{
}

void PinService::requireOpen(const Id& pinId) const
{
    if (!projection_.isOpen(pinId))
        throw std::invalid_argument("Pin not found or already closed");
}

void PinService::append(int64_t ts, const std::string& type, const std::string& author,
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

Id PinService::create(PinCategory category, const std::string& text,
                      const std::vector<std::string>& tags, const Id& authorId,
                      std::optional<Id> taskId)
{
    const std::string cleanText = requireText(text);
    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["category"] = q(categoryToString(category));
    payload["text"] = q(cleanText);
    payload["tags"] = toJson(normaliseTags(tags));
    payload["authorId"] = q(authorId);
    if (taskId)
        payload["taskId"] = q(*taskId);

    // Two identical pins in the same millisecond would get the same id, so add a counter.
    const Id base = "pin-" + makeEventId(ts, authorId, payload);
    Id pinId = base;
    for (int n = 2; projection_.hasPin(pinId); ++n)
        pinId = base + "-" + std::to_string(n);
    payload["pinId"] = q(pinId);

    append(ts, PinCreated, authorId, payload);
    return pinId;
}

void PinService::comment(const Id& pinId, const Id& authorId, const std::string& text)
{
    requireOpen(pinId);
    const std::string cleanText = requireText(text);
    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["pinId"] = q(pinId);
    payload["authorId"] = q(authorId);
    payload["text"] = q(cleanText);

    const Id base = "comment-" + makeEventId(ts, authorId, payload);
    Id commentId = base;
    for (int n = 2; projection_.hasComment(commentId); ++n)
        commentId = base + "-" + std::to_string(n);
    payload["commentId"] = q(commentId);

    append(ts, PinCommentAdded, authorId, payload);
}

void PinService::resolve(const Id& pinId, const Id& by)
{
    requireOpen(pinId);
    QJsonObject payload;
    payload["pinId"] = q(pinId);
    payload["by"] = q(by);
    append(clock_.nowMs(), PinResolved, by, payload);
}

void PinService::edit(const Id& pinId, const Id& by, const std::string& text,
                      const std::vector<std::string>& tags, std::optional<Id> taskId)
{
    requireOpen(pinId);
    const std::string cleanText = requireText(text);

    QJsonObject payload;
    payload["pinId"] = q(pinId);
    payload["by"] = q(by);
    payload["text"] = q(cleanText);
    payload["tags"] = toJson(normaliseTags(tags));
    if (taskId)
        payload["taskId"] = q(*taskId);
    append(clock_.nowMs(), PinEdited, by, payload);
}

void PinService::move(const Id& pinId, const Id& by, PinCategory to)
{
    requireOpen(pinId);
    QJsonObject payload;
    payload["pinId"] = q(pinId);
    payload["by"] = q(by);
    payload["category"] = q(categoryToString(to));
    append(clock_.nowMs(), PinMoved, by, payload);
}

void PinService::remove(const Id& pinId, const Id& by)
{
    requireOpen(pinId);
    QJsonObject payload;
    payload["pinId"] = q(pinId);
    payload["by"] = q(by);
    append(clock_.nowMs(), PinRemoved, by, payload);
}

}  // namespace planova
