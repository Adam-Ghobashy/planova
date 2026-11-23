#pragma once

// Everything the splash needs to draw one moment of its animation. All values are 0..1 except subtitleRise (pixels).
struct SplashFrame {
    double windowOpacity = 0.0;
    double wordReveal = 0.0;       // how much of "Welcome" is written
    double markReveal = 0.0;       // how much of "!" is written
    double subtitleOpacity = 0.0;
    double subtitleRise = 4.0;     // pixels the subtitle still has to move up
};

// Pure maths of the 2.4 s welcome splash, no widgets. SplashWindow asks for a frame on every tick.
class SplashTimeline {
public:
    static constexpr int kTotalMs = 2400;
    static constexpr int kSkipMs = 2100;   // a click or key press jumps here, which is where the fade-out starts

    // Times outside 0..kTotalMs are clamped.
    static SplashFrame at(int ms);
};
