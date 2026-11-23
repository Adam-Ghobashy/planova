#pragma once
#include <optional>
#include <string>
#include <vector>
#include "planova/common/Types.h"
struct Task {
    Id id;
    std::string title;
    SkillCategory skill = SkillCategory::Backend;
    Priority priority = Priority::Medium;
    int complexity = 1;
    int estHours = 1;
    TaskStatus status = TaskStatus::ToDo;
    std::optional<Id> assignee;
    std::vector<Id> deps;
};

