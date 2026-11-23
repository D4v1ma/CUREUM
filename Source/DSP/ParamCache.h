#pragma once

#include "../Core/Params.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace cureum
{

// Snapshot of every parameter, refreshed once per audio block. Holds both the plain value and the
// normalised (0..1) value so modulation can be applied in normalised space cheaply.
class ParamCache
{
public:
    void attach (juce::AudioProcessorValueTreeState& state)
    {
        const auto& reg = ParamRegistry::get();
        raw.resize ((size_t) pid::total);
        plainV.resize ((size_t) pid::total);
        normV.resize ((size_t) pid::total);
        for (int i = 0; i < pid::total; ++i)
        {
            raw[(size_t) i] = state.getRawParameterValue (reg.spec (i).id);
            jassert (raw[(size_t) i] != nullptr);
        }
        update();
    }

    // Detached mode (tests, offline rendering): uses default values.
    void attachDefaults()
    {
        const auto& reg = ParamRegistry::get();
        raw.assign ((size_t) pid::total, nullptr);
        plainV.resize ((size_t) pid::total);
        normV.resize ((size_t) pid::total);
        for (int i = 0; i < pid::total; ++i)
            plainV[(size_t) i] = reg.spec (i).defaultValue;
        updateNormalised();
    }

    void update() noexcept
    {
        for (size_t i = 0; i < raw.size(); ++i)
            if (raw[i] != nullptr)
                plainV[i] = raw[i]->load (std::memory_order_relaxed);
        updateNormalised();
    }

    void setPlain (int i, float v) noexcept { plainV[(size_t) i] = v; normV[(size_t) i] = ParamRegistry::get().spec (i).toNormalised (v); }

    float plain (int i) const noexcept   { return plainV[(size_t) i]; }
    float norm (int i) const noexcept    { return normV[(size_t) i]; }
    bool on (int i) const noexcept       { return plainV[(size_t) i] >= 0.5f; }
    int choice (int i) const noexcept    { return juce::roundToInt (plainV[(size_t) i]); }

    // Applies a normalised modulation offset and returns the plain value.
    float modulated (int i, float normOffset) const noexcept
    {
        return ParamRegistry::get().spec (i).fromNormalised (normV[(size_t) i] + normOffset);
    }

private:
    void updateNormalised() noexcept
    {
        const auto& reg = ParamRegistry::get();
        for (size_t i = 0; i < plainV.size(); ++i)
        {
            const auto& s = reg.spec ((int) i);
            if (s.kind == ParamKind::continuous || s.kind == ParamKind::integer || s.isModulatable())
                normV[i] = s.toNormalised (plainV[i]);
        }
    }

    std::vector<std::atomic<float>*> raw;
    std::vector<float> plainV, normV;
};

} // namespace cureum
