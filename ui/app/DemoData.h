#pragma once
#include <string>

class PlanovaCore;

namespace demo {

// DEV ONLY: fills an empty workspace with the Figma sample board (6 members, 9 tasks).
void seedIfEmpty(PlanovaCore& core);

// TEMP until S5 (Pins) exists: pin badge count shown on the sample cards.
int pinCountFor(const std::string& taskTitle);

} // namespace demo