#pragma once
#include <vector>

namespace planova::slicer {

struct SliceFrame {
    std::vector<double> cutProgress;
    bool wheelVisible = false;
    double wheelDistance = 1.0;
    double wheelAngleDeg = 0.0;
    bool wheelLifted = false;
    double spread = 0.0;
};

class SliceTimeline {
public:
    static constexpr int kCutMs = 400;
    static constexpr int kMinCutMs = 150;
    static constexpr int kMaxCuttingMs = 3000;
    static constexpr int kSpreadMs = 700;

    explicit SliceTimeline(std::vector<double> cutAnglesDeg);

    int totalMs() const;
    SliceFrame at(int elapsedMs) const;

private:
    std::vector<double> cutAngles_;
    int cutMs_ = kCutMs;
};

}
