#pragma once

enum class Token {
    Background, Sidebar, StatusBar, Surface, SurfaceAlt, Track, Border,
    TextPrimary, TextMuted, Accent, AccentText, AccentSoft,
    SuccessSoft, WarningSoft, DangerSoft,
    PinHandoff, PinBlocker, PinTesting, PinArchitecture,
    Member1, Member2, Member3, Member4, Member5, Member6,
    PizzaCrust, PizzaEdge, PizzaCheese, PizzaSauce, PizzaBasil,
    DangerText, WarningText, Complexity2, Complexity4
};

inline constexpr const char* kLightHex[] = {
    "#FAF8F5", "#F5F1E8", "#EDE9E1", "#FFFFFF", "#F5F2EA", "#ECE7DC", "#E7E2D6",
    "#1A1A1F", "#6B6B73", "#5438DC", "#FFFFFF", "#F3F0FF",
    "#D4F981", "#FBE9C2", "#FEE2E2",
    "#E8F8F0", "#FDF2F8", "#FEFCE8", "#EFF6FF",
    "#EF4444", "#6366F1", "#10B981", "#8B5CF6", "#F59E0B", "#EC4899",
    "#D97706", "#92400E", "#FBBF24", "#DC2626", "#15803D",
    "#991B1B", "#92400E", "#34D399", "#FB923C"
};

static_assert(sizeof(kLightHex) / sizeof(kLightHex[0]) == static_cast<int>(Token::Complexity4) + 1,
              "kLightHex must have one entry per Token");
