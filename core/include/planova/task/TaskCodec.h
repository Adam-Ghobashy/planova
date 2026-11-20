#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include "planova/common/Types.h"
namespace task_codec {
inline constexpr std::array<const char*, 4> kStatus   = {"ToDo", "InProgress", "Review", "Done"};
inline constexpr std::array<const char*, 4> kPriority = {"Low", "Medium", "High", "Critical"};
inline constexpr std::array<const char*, 7> kSkill    = {"Backend", "Frontend", "Storage", "UiUx",
                                                       "Integration", "Testing", "Documentation"};
template <class E, std::size_t N>
std::optional<E> fromName(const std::array<const char*, N>& names, const std::string& s) {
    for (std::size_t i = 0; i < N; ++i)
        if (s == names[i]) return static_cast<E>(i);
    return std::nullopt;
}
inline std::string toString(TaskStatus s)    { return kStatus[static_cast<std::size_t>(s)]; }
inline std::string toString(Priority p)      { return kPriority[static_cast<std::size_t>(p)]; }
inline std::string toString(SkillCategory c) { return kSkill[static_cast<std::size_t>(c)]; }
inline std::optional<TaskStatus>    statusFromString(const std::string& s)   { return fromName<TaskStatus>(kStatus, s); }
inline std::optional<Priority>      priorityFromString(const std::string& s) { return fromName<Priority>(kPriority, s); }
inline std::optional<SkillCategory> skillFromString(const std::string& s)    { return fromName<SkillCategory>(kSkill, s); }
} // namespace task_codec

