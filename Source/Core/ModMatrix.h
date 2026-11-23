#pragma once

#include "Params.h"
#include "ModSources.h"
#include "DspMath.h"
#include <array>
#include <memory>

namespace cureum
{

// One row of the modulation matrix. Only the amount is a host parameter (mod<N>_amount); the
// routing itself is part of the preset state.
struct ModSlot
{
    ModSource source = ModSource::none;
    ModSource aux = ModSource::none;
    int dest = -1;              // parameter index (ParamRegistry), -1 = none
    bool bipolar = false;
    float curve = 0.0f;         // -1..1, 0 = linear
    float output = 1.0f;        // 0..1 output scaling
    bool enabled = true;

    bool isActive() const noexcept { return enabled && source != ModSource::none && dest >= 0; }
    bool operator== (const ModSlot& o) const noexcept
    {
        return source == o.source && aux == o.aux && dest == o.dest && bipolar == o.bipolar
            && curve == o.curve && output == o.output && enabled == o.enabled;
    }
};

using ModSlots = std::array<ModSlot, kNumModSlots>;

// Immutable, audio-thread friendly form of the matrix.
struct CompiledMatrix
{
    struct Route
    {
        int slot = 0, source = 0, aux = 0, dest = 0;
        bool bipolar = false;
        float curve = 0.0f, output = 1.0f;
    };

    std::vector<Route> voiceRoutes, globalRoutes;
    std::vector<int> voiceDests, globalDests;
    std::array<bool, (size_t) pid::total> modulated {};
    std::array<bool, (size_t) kNumModSources> sourceUsed {};

    static std::shared_ptr<const CompiledMatrix> compile (const ModSlots& slots);

    // Evaluates a route value (before the amount parameter is applied).
    static inline float evaluate (const Route& r, const float* sources) noexcept
    {
        float v = sources[r.source];
        const bool naturalBipolar = isNaturallyBipolar ((ModSource) r.source);
        if (r.bipolar && ! naturalBipolar) v = v * 2.0f - 1.0f;
        if (r.curve != 0.0f)
        {
            if (r.bipolar || naturalBipolar) v = std::copysign (dsp::shapeCurve (std::abs (v), r.curve), v);
            else                             v = dsp::shapeCurve (v, r.curve);
        }
        if (r.aux != 0) v *= sources[r.aux];
        return v * r.output;
    }
};

juce::ValueTree modSlotsToTree (const ModSlots& slots);
void modSlotsFromTree (const juce::ValueTree& tree, ModSlots& slots);

} // namespace cureum
