// BABY AIR - Lock-free audio-thread -> UI metering.
// Audio thread: process() updates atomics (wait-free). UI thread: read() polls them.
#pragma once
#include <atomic>
#include <cmath>

namespace babyair
{
class MeteringEngine
{
public:
    struct Reading { float peak = 0.0f; float rms = 0.0f; bool clipped = false; };

    void prepare (double sampleRate) noexcept
    {
        sr_ = sampleRate; ms_ = 0.0f;
        peak_.store (0.0f); rms_.store (0.0f); clip_.store (false);
    }

    void process (const float* const* data, int nCh, int n) noexcept
    {
        if (nCh <= 0 || n <= 0) return;
        float bp = 0.0f; double sumSq = 0.0;
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float v = data[c][i];
                bp = std::fmax (bp, std::fabs (v));
                sumSq += (double) v * v;
            }
        if (! std::isfinite (bp)) bp = 0.0f;
        float cur = peak_.load (std::memory_order_relaxed);
        while (bp > cur && ! peak_.compare_exchange_weak (cur, bp, std::memory_order_relaxed)) {}
        if (bp >= 0.9999f) clip_.store (true, std::memory_order_relaxed);

        const float blockMs = (float) (sumSq / (double) (nCh * n));
        const float coef = 1.0f - std::exp (-(float) n / (float) (0.3 * sr_)); // 300 ms RMS window
        ms_ += coef * (blockMs - ms_);
        rms_.store (std::sqrt (std::fmax (ms_, 0.0f)), std::memory_order_relaxed);
    }

    // UI thread. Peak is "max since last read".
    Reading read() noexcept
    {
        Reading r;
        r.peak = peak_.exchange (0.0f, std::memory_order_relaxed);
        r.rms = rms_.load (std::memory_order_relaxed);
        r.clipped = clip_.exchange (false, std::memory_order_relaxed);
        return r;
    }

private:
    double sr_ = 48000.0;
    float ms_ = 0.0f;                       // audio thread only
    std::atomic<float> peak_ { 0.0f }, rms_ { 0.0f };
    std::atomic<bool> clip_ { false };
};
} // namespace babyair
