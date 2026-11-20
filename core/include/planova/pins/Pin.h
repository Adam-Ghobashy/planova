#pragma once
// S5 contract header. The backend owner may extend this file; keep PinComment as is.
#include <cstdint>
#include <string>

#include "planova/common/Queries.h"
#include "planova/common/Types.h"

namespace planova {

// The Phase 0 common headers declare these names in the global namespace.
// Re-export the names S5 uses so planova:: and planova::s5:: code can use them unqualified.
using ::Id;
using ::Priority;
using ::SkillCategory;
using ::TaskStatus;
using ::PinCategory;
using ::TaskDto;
using ::MemberDto;
using ::PinDto;
using ::ITaskQuery;
using ::IMemberQuery;
using ::IPinQuery;
using ::IConflictQuery;

struct PinComment {
    Id id;
    Id authorId;
    std::string text;
    int64_t ts;
};

}  // namespace planova
