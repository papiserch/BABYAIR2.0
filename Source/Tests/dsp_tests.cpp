// BABY AIR - DSP validation suite. Standalone (no JUCE). Build: see README / Tests/CMakeLists.txt
//   ./baby_air_dsp_tests            run all tests
//   ./baby_air_dsp_tests --calibrate  bisect levelMax per mode to hit the target "excitation" (see docs/CALIBRATION.md)
#include "../DSP/ExciterEngine.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace babyair;
using cd = std::complex<double>;

static int g_fail = 0;
#define CHECK(cond, ...)                                                    \
    do { if (! (cond)) { ++g_fail; std::printf ("  [FAIL] "); std::printf (__VA_ARGS__); std::printf ("\n"); } \
         else { std::printf ("  [ ok ] "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

// ----------------------------------------------------------------------------- utilities
static void fft (std::vector<cd>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPi / (double) len;
        const cd wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            cd w (1);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const cd u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
            }
        }
    }
}

struct Stereo { std::vector<float> l, r; };

static Stereo pinkNoise (size_t n, float rmsDb, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (auto* ch : { &s.l, &s.r })
    {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0; double sq = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const float w = u (rng);   // Paul Kellet pink filter
            b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
            const float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f; b6 = w * 0.115926f;
            (*ch)[i] = p; sq += (double) p * p;
        }
        const float g = dbToLin (rmsDb) / (float) std::sqrt (sq / (double) n);
        for (auto& v : *ch) v *= g;
    }
    return s;
}

static Stereo sine (size_t n, double fs, double f, float db)
{
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    const float a = dbToLin (db);
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = a * (float) std::sin (2.0 * kPi * f * (double) i / fs);
    return s;
}

static Stereo runEngine (const ExciterParams& p, const Stereo& in, double fs, int block = 512, int* latencyOut = nullptr, int nCh = 2)
{
    ExciterEngine e;
    e.setParams (p);
    e.prepare (fs, block, nCh);
    if (latencyOut) *latencyOut = e.getLatencySamples();
    Stereo out = in;
    const size_t n = in.l.size();
    for (size_t pos = 0; pos < n; pos += (size_t) block)
    {
        const int len = (int) std::min ((size_t) block, n - pos);
        float* ch[2] = { out.l.data() + pos, out.r.data() + pos };
        e.process (ch, nCh, len);
    }
    return out;
}

// Mean band power (Welch, Hann, 4096) in [f1,f2] (dB, arbitrary reference), skipping the first `skip` samples
static double bandPowerDb (const std::vector<float>& x, double fs, double f1, double f2, size_t skip)
{
    const size_t N = 4096, hop = 2048;
    double acc = 0; int frames = 0;
    for (size_t p = skip; p + N <= x.size(); p += hop, ++frames)
    {
        std::vector<cd> a (N);
        for (size_t i = 0; i < N; ++i) a[i] = (double) x[p + i] * (0.5 - 0.5 * std::cos (2 * kPi * (double) i / (double) N));
        fft (a);
        for (size_t k = 0; k < N / 2; ++k)
        {
            const double f = (double) k * fs / (double) N;
            if (f >= f1 && f <= f2) acc += std::norm (a[k]);
        }
    }
    return 10.0 * std::log10 (std::max (acc / std::max (frames, 1), 1e-30));
}

// coherent single-bin helper: choose f so that it falls exactly on a bin of an N-point FFT
static double coherentFreq (double fs, size_t N, double f) { return std::round (f / (fs / (double) N)) * fs / (double) N; }

static std::vector<double> spectrumMag (const std::vector<float>& x, size_t start, size_t N)
{
    std::vector<cd> a (N);
    for (size_t i = 0; i < N; ++i) a[i] = (double) x[start + i];
    fft (a);
    std::vector<double> m (N / 2);
    for (size_t k = 0; k < N / 2; ++k) m[k] = std::abs (a[k]) * 2.0 / (double) N;
    return m;
}

