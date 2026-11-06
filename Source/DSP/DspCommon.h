// BABY AIR - DSP common helpers (pure C++17, no JUCE dependency)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace babyair
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxChannels = 2;

inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float linToDb (float x) noexcept { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }

// NOTE: std::isfinite relies on IEEE semantics -> never build this code with -ffast-math.
inline float sanitize (float x) noexcept { return std::isfinite (x) ? x : 0.0f; }

// Absolute safety bound (+24 dBFS). This is NOT a limiter: it only prevents a runaway value
// from propagating. Normal signals never reach it.
inline float safetyBound (float x) noexcept { return std::clamp (x, -16.0f, 16.0f); }

// One-pole smoother (linear domain), per-sample, zipper-free.
struct Smoother
{
    void prepare (double sampleRate, double timeMs) noexcept
    {
        coef = 1.0f - std::exp (-1.0f / (float) (0.001 * timeMs * sampleRate));
    }
    void snap (float v) noexcept { cur = target = v; }
    void setTarget (float v) noexcept { target = v; }
    inline float next() noexcept
    {
        cur += coef * (target - cur);
        if (std::abs (target - cur) < 1.0e-7f) cur = target;
        return cur;
    }
    bool settled() const noexcept { return cur == target; }
    float cur = 0.0f, target = 0.0f, coef = 0.01f;
};

// Envelope follower on signal power, returns RMS-like amplitude.
struct EnvFollower
{
    void prepare (double sampleRate, double attackMs, double releaseMs) noexcept
    {
        a = 1.0f - std::exp (-1.0f / (float) (0.001 * attackMs * sampleRate));
        r = 1.0f - std::exp (-1.0f / (float) (0.001 * releaseMs * sampleRate));
    }
    void reset (float v = 0.0f) noexcept { state = v * v; }
    inline float processPower (float p) noexcept
    {
        state += (p > state ? a : r) * (p - state);
        if (state < 1.0e-20f) state = 0.0f;
        return std::sqrt (state);
    }
    inline float process (float x) noexcept { return processPower (x * x); }
    float a = 0.01f, r = 0.001f, state = 0.0f;
};

// First-order DC blocker (~35 Hz @ 48k).
struct DcBlocker
{
    void reset() noexcept { x1 = y1 = 0.0f; }
    inline float process (float x) noexcept
    {
        const float y = x - x1 + 0.995f * y1;
        x1 = x;
        y1 = (std::abs (y) < 1.0e-20f) ? 0.0f : y;
        return y;
    }
    float x1 = 0.0f, y1 = 0.0f;
};
} // namespace babyair
