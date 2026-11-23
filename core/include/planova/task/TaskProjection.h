#pragma once
#include <map>
#include <optional>
#include <set>
#include <vector>
#include "planova/common/Event.h"
#include "planova/common/IProjection.h"
#include "planova/common/Queries.h"
#include "planova/task/Task.h"
#include "planova/task/TaskGraph.h"
class TaskProjection : public IProjection, public ITaskQuery {
public:
    void reset() override;
    void apply(const Event&) override;
    std::vector<TaskDto> all() const override;                 // creation order
    std::optional<TaskDto> byId(const Id&) const override;
    const TaskGraph& graph() const { return graph_; }
private:
    std::set<Id> doneTaskIds() const;
    TaskDto toDto(const Task&, const std::set<Id>& doneIds) const;
    std::map<Id, Task> tasks_;
    std::vector<Id> order_;
    TaskGraph graph_;
};