static double excitationDb (int mode, double fs, int osLog2, float inDb = -18.0f, double f1 = 5000, double f2 = 16000)
{
    const size_t n = (size_t) (fs * 4);
    const auto in = pinkNoise (n, inDb, 1234);
    ExciterParams p; p.mode = mode; p.midHigh = 1.0f; p.high = 1.0f; p.osLog2 = osLog2;
    const auto out = runEngine (p, in, fs);
    const size_t skip = (size_t) fs;
    return bandPowerDb (out.l, fs, f1, f2, skip) - bandPowerDb (in.l, fs, f1, f2, skip);
}


// THD (%) of a 3 kHz / -20 dBFS tone at 100%/100% (musical-safety metric used by the calibration)
static double toneThdPct (int mode)
{
    const double fs = 48000; const size_t N = 32768, warm = 48000;
    const double f = coherentFreq (fs, N, 3000.0);
    const auto in = sine (warm + N, fs, f, -20.0f);
    ExciterParams p; p.mode = mode; p.midHigh = 1; p.high = 1;
    const auto m = spectrumMag (runEngine (p, in, fs).l, warm, N);
    const size_t k0 = (size_t) std::llround (f / (fs / (double) N));
    double all = 0;
    for (size_t k = 0; k < m.size(); ++k) if (std::abs ((long) k - (long) k0) > 2 && k > 2) all += m[k] * m[k];
    return 100.0 * std::sqrt (all) / m[k0];
}

// Voice-like test signal: harmonic series on 190 Hz with a -10 dB/oct tilt above 500 Hz and slow vibrato, -20 dBFS RMS
static Stereo voiceLike (double fs, size_t n)
{
    Stereo s { std::vector<float> (n, 0.0f), std::vector<float> (n, 0.0f) };
    std::mt19937 rng (21); std::uniform_real_distribution<double> u (0, 2 * kPi);
    double ph[80]; for (auto& p : ph) p = u (rng);
    double acc = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / fs, f0 = 190.0 * (1.0 + 0.012 * std::sin (2 * kPi * 5.2 * t));
        double v = 0;
        for (int h = 1; h <= 80 && h * f0 < 0.45 * fs; ++h)
        {
            const double fr = h * f0, a = fr < 500 ? 1.0 : std::pow (fr / 500.0, -10.0 / 6.02);
            v += a * std::sin (ph[h - 1] + 2 * kPi * fr * t);
        }
        s.l[i] = (float) v; acc += v * v;
    }
    const float g = dbToLin (-20.0f) / (float) std::sqrt (acc / (double) n);
    for (size_t i = 0; i < n; ++i) { s.l[i] *= g; s.r[i] = s.l[i]; }
    return s;
}

// ----------------------------------------------------------------------------- tests
static void testOversampler()
{
    std::printf ("\n== Oversampler: latency + transparency (identity shaper) ==\n");
    const double rates[] = { 44100, 48000, 96000, 192000 };
    for (double fs : rates)
        for (int lg = 0; lg <= 4; ++lg)
        {
            Oversampler os; os.prepare (fs, lg);
            const int L = os.latencySamples();
            const size_t n = 16384;
            std::vector<float> x (n), y (n);
            const double fr[3] = { 500.0, 5000.0, std::min (15000.0, 0.34 * fs) };
            for (size_t i = 0; i < n; ++i)
            {
                double v = 0; for (double f : fr) v += 0.25 * std::sin (2 * kPi * f * (double) i / fs);
                x[i] = (float) v;
                y[i] = os.process (x[i], [] (float*, int) {});
            }
            double e = 0, s = 0;
            for (size_t i = 2000; i < n; ++i) { const double d = (double) y[i] - (double) x[i - (size_t) L]; e += d * d; s += (double) x[i] * x[i]; }
            const double errDb = 10 * std::log10 (e / s + 1e-30);
            std::printf ("  fs=%6.0f  %2dx  latency=%3d samples (%.2f ms)  error=%.1f dB\n", fs, 1 << lg, L, 1000.0 * L / fs, errDb);
            if (errDb > -60.0) { ++g_fail; std::printf ("  [FAIL] identity error too high\n"); }
        }
}

