// BABY AIR - Non-linear shapers with first-order ADAA (antiderivative anti-aliasing).
//
// Curves (all C0/C1-continuous, no hard clipping):
//   Tanh       : tanh(x+b) - tanh(b)   (b = bias -> asymmetric, even+odd harmonics, DC removed analytically)
//   Vintage    : x>=0 ? tanh(x) : x/(1+|x|)  (C1 at 0; soft negative side => asymmetric, 2nd-harmonic rich)
//   Rectifier  : sqrt(x^2+e^2)-e       (smooth full-wave rectifier => even harmonics / "air")
//
// ADAA: y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1]) with F the antiderivative of the curve,
// evaluated in double precision. When ADAA is disabled the curve is evaluated directly (used with
// 8x/16x oversampling where its benefit is negligible and its CPU cost is not).
#pragma once
#include "DspCommon.h"
#include <array>

namespace babyair
{
inline double logCosh (double x) noexcept
{
    x = std::abs (x);
    return x + std::log1p (std::exp (-2.0 * x)) - 0.69314718055994531;
}

// Describing-function gain of tanh for a sine of peak amplitude a:
//   N(a) = 2/(pi a) * integral_0^pi tanh(a sin t) sin t dt
// i.e. the exact amplitude of the FUNDAMENTAL at the output of tanh. Used to remove the fundamental from the
// non-linear residual so that the added signal is harmonic content only (no hidden EQ boost/cut).
// Table is built once in prepare() (never on the audio thread) and linearly interpolated in log2(a).
class DescribingFunction
{
public:
    static void init() { table(); }
    static float tanhGain (float a) noexcept
    {
        if (a < 0.0156f) return 1.0f;
        if (a > 63.0f) return 1.2732395f / a;                 // 4/(pi a)
        const float pos = (std::log2 (a) + 6.0f) * ((kSize - 1) / 12.0f);
        const int i = std::min ((int) pos, kSize - 2);
        const float fr = pos - (float) i;
        const auto& t = table();
        return t[(size_t) i] + fr * (t[(size_t) i + 1] - t[(size_t) i]);
    }
private:
    static constexpr int kSize = 512;
    static const std::array<float, kSize>& table()
    {
        static const std::array<float, kSize> t = [] {
            std::array<float, kSize> r {};
            for (int i = 0; i < kSize; ++i)
            {
                const double a = std::pow (2.0, -6.0 + 12.0 * i / (kSize - 1));
                double acc = 0.0; const int M = 2000;
                for (int k = 0; k < M; ++k)
                {
                    const double th = kPi * (k + 0.5) / M;
                    acc += std::tanh (a * std::sin (th)) * std::sin (th);
                }
                r[(size_t) i] = (float) (2.0 / (kPi * a) * acc * kPi / M);
            }
            return r;
        }();
        return t;
    }
};

class AdaaShaper
{
public:
    enum class Type { Tanh, Vintage, Rectifier };

    void setup (Type t, float bias, float eps, bool useAdaa) noexcept
    {
        type_ = t; bias_ = bias; eps_ = std::max (eps, 1.0e-3f); adaa_ = useAdaa;
        biasTanh_ = std::tanh ((double) bias_);
        reset();
    }
    void reset() noexcept { x1_ = 0.0; F1_ = F (0.0); }

    inline float process (float x) noexcept
    {
        if (! adaa_) return (float) f ((double) x);
        const double xd = x, Fx = F (xd), dx = xd - x1_;
        const double y = (std::abs (dx) < 1.0e-6) ? f (0.5 * (xd + x1_)) : (Fx - F1_) / dx;
        x1_ = xd; F1_ = Fx;
        return (float) y;
    }

private:
    inline double f (double x) const noexcept
    {
        switch (type_)
        {
            case Type::Tanh:      return std::tanh (x + bias_) - biasTanh_;
            case Type::Vintage:   return x >= 0.0 ? std::tanh (x) : x / (1.0 - x);
            case Type::Rectifier: return std::sqrt (x * x + (double) eps_ * eps_) - eps_;
        }
        return x;
    }
    inline double F (double x) const noexcept
    {
        switch (type_)
        {
            case Type::Tanh:      return logCosh (x + bias_) - biasTanh_ * x;
            case Type::Vintage:   return x >= 0.0 ? logCosh (x) : (-x) - std::log1p (-x);
            case Type::Rectifier:
            {
                const double e = eps_, r = std::sqrt (x * x + e * e);
                return 0.5 * (x * r + e * e * std::asinh (x / e)) - e * x;
            }
        }
        return 0.5 * x * x;
    }
    Type type_ = Type::Tanh;
    float bias_ = 0.0f, eps_ = 0.3f;
    double biasTanh_ = 0.0, x1_ = 0.0, F1_ = 0.0;
    bool adaa_ = true;
};
} // namespace babyair
