#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Choices.h"
#include "FilterTypes.h"

namespace cureum
{

constexpr int kNumOscillators = 3;
constexpr int kNumFilters     = 2;
constexpr int kNumEnvelopes   = 4;
constexpr int kNumLfos        = 10;
constexpr int kNumMacros      = 8;
constexpr int kNumModSlots    = 64;
constexpr int kMaxVoices      = 32;
constexpr int kMaxUnison      = 16;

//==============================================================================
// Every parameter has a fixed integer index. Modules are laid out one after another so the
// index can be computed at compile time: pid::osc (1, pid::Osc::level) etc.
namespace pid
{
    enum class Osc : int
    {
        enable, level, pan, octave, semi, fine, coarse, wtPos,
        warp1Mode, warp1Amt, warp2Mode, warp2Amt,
        unison, detune, blend, width, uniMode, uniStack, uniWarp, uniWtPos,
        phase, randPhase, keyTrack,
        count
    };
    enum class Sub : int { enable, level, pan, octave, coarse, shape, phase, keyTrack, count };
    enum class Noise : int { enable, level, pan, type, pitch, fine, phase, randPhase, oneShot, keyTrack, stereo, count };
    enum class Route : int { oscA, oscB, oscC, noise, sub, filterConfig, count };
    enum class Filter : int { enable, type, cutoff, reso, drive, var, mix, stereo, level, keyTrack, count };
    enum class Env : int { attack, hold, decay, sustain, release, aCurve, dCurve, rCurve, count };
    enum class Lfo : int { type, mode, sync, rate, syncRate, syncFeel, rise, delay, smooth, phase, count };
    enum class Global : int
    {
        master, polyphony, mono, legato, portaTime, portaCurve, portaAlways, portaScaled,
        bendUp, bendDown, tune, transpose, quality, velToAmp, mpeEnable, mpeBendRange,
        count
    };
    enum class Hyper : int { enable, mix, rate, detune, unison, retrig, dimSize, dimMix, count };
    enum class Dist : int { enable, mix, mode, drive, filterPos, filterType, freq, bw, count };
    enum class Flanger : int { enable, mix, rate, sync, syncRate, depth, feedback, stereo, count };
    enum class Phaser : int { enable, mix, rate, sync, syncRate, depth, freq, feedback, stereo, stages, count };
    enum class Chorus : int { enable, mix, rate, sync, syncRate, delay1, delay2, depth, feedback, lpf, count };
    enum class Delay : int { enable, mix, mode, sync, timeL, timeR, syncL, syncR, link, feedback, freq, bw, count };
    enum class Comp : int { enable, mix, threshold, ratio, attack, release, gain, multiband, lowGain, midGain, highGain, count };
    enum class Reverb : int { enable, mix, mode, size, preDelay, lowCut, damp, highCut, width, count };
    enum class Eq : int { enable, lowType, lowFreq, lowQ, lowGain, highType, highFreq, highQ, highGain, count };
    enum class FxFilter : int { enable, mix, type, cutoff, reso, drive, var, stereo, count };
    enum class Bode : int { enable, mix, shift, feedback, spread, count };
    enum class Util : int { enable, gain, pan, width, monoBass, monoFreq, invertL, invertR, count };
    enum class Conv : int { enable, mix, ir, preDelay, lowCut, highCut, gain, count };
    enum class Plugin : int { enable, mix, count };
    enum class Mod : int { amount, count };

    template <typename E> constexpr int n() { return (int) E::count; }

    constexpr int oscBase     = 0;
    constexpr int subBase     = oscBase + kNumOscillators * n<Osc>();
    constexpr int noiseBase   = subBase + n<Sub>();
    constexpr int routeBase   = noiseBase + n<Noise>();
    constexpr int filterBase  = routeBase + n<Route>();
    constexpr int envBase     = filterBase + kNumFilters * n<Filter>();
    constexpr int lfoBase     = envBase + kNumEnvelopes * n<Env>();
    constexpr int macroBase   = lfoBase + kNumLfos * n<Lfo>();
    constexpr int globalBase  = macroBase + kNumMacros;
    constexpr int hyperBase   = globalBase + n<Global>();
    constexpr int distBase    = hyperBase + n<Hyper>();
    constexpr int flangerBase = distBase + n<Dist>();
    constexpr int phaserBase  = flangerBase + n<Flanger>();
    constexpr int chorusBase  = phaserBase + n<Phaser>();
    constexpr int delayBase   = chorusBase + n<Chorus>();
    constexpr int compBase    = delayBase + n<Delay>();
    constexpr int reverbBase  = compBase + n<Comp>();
    constexpr int eqBase      = reverbBase + n<Reverb>();
    constexpr int fxFiltBase  = eqBase + n<Eq>();
    constexpr int bodeBase    = fxFiltBase + n<FxFilter>();
    constexpr int utilBase    = bodeBase + n<Bode>();
    constexpr int convBase    = utilBase + n<Util>();
    constexpr int pluginBase  = convBase + n<Conv>();
    constexpr int modBase     = pluginBase + 4 * n<Plugin>();
    constexpr int total       = modBase + kNumModSlots * n<Mod>();

