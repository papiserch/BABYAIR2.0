#include "ModeProcessor.h"

namespace babyair
{
using T = AdaaShaper::Type;

// ---------------------------------------------------------------------------------------------
// Mode tables. Each mode has a genuinely different engine configuration:
//   CLEAN      low drive, tanh with tiny bias (soft 2nd/3rd), almost no rectifier, light leveling
//   BRIGHT     pre-emphasis, odd-dominant, stronger sibilance control, moderate rectifier
//   VINTAGE    asymmetric Vintage curve (even-rich), stronger leveling (soft compression), LP on harmonics
//   BABY BOOST hybrid: asym-tanh + odd tanh + rectifier "air" in parallel, heavy pre-emphasis,
//              strong program-dependent leveling, maximum sibilance protection
// levelMax values were calibrated with Tests/dsp_tests.cpp --calibrate (tone-THD safety cap, see docs/CALIBRATION.md)
// ---------------------------------------------------------------------------------------------
static ModeConfig kModes[(int) Mode::Count] =
{
    { "CLEAN",
      { //            inHp  inLp   outHp  outLp  emphDb emphHz dMin  dMax  rho   level   curve1    bias   w1    w2    w3    shelfDb shelfHz
        BandConfig { 2000, 7500,  3500,  0,     0.0f,  3500,  0.8f, 6.0f, 0.45f, 0.87f, T::Tanh,    0.10f, 1.0f, 0.25f, 0.10f, 10.0f, 5000 },
        BandConfig { 7000, 18000, 9000,  0,     0.0f,  9000,  0.8f, 6.0f, 0.45f, 0.87f, T::Tanh,    0.10f, 1.0f, 0.25f, 0.10f, 10.0f, 12000 } },
      0.50f },
    { "BRIGHT",
      {
        BandConfig { 2000, 7500,  3200,  0,     5.0f,  3500,  1.0f, 4.5f, 0.60f, 1.37f, T::Tanh,    0.04f, 0.7f, 0.70f, 0.12f, 6.0f, 6000 },
        BandConfig { 7000, 18000, 8500,  0,     6.0f,  9000,  1.0f, 5.0f, 0.60f, 1.37f, T::Tanh,    0.04f, 0.7f, 0.70f, 0.12f, 4.0f, 12000 } },
      0.80f },
    { "VINTAGE",
      {
        BandConfig { 2000, 7500,  2400,  11000, 0.0f,  3000,  0.9f, 3.8f, 0.30f, 3.15f, T::Vintage, 0.00f, 1.0f, 0.00f, 0.18f, 0.0f, 6000 },
        BandConfig { 7000, 18000, 7500,  14000, 0.0f,  9000,  0.9f, 4.2f, 0.30f, 3.15f, T::Vintage, 0.00f, 1.0f, 0.00f, 0.18f, 0.0f, 12000 } },
      0.60f },
    { "BABY BOOST",
      {
        BandConfig { 2000, 7500,  3000,  0,     8.0f,  3000,  1.5f, 6.0f, 0.75f, 1.15f, T::Tanh,    0.10f, 0.7f, 0.60f, 0.45f, 8.0f, 6000 },
        BandConfig { 7000, 18000, 8500,  0,     9.0f,  8000,  1.5f, 7.0f, 0.80f, 1.15f, T::Tanh,    0.10f, 0.6f, 0.60f, 0.55f, 6.0f, 12000 } },
      1.00f }
};

const ModeConfig& getModeConfig (int mode) noexcept
{
    return kModes[std::clamp (mode, 0, (int) Mode::Count - 1)];
}

ModeConfig& modeConfigForCalibration (int mode) noexcept
{
    return kModes[std::clamp (mode, 0, (int) Mode::Count - 1)];
}

// ---------------------------------------------------------------------------------------------
// BandProcessor
// ---------------------------------------------------------------------------------------------
static constexpr float kRefLevel = 0.08f;   // target band RMS the leveler steers towards (~ -22 dBFS)
static constexpr float kGMin = 0.05f, kGMax = 30.0f;

void BandProcessor::prepare (double fs, int log2Os, const BandConfig& cfg)
{
    cfg_ = cfg;
    DescribingFunction::init();
    const bool useAdaa = log2Os <= 2; // ADAA for 1x/2x/4x; 8x/16x rely on oversampling alone (see README)
    os_.prepare (fs, log2Os);
    hp_.setHighpass (fs, cfg.inHpHz);
    lp_.setLowpass (fs, cfg.inLpHz);
    outHp_.setHighpass (fs, cfg.outHpHz);
    outLpOn_ = cfg.outLpHz > 0.0f;
    if (outLpOn_) outLp_.setLowpass (fs, cfg.outLpHz);
    oShelfOn_ = std::abs (cfg.oShelfDb) > 0.01f;
    if (oShelfOn_) oShelf_.setCoeffs (biquad::highShelf (fs, cfg.oShelfHz, cfg.oShelfDb));
    emphOn_ = std::abs (cfg.emphDb) > 0.01f;
    if (emphOn_) emph_.setCoeffs (biquad::highShelf (fs, cfg.emphHz, cfg.emphDb));
    env_.prepare (fs, 8.0, 8.0);   // symmetric => exact RMS of a sine at any frequency (needed for the fundamental correction)
    sh1_.setup (cfg.curve1, cfg.bias1, 0.3f, useAdaa);
    sh2_.setup (T::Tanh, 0.0f, 0.3f, useAdaa);
    sh3_.setup (T::Rectifier, 0.0f, 0.35f, useAdaa);
    gLen_ = std::min (os_.latencySamples() + 1, kMaxGDelay);
    logRef_ = std::log (kRefLevel);
    reset();
}

void BandProcessor::reset() noexcept
{
    hp_.reset(); lp_.reset(); outHp_.reset(); outLp_.reset(); emph_.reset(); oShelf_.reset(); dc_.reset();
    os_.reset();
    sh1_.reset(); sh2_.reset(); sh3_.reset();
    env_.reset (kRefLevel);
    std::fill (std::begin (gDelay_), std::end (gDelay_), 1.0f);
    gPos_ = 0;
    idle_ = true;
}

float BandProcessor::process (float x, float amt, float sibGain) noexcept
{
    if (amt < 1.0e-5f) { idle_ = true; return 0.0f; }        // 0% = no excitation at all
    if (idle_) { reset(); idle_ = false; }

    float b = lp_.process (hp_.process (x));
    if (emphOn_) b = emph_.process (b);

    const float e = env_.process (b) + 1.0e-6f;
    const float g = std::clamp (std::exp (cfg_.rho * (logRef_ - std::log (e))), kGMin, kGMax);

    // Delay line of g, aligned with the oversampler latency, to undo the leveler exactly in time.
    const float gD = gDelay_[gPos_];
    gDelay_[gPos_] = g;
    if (++gPos_ >= gLen_) gPos_ = 0;

    const float d1 = cfg_.driveMin + (cfg_.driveMax - cfg_.driveMin) * amt;
    const float d2 = d1 * 1.6f, d3 = d1 * 0.8f;
    const float i1 = 1.0f / d1, i2 = 1.0f / d2, i3 = 1.0f / d3;

    // exact describing-function gain of tanh at the expected peak level => removes the FUNDAMENTAL from the
    // residual, so what is added is harmonic/intermodulation content only (not a hidden EQ boost or cut)
    const float A = 1.4142f * e * g;
    const float k1 = DescribingFunction::tanhGain (d1 * A), k2 = DescribingFunction::tanhGain (d2 * A);
    const float w1 = cfg_.w1, w2 = cfg_.w2, w3 = cfg_.w3;

    float y = os_.process (b * g, [&] (float* buf, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float u = buf[i];
            const float s1 = sh1_.process (d1 * u) * i1;
            const float s2 = sh2_.process (d2 * u) * i2;
            const float s3 = sh3_.process (d3 * u) * i3;
            buf[i] = w1 * (s1 - k1 * u) + w2 * (s2 - k2 * u) + w3 * s3;
        }
    });

