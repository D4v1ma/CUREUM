#pragma once

// Helpers shared by the Serum importer translation units.
#include "StateFormat.h"
#include <juce_core/juce_core.h>

namespace cureum::serumdetail
{
    juce::String normaliseName (juce::String s);
    float num (const juce::var& obj, const char* key, float def);
    bool has (const juce::var& obj, const char* key);
    juce::String str (const juce::var& obj, const char* key);
    juce::var plain (const juce::var& module);
    float pct (float v);
    WavetablePtr resolveWavetable (const juce::String& relPath, juce::StringArray& warnings);
    NoiseType noiseFromName (const juce::String& n);
    void applyNoiseSample (PresetBuilder& b, const juce::String& relPath);
    int distModeFromName (const juce::String& n);
}