static double aliasDb (int lg, bool adaa, double fs)
{
    const size_t N = 32768, warm = 8192;
    const double f0 = coherentFreq (fs, N, 9000.0);
    Oversampler os; os.prepare (fs, lg);
    AdaaShaper sh; sh.setup (AdaaShaper::Type::Tanh, 0.0f, 0.3f, adaa);
    const float drive = 8.0f;
    std::vector<float> y (N + warm);
    for (size_t i = 0; i < N + warm; ++i)
    {
        const float x = 0.9f * (float) std::sin (2 * kPi * f0 * (double) i / fs);
        y[i] = os.process (x, [&] (float* b, int n) { for (int k = 0; k < n; ++k) b[k] = sh.process (drive * b[k]); });
    }
    const auto m = spectrumMag (y, warm, N);
    const size_t k0 = (size_t) std::llround (f0 / (fs / (double) N));
    double harm = m[k0] * m[k0], non = 0;
    for (size_t k = 1; k < m.size(); ++k) if (k % k0 != 0 && k != 0) non += m[k] * m[k];
    return 10 * std::log10 (non / harm + 1e-30);
}

static void testAliasing()
{
    std::printf ("\n== Aliasing (9 kHz sine through tanh drive x8 @44.1k; non-harmonic energy re fundamental) ==\n");
    const double fs = 44100;
    const double a1 = aliasDb (0, false, fs), a1d = aliasDb (0, true, fs), a2d = aliasDb (1, true, fs),
                 a4d = aliasDb (2, true, fs), a8 = aliasDb (3, false, fs), a16 = aliasDb (4, false, fs);
    std::printf ("  1x naive      : %7.1f dB\n  1x ADAA       : %7.1f dB\n  2x ADAA       : %7.1f dB\n  4x ADAA       : %7.1f dB\n  8x naive      : %7.1f dB\n  16x naive     : %7.1f dB\n", a1, a1d, a2d, a4d, a8, a16);
    CHECK (a1d < a1 - 6.0, "ADAA reduces aliasing vs naive at 1x (%.1f dB better)", a1 - a1d);
    CHECK (a4d < a2d, "4x ADAA cleaner than 2x ADAA");
    CHECK (a8 < a1 - 25.0, "8x oversampling >25 dB cleaner than no oversampling (%.1f dB)", a1 - a8);
    CHECK (a16 <= a8 + 3.0, "16x not worse than 8x");
}

static void testBypassAndLatency()
{
    std::printf ("\n== Dry path alignment / bypass / zero-amount ==\n");
    const double fs = 48000;
    const auto in = pinkNoise (48000, -12.0f, 7);
    for (int lg : { 1, 2, 3, 4 })
    {
        ExciterParams p; p.midHigh = 0; p.high = 0; p.osLog2 = lg;
        int L = 0; const auto out = runEngine (p, in, fs, 256, &L);
        double maxd = 0; for (size_t i = (size_t) L; i < in.l.size(); ++i) maxd = std::max (maxd, (double) std::abs (out.l[i] - in.l[i - (size_t) L]));
        CHECK (maxd < 1e-6, "amount=0%% is the latency-aligned dry signal, %2dx, L=%d, max err=%.2e", 1 << lg, L, maxd);
    }
    ExciterParams p; p.mix = 0.0f; int L = 0;
    auto out = runEngine (p, in, fs, 100, &L);
    double maxd = 0; for (size_t i = (size_t) L + 100; i < in.l.size(); ++i) maxd = std::max (maxd, (double) std::abs (out.l[i] - in.l[i - (size_t) L]));
    CHECK (maxd < 1e-6, "MIX=0%% == dry (aligned), max err=%.2e", maxd);
    p = ExciterParams(); p.power = false;
    out = runEngine (p, in, fs, 100, &L);
    maxd = 0; for (size_t i = (size_t) L + 100; i < in.l.size(); ++i) maxd = std::max (maxd, (double) std::abs (out.l[i] - in.l[i - (size_t) L]));
    CHECK (maxd < 1e-6, "Power OFF == true bypass (delayed by PDC), max err=%.2e", maxd);
}

