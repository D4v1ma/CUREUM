#pragma once

#include "../Core/FilterTypes.h"
#include "../Core/DspMath.h"
#include <vector>

namespace cureum
{

// A stereo multimode filter that implements every CUREUM filter type. Used per voice (two per
// voice), as the FX-rack filter, and offline for drawing frequency responses.
class MultiFilter
{
public:
    void prepare (double sampleRate, double maxSampleRate);
    void setSampleRate (double sampleRate) { sr = sampleRate; reset(); }   // no allocation
    void reset();

    void setType (int typeIndex);
    int getType() const noexcept { return type; }

    // cutoff in Hz; reso, drive, var in [0, 1]; stereo in [-1, 1] (+/- 1 octave between channels)
    void setParameters (float cutoffHz, float reso, float drive, float var, float stereo);

    void process (float& l, float& r) noexcept;

    // Magnitude response (linear) at the given frequencies, measured with an impulse.
    static void measureResponse (int typeIndex, float cutoffHz, float reso, float drive, float var,
                                 double sampleRate, const std::vector<float>& freqs, std::vector<float>& magnitudes);

private:
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        inline void tick (float x, float g, float k, float& lp, float& bp, float& hp) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
        void reset() { ic1 = ic2 = 0.0f; }
    };

    struct OnePole
    {
        float s = 0.0f;
        inline float lp (float x, float G) noexcept { const float v = (x - s) * G; const float y = v + s; s = y + v; return y; }
        inline float hp (float x, float G) noexcept { return x - lp (x, G); }
        void reset() { s = 0.0f; }
    };

    struct Allpass1   // first-order allpass, y = a*x + x1 - a*y1
    {
        float x1 = 0.0f, y1 = 0.0f;
        inline float process (float x, float a) noexcept { const float y = a * x + x1 - a * y1; x1 = x; y1 = y; return y; }
        void reset() { x1 = y1 = 0.0f; }
    };

    struct DelayLine
    {
        float* buf = nullptr;
        int size = 0, pos = 0;
        inline void write (float x) noexcept { buf[pos] = x; if (++pos >= size) pos = 0; }
        inline float read (float delay) const noexcept
        {
            float rp = (float) pos - delay;
            while (rp < 0.0f) rp += (float) size;
            int i0 = (int) rp;
            const float f = rp - (float) i0;
            if (i0 >= size) i0 -= size;
            int i1 = i0 + 1; if (i1 >= size) i1 = 0;
            return buf[i0] + f * (buf[i1] - buf[i0]);
        }
        void clear() { if (buf != nullptr) std::fill (buf, buf + size, 0.0f); pos = 0; }
    };

    struct Channel
    {
        Svf svf[4];
        OnePole op[6];
        Allpass1 ap[24];
        float ladder[4] {};
        float lastOut = 0.0f, hold = 0.0f, holdPhase = 0.0f, ringPhase = 0.0f, ringPhase2 = 0.0f, smooth = 0.0f;
        DelayLine lines[4];
        float bq[4][4] {}; // biquad states for EQ types
    };

    struct Coeffs
    {
        float g = 0.0f, G = 0.0f, k = 2.0f, fc = 1000.0f, delaySamples = 100.0f;
        float g2 = 0.0f, g3 = 0.0f;  // secondary frequencies (spread types)
        float apA[24] {};
        float bq[5] {}; float bq2[5] {};
    };

    float processChannel (int ch, float x) noexcept;
    void computeCoeffs (Coeffs& c, float fc) const noexcept;

    double sr = 44100.0;
    int type = kDefaultFilterType;
    FilterTypeDef def = filterTypes()[(size_t) kDefaultFilterType];
    float reso = 0.1f, drive = 0.0f, var = 0.0f;
    float inGain = 1.0f, outGain = 1.0f;
    Coeffs coeffs[2];
    Channel chan[2];
    std::vector<float> memory;
    int lineSize = 0;
};

} // namespace cureum
