#include "planova/task/TaskProjection.h"
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>
#include "planova/common/EventTypes.h"
#include "planova/task/TaskCodec.h"
namespace {
std::string str(const QJsonObject& o, const char* key) {
    return o.value(key).toString().toStdString();
}
}
void TaskProjection::reset() {
    tasks_.clear();
    order_.clear();
    graph_ = TaskGraph{};
}
void TaskProjection::apply(const Event& e) {
    if (e.type == TaskCreated) {
        Task t;
        t.id = str(e.payload, "taskId");
        if (t.id.empty() || tasks_.count(t.id)) return;
        t.title = str(e.payload, "title");
        t.skill = task_codec::skillFromString(str(e.payload, "skill")).value_or(SkillCategory::Backend);
        t.priority = task_codec::priorityFromString(str(e.payload, "priority")).value_or(Priority::Medium);
        t.complexity = e.payload.value("complexity").toInt(1);
        t.estHours = e.payload.value("estHours").toInt(1);
        t.status = TaskStatus::ToDo;
        graph_.addNode(t.id, t.estHours);
        const QJsonArray depsArr = e.payload.value("deps").toArray();
        for (const auto& d : depsArr) {
            Id depId = d.toString().toStdString();
            if (tasks_.count(depId) && graph_.addDependency(t.id, depId))
                t.deps.push_back(depId);
        }
        order_.push_back(t.id);
        tasks_[t.id] = std::move(t);
    } else if (e.type == TaskMoved) {
        auto it = tasks_.find(str(e.payload, "taskId"));
        auto to = task_codec::statusFromString(str(e.payload, "to"));
        if (it != tasks_.end() && to) it->second.status = *to;
    } else if (e.type == TaskAssigned) {
        auto it = tasks_.find(str(e.payload, "taskId"));
        if (it != tasks_.end()) it->second.assignee = str(e.payload, "memberId");
    } else if (e.type == TaskUnassigned) {
        auto it = tasks_.find(str(e.payload, "taskId"));
        if (it != tasks_.end()) it->second.assignee = std::nullopt;
    } else if (e.type == DependencyAdded) {
        const Id taskId = str(e.payload, "taskId");
        const Id depId = str(e.payload, "dependsOnId");
        auto it = tasks_.find(taskId);
        if (it == tasks_.end() || !tasks_.count(depId)) return;
        if (graph_.addDependency(taskId, depId) &&
            std::find(it->second.deps.begin(), it->second.deps.end(), depId) == it->second.deps.end())
            it->second.deps.push_back(depId);
    }
    // unknown event types are ignored
}
std::set<Id> TaskProjection::doneTaskIds() const {
    std::set<Id> done;
    for (const auto& [id, t] : tasks_)
        if (t.status == TaskStatus::Done) done.insert(id);
    return done;
}
TaskDto TaskProjection::toDto(const Task& t, const std::set<Id>& doneIds) const {
    TaskDto dto;
    dto.id = t.id;
    dto.title = t.title;
    dto.skill = t.skill;
    dto.priority = t.priority;
    dto.complexity = t.complexity;
    dto.estHours = t.estHours;
    dto.status = t.status;
    dto.assignee = t.assignee;
    dto.blockedBy = graph_.blockedBy(t.id, doneIds);
    return dto;
}
std::vector<TaskDto> TaskProjection::all() const {
    std::vector<TaskDto> res;
    const auto done = doneTaskIds();
    res.reserve(order_.size());
    for (const auto& id : order_) res.push_back(toDto(tasks_.at(id), done));
    return res;
}
std::optional<TaskDto> TaskProjection::byId(const Id& id) const {
    auto it = tasks_.find(id);
    if (it == tasks_.end()) return std::nullopt;
    return toDto(it->second, doneTaskIds());
}