static void testLowEndUntouched()
{
    std::printf ("\n== Frequency response of the fundamental (sine -24 dBFS, 100%%/100%%, gain in dB) ==\n");
    const double fs = 48000; const size_t N = 16384, warm = 48000;
    const double freqs[] = { 100, 500, 1000, 2000, 3000, 5000, 8000, 12000, 16000 };
    std::printf ("  %-11s", "mode\\Hz"); for (double f : freqs) std::printf ("%7.0f", f); std::printf ("\n");
    for (int mode = 0; mode < 4; ++mode)
    {
        std::printf ("  %-11s", getModeConfig (mode).name);
        double lowDev = 0;
        for (double f0 : freqs)
        {
            const double f = coherentFreq (fs, N, f0);
            const auto in = sine (warm + N, fs, f, -24.0f);
            ExciterParams p; p.mode = mode; p.midHigh = 1; p.high = 1;
            int L; const auto out = runEngine (p, in, fs, 512, &L);
            const size_t k = (size_t) std::llround (f / (fs / (double) N));
            const double g = 20 * std::log10 (spectrumMag (out.l, warm, N)[k] / spectrumMag (in.l, warm, N)[k]);
            std::printf ("%7.2f", g);
            if (f0 <= 1000) lowDev = std::max (lowDev, std::abs (g));
        }
        std::printf ("\n");
        CHECK (lowDev < 0.05, "%s: <=1 kHz untouched (max dev %.3f dB)", getModeConfig (mode).name, lowDev);
    }
}

static void testModesDiffer()
{
    std::printf ("\n== Harmonic spectra, 3 kHz sine -20 dBFS, 100%%/100%% (dB re output fundamental) ==\n");
    const double fs = 48000; const size_t N = 32768, warm = 48000;
    const double f = coherentFreq (fs, N, 3000.0);
    const auto in = sine (warm + N, fs, f, -20.0f);
    double H[4][6], thd[4];
    std::printf ("  %-11s %7s %7s %7s %7s %7s | THD    even/odd\n", "mode", "H2", "H3", "H4", "H5", "H6");
    for (int mode = 0; mode < 4; ++mode)
    {
        ExciterParams p; p.mode = mode; p.midHigh = 1; p.high = 1;
        const auto out = runEngine (p, in, fs);
        const auto m = spectrumMag (out.l, warm, N);
        const size_t k0 = (size_t) std::llround (f / (fs / (double) N));
        double ev = 0, od = 0;
        for (int h = 2; h <= 6; ++h)
        {
            const size_t k = k0 * (size_t) h;
            H[mode][h - 1] = k < m.size() ? 20 * std::log10 (m[k] / m[k0] + 1e-12) : -200;
            const double pw = k < m.size() ? (m[k] / m[k0]) * (m[k] / m[k0]) : 0;
            (h % 2 ? od : ev) += pw;
        }
        double all = 0; for (size_t k = k0 + 2; k < m.size(); ++k) if (std::abs ((long) k - (long) k0) > 2) all += m[k] * m[k];
        thd[mode] = 100.0 * std::sqrt (all) / m[k0];
        std::printf ("  %-11s %7.1f %7.1f %7.1f %7.1f %7.1f | %5.2f%%  %5.1f dB\n", getModeConfig (mode).name, H[mode][1], H[mode][2], H[mode][3], H[mode][4], H[mode][5], thd[mode], 10 * std::log10 ((ev + 1e-20) / (od + 1e-20)));
    }
    int diffPairs = 0, pairs = 0;
    for (int a = 0; a < 4; ++a) for (int b = a + 1; b < 4; ++b)
    {
        ++pairs; double d = 0; for (int h = 1; h < 6; ++h) d = std::max (d, std::abs (H[a][h] - H[b][h]));
        if (d > 2.0) ++diffPairs;
    }
    CHECK (diffPairs == pairs, "all %d mode pairs differ by >2 dB in at least one harmonic (%d/%d)", pairs, diffPairs, pairs);
    CHECK (thd[3] > thd[0], "BABY BOOST THD (%.2f%%) > CLEAN THD (%.2f%%)", thd[3], thd[0]);
}

