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
    PositiveText, ReviewSoft, ReviewText,
    TextTertiary, BorderDefault, BorderSubtle, ControlBorder, BorderSoft, Divider,
    ScrollHandle, ScrollHandleStrong, ScrollThumb, ScrollThumbHover,
    SurfaceDialog, AccentSoftStrong, DoneChipText, ReviewBorder,
    DangerAction, DangerBorder, StatusConnected, ShadowSoft,
    StickyGreenBorder, StickyGreenText, StickyPinkBorder, StickyPinkText,
    StickyBlueBorder, StickyBlueText, StickyYellowBorder, StickyYellowText,
    ChartDone,
    BackdropTop, BackdropBottom, GlowPrimary, GlowSecondary, GlassFill, GlassFillStrong, GlassEdge,
    MemberText   // avatar initials on the Member1..Member6 fills, same in both themes
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
    "PositiveText", "ReviewSoft", "ReviewText",
    "TextTertiary", "BorderDefault", "BorderSubtle", "ControlBorder", "BorderSoft", "Divider",
    "ScrollHandle", "ScrollHandleStrong", "ScrollThumb", "ScrollThumbHover",
    "SurfaceDialog", "AccentSoftStrong", "DoneChipText", "ReviewBorder",
    "DangerAction", "DangerBorder", "StatusConnected", "ShadowSoft",
    "StickyGreenBorder", "StickyGreenText", "StickyPinkBorder", "StickyPinkText",
    "StickyBlueBorder", "StickyBlueText", "StickyYellowBorder", "StickyYellowText",
    "ChartDone",
    "BackdropTop", "BackdropBottom", "GlowPrimary", "GlowSecondary", "GlassFill", "GlassFillStrong", "GlassEdge",
    "MemberText"
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
    "#13380B", "#FEF3C7", "#92400E",
    "#71717A", "#1A18181B", "#0F18181B", "#E0DBCF", "#EFEBE3", "#DDD8CC",
    "#D9D4C8", "#CFCABD", "#596B6B73", "#996B6B73",
    "#EAE6DF", "#EAE5FF", "#3F6212", "#F7E3A1",
    "#DC2626", "#EF4444", "#10B981", "#0D18181B",
    "#86EFAC", "#14532D", "#F9A8D4", "#831843",
    "#93C5FD", "#1E3A8A", "#FDE047", "#713F12",
    "#D4F981",
    "#FAF8F5", "#F5F1E8", "#0F5438DC", "#14F59E0B", "#BFFFFFFF", "#E0FFFFFF", "#1418181B",
    "#05040C"
};

inline constexpr const char* kDarkHex[] = {
    // Smoke glass: neutral grey surfaces and a white accent (tst_Contrast keeps every non-colour token grey). Text
    // colours are set so they pass 4.5:1 on a glass panel, and on a strong glass card inside a glass panel, at the
    // brightest backdrop points (tst_Contrast).
    "#0E0F12", "#121317", "#121317", "#18191D", "#202226", "#2C2E33", "#2E3035",
    "#F2F3F5", "#AEB1B7", "#E8EAEE", "#0E0F12", "#26282D",
    "#1C3A30", "#3A2D1A", "#3D1C2A",
    "#24262A", "#26282C", "#26272A", "#24262A",
    "#EF4444", "#6366F1", "#10B981", "#8B5CF6", "#F59E0B", "#EC4899",
    "#D97706", "#92400E", "#FBBF24", "#DC2626", "#15803D",
    "#FCA5A5", "#FCD34D", "#94969C", "#DFE0E4", "#B9BBC1",
    "#B4B7BD", "#0E0F12", "#202226", "#25272B", "#B8141518", "#2EFFFFFF",
    "#3E4046", "#B3050507", "#CC000000", "#FFFFFF", "#CACDD3",
    "#383A40", "#F2F3F5", "#2A2C31", "#C5C7CC",
    "#F2F3F5", "#2E2F33", "#FCD34D",
    "#AEB1B7", "#17FFFFFF", "#0FFFFFFF", "#3E4046", "#2A2C31", "#2E3035",
    "#3E4046", "#505259", "#59A9ACB2", "#99A9ACB2",
    "#1B1C20", "#303237", "#F4F4F7", "#40F59E0B",
    "#FCA5A5", "#EF4444", "#34D399", "#99000000",
    "#4010B981", "#D1FAE5", "#40EC4899", "#FCE7F3",
    "#403B82F6", "#DBEAFE", "#40F59E0B", "#FEF3C7",
    "#34D399",
    "#0E0F12", "#16181D", "#14FFFFFF", "#0AFFFFFF", "#0FFFFFFF", "#17FFFFFF", "#24FFFFFF",
    "#05040C"
};

constexpr int kTokenCount = static_cast<int>(Token::MemberText) + 1;

static_assert(sizeof(kLightHex) / sizeof(kLightHex[0]) == kTokenCount, "kLightHex must have one entry per Token");
static_assert(sizeof(kDarkHex) / sizeof(kDarkHex[0]) == kTokenCount, "kDarkHex must have one entry per Token");
static_assert(sizeof(kTokenNames) / sizeof(kTokenNames[0]) == kTokenCount, "kTokenNames must have one entry per Token");
