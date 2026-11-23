#pragma once
#include "planova/common/Dtos.h"
#include "planova/task/Task.h"
class TaskFactory {
public:
    // Throws std::invalid_argument on invalid input.
    static Task create(const Id& id, const NewTask& input);
};

