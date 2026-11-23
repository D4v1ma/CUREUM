#pragma once

#include <juce_core/juce_core.h>

namespace cureum
{

enum class ModSource : int
{
    none = 0,
    env1, env2, env3, env4,
    lfo1, lfo2, lfo3, lfo4, lfo5, lfo6, lfo7, lfo8, lfo9, lfo10,
    velocity, note, modWheel, pitchBend, aftertouch, polyAftertouch,
    random1, random2,
    macro1, macro2, macro3, macro4, macro5, macro6, macro7, macro8,
    constant, gate, releaseVelocity, mpeSlide, mpePressure,
    count
};

constexpr int kNumModSources = (int) ModSource::count;

inline const juce::StringArray& modSourceNames()
{
    static const juce::StringArray n {
        "None",
        "Env 1", "Env 2", "Env 3", "Env 4",
        "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8", "LFO 9", "LFO 10",
        "Velocity", "Note", "Mod Wheel", "Pitch Bend", "Aftertouch", "Poly AT",
        "Random 1", "Random 2",
        "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6", "Macro 7", "Macro 8",
        "Constant", "Gate", "Release Vel", "MPE Slide", "MPE Pressure"
    };
    return n;
}

inline const juce::StringArray& modSourceKeys()  // stable identifiers stored in presets
{
    static const juce::StringArray n {
        "none",
        "env1", "env2", "env3", "env4",
        "lfo1", "lfo2", "lfo3", "lfo4", "lfo5", "lfo6", "lfo7", "lfo8", "lfo9", "lfo10",
        "velocity", "note", "modwheel", "pitchbend", "aftertouch", "polyat",
        "random1", "random2",
        "macro1", "macro2", "macro3", "macro4", "macro5", "macro6", "macro7", "macro8",
        "constant", "gate", "releasevel", "mpeslide", "mpepressure"
    };
    return n;
}

inline ModSource modSourceFromKey (const juce::String& key)
{
    const int i = modSourceKeys().indexOf (key);
    return i >= 0 ? (ModSource) i : ModSource::none;
}

inline ModSource envSource (int e)   { return (ModSource) ((int) ModSource::env1 + e); }
inline ModSource lfoSource (int l)   { return (ModSource) ((int) ModSource::lfo1 + l); }
inline ModSource macroSource (int m) { return (ModSource) ((int) ModSource::macro1 + m); }

// Sources whose natural range is already bipolar (-1..1).
inline bool isNaturallyBipolar (ModSource s) { return s == ModSource::pitchBend || s == ModSource::random2; }

inline juce::String modSourceDescription (ModSource s)
{
    switch (s)
    {
        case ModSource::velocity:        return "Note-on velocity (0..1).";
        case ModSource::note:            return "Played note number (0 = C-2 .. 1 = G8). Use bipolar mode for key tracking around the middle.";
        case ModSource::modWheel:        return "MIDI mod wheel (CC 1).";
        case ModSource::pitchBend:       return "Pitch-bend wheel (-1..+1).";
        case ModSource::aftertouch:      return "Channel aftertouch (pressure).";
        case ModSource::polyAftertouch:  return "Polyphonic (per-note) aftertouch.";
        case ModSource::random1:         return "A new random value (0..1) for every note.";
        case ModSource::random2:         return "A new bipolar random value (-1..+1) for every note.";
        case ModSource::constant:        return "Always 1. Use it to offset a destination via the matrix.";
        case ModSource::gate:            return "1 while the key is held, 0 after release.";
        case ModSource::releaseVelocity: return "Note-off velocity.";
        case ModSource::mpeSlide:        return "MPE slide (CC 74 per note).";
        case ModSource::mpePressure:     return "MPE per-note pressure.";
        default: break;
    }
    const int i = (int) s;
    if (i >= (int) ModSource::env1 && i <= (int) ModSource::env4)    return "Envelope " + juce::String (i - (int) ModSource::env1 + 1) + ".";
    if (i >= (int) ModSource::lfo1 && i <= (int) ModSource::lfo10)   return "LFO " + juce::String (i - (int) ModSource::lfo1 + 1) + ".";
    if (i >= (int) ModSource::macro1 && i <= (int) ModSource::macro8) return "Macro knob " + juce::String (i - (int) ModSource::macro1 + 1) + ".";
    return {};
}

} // namespace cureum
