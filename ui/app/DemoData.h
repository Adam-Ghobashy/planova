#pragma once
#include <string>

class PlanovaCore;

namespace demo {

// DEV ONLY: fills an empty workspace with the Figma sample board (6 members, 9 tasks).
void seedIfEmpty(PlanovaCore& core);

// Sample pin counts for the demo task titles; remove when the Pins backend exists.
int pinCountFor(const std::string& taskTitle);

} // namespace demo