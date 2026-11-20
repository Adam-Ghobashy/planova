#include "planova/task/TaskFactory.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace {
bool isBlank(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
}
}
Task TaskFactory::create(const Id& id, const NewTask& in) {
    if (isBlank(in.title))
        throw std::invalid_argument("Task title must not be empty");
    if (in.complexity < 1 || in.complexity > 5)
        throw std::invalid_argument("Complexity must be 1..5");
    if (in.estHours <= 0)
        throw std::invalid_argument("estHours must be > 0");
    Task t;
    t.id = id;
    t.title = in.title;
    t.skill = in.skill;
    t.priority = in.priority;
    t.complexity = in.complexity;
    t.estHours = in.estHours;
    t.status = TaskStatus::ToDo;
    t.deps = in.deps;
    return t;
}
