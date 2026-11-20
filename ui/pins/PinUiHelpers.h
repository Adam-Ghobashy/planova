#pragma once
#include <QString>
#include <QStyle>
#include <QWidget>

#include "planova/pins/Pin.h"

namespace planova::s5 {

inline constexpr const char* kPinMime = "application/x-planova-pin";

// QSS only re-evaluates a dynamic property after unpolish/polish.
inline void repolish(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

// Popups get a frameless window. translucent = true lets pins.qss round the corners (menus only):
// a translucent window can flash black on first show and shows through wherever QSS paints nothing,
// so the combo list stays opaque. Call before the popup is shown for the first time.
inline void applyPopupChrome(QWidget* popup, bool translucent)
{
    popup->setWindowFlag(Qt::FramelessWindowHint, true);
    popup->setWindowFlag(Qt::NoDropShadowWindowHint, true);
    popup->setAttribute(Qt::WA_TranslucentBackground, translucent);
}

// Value of the `category` QSS property.
inline const char* categoryKey(PinCategory c)
{
    switch (c) {
    case PinCategory::HandoffNote: return "handoff";
    case PinCategory::FeedbackBlocker: return "blocker";
    case PinCategory::ReadyForTesting: return "testing";
    case PinCategory::ArchitectureDecision: return "architecture";
    }
    return "handoff";
}

inline QString categoryTitle(PinCategory c)
{
    switch (c) {
    case PinCategory::HandoffNote: return QStringLiteral("Handoff Notes");
    case PinCategory::FeedbackBlocker: return QStringLiteral("Feedback and Blockers");
    case PinCategory::ReadyForTesting: return QStringLiteral("Ready for Testing");
    case PinCategory::ArchitectureDecision: return QStringLiteral("Architecture and Decisions");
    }
    return {};
}

// First letter of up to two words, uppercase.
inline QString initialsOf(const QString& name)
{
    QString out;
    const QStringList parts = name.split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size() && i < 2; ++i) out += parts[i].at(0).toUpper();
    return out.isEmpty() ? QStringLiteral("?") : out;
}

inline QString firstNameOf(const QString& name)
{
    const QStringList parts = name.split(' ', Qt::SkipEmptyParts);
    return parts.isEmpty() ? name : parts.first();
}

}  // namespace planova::s5
