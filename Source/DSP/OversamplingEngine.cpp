#include "OversamplingEngine.h"

namespace babyair
{
static double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 60; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1.0e-14 * sum) break;
    }
    return sum;
}

void HalfbandStage::design (int m)
{
    m_ = std::clamp (m, 1, 19);
    H_ = 2 * m_ + 2;
    const int len = 4 * m_ + 3, c = 2 * m_ + 1;
    const double beta = 8.0, i0b = besselI0 (beta);
    double sum = 0.0;
    double h[kMaxHalf] = {};
    for (int j = 0; j < H_; ++j)
    {
        const int k = 2 * j, dist = k - c; // odd distance
        const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - std::pow (2.0 * k / (len - 1) - 1.0, 2.0)))) / i0b;
        h[j] = std::sin (kPi * dist / 2.0) / (kPi * dist) * w;
        sum += h[j];
    }
    const double norm = 0.5 / sum; // even-phase taps sum to exactly 0.5 (centre tap is exactly 0.5)
    for (int j = 0; j < H_; ++j) e_[j] = (float) (h[j] * norm);
    reset();
}

void HalfbandStage::reset() noexcept
{
    std::fill (std::begin (upBuf_), std::end (upBuf_), 0.0f);
    std::fill (std::begin (dnA_), std::end (dnA_), 0.0f);
    std::fill (std::begin (dnB_), std::end (dnB_), 0.0f);
    upPos_ = dnPos_ = 0;
}

void Oversampler::prepare (double sampleRate, int log2Factor)
{
    stages_ = std::clamp (log2Factor, 0, kMaxStages);
    // Half-length m per stage, chosen from the base sample rate. Later stages run at very high rates
    // where the transition band is wide, but they still need enough taps to reject the images that the
    // up-sampler creates (measured: Tests/dsp_tests.cpp "Oversampler" section).
    int ms[kMaxStages];
    if (sampleRate < 60000.0)       { ms[0] = 19; ms[1] = 5; ms[2] = 4; ms[3] = 3; }
    else if (sampleRate < 130000.0) { ms[0] = 9;  ms[1] = 4; ms[2] = 3; ms[3] = 3; }
    else                            { ms[0] = 7;  ms[1] = 4; ms[2] = 3; ms[3] = 3; }

    const int N = 1 << stages_;
    for (int s = 0; s < stages_; ++s) stage_[s].design (ms[s]);

    // Group delay bookkeeping. Stage s (1-based) delays by c_s = 2*m_s+1 samples at its own output rate,
    // on both the up and the down path. Every down-stage decimates by picking even samples, so the pulse
    // centre must land on an even index at every decimation. A pad delay (0..N-1 samples at the top rate)
    // inserted between up and down chains is searched to satisfy that at all stages; the resulting total
    // latency is then an exact integer number of base-rate samples (dry path can be aligned exactly).
    long dUp = 0;
    for (int s = 1; s <= stages_; ++s) dUp += (long) (2 * ms[s - 1] + 1) * (N >> s);
    pad_ = 0; latency_ = 0;
    for (int j = 0; j < N; ++j)
    {
        long pos = dUp + j;
        bool ok = true;
        for (int s = stages_; s >= 1; --s)
        {
            pos += 2 * ms[s - 1] + 1;
            if (pos & 1) { ok = false; break; }
            pos /= 2;
        }
        if (ok) { pad_ = j; latency_ = (int) pos; break; }
    }
    reset();
}

void Oversampler::reset() noexcept
{
    for (auto& s : stage_) s.reset();
    std::fill (std::begin (padBuf_), std::end (padBuf_), 0.0f);
    padPos_ = 0;
}
} // namespace babyair
