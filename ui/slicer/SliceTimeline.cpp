#include "slicer/SliceTimeline.h"
#include <algorithm>
#include <cmath>

namespace planova::slicer {

namespace {
// Fractions of one cut's time: cutting in, lifting out, then travelling to the next cut.
constexpr double kCuttingEnd = 0.55;
constexpr double kLiftingEnd = 0.80;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double normalizeDegrees(double deg) {
    deg = std::fmod(deg, 360.0);
    return deg < 0.0 ? deg + 360.0 : deg;
}
} // namespace

SliceTimeline::SliceTimeline(std::vector<double> cutAnglesDeg) : cutAngles_(std::move(cutAnglesDeg)) {
    if (!cutAngles_.empty()) {
        cutMs_ = std::clamp(kMaxCuttingMs / static_cast<int>(cutAngles_.size()), kMinCutMs, kCutMs);
    }
}

int SliceTimeline::totalMs() const {
    return static_cast<int>(cutAngles_.size()) * cutMs_ + kSpreadMs;
}

SliceFrame SliceTimeline::at(int elapsedMs) const {
    const int cuts = static_cast<int>(cutAngles_.size());
    const int cuttingMs = cuts * cutMs_;

    SliceFrame frame;
    frame.cutProgress.assign(cutAngles_.size(), 0.0);
    for (int i = 0; i < cuts; ++i) {
        const double u = clamp01(static_cast<double>(elapsedMs - i * cutMs_) / cutMs_);
        frame.cutProgress[i] = (std::min)(1.0, u / kCuttingEnd);
    }

    if (cuts > 0 && elapsedMs < cuttingMs) {
        const int i = std::clamp(elapsedMs / cutMs_, 0, cuts - 1);
        const double u = clamp01(static_cast<double>(elapsedMs - i * cutMs_) / cutMs_);
        frame.wheelVisible = true;
        frame.wheelAngleDeg = cutAngles_[i];
        if (u < kCuttingEnd) {
            frame.wheelDistance = 1.0 - u / kCuttingEnd;                              // rim -> centre
        } else if (u < kLiftingEnd) {
            frame.wheelDistance = (u - kCuttingEnd) / (kLiftingEnd - kCuttingEnd);    // centre -> rim
            frame.wheelLifted = true;
        } else {
            frame.wheelLifted = true;
            if (i + 1 < cuts) {
                // Round the rim, clockwise, to the start of the next cut.
                const double arc = normalizeDegrees(cutAngles_[i + 1] - cutAngles_[i]);
                frame.wheelAngleDeg = normalizeDegrees(cutAngles_[i] + arc * (u - kLiftingEnd) / (1.0 - kLiftingEnd));
            }
        }
    }

    frame.spread = clamp01(static_cast<double>(elapsedMs - cuttingMs) / kSpreadMs);
    return frame;
}

} // namespace planova::slicer
