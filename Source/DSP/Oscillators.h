#pragma once

#include "Wavetable.h"
#include "../Core/Choices.h"
#include "../Core/DspMath.h"
#include "../Core/Params.h"

namespace cureum
{

//==============================================================================
// Signals another oscillator can use for FM/PM/AM/RM warps (previous/current sample, per unison voice).
struct CrossModSignals
{
    const float* oscOut[kNumOscillators] {};   // per-unison raw waveform output of each oscillator
    int oscUnison[kNumOscillators] {};
    float sub = 0.0f;
    float noise = 0.0f;
};

//==============================================================================
// Per-voice wavetable oscillator with unison and two warp stages.
class WavetableOscillator
{
public:
    struct Settings   // computed by the voice once per control block
    {
        const Wavetable* table = nullptr;
        float freqHz = 440.0f;
        float level = 0.75f;         // knob value (gain = level^2)
        float pan = 0.0f;
        float wtPos = 0.0f;
        WarpMode warp1 = WarpMode::off, warp2 = WarpMode::off;
        float warp1Amt = 0.0f, warp2Amt = 0.0f;
        int unison = 1;
        float detune = 0.5f, blend = 0.75f, width = 1.0f;
        UnisonMode uniMode = UnisonMode::linear;
        UnisonStack uniStack = UnisonStack::off;
        float uniWarp = 0.0f, uniWtPos = 0.0f;
    };

    void prepare (double sampleRate);
    void noteOn (float phaseDeg, float randomPhase, uint32_t seed);
    void setup (const Settings& s, int oscIndex);   // call once per control block

    // Renders one sample (stereo) and stores the per-unison raw outputs for cross modulation.
    inline void renderSample (float& outL, float& outR, const CrossModSignals& x) noexcept;

    const float* rawOutputs() const noexcept { return lastOut; }
    int numUnison() const noexcept           { return unison; }
    float monoOutput() const noexcept        { return lastMono; }

private:
    struct Uni
    {
        const float* t0 = nullptr;
        const float* t1 = nullptr;
        int size = 2048;
        float frac = 0.0f;
        float inc = 0.0f;
        float gainL = 0.0f, gainR = 0.0f;
        float warpA = 0.0f, warpB = 0.0f;
    };

    inline float warpPhase (WarpMode m, float ph, float amt, int u, const CrossModSignals& x, float& gate) const noexcept;
    inline float warpAmp (WarpMode m, float s, float phBase, float amt, int u, const CrossModSignals& x) const noexcept;
    inline float fmSource (WarpMode m, int u, const CrossModSignals& x) const noexcept;
    static bool isFrequencyMod (WarpMode m) noexcept;

    double sr = 44100.0;
    int osc = 0;
    int unison = 1;
    WarpMode warp1 = WarpMode::off, warp2 = WarpMode::off;
    float phase[kMaxUnison] {};
    float lastOut[kMaxUnison] {};
    float unisonRandom[kMaxUnison] {};
    float lastMono = 0.0f;
    Uni uni[kMaxUnison];
};

//==============================================================================
class SubOscillator
{
public:
    void prepare (double sampleRate) { sr = sampleRate; }
    void noteOn (float phaseDeg) { phase = phaseDeg / 360.0f; phase -= std::floor (phase); }
    void setup (float freqHz, SubShape shape, float level, float pan);
    inline float renderSample (float& outL, float& outR) noexcept
    {
        const float s = Wavetable::lookup (table, size, phase);
        phase += inc;
        phase -= (float) (int) phase;
        outL += s * gainL;
        outR += s * gainR;
        return s;
    }

