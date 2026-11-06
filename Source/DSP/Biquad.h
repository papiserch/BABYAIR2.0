// BABY AIR - Biquad filters (RBJ cookbook), double precision coefficients/state.
#pragma once
#include "DspCommon.h"

namespace babyair
{
struct BiquadCoeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

namespace biquad
{
inline double clampFreq (double fs, double f) { return std::clamp (f, 10.0, 0.45 * fs); }

inline BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}
inline BiquadCoeffs lowpass (double fs, double f0, double q)
{
    const double w = 2.0 * kPi * clampFreq (fs, f0) / fs, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
}
inline BiquadCoeffs highpass (double fs, double f0, double q)
{
    const double w = 2.0 * kPi * clampFreq (fs, f0) / fs, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
}
inline BiquadCoeffs highShelf (double fs, double f0, double gainDb, double q = 0.7071)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPi * clampFreq (fs, f0) / fs;
    const double cs = std::cos (w), al = std::sin (w) / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
    return normalise (A * ((A + 1) + (A - 1) * cs + sA), -2 * A * ((A - 1) + (A + 1) * cs),
                      A * ((A + 1) + (A - 1) * cs - sA), (A + 1) - (A - 1) * cs + sA,
                      2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sA);
}
} // namespace biquad

class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept { c_ = c; }
    void reset() noexcept { z1_ = z2_ = 0.0; }
    inline float process (float x) noexcept
    {
        const double xd = x;
        const double y = c_.b0 * xd + z1_;
        z1_ = c_.b1 * xd - c_.a1 * y + z2_;
        z2_ = c_.b2 * xd - c_.a2 * y;
        if (std::abs (z1_) < 1.0e-30) z1_ = 0.0;
        if (std::abs (z2_) < 1.0e-30) z2_ = 0.0;
        return (float) y;
    }
private:
    BiquadCoeffs c_;
    double z1_ = 0.0, z2_ = 0.0;
};

// 2nd-order Butterworth (12 dB/oct)
struct Butter2
{
    void setLowpass (double fs, double f)  { bq.setCoeffs (biquad::lowpass  (fs, f, 0.70710678)); }
    void setHighpass (double fs, double f) { bq.setCoeffs (biquad::highpass (fs, f, 0.70710678)); }
    void reset() { bq.reset(); }
    inline float process (float x) noexcept { return bq.process (x); }
    Biquad bq;
};

// 4th-order Butterworth (24 dB/oct) = two cascaded biquads with the Butterworth Q pair.
struct Butter4
{
    void setLowpass (double fs, double f)
    {
        s[0].setCoeffs (biquad::lowpass (fs, f, 0.54119610));
        s[1].setCoeffs (biquad::lowpass (fs, f, 1.30656296));
    }
    void setHighpass (double fs, double f)
    {
        s[0].setCoeffs (biquad::highpass (fs, f, 0.54119610));
        s[1].setCoeffs (biquad::highpass (fs, f, 1.30656296));
    }
    void reset() { s[0].reset(); s[1].reset(); }
    inline float process (float x) noexcept { return s[1].process (s[0].process (x)); }
    Biquad s[2];
};
} // namespace babyair
