#include "app/SplashTimeline.h"
#include <algorithm>

namespace {
constexpr int kFadeInEndMs = 200;
constexpr int kWordStartMs = 150;
constexpr int kWordEndMs = 1250;
constexpr int kMarkStartMs = 1320;
constexpr int kMarkEndMs = 1540;
constexpr int kSubtitleStartMs = 1450;
constexpr int kSubtitleEndMs = 1800;
constexpr double kSubtitleRisePx = 4.0;

// Position of ms between from and to as 0..1.
double progress(int ms, int from, int to)
{
    return std::clamp(static_cast<double>(ms - from) / (to - from), 0.0, 1.0);
}

double outQuart(double t)
{
    const double rest = 1.0 - t;
    return 1.0 - rest * rest * rest * rest;
}
}

SplashFrame SplashTimeline::at(int ms)
{
    ms = std::clamp(ms, 0, kTotalMs);

    SplashFrame frame;
    if (ms < kSkipMs)
        frame.windowOpacity = progress(ms, 0, kFadeInEndMs);
    else
        frame.windowOpacity = 1.0 - progress(ms, kSkipMs, kTotalMs);
    frame.wordReveal = outQuart(progress(ms, kWordStartMs, kWordEndMs));
    frame.markReveal = progress(ms, kMarkStartMs, kMarkEndMs);
    frame.subtitleOpacity = progress(ms, kSubtitleStartMs, kSubtitleEndMs);
    frame.subtitleRise = kSubtitleRisePx * (1.0 - frame.subtitleOpacity);
    return frame;
}
