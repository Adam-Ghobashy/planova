#pragma once

class PlanovaCore;

namespace demo {

// DEV ONLY (planova.exe --demo): fills an empty workspace with the sample board
// (6 members, 9 tasks, 4 pins).
void seedIfEmpty(PlanovaCore& core);

} // namespace demo
