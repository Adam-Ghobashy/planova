#pragma once
// S5 contract header. The backend owner may extend this file; keep PinComment as is.
#include <cstdint>
#include <string>

#include "planova/common/Queries.h"
#include "planova/common/Types.h"

namespace planova {

// The Phase 0 common headers currently declare everything in planova::s6.
// Re-export the names S5 uses so planova:: and planova::s5:: code can use them unqualified.
using s6::Id;
using s6::Priority;
using s6::SkillCategory;
using s6::TaskStatus;
using s6::PinCategory;
using s6::TaskDto;
using s6::MemberDto;
using s6::PinDto;
using s6::ITaskQuery;
using s6::IMemberQuery;
using s6::IPinQuery;
using s6::IConflictQuery;

struct PinComment {
    Id id;
    Id authorId;
    std::string text;
    int64_t ts;
};

}  // namespace planova
