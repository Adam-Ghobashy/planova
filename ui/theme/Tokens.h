#pragma once

enum class Token {
    Background, Sidebar, StatusBar, Surface, SurfaceAlt, Track, Border,
    TextPrimary, TextMuted, Accent, AccentText, AccentSoft,
    SuccessSoft, WarningSoft, DangerSoft,
    PinHandoff, PinBlocker, PinTesting, PinArchitecture,
    Member1, Member2, Member3, Member4, Member5, Member6,
    PizzaCrust, PizzaEdge, PizzaCheese, PizzaSauce, PizzaBasil,
    DangerText, WarningText, Complexity2, Complexity4, Complexity3,
    TextSecondary, TextInverse, SurfaceRaised, SurfaceHover, SurfaceGlass, GlassSheen,
    BorderStrong, Overlay, Shadow, AccentHover, AccentPressed,
    NavActiveBg, NavActiveText, NavBadgeBg, NavBadgeText,
    PositiveText, ReviewSoft, ReviewText
};

// Enum names as text, same order as Token. Used to resolve @TokenName in qss files.
inline constexpr const char* kTokenNames[] = {
    "Background", "Sidebar", "StatusBar", "Surface", "SurfaceAlt", "Track", "Border",
    "TextPrimary", "TextMuted", "Accent", "AccentText", "AccentSoft",
    "SuccessSoft", "WarningSoft", "DangerSoft",
    "PinHandoff", "PinBlocker", "PinTesting", "PinArchitecture",
    "Member1", "Member2", "Member3", "Member4", "Member5", "Member6",
    "PizzaCrust", "PizzaEdge", "PizzaCheese", "PizzaSauce", "PizzaBasil",
    "DangerText", "WarningText", "Complexity2", "Complexity4", "Complexity3",
    "TextSecondary", "TextInverse", "SurfaceRaised", "SurfaceHover", "SurfaceGlass", "GlassSheen",
    "BorderStrong", "Overlay", "Shadow", "AccentHover", "AccentPressed",
    "NavActiveBg", "NavActiveText", "NavBadgeBg", "NavBadgeText",
    "PositiveText", "ReviewSoft", "ReviewText"
};

// Eight-digit values are #AARRGGBB (alpha first), as QColor reads them.
inline constexpr const char* kLightHex[] = {
    "#FAF8F5", "#F5F1E8", "#EDE9E1", "#FFFFFF", "#F5F2EA", "#ECE7DC", "#E7E2D6",
    "#1A1A1F", "#6B6B73", "#5438DC", "#FFFFFF", "#F3F0FF",
    "#D4F981", "#FBE9C2", "#FEE2E2",
    "#E8F8F0", "#FDF2F8", "#FEFCE8", "#EFF6FF",
    "#EF4444", "#6366F1", "#10B981", "#8B5CF6", "#F59E0B", "#EC4899",
    "#D97706", "#92400E", "#FBBF24", "#DC2626", "#15803D",
    "#991B1B", "#92400E", "#34D399", "#FB923C", "#FBBF24",
    "#52525B", "#FFFFFF", "#F5F2EA", "#ECE7DC", "#B8FFFFFF", "#66FFFFFF",
    "#2E18181B", "#B3000000", "#1718181B", "#4526CB", "#381BAF",
    "#5438DC", "#FFFFFF", "#1418181B", "#52525B",
    "#13380B", "#FEF3C7", "#92400E"
};

inline constexpr const char* kDarkHex[] = {
    "#0A0A0B", "#121215", "#121215", "#18181C", "#222228", "#2A2A31", "#2C2C33",
    "#F4F4F5", "#A1A1AA", "#F4F4F5", "#0A0A0B", "#2A2A31",
    "#36363B", "#3A2E14", "#3B1D1F",
    "#132A22", "#2E1626", "#2E2512", "#15223A",
    "#EF4444", "#6366F1", "#10B981", "#8B5CF6", "#F59E0B", "#EC4899",
    "#D97706", "#92400E", "#FBBF24", "#DC2626", "#15803D",
    "#FCA5A5", "#FCD34D", "#8E8E95", "#DCDCE0", "#B8B8BE",
    "#A1A1AA", "#0A0A0B", "#222228", "#1F1F24", "#B8121216", "#1FFFFFFF",
    "#3F3F46", "#B3000000", "#CC000000", "#FFFFFF", "#E4E4E7",
    "#2A2A31", "#F4F4F5", "#26262B", "#A1A1AA",
    "#F4F4F5", "#3A2E14", "#FCD34D"
};

constexpr int kTokenCount = static_cast<int>(Token::ReviewText) + 1;

static_assert(sizeof(kLightHex) / sizeof(kLightHex[0]) == kTokenCount, "kLightHex must have one entry per Token");
static_assert(sizeof(kDarkHex) / sizeof(kDarkHex[0]) == kTokenCount, "kDarkHex must have one entry per Token");
static_assert(sizeof(kTokenNames) / sizeof(kTokenNames[0]) == kTokenCount, "kTokenNames must have one entry per Token");
