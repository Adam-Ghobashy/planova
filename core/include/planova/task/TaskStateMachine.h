#pragma once
#include "planova/common/Types.h"
class TaskStateMachine {
public:
    // ToDo->InProgress, InProgress->Review, Review->Done, Review->InProgress, InProgress->ToDo
    static bool canMove(TaskStatus from, TaskStatus to);
};

