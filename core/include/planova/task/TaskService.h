#pragma once
#include <string>
#include <QJsonObject>
#include "planova/common/Clock.h"
#include "planova/common/Dtos.h"
#include "planova/store/EventLog.h"
#include "planova/task/TaskProjection.h"
class TaskService {
public:
    TaskService(EventLog&, TaskProjection&, IClock&);
    Id create(const NewTask&);                                   // throws std::invalid_argument
    void move(const Id&, TaskStatus to);                         // throws on illegal transition or if blocked
    void assign(const Id&, const Id& memberId, const std::string& by);
    void unassign(const Id&, const std::string& reason);
    bool addDependency(const Id&, const Id& dependsOn);          // false on cycle / unknown task
private:
    void appendEvent(const std::string& type, const std::string& author, const QJsonObject& payload);
    EventLog& log_;
    TaskProjection& proj_;
    IClock& clock_;
};



