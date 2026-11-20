#include "planova/task/TaskStateMachine.h"
bool TaskStateMachine::canMove(TaskStatus from, TaskStatus to) {
    //                              ToDo   InProg Review Done
    static constexpr bool table[4][4] = {
                                          /* ToDo       */ { false, true,  false, false },
                                          /* InProgress */ { true,  false, true,  false },
                                          /* Review     */ { false, true,  false, true  },
                                          /* Done       */ { false, false, false, false },
                                          };
    const int f = static_cast<int>(from);
    const int t = static_cast<int>(to);
    if (f < 0 || f >= 4 || t < 0 || t >= 4) return false;
    return table[f][t];
}
