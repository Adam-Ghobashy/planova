#pragma once
#include "planova/common/EventTypes.h"
#include "planova/common/Types.h"
#include <string>

namespace planova::slicer {


inline std::string skillToString(SkillCategory s) {
    switch (s) {
    case SkillCategory::Backend: return "Backend";
    case SkillCategory::Frontend: return "Frontend";
    case SkillCategory::Storage: return "Storage";
    case SkillCategory::UiUx: return "UI/UX";
    case SkillCategory::Integration: return "Integration";
    case SkillCategory::Testing: return "Testing";
    case SkillCategory::Documentation: return "Documentation";
    }
    return "Unknown";
}

inline std::string priorityToString(Priority p) {
    switch (p) {
    case Priority::Low: return "Low";
    case Priority::Medium: return "Medium";
    case Priority::High: return "High";
    case Priority::Critical: return "Critical";
    }
    return "Unknown";
}

}
