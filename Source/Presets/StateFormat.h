#pragma once

#include "../Core/Params.h"
#include "../Core/ModMatrix.h"
#include "../DSP/Wavetable.h"
#include "../DSP/Lfo.h"
#include "../DSP/FxRack.h"
#include <juce_data_structures/juce_data_structures.h>

namespace cureum
{

// Serialisation helpers shared by the processor (state save/load) and the importers.
namespace StateFormat
{
    juce::ValueTree wavetableToTree (const Wavetable& t, int osc);
    WavetablePtr wavetableFromTree (const juce::ValueTree& tree);

    constexpr const char* kPresetExtension = ".cureum";

    // .cureum files are GZIP-compressed XML.
    bool writePresetFile (const juce::ValueTree& state, const juce::File& file);
    juce::ValueTree readPresetFile (const juce::File& file);
    juce::ValueTree readPresetData (const void* data, size_t size);
}

// Builds a CUREUM state tree from scratch (starting from default values). Importers fill it in
// and then hand the tree to CureumProcessor::loadStateTree().
class PresetBuilder
{
public:
    PresetBuilder();

    void set (int index, float plainValue);
    void setNormalised (int index, float norm);
    void setChoice (int index, int choiceIndex) { set (index, (float) choiceIndex); }
    void setBool (int index, bool v)            { set (index, v ? 1.0f : 0.0f); }
    float get (int index) const                 { return values[(size_t) index]; }

    void setWavetable (int osc, WavetablePtr table) { tables[(size_t) osc] = std::move (table); }
    WavetablePtr getWavetable (int osc) const       { return tables[(size_t) osc]; }
    void setLfoShape (int lfo, const LfoShape& s)   { shapes[(size_t) lfo] = s; }
    int addModulation (ModSource src, int dest, float amount, bool bipolar = false,
                       ModSource aux = ModSource::none, float curve = 0.0f, float output = 1.0f);
    void setFxOrder (const FxOrder& o)              { fxOrder = o; }
    const FxOrder& getFxOrder() const               { return fxOrder; }
    void setMacroName (int m, const juce::String& n){ macroNames[(size_t) m] = n; }
    void setNoiseFile (const juce::File& f)         { noiseFile = f; }

    juce::String name { "Imported" }, author, description, category;
    juce::StringArray tags;
    juce::StringArray warnings;

    juce::ValueTree build() const;

private:
    std::vector<float> values;
    std::array<WavetablePtr, kNumOscillators> tables;
    std::array<LfoShape, kNumLfos> shapes;
    ModSlots slots;
    FxOrder fxOrder;
    std::array<juce::String, (size_t) kNumMacros> macroNames;
    juce::File noiseFile;
};

} // namespace cureum