static void testExcitation()
{
    std::printf ("\n== Excitation, 100%%/100%%, 8x, 48k. Metric = change of 5-16 kHz band energy ==\n");
    const double spec[4]   = { 10, 20, 14, 18 };      // spec "maximum" targets (pink noise)
    const double thdCap[4] = { 10, 14, 22, 40 };       // musical-safety caps (3 kHz tone, %)
    std::printf ("  %-11s %10s %10s %10s %12s\n", "mode", "pink -18", "voice -20", "spec tgt", "tone THD");
    const auto voice = voiceLike (48000, 48000 * 4);
    for (int mode = 0; mode < 4; ++mode)
    {
        const double d = excitationDb (mode, 48000, 3);
        ExciterParams p; p.mode = mode; p.midHigh = 1; p.high = 1;
        const auto vo = runEngine (p, voice, 48000);
        const double dv = bandPowerDb (vo.l, 48000, 5000, 16000, 48000) - bandPowerDb (voice.l, 48000, 5000, 16000, 48000);
        const double thd = toneThdPct (mode);
        std::printf ("  %-11s %+8.1f dB %+8.1f dB %+8.0f dB %9.1f %%\n", getModeConfig (mode).name, d, dv, spec[mode], thd);
        const double air = bandPowerDb (vo.l, 48000, 8000, 16000, 48000) - bandPowerDb (voice.l, 48000, 8000, 16000, 48000);
        std::printf ("  %-11s   air band 8-16 kHz on voice-like signal: %+.1f dB\n", "", air);
        CHECK (air > 0.3 && d <= spec[mode] + 1.0, "%s: measurable air excitation (%+.1f dB) and never above the spec ceiling", getModeConfig (mode).name, air);
        CHECK (thd <= thdCap[mode] * 1.05, "%s: tone THD %.1f%% within safety cap %.0f%%", getModeConfig (mode).name, thd, thdCap[mode]);
    }
    std::printf ("  Amount law (BABY BOOST, pink, 5-16 kHz): ");
    for (float a : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const double fs = 48000; const auto in = pinkNoise (192000, -18.0f, 1234);
        ExciterParams p; p.mode = 3; p.midHigh = a; p.high = a;
        const auto out = runEngine (p, in, fs);
        std::printf ("%3.0f%%:%+.1f  ", a * 100, bandPowerDb (out.l, fs, 5000, 16000, 48000) - bandPowerDb (in.l, fs, 5000, 16000, 48000));
    }
    std::printf ("dB\n  Level dependence (BABY BOOST): ");
    for (float lvl : { -40.0f, -30.0f, -18.0f, -9.0f }) std::printf ("%4.0f dBFS:%+.1f  ", lvl, excitationDb (3, 48000, 3, lvl));
    std::printf ("dB\n");
}

