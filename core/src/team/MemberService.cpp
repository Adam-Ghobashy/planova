#include "planova/team/MemberService.h"
#include "planova/common/EventTypes.h"
#include "planova/common/Ids.h"
#include "planova/team/SkillCategory.h"

#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <stdexcept>

using namespace std;

namespace {

string trimmed(const string& text)
{
    return QString::fromStdString(text).trimmed().toStdString();
}

} // namespace

MemberService::MemberService(EventLog& log, IClock& clock, const IMemberQuery& members,
                             const ITaskQuery& tasks)
    : log_(log), clock_(clock), members_(members), tasks_(tasks)
{
}

void MemberService::requireMember(const Id& id) const
{
    if (!members_.byId(id)) {
        throw invalid_argument("Member not found: " + id);
    }
}

Id MemberService::add(const string& rawName, const string& role, bool isLead)
{
    const string name = trimmed(rawName);
    if (name.empty()) {
        throw invalid_argument("Member name cannot be empty");
    }

    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["name"] = QString::fromStdString(name);
    payload["role"] = QString::fromStdString(role);
    payload["isLead"] = isLead;

    const Id memberId = makeEventId(ts, name, payload);
    payload["memberId"] = QString::fromStdString(memberId);

    Event event;
    event.id = makeEventId(ts, name, payload);
    event.ts = ts;
    event.author = name;
    event.type = MemberAdded;
    event.payload = payload;

    log_.append(event);
    return memberId;
}

void MemberService::update(const Id& id, const string& role, int understanding)
{
    requireMember(id);
    int clampedUnderstanding = std::clamp(understanding, 0, 100);

    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["memberId"] = QString::fromStdString(id);
    payload["role"] = QString::fromStdString(role);
    payload["understanding"] = clampedUnderstanding;

    Event event;
    event.id = makeEventId(ts, id, payload);
    event.ts = ts;
    event.author = id;
    event.type = MemberUpdated;
    event.payload = payload;

    log_.append(event);
}

void MemberService::remove(const string& id)
{
    requireMember(id);

    // Free the member's open tasks first; finished tasks keep their assignee for the leaderboard.
    for (const auto& task : tasks_.all()) {
        if (task.assignee == id && task.status != TaskStatus::Done) {
            const int64_t unassignTs = clock_.nowMs();
            QJsonObject unassign; // same shape as TaskService::unassign
            unassign["taskId"] = QString::fromStdString(task.id);
            unassign["reason"] = "member removed";

            Event unassignEvent;
            unassignEvent.id = makeEventId(unassignTs, "system", unassign);
            unassignEvent.ts = unassignTs;
            unassignEvent.author = "system";
            unassignEvent.type = TaskUnassigned;
            unassignEvent.payload = unassign;
            log_.append(unassignEvent);
        }
    }

    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["memberId"] = QString::fromStdString(id);

    Event event;
    event.id = makeEventId(ts, id, payload);
    event.ts = ts;
    event.author = id;
    event.type = MemberRemoved;
    event.payload = payload;

    log_.append(event);
}

void MemberService::rate(const Id& id, SkillCategory category, int rating)
{
    requireMember(id);
    if (rating < 1 || rating > 5) {
        throw invalid_argument("Skill rating must be between 1 and 5");
    }

    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["memberId"] = QString::fromStdString(id);
    payload["category"] = QString::fromStdString(toString(category));
    payload["rating"] = rating;

    Event event;
    event.id = makeEventId(ts, id, payload);
    event.ts = ts;
    event.author = id;
    event.type = SkillRated;
    event.payload = payload;

    log_.append(event);
}

void MemberService::setSetting(const string& key, bool value)
{
    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["key"] = QString::fromStdString(key);
    payload["value"] = value;

    Event event;
    event.id = makeEventId(ts, "system", payload);
    event.ts = ts;
    event.author = "system";
    event.type = SettingChanged;
    event.payload = payload;

    log_.append(event);
}