    static const Wavetable& shapes();   // six-frame table of sub shapes

private:
    double sr = 44100.0;
    float phase = 0.0f, inc = 0.0f, gainL = 0.0f, gainR = 0.0f;
    const float* table = nullptr;
    int size = 64;
};

//==============================================================================
// Sample-based noise oscillator.
struct NoiseSample
{
    juce::String name;
    std::vector<float> left, right;
    double sampleRate = 48000.0;
    bool oneShot = false;
    int length() const noexcept { return (int) left.size(); }
};
using NoiseSamplePtr = std::shared_ptr<const NoiseSample>;

class NoiseBank
{
public:
    static const NoiseBank& get();
    const NoiseSample& sample (NoiseType t) const;
private:
    NoiseBank();
    std::vector<NoiseSample> samples;
};

class NoiseOscillator
{
public:
    void prepare (double sampleRate) { sr = sampleRate; }
    void noteOn (const NoiseSample* src, float start, float randomStart, uint32_t seed);
    void setup (const NoiseSample* src, float rate, float level, float pan, bool oneShot, bool stereo);
    inline float renderSample (float& outL, float& outR) noexcept;

private:
    double sr = 44100.0;
    const NoiseSample* sample = nullptr;
    double pos = 0.0, inc = 1.0;
    float gainL = 0.0f, gainR = 0.0f;
    bool oneShot = false, stereo = true, finished = false;
};

//==============================================================================
// Inline implementations
//==============================================================================
inline bool WavetableOscillator::isFrequencyMod (WarpMode m) noexcept
{
    return m == WarpMode::fmOsc || m == WarpMode::fmNoise || m == WarpMode::fmSub
        || m == WarpMode::fmA || m == WarpMode::fmB || m == WarpMode::fmC;
}

inline float WavetableOscillator::fmSource (WarpMode m, int u, const CrossModSignals& x) const noexcept
{
    auto fromOsc = [&] (int o) -> float
    {
        if (x.oscOut[o] == nullptr || x.oscUnison[o] <= 0) return 0.0f;
        return x.oscOut[o][u % x.oscUnison[o]];
    };
    const int partner = osc == 0 ? 1 : 0;
    switch (m)
    {
        case WarpMode::fmOsc: case WarpMode::amOsc: case WarpMode::rmOsc: case WarpMode::pmOsc: return fromOsc (partner);
        case WarpMode::fmA: case WarpMode::pmA: return fromOsc (0);
        case WarpMode::fmB: case WarpMode::pmB: return fromOsc (1);
        case WarpMode::fmC: case WarpMode::pmC: return fromOsc (2);
        case WarpMode::fmNoise: case WarpMode::amNoise: case WarpMode::rmNoise: return x.noise;
        case WarpMode::fmSub: case WarpMode::amSub: case WarpMode::rmSub: return x.sub;
        case WarpMode::pmSelf: return lastOut[u];
        default: return 0.0f;
    }
}

inline float WavetableOscillator::warpPhase (WarpMode m, float ph, float a, int u, const CrossModSignals& x, float& gate) const noexcept
{
    using namespace dsp;
    switch (m)
    {
        case WarpMode::sync:
        case WarpMode::syncWindow:
        {
            const float p = ph * (1.0f + a * a * 15.0f);
            return p - std::floor (p);
        }
        case WarpMode::bendPlus:
        case WarpMode::bendMinus:
        case WarpMode::bendPlusMinus:
        {
            float k = a;
            bool expand = m == WarpMode::bendPlus;
            if (m == WarpMode::bendPlusMinus) { expand = a >= 0.5f; k = std::abs (a - 0.5f) * 2.0f; }
            const float t = ph * 2.0f - 1.0f;
            const float e = expand ? 1.0f / (1.0f + k * 5.0f) : 1.0f + k * 5.0f;
            const float y = std::copysign (std::pow (std::abs (t), e), t);
            return clamp01 ((y + 1.0f) * 0.5f) * 0.99999f;
        }
        case WarpMode::pwm:
        {
            const float w = 1.0f - a * 0.98f;
            if (ph >= w) { gate = 0.0f; return 0.0f; }
            return ph / w;
        }
        case WarpMode::asymPlus:      return std::pow (ph, 1.0f / (1.0f + a * 7.0f));
        case WarpMode::asymMinus:     return std::pow (ph, 1.0f + a * 7.0f);
        case WarpMode::asymPlusMinus:
        {
            const float k = (a - 0.5f) * 2.0f;
            return k >= 0.0f ? std::pow (ph, 1.0f / (1.0f + k * 7.0f)) : std::pow (ph, 1.0f - k * 7.0f);
        }
        case WarpMode::mirror:
        {
            const float mrr = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;
            return clamp01 (lerp (ph, mrr, a)) * 0.99999f;
        }
        case WarpMode::remap1:
        {
            const float s = ph * ph * (3.0f - 2.0f * ph);
            const float s2 = s * s * (3.0f - 2.0f * s);
            return lerp (ph, s2, a);
        }
        case WarpMode::remap2:
        {
            const float s = 0.5f - 0.5f * std::cos (kPi * ph);
            return lerp (ph, s, a);
        }
        case WarpMode::remap3:
        {
            if (a <= 0.0f) return ph;
            const float steps = 34.0f - a * 31.0f;
            return std::floor (ph * steps) / steps;
        }
        case WarpMode::remap4:
        {
            const float p = ph + a * 0.16f * fastSin01 (ph * 3.0f);
            return p - std::floor (p);
        }
        case WarpMode::quantize:
        {
            if (a <= 0.0f) return ph;
            const float steps = std::exp2 (1.0f + (1.0f - a) * 10.0f);
            return std::floor (ph * steps) / steps;
        }
        case WarpMode::pmOsc: case WarpMode::pmSelf: case WarpMode::pmA: case WarpMode::pmB: case WarpMode::pmC:
        {
            const float depth = m == WarpMode::pmSelf ? a * a * 1.2f : a * a * 2.5f;
            const float p = ph + depth * fmSource (m, u, x);
            return p - std::floor (p);
        }
        case WarpMode::phaseDistort:
        {
            const float d = 0.5f - 0.49f * a;
            return ph < d ? ph * (0.5f / d) : 0.5f + (ph - d) * (0.5f / (1.0f - d));
        }
        default: return ph;
    }
}

inline float WavetableOscillator::warpAmp (WarpMode m, float s, float phBase, float a, int u, const CrossModSignals& x) const noexcept
{
    using namespace dsp;
    switch (m)
    {
        case WarpMode::syncWindow: return s * (0.5f - 0.5f * std::cos (kTwoPi * phBase));
        case WarpMode::flip:       return phBase >= 1.0f - a ? -s : s;
        case WarpMode::amOsc: case WarpMode::amNoise: case WarpMode::amSub:
            return s * (1.0f - a + a * (0.5f + 0.5f * fmSource (m, u, x)));
        case WarpMode::rmOsc: case WarpMode::rmNoise: case WarpMode::rmSub:
            return lerp (s, s * fmSource (m, u, x), a);
        case WarpMode::squash:
        {
            const float g = 1.0f + a * 12.0f;
            return fastTanh (s * g) / fastTanh (g);
        }
        case WarpMode::fold:
        {
            const float g = 1.0f + a * 7.0f;
            return std::sin (s * g * kPi * 0.5f);
        }
        case WarpMode::bitCrush:
        {
            if (a <= 0.0f) return s;
            const float levels = std::exp2 (1.0f + (1.0f - a) * 11.0f);
            return std::round (s * levels) / levels;
        }
        default: return s;
    }
}

inline void WavetableOscillator::renderSample (float& outL, float& outR, const CrossModSignals& x) noexcept
{
    float sumL = 0.0f, sumR = 0.0f, mono = 0.0f;
    const bool fm1 = isFrequencyMod (warp1), fm2 = isFrequencyMod (warp2);

    for (int u = 0; u < unison; ++u)
    {
        auto& v = uni[u];
        const float base = phase[u];
        float gate = 1.0f;
        float ph = base;
        if (warp1 != WarpMode::off) ph = warpPhase (warp1, ph, v.warpA, u, x, gate);
        if (warp2 != WarpMode::off) ph = warpPhase (warp2, ph, v.warpB, u, x, gate);

        const float s0 = Wavetable::lookup (v.t0, v.size, ph);
        float s = s0;
        if (v.frac > 0.0f)
            s = s0 + v.frac * (Wavetable::lookup (v.t1, v.size, ph) - s0);

        if (warp1 != WarpMode::off) s = warpAmp (warp1, s, base, v.warpA, u, x);
        if (warp2 != WarpMode::off) s = warpAmp (warp2, s, base, v.warpB, u, x);
        s *= gate;

        lastOut[u] = s;
        mono += s;
        sumL += s * v.gainL;
        sumR += s * v.gainR;

        float inc = v.inc;
        if (fm1) inc *= 1.0f + v.warpA * v.warpA * 6.0f * fmSource (warp1, u, x);
        if (fm2) inc *= 1.0f + v.warpB * v.warpB * 6.0f * fmSource (warp2, u, x);
        float p = base + inc;
        p -= std::floor (p);
        phase[u] = p;
    }

    lastMono = mono / (float) unison;
    outL += sumL;
    outR += sumR;
}

inline float NoiseOscillator::renderSample (float& outL, float& outR) noexcept
{
    if (sample == nullptr || finished) return 0.0f;
    const int len = sample->length();
    if (len < 2) return 0.0f;

    const int i0 = (int) pos;
    const float f = (float) (pos - (double) i0);
    const int i1 = i0 + 1 < len ? i0 + 1 : (oneShot ? i0 : 0);
    const float l = sample->left[(size_t) i0] + f * (sample->left[(size_t) i1] - sample->left[(size_t) i0]);
    const float r = stereo ? sample->right[(size_t) i0] + f * (sample->right[(size_t) i1] - sample->right[(size_t) i0]) : l;

    pos += inc;
    if (pos >= (double) len)
    {
        if (oneShot) finished = true;
        else pos -= (double) len * std::floor (pos / (double) len);
    }

    outL += l * gainL;
    outR += r * gainR;
    return 0.5f * (l + r);
}

} // namespace cureum