    constexpr int osc (int o, Osc p)         { return oscBase + o * n<Osc>() + (int) p; }
    constexpr int sub (Sub p)                { return subBase + (int) p; }
    constexpr int noise (Noise p)            { return noiseBase + (int) p; }
    constexpr int route (Route p)            { return routeBase + (int) p; }
    constexpr int filter (int f, Filter p)   { return filterBase + f * n<Filter>() + (int) p; }
    constexpr int env (int e, Env p)         { return envBase + e * n<Env>() + (int) p; }
    constexpr int lfo (int l, Lfo p)         { return lfoBase + l * n<Lfo>() + (int) p; }
    constexpr int macro (int m)              { return macroBase + m; }
    constexpr int global (Global p)          { return globalBase + (int) p; }
    constexpr int hyper (Hyper p)            { return hyperBase + (int) p; }
    constexpr int dist (Dist p)              { return distBase + (int) p; }
    constexpr int flanger (Flanger p)        { return flangerBase + (int) p; }
    constexpr int phaser (Phaser p)          { return phaserBase + (int) p; }
    constexpr int chorus (Chorus p)          { return chorusBase + (int) p; }
    constexpr int delay (Delay p)            { return delayBase + (int) p; }
    constexpr int comp (Comp p)              { return compBase + (int) p; }
    constexpr int reverb (Reverb p)          { return reverbBase + (int) p; }
    constexpr int eq (Eq p)                  { return eqBase + (int) p; }
    constexpr int fxFilter (FxFilter p)      { return fxFiltBase + (int) p; }
    constexpr int bode (Bode p)              { return bodeBase + (int) p; }
    constexpr int util (Util p)              { return utilBase + (int) p; }
    constexpr int conv (Conv p)              { return convBase + (int) p; }
    constexpr int plugin (int slot, Plugin p){ return pluginBase + slot * n<Plugin>() + (int) p; }
    constexpr int mod (int slot)             { return modBase + slot; }

    // The "enable" parameter of an effect unit.
    int fxEnable (FxType t);
    int fxMix (FxType t); // -1 if the unit has no mix parameter
}

//==============================================================================
enum class ParamKind { continuous, integer, choice, toggle };

enum class ParamUnit
{
    none, percent, bipolarPercent, decibel, gainAsDecibel, hertz, seconds, milliseconds,
    semitones, cents, degrees, octaves, voices, pan, ratio, frames, multiplier
};

// Whether a parameter can be a modulation-matrix destination and where it is evaluated.
enum class ModScope { none, voice, global };

struct ParamSpec
{
    int index = -1;
    juce::String id;          // stable string ID used by hosts and presets
    juce::String name;        // full display name, e.g. "Osc A Level"
    juce::String shortName;   // knob label, e.g. "LEVEL"
    juce::String group;       // module name, e.g. "Osc A"
    ParamKind kind = ParamKind::continuous;
    ParamUnit unit = ParamUnit::none;
    juce::NormalisableRange<float> range;
    float defaultValue = 0.0f;
    juce::StringArray choices;
    ModScope modScope = ModScope::none;
    juce::String help;        // contextual help shown in the status bar and help panel

    float toNormalised (float plain) const   { return range.convertTo0to1 (range.snapToLegalValue (plain)); }
    float fromNormalised (float norm) const  { return range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm)); }
    juce::String formatValue (float plain, int maxLen = 0) const;
    bool isModulatable() const { return modScope != ModScope::none; }
};

class ParamRegistry
{
public:
    static const ParamRegistry& get();

    const std::vector<ParamSpec>& all() const noexcept { return specs; }
    const ParamSpec& spec (int index) const noexcept   { return specs[(size_t) index]; }
    const ParamSpec* find (const juce::String& id) const;
    int indexOf (const juce::String& id) const;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout() const;

    // Destinations grouped for menus (module name -> parameter indices)
    std::vector<std::pair<juce::String, std::vector<int>>> modDestinationGroups() const;

private:
    ParamRegistry();
    std::vector<ParamSpec> specs;
    std::unordered_map<juce::String, int> idToIndex;
};

// Convenience: module names used as ID prefixes.
juce::String oscLetter (int osc);           // "A", "B", "C"
juce::String oscIdPrefix (int osc);         // "oscA"

} // namespace cureum
