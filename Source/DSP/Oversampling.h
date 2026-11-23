#pragma once

#include <cmath>
#include <array>
#include <vector>

namespace cureum
{

// Polyphase IIR half-band filters (two parallel chains of first-order allpasses), designed with
// the classic elliptic-based method used by Laurent de Soras' HIIR library. Near-zero latency,
// ~100 dB of image rejection with 12 coefficients.
namespace halfband
{
    inline double ipow (double x, int n) { double r = 1.0; while (n-- > 0) r *= x; return r; }

    inline std::vector<double> design (int numCoefs, double transition)
    {
        constexpr double pi = 3.14159265358979323846;
        double k = std::tan ((1.0 - transition * 2.0) * pi / 4.0);
        k *= k;
        const double kksqrt = std::pow (1.0 - k * k, 0.25);
        const double e = 0.5 * (1.0 - kksqrt) / (1.0 + kksqrt);
        const double e2 = e * e, e4 = e2 * e2;
        const double q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
        const int order = numCoefs * 2 + 1;

        std::vector<double> coefs ((size_t) numCoefs);
        for (int index = 0; index < numCoefs; ++index)
        {
            const int c = index + 1;
            double num = 0.0, acc;
            int i = 0, j = 1;
            do { acc = ipow (q, i * (i + 1)) * std::sin ((i * 2 + 1) * c * pi / order) * j; num += acc; j = -j; ++i; }
            while (std::abs (acc) > 1e-100 && i < 100);
            double den = 0.0;
            i = 1; j = -1;
            do { acc = ipow (q, i * i) * std::cos (i * 2 * c * pi / order) * j; den += acc; j = -j; ++i; }
            while (std::abs (acc) > 1e-100 && i < 100);
            num *= std::pow (q, 0.25);
            den += 0.5;
            const double ww = num / den;
            const double wwsq = ww * ww;
            const double x = std::sqrt ((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
            coefs[(size_t) index] = (1.0 - x) / (1.0 + x);
        }
        return coefs;
    }

    inline const std::vector<float>& defaultCoefs()
    {
        static const std::vector<float> c = []
        {
            const auto d = design (12, 0.035);
            return std::vector<float> (d.begin(), d.end());
        }();
        return c;
    }
}

// 2:1 decimator for one channel.
class Downsampler2x
{
public:
    Downsampler2x() : coefs (halfband::defaultCoefs()) { reset(); }

    void reset() { x1.fill (0.0f); y1.fill (0.0f); }

    inline float process (float in0, float in1) noexcept
    {
        float a = in1, b = in0;
        const int n = (int) coefs.size();
        for (int i = 0; i < n; i += 2)
        {
            const float ya = coefs[(size_t) i] * (a - y1[(size_t) i]) + x1[(size_t) i];
            x1[(size_t) i] = a; y1[(size_t) i] = ya; a = ya;
            if (i + 1 < n)
            {
                const float yb = coefs[(size_t) i + 1] * (b - y1[(size_t) i + 1]) + x1[(size_t) i + 1];
                x1[(size_t) i + 1] = b; y1[(size_t) i + 1] = yb; b = yb;
            }
        }
        return 0.5f * (a + b);
    }

private:
    const std::vector<float>& coefs;
    std::array<float, 32> x1 {}, y1 {};
};

// Decimates an oversampled stereo buffer (factor 1, 2 or 4) in place into the output pointers.
class Decimator
{
public:
    void reset() { for (auto& d : stage) d.reset(); }

    // in: factor * numSamples samples; out: numSamples samples.
    void process (const float* in, float* out, int numSamples, int factor, int channel) noexcept
    {
        if (factor == 1) { for (int i = 0; i < numSamples; ++i) out[i] = in[i]; return; }
        auto& s1 = stage[(size_t) channel * 2];
        if (factor == 2)
        {
            for (int i = 0; i < numSamples; ++i)
                out[i] = s1.process (in[2 * i], in[2 * i + 1]);
            return;
        }
        auto& s2 = stage[(size_t) channel * 2 + 1];
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = s1.process (in[4 * i], in[4 * i + 1]);
            const float b = s1.process (in[4 * i + 2], in[4 * i + 3]);
            out[i] = s2.process (a, b);
        }
    }

private:
    std::array<Downsampler2x, 4> stage;
};

} // namespace cureum