    const float level = cfg_.levelMax * amt * std::sqrt (std::sqrt (amt)); // amt^1.25
    y *= level * sibGain / gD;
    y = outHp_.process (y);
    if (oShelfOn_) y = oShelf_.process (y);
    if (outLpOn_) y = outLp_.process (y);
    y = dc_.process (y);
    return sanitize (y);
}

// ---------------------------------------------------------------------------------------------
// ModeVoice
// ---------------------------------------------------------------------------------------------
void ModeVoice::prepare (double sampleRate, int log2Os)
{
    fs_ = sampleRate;
    log2Os_ = log2Os;
    for (int c = 0; c < kMaxChannels; ++c)
    {
        sibHp_[c].setHighpass (fs_, 5500.0);
        sibLp_[c].setLowpass (fs_, 9000.0);
    }
    sibEnv_.prepare (fs_, 0.3, 30.0);
    fullEnv_.prepare (fs_, 10.0, 80.0);
    sibAtk_ = 1.0f - std::exp (-1.0f / (float) (0.001 * 1.0 * fs_));    // 1 ms attack
    sibRel_ = 1.0f - std::exp (-1.0f / (float) (0.001 * 40.0 * fs_));   // 40 ms release
    setMode (mode_);
}

void ModeVoice::setMode (int mode)
{
    mode_ = std::clamp (mode, 0, (int) Mode::Count - 1);
    const ModeConfig& m = getModeConfig (mode_);
    for (int b = 0; b < 2; ++b)
        for (int c = 0; c < kMaxChannels; ++c)
            band_[b][c].prepare (fs_, log2Os_, m.band[b]);
    reset();
}

void ModeVoice::reset() noexcept
{
    for (int b = 0; b < 2; ++b)
        for (int c = 0; c < kMaxChannels; ++c)
            band_[b][c].reset();
    for (int c = 0; c < kMaxChannels; ++c) { sibHp_[c].reset(); sibLp_[c].reset(); }
    sibEnv_.reset(); fullEnv_.reset();
    sibGain_ = 1.0f;
}

void ModeVoice::process (const float* in, int nCh, float amtMid, float amtHigh, float* out) noexcept
{
    // --- stereo-linked sibilance detector: ratio of 5.5-9 kHz energy to broadband energy ---
    float pSib = 0.0f, pFull = 0.0f;
    for (int c = 0; c < nCh; ++c)
    {
        const float s = sibLp_[c].process (sibHp_[c].process (in[c]));
        pSib += s * s;
        pFull += in[c] * in[c];
    }
    const float eSib = sibEnv_.processPower (pSib / (float) nCh);
    const float eFull = fullEnv_.processPower (pFull / (float) nCh);
    const float ratio = eSib / (eFull + 1.0e-5f);
    const float amount = getModeConfig (mode_).sibAmount;
    const float over = std::max (0.0f, ratio - 0.35f);
    const float target = 1.0f / (1.0f + amount * 8.0f * over);
    sibGain_ += (target < sibGain_ ? sibAtk_ : sibRel_) * (target - sibGain_);

    for (int c = 0; c < nCh; ++c)
        out[c] = band_[0][c].process (in[c], amtMid, sibGain_) + band_[1][c].process (in[c], amtHigh, sibGain_);
}
} // namespace babyair
