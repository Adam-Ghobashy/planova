#pragma once
#include <vector>

namespace planova::slicer {

// What the slicing animation looks like at one moment.
struct SliceFrame {
    std::vector<double> cutProgress;  // per cut: 0 = not started, 1 = fully cut
    bool wheelVisible = false;        // the pizza cutter is on screen
    double wheelDistance = 1.0;       // 0 = pizza centre, 1 = just outside the rim
    double wheelAngleDeg = 0.0;       // clockwise from 12 o'clock
    bool wheelLifted = false;         // the cutter is moving without cutting
    double spread = 0.0;              // 0 = slices together, 1 = slices settled apart
};

// The schedule of the slicing animation. For each cut the cutter rolls from the rim to the
// centre (cutting), lifts back out along the same line, then moves round the rim to the next
// cut. When every cut is done the slices spread apart. Pure maths, so it is easy to test.
class SliceTimeline {
public:
    static constexpr int kCutMs = 400;           // longest time for one cut, including the move to the next
    static constexpr int kMinCutMs = 150;        // shortest time for one cut
    static constexpr int kMaxCuttingMs = 3000;   // all cuts together; with many cuts each one is sped up
    static constexpr int kSpreadMs = 700;        // time for the slices to spread apart

    // One angle per cut (clockwise from 12 o'clock), in the order they are cut.
    explicit SliceTimeline(std::vector<double> cutAnglesDeg);

    int totalMs() const;
    SliceFrame at(int elapsedMs) const;

private:
    std::vector<double> cutAngles_;
    int cutMs_ = kCutMs;  // time per cut for this timeline
};

} // namespace planova::slicer
