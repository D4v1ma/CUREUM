#pragma once

#include "Plugin/PluginProcessor.h"
#include "Presets/SerumImport.h"
#include "Presets/PresetManager.h"
#include <iostream>

namespace cureumtest
{
    using namespace cureum;

    // Renders `seconds` of audio from a processor while playing the given notes.
    struct RenderResult { float peak = 0.0f, rms = 0.0f; bool finite = true; juce::AudioBuffer<float> audio; };
    RenderResult renderProcessor (CureumProcessor& p, double seconds, std::vector<int> notes, double sampleRate = 48000.0, int block = 256);

    // Builds synthetic Serum files (so tests do not depend on third-party preset files).
    juce::MemoryBlock makeSerum2Preset (const juce::var& data, const juce::String& name);
    juce::MemoryBlock makeSerum1Fxp (const std::vector<float>& params, const juce::String& name,
                                     std::vector<std::array<int, 3>> matrix /* source, dest, amount% */);

    int runImport (const juce::File& file);
    int runRender (const juce::String& preset, const juce::File& out);
    int runScreenshot (const juce::File& out, const juce::String& tab, float scale = 1.0f, const juce::String& preset = {});
    int runValidate (const juce::File& pluginFile);   // host-side checks of a built VST3/AU
}
