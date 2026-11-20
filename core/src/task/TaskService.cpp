#include "planova/task/TaskService.h"
#include <QJsonArray>
#include <stdexcept>
#include "planova/common/EventTypes.h"
#include "planova/common/Ids.h"
#include "planova/task/TaskCodec.h"
#include "planova/task/TaskFactory.h"
#include "planova/task/TaskStateMachine.h"
TaskService::TaskService(EventLog& log, TaskProjection& projection, IClock& clock)
    : log_(log), proj_(projection), clock_(clock) {}
void TaskService::appendEvent(const std::string& type, const std::string& author, const QJsonObject& payload) {
    Event ev;
    ev.ts = clock_.nowMs();
    ev.author = author;
    ev.type = type;
    ev.payload = payload;
    ev.id = makeEventId(ev.ts, ev.author, ev.payload);
    log_.append(ev);
}
Id TaskService::create(const NewTask& input) {
    QJsonObject payload;
    payload["title"] = QString::fromStdString(input.title);
    payload["skill"] = QString::fromStdString(task_codec::toString(input.skill));
    payload["priority"] = QString::fromStdString(task_codec::toString(input.priority));
    payload["complexity"] = input.complexity;
    payload["estHours"] = input.estHours;
    QJsonArray depsArr;
    for (const auto& d : input.deps) depsArr.append(QString::fromStdString(d));
    payload["deps"] = depsArr;
    const Id base = "task-" + makeEventId(clock_.nowMs(), "system", payload);
    Id taskId = base;
    for (int n = 2; proj_.byId(taskId).has_value(); ++n)
        taskId = base + "-" + std::to_string(n);
    Task validated = TaskFactory::create(taskId, input);          // throws before any event is written
    for (const auto& d : input.deps)
        if (!proj_.byId(d).has_value())
            throw std::invalid_argument("Dependency task not found: " + d);
    payload["taskId"] = QString::fromStdString(validated.id);
    appendEvent(TaskCreated, "system", payload);
    return taskId;
}
void TaskService::move(const Id& taskId, TaskStatus to) {
    auto dto = proj_.byId(taskId);
    if (!dto) throw std::invalid_argument("Task not found");
    if (!TaskStateMachine::canMove(dto->status, to))
        throw std::invalid_argument("Illegal status transition");
    if (to == TaskStatus::InProgress && !dto->blockedBy.empty())
        throw std::invalid_argument("Cannot move blocked task to InProgress");
    QJsonObject payload;
    payload["taskId"] = QString::fromStdString(taskId);
    payload["from"] = QString::fromStdString(task_codec::toString(dto->status));
    payload["to"] = QString::fromStdString(task_codec::toString(to));
    payload["estHours"] = dto->estHours;                          // S2's velocity reads this
    appendEvent(TaskMoved, "system", payload);
}
void TaskService::assign(const Id& taskId, const Id& memberId, const std::string& by) {
    if (!proj_.byId(taskId)) throw std::invalid_argument("Task not found");
    if (memberId.empty()) throw std::invalid_argument("memberId must not be empty");
    QJsonObject payload;
    payload["taskId"] = QString::fromStdString(taskId);
    payload["memberId"] = QString::fromStdString(memberId);
    payload["by"] = QString::fromStdString(by);
    appendEvent(TaskAssigned, by, payload);
}
void TaskService::unassign(const Id& taskId, const std::string& reason) {
    if (!proj_.byId(taskId)) throw std::invalid_argument("Task not found");
    QJsonObject payload;
    payload["taskId"] = QString::fromStdString(taskId);
    payload["reason"] = QString::fromStdString(reason);
    appendEvent(TaskUnassigned, "system", payload);
}
bool TaskService::addDependency(const Id& taskId, const Id& dependsOn) {
    if (!proj_.byId(taskId) || !proj_.byId(dependsOn)) return false;
    if (proj_.graph().hasDependency(taskId, dependsOn)) return true;   // already there, no new event
    TaskGraph probe = proj_.graph();                                   // copy: check without touching live state
    if (!probe.addDependency(taskId, dependsOn)) return false;
    QJsonObject payload;
    payload["taskId"] = QString::fromStdString(taskId);
    payload["dependsOnId"] = QString::fromStdString(dependsOn);
    appendEvent(DependencyAdded, "system", payload);
    return true;
}

