// BABY AIR - Linear-phase polyphase half-band oversampler (1x/2x/4x/8x/16x).
//
// Design decisions
//  * Cascade of 2x half-band FIR stages (Kaiser window, beta 8 => ~80 dB stop-band).
//    Half-band structure: every odd-distance tap is non-zero, every even-distance tap (except
//    the centre) is zero, so each stage costs ~half of a generic FIR.
//  * Linear phase => no phase distortion of the dry/wet relationship, constant latency.
//  * Stage 1 (base rate -> 2x) is long and sharp (protects 20 kHz at 44.1 kHz); later stages run
//    at high rates where the transition band is huge, so they are short.
//  * The cascade latency is generally fractional in base-rate samples. A small pad delay is added
//    at the top (oversampled) rate so the total is an exact integer number of base-rate samples,
//    which lets the dry path be aligned sample-exactly (no comb filtering when mixing).
//  * Fixed-size storage only: no allocation in the audio thread.
#pragma once
#include "DspCommon.h"

namespace babyair
{
class HalfbandStage
{
public:
    static constexpr int kMaxHalf = 42; // 2m+2, m <= 19

    void design (int m);
    void reset() noexcept;
    int m() const noexcept { return m_; }

    // Upsample: one input -> two outputs (time ordered).
    inline void up (float x, float& y0, float& y1) noexcept
    {
        pushUp (x);
        const float* h = &upBuf_[upPos_];
        float acc = 0.0f;
        for (int j = 0; j < H_; ++j) acc += e_[j] * h[j];
        y0 = 2.0f * acc;
        y1 = h[m_];
    }
    // Downsample: two inputs (time ordered) -> one output.
    inline float down (float a, float b) noexcept
    {
        dnPos_ = (dnPos_ == 0) ? H_ - 1 : dnPos_ - 1;
        dnA_[dnPos_] = dnA_[dnPos_ + H_] = a;
        dnB_[dnPos_] = dnB_[dnPos_ + H_] = b;
        const float* ha = &dnA_[dnPos_];
        float acc = 0.0f;
        for (int j = 0; j < H_; ++j) acc += e_[j] * ha[j];
        return acc + 0.5f * dnB_[dnPos_ + m_ + 1];
    }

private:
    inline void pushUp (float x) noexcept
    {
        upPos_ = (upPos_ == 0) ? H_ - 1 : upPos_ - 1;
        upBuf_[upPos_] = upBuf_[upPos_ + H_] = x;
    }
    int m_ = 1, H_ = 4, upPos_ = 0, dnPos_ = 0;
    float e_[kMaxHalf] {};
    float upBuf_[2 * kMaxHalf] {}, dnA_[2 * kMaxHalf] {}, dnB_[2 * kMaxHalf] {};
};

class Oversampler
{
public:
    static constexpr int kMaxStages = 4; // 16x

    // log2Factor: 0 (1x, pass-through) .. 4 (16x)
    void prepare (double sampleRate, int log2Factor);
    void reset() noexcept;
    int factor() const noexcept { return 1 << stages_; }
    int log2Factor() const noexcept { return stages_; }
    int latencySamples() const noexcept { return latency_; } // in base-rate samples (integer)

    // Process one base-rate sample. `shape(float* buf, int n)` is called with the n oversampled samples
    // and must process them in place (this is where all non-linear processing lives).
    template <class Shape>
    inline float process (float x, Shape&& shape) noexcept
    {
        float a[16], b[16];
        float* cur = a;
        float* nxt = b;
        cur[0] = x;
        int count = 1;
        for (int s = 0; s < stages_; ++s)
        {
            for (int i = 0; i < count; ++i)
                stage_[s].up (cur[i], nxt[2 * i], nxt[2 * i + 1]);
            std::swap (cur, nxt);
            count *= 2;
        }
        shape (cur, count);
        if (pad_ > 0)
            for (int i = 0; i < count; ++i)
            {
                const float out = padBuf_[padPos_];
                padBuf_[padPos_] = cur[i];
                padPos_ = (padPos_ + 1 == pad_) ? 0 : padPos_ + 1;
                cur[i] = out;
            }
        for (int s = stages_ - 1; s >= 0; --s)
        {
            for (int i = 0; i < count / 2; ++i)
                nxt[i] = stage_[s].down (cur[2 * i], cur[2 * i + 1]);
            std::swap (cur, nxt);
            count /= 2;
        }
        return cur[0];
    }

private:
    HalfbandStage stage_[kMaxStages];
    int stages_ = 0, latency_ = 0, pad_ = 0, padPos_ = 0;
    float padBuf_[16] {};
};
} // namespace babyair