static void testSampleRatesAndRobustness()
{
    std::printf ("\n== Sample rates, extreme input, mono, block sizes ==\n");
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        const auto in = pinkNoise ((size_t) (fs * 2), -18.0f, 99);
        ExciterParams p; p.mode = 3; p.midHigh = 1; p.high = 1;
        int L; const auto out = runEngine (p, in, fs, 512, &L);
        bool fin = true; float pk = 0; for (float v : out.l) { fin &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
        std::printf ("  fs=%6.0f  latency=%3d (%.2f ms)  excitation(5-16k)=+%4.1f dB  peak=%.3f\n", fs, L, 1000.0 * L / fs, excitationDb (3, fs, 3), pk);
        CHECK (fin && pk < 2.0f, "finite and bounded at %.0f Hz", fs);
    }
    {   // full-scale white noise + clipped square + NaN/Inf injection
        const double fs = 48000; std::mt19937 rng (5); std::uniform_real_distribution<float> u (-1, 1);
        Stereo in { std::vector<float> (96000), std::vector<float> (96000) };
        for (size_t i = 0; i < in.l.size(); ++i) { in.l[i] = u (rng); in.r[i] = ((i / 40) & 1) ? 1.0f : -1.0f; }
        in.l[5000] = NAN; in.r[6000] = INFINITY;
        for (int mode = 0; mode < 4; ++mode)
        {
            ExciterParams p; p.mode = mode; p.midHigh = 1; p.high = 1; p.trimDb = 18;
            const auto out = runEngine (p, in, fs);
            bool fin = true; float pk = 0; for (float v : out.l) { fin &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
            for (float v : out.r) fin &= std::isfinite (v);
            CHECK (fin && pk <= 16.0f, "%s: finite & bounded with 0 dBFS noise/square, NaN/Inf and +18 dB trim (peak %.2f)", getModeConfig (mode).name, pk);
        }
    }
    {   // block-size independence
        const auto in = pinkNoise (48000, -14.0f, 3);
        ExciterParams p; p.mode = 3; p.midHigh = 0.7f; p.high = 0.4f;
        const auto a = runEngine (p, in, 48000, 1), b = runEngine (p, in, 48000, 64), c = runEngine (p, in, 48000, 513);
        CHECK (a.l == b.l && b.l == c.l && a.r == c.r, "bit-identical output for block sizes 1 / 64 / 513");
    }
    {   // mono
        const auto in = pinkNoise (48000, -18.0f, 3);
        ExciterParams p; p.mode = 3; p.midHigh = 1; p.high = 1;
        const auto out = runEngine (p, in, 48000, 512, nullptr, 1);
        bool fin = true; for (float v : out.l) fin &= std::isfinite (v);
        CHECK (fin, "mono processing is finite");
    }
    {   // mono compatibility: identical L/R -> identical outputs; and L/R = (M,S) -> sum stays sane
        const auto m = pinkNoise (48000, -18.0f, 3);
        Stereo in { m.l, m.l };
        ExciterParams p; p.mode = 3; p.midHigh = 1; p.high = 1;
        const auto out = runEngine (p, in, 48000);
        CHECK (out.l == out.r, "identical L/R inputs give identical L/R outputs (centre stays centred)");
    }
}

static void testTransitions()
{
    std::printf ("\n== Click tests (mode crossfade, power toggle, parameter jumps) ==\n");
    const double fs = 48000; const size_t n = 96000;
    const auto in = sine (n, fs, 4000.0, -12.0f);
    auto maxJump = [] (const std::vector<float>& y, size_t a, size_t b) { double m = 0; for (size_t i = a + 1; i < b; ++i) m = std::max (m, (double) std::abs (y[i] - y[i - 1])); return m; };

    ExciterEngine e; ExciterParams p; p.mode = 0; p.midHigh = 1; p.high = 1; e.setParams (p); e.prepare (fs, 256, 2);
    Stereo out = in;
    for (size_t pos = 0; pos < n; pos += 256)
    {
        if (pos == 24576) { p.mode = 3; e.setParams (p); }
        if (pos == 49152) { p.mode = 2; p.midHigh = 0.1f; p.high = 1.0f; e.setParams (p); }
        if (pos == 73728) { p.power = false; e.setParams (p); }
        float* ch[2] = { out.l.data() + pos, out.r.data() + pos };
        e.process (ch, 2, (int) std::min<size_t> (256, n - pos));
    }
    const double ref = maxJump (out.l, 12000, 24000);
    std::printf ("  steady-state max |dy| = %.4f\n", ref);
    CHECK (maxJump (out.l, 24500, 34000) < ref * 1.6, "CLEAN -> BABY BOOST crossfade is click-free (max |dy| %.4f)", maxJump (out.l, 24500, 34000));
    CHECK (maxJump (out.l, 49500, 59000) < ref * 1.6, "BABY BOOST -> VINTAGE + knob jump is click-free (max |dy| %.4f)", maxJump (out.l, 49500, 59000));
    CHECK (maxJump (out.l, 73700, 80000) < ref * 1.6, "Power OFF transition is click-free (max |dy| %.4f)", maxJump (out.l, 73700, 80000));
}

static void testCpu()
{
    std::printf ("\n== CPU (single core, -O2, stereo, 44.1 kHz, realtime %% = process time / audio time) ==\n");
    const double fs = 44100; const auto in = pinkNoise ((size_t) (fs * 5), -18.0f, 11);
    for (int mode : { 0, 3 })
        for (int lg = 1; lg <= 4; ++lg)
        {
            ExciterParams p; p.mode = mode; p.midHigh = 0.8f; p.high = 0.8f; p.osLog2 = lg;
            const auto t0 = std::chrono::steady_clock::now();
            const auto out = runEngine (p, in, fs);
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-11s %2dx : %5.1f %% of one core  (%s)\n", getModeConfig (mode).name, 1 << lg, 100.0 * sec / 5.0, out.l[1000] == out.l[1000] ? "ok" : "nan");
        }
}

// ----------------------------------------------------------------------------- calibration
static void calibrate()
{
    std::printf ("Calibrating levelMax = min( level reaching the spec target on pink noise, level reaching the tone-THD safety cap )\n");
    const double target[4] = { 10, 20, 14, 18 }, thdCap[4] = { 10, 14, 22, 40 };
    for (int mode = 0; mode < 4; ++mode)
    {
        auto bisect = [&] (auto metric, double goal) {
            double lo = 0.05, hi = 200.0;
            for (int it = 0; it < 16; ++it)
            {
                const double mid = std::sqrt (lo * hi);
                for (auto& b : modeConfigForCalibration (mode).band) b.levelMax = (float) mid;
                (metric() < goal ? lo : hi) = mid;
            }
            return std::sqrt (lo * hi);
        };
        const double lA = bisect ([&] { return excitationDb (mode, 48000, 3); }, target[mode]);
        const double lB = bisect ([&] { return toneThdPct (mode); }, thdCap[mode]);
        const double l = std::min (lA, lB);
        for (auto& b : modeConfigForCalibration (mode).band) b.levelMax = (float) l;
        std::printf ("  %-11s pink-target level=%7.3f | THD-cap level=%6.3f | chosen levelMax=%.2f -> pink %+.1f dB, THD %.1f%%\n",
                     getModeConfig (mode).name, lA, lB, l, excitationDb (mode, 48000, 3), toneThdPct (mode));
    }
}

int main (int argc, char** argv)
{
    if (argc > 1 && ! std::strcmp (argv[1], "--calibrate")) { calibrate(); return 0; }
    testOversampler();
    testAliasing();
    testBypassAndLatency();
    testLowEndUntouched();
    testModesDiffer();
    testExcitation();
    testSampleRatesAndRobustness();
    testTransitions();
    testCpu();
    std::printf ("\n%s (%d failure%s)\n", g_fail ? "SOME TESTS FAILED" : "ALL TESTS PASSED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
