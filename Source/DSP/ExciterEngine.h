// BABY AIR - Top level DSP engine (no JUCE dependency).
#pragma once
#include "ModeProcessor.h"

namespace babyair
{
struct ExciterParams
{
    bool  power    = true;
    int   mode     = 0;      // 0 CLEAN, 1 BRIGHT, 2 VINTAGE, 3 BABY BOOST
    float midHigh  = 0.5f;   // 0..1
    float high     = 0.5f;   // 0..1
    float trimDb   = 0.0f;   // -18..+18
    float mix      = 1.0f;   // 0..1
    int   osLog2   = 3;      // 1=2x 2=4x 3=8x 4=16x (0 = 1x, test only)
};

class ExciterEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setParams (const ExciterParams& p);
    int  getLatencySamples() const noexcept { return latency_; }
    // In-place processing of up to 2 channels.
    void process (float* const* io, int numChannels, int numSamples) noexcept;

private:
    void configureOversampling (int log2);

    static constexpr int kDelayMask = 255;
    double fs_ = 48000.0;
    int nCh_ = 2, latency_ = 0, osLog2_ = 3;
    ExciterParams p_;
    ModeVoice voice_[2];
    int active_ = 0, incoming_ = 1;
    bool fading_ = false;
    float fadePos_ = 0.0f, fadeStep_ = 0.001f;
    int targetMode_ = 0;
    float dryBuf_[kMaxChannels][kDelayMask + 1] {};
    int wr_ = 0;
    Smoother smMid_, smHigh_, smMix_, smTrim_, smPower_;
    bool wasBypassed_ = false;
};
} // namespace babyair
