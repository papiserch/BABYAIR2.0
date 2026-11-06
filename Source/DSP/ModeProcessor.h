// BABY AIR - Mode definitions and per-band harmonic generators.
//
// Architecture (per channel, per band):
//
//   x --> band isolation (Butter4 HP+LP) --> [pre-emphasis high-shelf] --> envelope follower
//     --> leveler gain g = (ref/env)^rho  (program-dependent drive, "signal dependent" saturation)
//     --> oversampler { 3 parallel shapers: asym-tanh (even+odd) / odd tanh / smooth rectifier (even/"air") }
//         each path contributes only its NON-LINEAR RESIDUAL  s(d*u)/d - k*u   (k = describing-function gain of the fundamental)
//         so the added signal is new harmonic content, not a linear gain/EQ boost.
//     --> /g (undo leveler, aligned by the oversampler latency) --> * level(amount) * sibilance gain
//     --> output HP (keeps only content above the band floor) --> optional LP --> DC blocker
//
//   wet = dry(delayed by latency) + mix * (harmonics_mid + harmonics_high)
//
// Because the original signal is never filtered, there are no crossover notches/peaks: the low end
// and the band interiors stay bit-identical to the input path at amount = 0.
#pragma once
#include "Biquad.h"
#include "OversamplingEngine.h"
#include "SaturationEngine.h"

namespace babyair
{
enum class Mode : int { Clean = 0, Bright, Vintage, BabyBoost, Count };

struct BandConfig
{
    float inHpHz, inLpHz;          // band isolation (excitation source region)
    float outHpHz, outLpHz;        // harmonic output region (outLp 0 = off)
    float emphDb, emphHz;          // pre-emphasis high shelf (0 dB = off)
    float driveMin, driveMax;      // shaper drive at 0% / 100%
    float rho;                     // leveler exponent (0 = level independent drive.. 1 = fully normalised)
    float levelMax;                // linear gain of added harmonics at 100%
    AdaaShaper::Type curve1;       // main path curve
    float bias1;                   // main path asymmetry
    float w1, w2, w3;              // path weights: main / odd-tanh / rectifier
    float oShelfDb, oShelfHz;      // high-shelf tilt on the generated harmonics (more of the budget goes to 'air')
};

struct ModeConfig
{
    const char* name;
    BandConfig band[2];            // 0 = MID HIGH, 1 = HIGH
    float sibAmount;               // 0..1 sibilance protection strength
};

const ModeConfig& getModeConfig (int mode) noexcept;
// Mutable access: ONLY for the calibration tool in Tests/ (never called by the plugin).
ModeConfig& modeConfigForCalibration (int mode) noexcept;

class BandProcessor
{
public:
    void prepare (double sampleRate, int log2Os, const BandConfig& cfg);
    void reset() noexcept;
    int latency() const noexcept { return os_.latencySamples(); }
    // amt: 0..1 (smoothed by caller). Returns the harmonic signal to ADD to the (latency-aligned) dry signal.
    float process (float x, float amt, float sibGain) noexcept;

private:
    BandConfig cfg_ {};
    Butter4 hp_, lp_, outHp_;
    Butter2 outLp_;
    Biquad emph_, oShelf_;
    bool emphOn_ = false, outLpOn_ = false, oShelfOn_ = false, idle_ = true;
    EnvFollower env_;
    DcBlocker dc_;
    Oversampler os_;
    AdaaShaper sh1_, sh2_, sh3_;
    static constexpr int kMaxGDelay = 128;   // >= max oversampler latency (48); fixed => no allocation on mode/quality change
    float gDelay_[kMaxGDelay] {};
    int gLen_ = 1, gPos_ = 0;
    float logRef_ = 0.0f;
};

class ModeVoice
{
public:
    void prepare (double sampleRate, int log2Os);
    void setMode (int mode);         // (re)configures and resets this voice
    void reset() noexcept;
    int latency() const noexcept { return band_[0][0].latency(); }
    // in/out: one sample per channel
    void process (const float* in, int nCh, float amtMid, float amtHigh, float* out) noexcept;

private:
    double fs_ = 48000.0;
    int log2Os_ = 3, mode_ = 0;
    BandProcessor band_[2][kMaxChannels];
    // sibilance detector (stereo linked)
    Butter2 sibHp_[kMaxChannels], sibLp_[kMaxChannels];
    EnvFollower sibEnv_, fullEnv_;
    float sibGain_ = 1.0f, sibAtk_ = 0.1f, sibRel_ = 0.001f;
};
} // namespace babyair
