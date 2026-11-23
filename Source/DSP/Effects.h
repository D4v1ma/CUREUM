#pragma once

#include "ParamCache.h"
#include "Filter.h"
#include "../Core/ModMatrix.h"
#include <juce_dsp/juce_dsp.h>

namespace cureum
{

// Read-only information the effects need for one block.
struct FxContext
{
    const ParamCache* params = nullptr;
    const float* globalMod = nullptr;
    const CompiledMatrix* matrix = nullptr;
    double sampleRate = 44100.0;
    double bpm = 120.0;
    bool noteStarted = false;

    float value (int idx) const noexcept
    {
        if (matrix != nullptr && globalMod != nullptr && matrix->modulated[(size_t) idx])
            return params->modulated (idx, globalMod[idx]);
        return params->plain (idx);
    }
    bool on (int idx) const noexcept    { return params->on (idx); }
    int choice (int idx) const noexcept { return params->choice (idx); }

    // Length in seconds of a synced division.
    double syncSeconds (int divisionIndex, double feel = 1.0) const noexcept
    {
        return 4.0 * syncRateWholeNotes (divisionIndex) * feel * 60.0 / juce::jmax (20.0, bpm);
    }
};

class FxUnit
{
public:
    virtual ~FxUnit() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    virtual void process (float* l, float* r, int n, const FxContext& ctx) = 0;
    virtual int getLatencySamples() const { return 0; }
};

// A simple circular delay line with fractional (linear) reads.
class FxDelayLine
{
public:
    void allocate (int maxSamples) { buf.assign ((size_t) juce::nextPowerOfTwo (maxSamples + 4), 0.0f); mask = (int) buf.size() - 1; pos = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) noexcept { pos = (pos + 1) & mask; buf[(size_t) pos] = x; }
    inline float read (float delay) const noexcept   // delay in samples (>= 0), relative to the last pushed sample
    {
        const float rp = (float) pos - delay;
        const int i0 = (int) std::floor (rp);
        const float f = rp - (float) i0;
        const float a = buf[(size_t) (i0 & mask)];
        const float b = buf[(size_t) ((i0 + 1) & mask)];
        return a + f * (b - a);
    }
    int capacity() const noexcept { return (int) buf.size() - 4; }
private:
    std::vector<float> buf { 0.0f, 0.0f, 0.0f, 0.0f };
    int mask = 3, pos = 0;
};

// Linear ramp for block-rate parameters.
struct Ramp
{
    float current = 0.0f, target = 0.0f, step = 0.0f;
    bool init = false;
    void set (float t, int n) { if (! init) { current = target = t; init = true; step = 0.0f; return; } target = t; step = (target - current) / (float) juce::jmax (1, n); }
    inline float next() noexcept { current += step; return current; }
    void finish() { current = target; step = 0.0f; }
};

//==============================================================================
class HyperDimension : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    double sr = 44100.0;
    FxDelayLine hyperL, hyperR, dimL, dimR;
    double phase[7] {};
    Ramp mix, dim;
};

class Distortion : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
    static float shape (DistMode mode, float x, float drive) noexcept;
private:
    double sr = 44100.0;
    MultiFilter filter;
    float hold[2] {}; int holdCount = 0;
    float dcX[2] {}, dcY[2] {};
    Ramp mix, drv;
};

class FlangerFx : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    double sr = 44100.0, phase = 0.0;
    FxDelayLine line[2];
    float fb[2] {};
    Ramp mix;
};

class PhaserFx : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    double sr = 44100.0, phase = 0.0;
    float x1[2][12] {}, y1[2][12] {}, fb[2] {};
    Ramp mix;
};

class ChorusFx : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    double sr = 44100.0, phase = 0.0;
    FxDelayLine line[2];
    float lp[2] {}, fb[2] {};
    Ramp mix;
};

class StereoDelay : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    double sr = 44100.0;
    FxDelayLine line[2];
    float timeL = 0.3f, timeR = 0.3f;   // smoothed, samples
    float lpS[2] {}, hpS[2] {};
    Ramp mix;
    bool first = true;
};

class Compressor : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
    float getGainReductionDb() const noexcept { return grDisplay; }
private:
    struct Biquad { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {}; void setLR (bool high, float f, double sr);
                    inline float process (int ch, float x) noexcept { const float y = b0 * x + z1[ch]; z1[ch] = b1 * x - a1 * y + z2[ch]; z2[ch] = b2 * x - a2 * y; return y; }
                    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; } };
    float compressSample (int band, float levelDb, float thr, float ratio, bool upward) noexcept;
    double sr = 44100.0;
    float env[3] {}, gainSm[3] { 1.0f, 1.0f, 1.0f };
    float attCoef = 0.0f, relCoef = 0.0f;
    Biquad lo1a, lo1b, hi1a, hi1b, lo2a, lo2b, hi2a, hi2b;
    float grDisplay = 0.0f;
    Ramp mix;
};

class Equalizer : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    struct Bq { float c[5] { 1, 0, 0, 0, 0 }; float z[2][2] {}; };
    double sr = 44100.0;
    Bq low, high;
};

class FxFilterUnit : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    MultiFilter filter;
    Ramp mix;
};

class BodeShifter : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    struct Hilbert
    {
        float xi[4][2] {}, yi[4][2] {}, xq[4][2] {}, yq[4][2] {}, qDelay = 0.0f;
        void process (float x, float& i, float& q) noexcept;
    };
    double sr = 44100.0;
    Hilbert h[2];
    double phase[2] {};
    float fb[2] {};
    Ramp mix;
};

class Utility : public FxUnit
{
public:
    void prepare (double sr, int) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
private:
    double sr = 44100.0;
    float lp1[2] {}, lp2[2] {};
    Ramp gain;
};

} // namespace cureum
