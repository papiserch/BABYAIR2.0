#include "ExciterEngine.h"

namespace babyair
{
void ExciterEngine::configureOversampling (int log2)
{
    osLog2_ = std::clamp (log2, 0, 4);
    for (auto& v : voice_) v.prepare (fs_, osLog2_);
    latency_ = voice_[0].latency();
    voice_[active_].setMode (targetMode_);
    fading_ = false;
}

void ExciterEngine::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    fs_ = sampleRate;
    nCh_ = std::clamp (numChannels, 1, kMaxChannels);
    targetMode_ = std::clamp (p_.mode, 0, (int) Mode::Count - 1);
    active_ = 0; incoming_ = 1;
    smMid_.prepare (fs_, 20.0); smHigh_.prepare (fs_, 20.0);
    smMix_.prepare (fs_, 20.0); smTrim_.prepare (fs_, 20.0);
    smPower_.prepare (fs_, 15.0);
    fadeStep_ = 1.0f / (float) (0.040 * fs_);   // 40 ms mode crossfade
    configureOversampling (p_.osLog2);
    smMid_.snap (p_.midHigh); smHigh_.snap (p_.high); smMix_.snap (p_.mix);
    smTrim_.snap (dbToLin (p_.trimDb)); smPower_.snap (p_.power ? 1.0f : 0.0f);
    reset();
}

void ExciterEngine::reset()
{
    for (auto& v : voice_) v.reset();
    for (auto& ch : dryBuf_) std::fill (std::begin (ch), std::end (ch), 0.0f);
    wr_ = 0;
    fading_ = false;
}

void ExciterEngine::setParams (const ExciterParams& p)
{
    const bool osChanged = p.osLog2 != p_.osLog2;
    p_ = p;
    smMid_.setTarget (std::clamp (p.midHigh, 0.0f, 1.0f));
    smHigh_.setTarget (std::clamp (p.high, 0.0f, 1.0f));
    smMix_.setTarget (std::clamp (p.mix, 0.0f, 1.0f));
    smTrim_.setTarget (dbToLin (std::clamp (p.trimDb, -18.0f, 18.0f)));
    smPower_.setTarget (p.power ? 1.0f : 0.0f);

    if (osChanged)
    {
        targetMode_ = p.mode;
        configureOversampling (p.osLog2);   // resets state; host latency must be re-reported
        return;
    }
    if (p.mode != targetMode_)
    {
        if (fading_) { active_ = incoming_; }          // finish the previous fade instantly, then start a new one
        incoming_ = 1 - active_;
        voice_[incoming_].setMode (p.mode);
        targetMode_ = p.mode;
        fadePos_ = 0.0f;
        fading_ = true;
    }
}

void ExciterEngine::process (float* const* io, int numChannels, int numSamples) noexcept
{
    const int nCh = std::min (numChannels, nCh_);
    for (int i = 0; i < numSamples; ++i)
    {
        const float amtM = smMid_.next(), amtH = smHigh_.next(), mix = smMix_.next();
        const float trim = smTrim_.next(), pw = smPower_.next();

        float in[kMaxChannels] = {}, dry[kMaxChannels] = {}, h[kMaxChannels] = {}, h2[kMaxChannels] = {};
        for (int c = 0; c < nCh; ++c)
        {
            in[c] = sanitize (io[c][i]);
            dryBuf_[c][wr_] = in[c];
            dry[c] = dryBuf_[c][(wr_ - latency_) & kDelayMask];   // latency-aligned dry path
        }
        wr_ = (wr_ + 1) & kDelayMask;

        const bool bypassed = (pw <= 1.0e-5f) && ! p_.power;
        if (bypassed)
        {
            wasBypassed_ = true;
            for (int c = 0; c < nCh; ++c) io[c][i] = dry[c];   // true bypass (delayed to stay in sync with host PDC)
            continue;
        }
        if (wasBypassed_) { for (auto& v : voice_) v.reset(); wasBypassed_ = false; }

        voice_[active_].process (in, nCh, amtM, amtH, h);
        if (fading_)
        {
            voice_[incoming_].process (in, nCh, amtM, amtH, h2);
            fadePos_ += fadeStep_;
            const float x = std::min (fadePos_, 1.0f);
            const float s = x * x * (3.0f - 2.0f * x);          // smoothstep
            for (int c = 0; c < nCh; ++c) h[c] += s * (h2[c] - h[c]);
            if (fadePos_ >= 1.0f) { active_ = incoming_; fading_ = false; }
        }

        for (int c = 0; c < nCh; ++c)
        {
            const float wet = (dry[c] + mix * h[c]) * trim;
            io[c][i] = safetyBound (sanitize (dry[c] + pw * (wet - dry[c])));
        }
    }
}
} // namespace babyair
