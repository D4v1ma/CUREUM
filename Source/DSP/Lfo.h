#pragma once

#include "../Core/Choices.h"
#include "../Core/DspMath.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <memory>

namespace cureum
{

// A drawable LFO shape: breakpoints with a curve value per segment, Serum 2 style
// (x, y in [0, 1]; curve 0.5 = straight line). Compiled into a lookup table for the audio thread.
struct LfoPoint
{
    float x = 0.0f, y = 0.0f, curve = 0.5f;
};

class LfoShape
{
public:
    static constexpr int kTableSize = 2048;

    LfoShape();                                      // default triangle
    explicit LfoShape (std::vector<LfoPoint> pts, juce::String name = {});

    const std::vector<LfoPoint>& points() const noexcept { return pts; }
    const juce::String& getName() const noexcept         { return name; }
    float evaluate (float x) const noexcept;         // exact evaluation from the points (UI)
    inline float lookup (float phase) const noexcept
    {
        const float p = (phase - std::floor (phase)) * (float) kTableSize;
        const int i = (int) p;
        const float f = p - (float) i;
        return table[(size_t) i] + f * (table[(size_t) i + 1] - table[(size_t) i]);
    }

    static float segmentCurve (float t, float curve) noexcept;   // curve 0..1, 0.5 linear
    static std::vector<juce::String> presetNames();
    static LfoShape preset (const juce::String& name);

    juce::var toVar() const;
    static LfoShape fromVar (const juce::var& v);

private:
    void compile();
    std::vector<LfoPoint> pts;
    juce::String name;
    std::vector<float> table;
};

using LfoShapePtr = std::shared_ptr<const LfoShape>;

// Per-voice (or global) LFO state.
class LfoGen
{
public:
    struct Settings
    {
        LfoType type = LfoType::shape;
        LfoMode mode = LfoMode::trig;
        float rise = 0.0f, delay = 0.0f, smooth = 0.0f;
        float phaseOffset = 0.0f; // 0..1
    };

    void noteOn (const Settings& s, uint32_t seed) noexcept;

    // Advances by `cycles` (rate * time) and `seconds` of wall time. Returns the output (0..1).
    // If `globalPhase` >= 0 the LFO follows that phase (Free mode).
    float advance (const Settings& s, const LfoShape* shape, double cycles, float seconds, float rateHz, double globalPhase) noexcept;

    float value() const noexcept { return out; }
    float currentPhase() const noexcept { return (float) phase; }

private:
    float sample (const Settings& s, const LfoShape* shape, double ph) noexcept;

    double phase = 0.0;
    float out = 0.0f, smoothed = 0.0f, elapsed = 0.0f;
    bool envDone = false;
    float shPrev = 0.0f, shNext = 0.0f;
    double lastCycle = 0.0;
    dsp::Random rng;
    float cx = 0.1f, cy = 0.0f, cz = 0.0f;
};

} // namespace cureum
