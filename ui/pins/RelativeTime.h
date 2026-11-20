#pragma once
#include <QString>

namespace planova::s5 {

// "Just now" / "12m ago" / "4h ago" / "Yesterday" / "5d ago". Negative diff (clock skew) = "Just now".
inline QString formatRelative(qint64 tsMs, qint64 nowMs)
{
    const qint64 diff = nowMs - tsMs;
    if (diff < 60 * 1000) return QStringLiteral("Just now");
    if (diff < 60LL * 60 * 1000) return QStringLiteral("%1m ago").arg(diff / (60 * 1000));
    if (diff < 24LL * 60 * 60 * 1000) return QStringLiteral("%1h ago").arg(diff / (60LL * 60 * 1000));
    if (diff < 48LL * 60 * 60 * 1000) return QStringLiteral("Yesterday");
    return QStringLiteral("%1d ago").arg(diff / (24LL * 60 * 60 * 1000));
}

}  // namespace planova::s5
