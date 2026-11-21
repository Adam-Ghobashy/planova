#pragma once
// Helpers shared by PinService and PinProjection. Header-only, std only.
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "planova/common/Types.h"

namespace planova {

inline constexpr std::array<const char*, 4> kPinCategoryNames = {
    "HandoffNote", "FeedbackBlocker", "ReadyForTesting", "ArchitectureDecision"};

inline std::string categoryToString(PinCategory c)
{
    return kPinCategoryNames[static_cast<std::size_t>(c)];
}

inline std::optional<PinCategory> categoryFromString(const std::string& s)
{
    for (std::size_t i = 0; i < kPinCategoryNames.size(); ++i)
        if (s == kPinCategoryNames[i])
            return static_cast<PinCategory>(i);
    return std::nullopt;
}

inline std::string trimmed(const std::string& s)
{
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    const auto first = std::find_if_not(s.begin(), s.end(), isSpace);
    const auto last = std::find_if_not(s.rbegin(), s.rend(), isSpace).base();
    return first < last ? std::string(first, last) : std::string();
}

// Trim, lowercase, strip one leading '#', drop empty tags and duplicates, keep order.
inline std::vector<std::string> normaliseTags(const std::vector<std::string>& tags)
{
    std::vector<std::string> result;
    for (const std::string& raw : tags) {
        std::string tag = trimmed(raw);
        if (!tag.empty() && tag[0] == '#')
            tag = trimmed(tag.substr(1));
        std::transform(tag.begin(), tag.end(), tag.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!tag.empty() && std::find(result.begin(), result.end(), tag) == result.end())
            result.push_back(tag);
    }
    return result;
}

}  // namespace planova
