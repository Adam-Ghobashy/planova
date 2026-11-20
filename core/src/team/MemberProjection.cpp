#include "planova/team/MemberProjection.h"
#include "planova/common/EventTypes.h"
#include "planova/team/SkillCategory.h"
#include "planova/team/VelocityCalculator.h"

#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <stdexcept>

using namespace std;

void MemberProjection::reset()
{
    members_.clear();
    memberOrder_.clear();
    settings_.clear();
    taskInProgressTs_.clear();
    taskAssignee_.clear();
}

void MemberProjection::apply(const Event& event)
{
    const QJsonObject& p = event.payload;

    if (event.type == MemberAdded) {
        Id id = p["memberId"].toString().toStdString();
        if (id.empty()) {
            return;
        }

        Member m;
        m.id = id;
        m.name = p["name"].toString().toStdString();
        m.role = p["role"].toString().toStdString();
        m.isLead = p["isLead"].toBool(false);
        m.understanding = 0;
        m.velocity = 1.0;

        if (members_.find(id) == members_.end()) {
            memberOrder_.push_back(id);
        }
        members_[id] = m;
    }
    else if (event.type == MemberRemoved) {
        Id id = p["memberId"].toString().toStdString();
        members_.erase(id);
        memberOrder_.erase(std::remove(memberOrder_.begin(), memberOrder_.end(), id), memberOrder_.end());
    }
    else if (event.type == MemberUpdated) {
        Id id = p["memberId"].toString().toStdString();
        auto it = members_.find(id);
        if (it != members_.end()) {
            it->second.role = p["role"].toString().toStdString();
            it->second.understanding = p["understanding"].toInt(0);
        }
    }
    else if (event.type == SkillRated) {
        Id id = p["memberId"].toString().toStdString();
        auto it = members_.find(id);
        if (it != members_.end()) {
            string catStr = p["category"].toString().toStdString();
            SkillCategory cat;
            try {
                cat = fromString(catStr);
            } catch (const std::invalid_argument&) {
                return; // unknown skill name (hand-edited or newer build): skip the event
            }
            int rating = p["rating"].toInt(1);
            it->second.ratings[cat] = rating;
        }
    }
    else if (event.type == SettingChanged) {
        string key = p["key"].toString().toStdString();
        bool val = p["value"].toBool(false);
        settings_[key] = val;
    }
    else if (event.type == TaskAssigned) {
        Id taskId = p["taskId"].toString().toStdString();
        Id memberId = p["memberId"].toString().toStdString();
        if (!taskId.empty() && !memberId.empty()) {
            taskAssignee_[taskId] = memberId;
        }
    }
    else if (event.type == TaskUnassigned) {
        taskAssignee_.erase(p["taskId"].toString().toStdString());
    }
    else if (event.type == TaskMoved) {
        // Contract payload: taskId, from, to, estHours (no assignee).
        Id taskId = p["taskId"].toString().toStdString();
        string toStatus = p["to"].toString().toStdString();

        if (toStatus == "InProgress" && !taskId.empty()) {
            taskInProgressTs_[taskId] = event.ts;
        }
        else if (toStatus == "Done") {
            // The assignee comes from the TaskAssigned events seen so far.
            auto assigneeIt = taskAssignee_.find(taskId);
            if (assigneeIt == taskAssignee_.end()) {
                return;
            }
            auto memIt = members_.find(assigneeIt->second);
            auto tsIt = taskInProgressTs_.find(taskId);
            if (memIt == members_.end() || tsIt == taskInProgressTs_.end()
                || event.ts <= tsIt->second) {
                return;
            }

            double estimatedHours = p["estHours"].toDouble(0.0);
            double actualHours = static_cast<double>(event.ts - tsIt->second) / 3600000.0;
            memIt->second.velocity = VelocityCalculator::update(
                memIt->second.velocity, estimatedHours, actualHours);
        }
    }
}

vector<MemberDto> MemberProjection::all() const
{
    vector<MemberDto> result;
    result.reserve(memberOrder_.size());

    for (const Id& id : memberOrder_) {
        auto it = members_.find(id);
        if (it != members_.end()) {
            result.push_back(it->second.toDto());
        }
    }
    return result;
}

optional<MemberDto> MemberProjection::byId(const Id& id) const
{
    auto it = members_.find(id);
    if (it != members_.end()) {
        return it->second.toDto();
    }
    return nullopt;
}

bool MemberProjection::getSetting(const string& key, bool defaultValue) const
{
    auto it = settings_.find(key);
    if (it != settings_.end()) {
        return it->second;
    }
    return defaultValue;
}