#include "Params.h"
#include "ValueMaps.h"

namespace cureum
{

juce::String oscLetter (int osc)    { return juce::String::charToString ((juce::juce_wchar) ('A' + osc)); }
juce::String oscIdPrefix (int osc)  { return "osc" + oscLetter (osc); }

int pid::fxEnable (FxType t)
{
    switch (t)
    {
        case FxType::hyper:      return hyper (Hyper::enable);
        case FxType::distortion: return dist (Dist::enable);
        case FxType::flanger:    return flanger (Flanger::enable);
        case FxType::phaser:     return phaser (Phaser::enable);
        case FxType::chorus:     return chorus (Chorus::enable);
        case FxType::delay:      return delay (Delay::enable);
        case FxType::compressor: return comp (Comp::enable);
        case FxType::reverb:     return reverb (Reverb::enable);
        case FxType::eq:         return eq (Eq::enable);
        case FxType::filter:     return fxFilter (FxFilter::enable);
        case FxType::bode:       return bode (Bode::enable);
        case FxType::utility:    return util (Util::enable);
        case FxType::convolve:   return conv (Conv::enable);
        case FxType::plugin1:    return plugin (0, Plugin::enable);
        case FxType::plugin2:    return plugin (1, Plugin::enable);
        case FxType::plugin3:    return plugin (2, Plugin::enable);
        case FxType::plugin4:    return plugin (3, Plugin::enable);
        case FxType::count:      break;
    }
    return -1;
}

int pid::fxMix (FxType t)
{
    switch (t)
    {
        case FxType::hyper:      return hyper (Hyper::mix);
        case FxType::distortion: return dist (Dist::mix);
        case FxType::flanger:    return flanger (Flanger::mix);
        case FxType::phaser:     return phaser (Phaser::mix);
        case FxType::chorus:     return chorus (Chorus::mix);
        case FxType::delay:      return delay (Delay::mix);
        case FxType::compressor: return comp (Comp::mix);
        case FxType::reverb:     return reverb (Reverb::mix);
        case FxType::filter:     return fxFilter (FxFilter::mix);
        case FxType::bode:       return bode (Bode::mix);
        case FxType::convolve:   return conv (Conv::mix);
        case FxType::plugin1:    return plugin (0, Plugin::mix);
        case FxType::plugin2:    return plugin (1, Plugin::mix);
        case FxType::plugin3:    return plugin (2, Plugin::mix);
        case FxType::plugin4:    return plugin (3, Plugin::mix);
        case FxType::eq:
        case FxType::utility:
        case FxType::count:      break;
    }
    return -1;
}

//==============================================================================
static juce::String fmt (float v, int decimals)
{
    return decimals <= 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals);
}

static juce::String formatHz (float hz)
{
    if (hz >= 1000.0f)
        return fmt (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
    return fmt (hz, hz < 10.0f ? 2 : (hz < 100.0f ? 1 : 0)) + " Hz";
}

static juce::String formatSeconds (float s)
{
    if (s < 0.001f)  return juce::String (s * 1000.0f, 2) + " ms";
    if (s < 0.1f)    return juce::String (s * 1000.0f, 1) + " ms";
    if (s < 1.0f)    return fmt (s * 1000.0f, 0) + " ms";
    return juce::String (s, 2) + " s";
}

juce::String ParamSpec::formatValue (float v, int) const
{
    switch (kind)
    {
        case ParamKind::toggle: return v >= 0.5f ? "On" : "Off";
        case ParamKind::choice:
        {
            auto i = juce::jlimit (0, juce::jmax (0, choices.size() - 1), juce::roundToInt (v));
            return choices[i];
        }
        case ParamKind::integer:
        case ParamKind::continuous: break;
    }

    switch (unit)
    {
        case ParamUnit::percent:        return juce::String (v * 100.0f, 1) + "%";
        case ParamUnit::bipolarPercent: return (v > 0.0005f ? "+" : "") + juce::String (v * 100.0f, 1) + "%";
        case ParamUnit::decibel:        return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
        case ParamUnit::gainAsDecibel:
        {
            auto amp = v * v;
            if (amp <= 0.00001f) return "-inf dB";
            return juce::String (juce::Decibels::gainToDecibels (amp), 1) + " dB";
        }
        case ParamUnit::hertz:          return formatHz (v);
        case ParamUnit::seconds:        return formatSeconds (v);
        case ParamUnit::milliseconds:   return formatSeconds (v / 1000.0f);
        case ParamUnit::semitones:
            if (kind == ParamKind::integer) return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " st";
            return (v > 0.005f ? "+" : "") + juce::String (v, 2) + " st";
        case ParamUnit::cents:          return (v > 0.05f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct";
        case ParamUnit::degrees:        return juce::String (juce::roundToInt (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
        case ParamUnit::octaves:        return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v));
        case ParamUnit::voices:         return juce::String (juce::roundToInt (v));
        case ParamUnit::pan:
        {
            auto p = juce::roundToInt (v * 100.0f);
            if (p == 0) return "C";
            return (p < 0 ? "L " : "R ") + juce::String (std::abs (p));
        }
        case ParamUnit::ratio:          return fmt (v, v < 10.0f ? 1 : 0) + ":1";
        case ParamUnit::frames:         return juce::String (v * 100.0f, 1) + "%";
        case ParamUnit::multiplier:     return "x" + juce::String (v, 2);
        case ParamUnit::none:           break;
    }

    if (kind == ParamKind::integer)
        return juce::String (juce::roundToInt (v));
    return juce::String (v, 2);
}

//==============================================================================
namespace
{
    using NR = juce::NormalisableRange<float>;

    NR linear (float lo, float hi, float step = 0.0f) { return NR (lo, hi, step); }

    NR skewed (float lo, float hi, float centre)
    {
        NR r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    NR mapped (float lo, float hi, float (*from01) (float), float (*to01) (float))
    {
        return NR (lo, hi,
                   [from01] (float, float, float x) { return from01 (x); },
                   [to01] (float, float, float v) { return to01 (v); },
                   [] (float s, float e, float v) { return juce::jlimit (s, e, v); });
    }

    NR envTimeRange()  { return mapped (0.0f, maps::kEnvMaxSeconds, maps::envTimeFrom01, maps::envTimeTo01); }
    NR cutoffRange()   { return mapped (maps::kCutoffMinHz, maps::kCutoffMaxHz, maps::cutoffFrom01, maps::cutoffTo01); }
    NR lfoHzRange()    { return mapped (maps::kLfoMinHz, maps::kLfoMaxHz, maps::lfoHzFrom01, maps::lfoHzTo01); }
    NR bodeRange()     { return mapped (-maps::kBodeMaxHz, maps::kBodeMaxHz, maps::bodeFrom01, maps::bodeTo01); }
    NR choiceRange (const juce::StringArray& c) { return NR (0.0f, (float) juce::jmax (1, c.size() - 1), 1.0f); }

    struct Builder
    {
        std::vector<ParamSpec>& specs;

        ParamSpec& add (int expectedIndex, juce::String id, juce::String name, juce::String shortName, juce::String group,
                        ParamKind kind, ParamUnit unit, NR range, float def, ModScope scope, juce::String help,
                        juce::StringArray choices = {})
        {
            jassert ((int) specs.size() == expectedIndex);
            juce::ignoreUnused (expectedIndex);
            ParamSpec s;
            s.index = (int) specs.size();
            s.id = std::move (id);
            s.name = std::move (name);
            s.shortName = std::move (shortName);
            s.group = std::move (group);
            s.kind = kind;
            s.unit = unit;
            s.range = range;
            s.defaultValue = def;
            s.modScope = scope;
            s.help = std::move (help);
            s.choices = std::move (choices);
            specs.push_back (std::move (s));
            return specs.back();
        }

        ParamSpec& cont (int idx, const juce::String& id, const juce::String& name, const juce::String& sn, const juce::String& g,
                         ParamUnit u, NR r, float def, ModScope sc, const juce::String& help)
        {
            return add (idx, id, name, sn, g, ParamKind::continuous, u, r, def, sc, help);
        }

        ParamSpec& integer (int idx, const juce::String& id, const juce::String& name, const juce::String& sn, const juce::String& g,
                            ParamUnit u, float lo, float hi, float def, ModScope sc, const juce::String& help)
        {
            return add (idx, id, name, sn, g, ParamKind::integer, u, NR (lo, hi, 1.0f), def, sc, help);
        }

        ParamSpec& choice (int idx, const juce::String& id, const juce::String& name, const juce::String& sn, const juce::String& g,
                           const juce::StringArray& c, int def, const juce::String& help, ModScope sc = ModScope::none)
        {
            return add (idx, id, name, sn, g, ParamKind::choice, ParamUnit::none, choiceRange (c), (float) def, sc, help, c);
        }

        ParamSpec& toggle (int idx, const juce::String& id, const juce::String& name, const juce::String& sn, const juce::String& g,
                           bool def, const juce::String& help)
        {
            return add (idx, id, name, sn, g, ParamKind::toggle, ParamUnit::none, NR (0.0f, 1.0f, 1.0f), def ? 1.0f : 0.0f,
                        ModScope::none, help, juce::StringArray { "Off", "On" });
        }
    };
}

//==============================================================================
ParamRegistry::ParamRegistry()
{
    using namespace pid;
    specs.reserve ((size_t) pid::total);
    Builder b { specs };
    const auto V = ModScope::voice;
    const auto G = ModScope::global;
    const auto N = ModScope::none;

    //------------------------------------------------------------------ Oscillators
    for (int o = 0; o < kNumOscillators; ++o)
    {
        const auto p = oscIdPrefix (o) + "_";
        const auto g = "Osc " + oscLetter (o);
        const auto n = g + " ";
        b.toggle (osc (o, Osc::enable), p + "enable", n + "On", "ON", g, o == 0,
                  "Turns oscillator " + oscLetter (o) + " on or off. A disabled oscillator uses no CPU.");
        b.cont (osc (o, Osc::level), p + "level", n + "Level", "LEVEL", g, ParamUnit::percent, linear (0, 1), 0.75f, V,
                "Output level of the oscillator. The knob is volume-tapered: 75% is about -5 dB.");
        b.cont (osc (o, Osc::pan), p + "pan", n + "Pan", "PAN", g, ParamUnit::pan, linear (-1, 1), 0.0f, V,
                "Stereo position of the oscillator before it reaches the filters.");
        b.integer (osc (o, Osc::octave), p + "octave", n + "Octave", "OCT", g, ParamUnit::octaves, -4, 4, 0, V,
                   "Transposes the oscillator in octaves.");
        b.integer (osc (o, Osc::semi), p + "semi", n + "Semitone", "SEM", g, ParamUnit::semitones, -12, 12, 0, V,
                   "Transposes the oscillator in semitones.");
        b.cont (osc (o, Osc::fine), p + "fine", n + "Fine", "FIN", g, ParamUnit::cents, linear (-100, 100), 0.0f, V,
                "Fine tuning in cents (1/100 of a semitone). Small offsets between oscillators thicken the sound.");
        b.cont (osc (o, Osc::coarse), p + "coarse", n + "Coarse Pitch", "CRS", g, ParamUnit::semitones, linear (-48, 48), 0.0f, V,
                "Continuous pitch offset in semitones. Ideal as a modulation target for pitch sweeps and risers.");
        b.cont (osc (o, Osc::wtPos), p + "wtpos", n + "WT Position", "WT POS", g, ParamUnit::frames, linear (0, 1), 0.0f, V,
                "Scans through the frames of the wavetable. Modulate it with an envelope or LFO for evolving timbres.");
        b.choice (osc (o, Osc::warp1Mode), p + "warp1mode", n + "Warp 1 Mode", "WARP 1", g, warpModeNames(), 0,
                  "First warp mode: bends, syncs, folds or cross-modulates the waveform. See Help > Oscillators > Warp.");
        b.cont (osc (o, Osc::warp1Amt), p + "warp1", n + "Warp 1 Amount", "WARP", g, ParamUnit::percent, linear (0, 1), 0.0f, V,
                "Amount of the first warp mode.");
        b.choice (osc (o, Osc::warp2Mode), p + "warp2mode", n + "Warp 2 Mode", "WARP 2", g, warpModeNames(), 0,
                  "Second warp mode, applied after the first one.");
        b.cont (osc (o, Osc::warp2Amt), p + "warp2", n + "Warp 2 Amount", "WARP 2", g, ParamUnit::percent, linear (0, 1), 0.0f, V,
                "Amount of the second warp mode.");
        b.integer (osc (o, Osc::unison), p + "unison", n + "Unison Voices", "UNISON", g, ParamUnit::voices, 1, kMaxUnison, 1, N,
                   "Number of stacked, detuned copies of the oscillator (1-16). More voices = thicker sound and more CPU.");
        b.cont (osc (o, Osc::detune), p + "detune", n + "Unison Detune", "DETUNE", g, ParamUnit::percent, linear (0, 1), 0.5f, V,
                "Pitch spread between unison voices. The knob is squared, so small values give subtle chorusing.");
        b.cont (osc (o, Osc::blend), p + "blend", n + "Unison Blend", "BLEND", g, ParamUnit::percent, linear (0, 1), 0.75f, V,
                "Balance between the centre voice and the detuned outer voices. 75% makes all voices equally loud.");
        b.cont (osc (o, Osc::width), p + "width", n + "Unison Width", "WIDTH", g, ParamUnit::percent, linear (0, 1), 1.0f, V,
                "Stereo spread of the unison voices.");
        b.choice (osc (o, Osc::uniMode), p + "unimode", n + "Unison Mode", "MODE", g, unisonModeNames(), 0,
                  "How detuned voices are distributed: Linear (even), Super (clustered like classic supersaws), Exp, Inverse or Random.");
        b.choice (osc (o, Osc::uniStack), p + "unistack", n + "Unison Stack", "STACK", g, unisonStackNames(), 0,
                  "Stacks unison voices at octave or fifth intervals for huge chords from a single note.");
        b.cont (osc (o, Osc::uniWarp), p + "uniwarp", n + "Unison Warp Spread", "UNI WARP", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.0f, V,
                "Offsets the warp amount differently for each unison voice.");
        b.cont (osc (o, Osc::uniWtPos), p + "uniwtpos", n + "Unison WT Spread", "UNI WT", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.0f, V,
                "Offsets the wavetable position differently for each unison voice.");
        b.cont (osc (o, Osc::phase), p + "phase", n + "Phase", "PHASE", g, ParamUnit::degrees, linear (0, 360), 180.0f, V,
                "Start phase of the waveform when a note begins.");
        b.cont (osc (o, Osc::randPhase), p + "randphase", n + "Random Phase", "RAND", g, ParamUnit::percent, linear (0, 1), 1.0f, N,
                "Randomises the start phase per note. 100% sounds analog and wide, 0% gives a consistent attack.");
        b.toggle (osc (o, Osc::keyTrack), p + "keytrack", n + "Key Track", "KEY", g, true,
                  "When off, the oscillator ignores the played note and stays at a fixed pitch (C3), useful for drones and FX.");
    }

    //------------------------------------------------------------------ Sub
    {
        const juce::String g = "Sub";
        b.toggle (sub (Sub::enable), "sub_enable", "Sub On", "ON", g, false, "Turns the sub oscillator on or off.");
        b.cont (sub (Sub::level), "sub_level", "Sub Level", "LEVEL", g, ParamUnit::percent, linear (0, 1), 0.75f, V, "Level of the sub oscillator.");
        b.cont (sub (Sub::pan), "sub_pan", "Sub Pan", "PAN", g, ParamUnit::pan, linear (-1, 1), 0.0f, V, "Stereo position of the sub oscillator.");
        b.integer (sub (Sub::octave), "sub_octave", "Sub Octave", "OCT", g, ParamUnit::octaves, -4, 4, 0, V, "Octave of the sub oscillator relative to the played note.");
        b.integer (sub (Sub::coarse), "sub_coarse", "Sub Semitone", "SEM", g, ParamUnit::semitones, -12, 12, 0, V, "Semitone offset of the sub oscillator.");
        b.choice (sub (Sub::shape), "sub_shape", "Sub Shape", "SHAPE", g, subShapeNames(), 0, "Waveform of the sub oscillator.");
        b.cont (sub (Sub::phase), "sub_phase", "Sub Phase", "PHASE", g, ParamUnit::degrees, linear (0, 360), 0.0f, N, "Start phase of the sub oscillator (always restarted on note-on).");
        b.toggle (sub (Sub::keyTrack), "sub_keytrack", "Sub Key Track", "KEY", g, true, "When off the sub stays at a fixed pitch.");
    }

    //------------------------------------------------------------------ Noise
    {
        const juce::String g = "Noise";
        b.toggle (noise (Noise::enable), "noise_enable", "Noise On", "ON", g, false, "Turns the noise oscillator on or off.");
        b.cont (noise (Noise::level), "noise_level", "Noise Level", "LEVEL", g, ParamUnit::percent, linear (0, 1), 0.25f, V, "Level of the noise oscillator.");
        b.cont (noise (Noise::pan), "noise_pan", "Noise Pan", "PAN", g, ParamUnit::pan, linear (-1, 1), 0.0f, V, "Stereo position of the noise oscillator.");
        b.choice (noise (Noise::type), "noise_type", "Noise Type", "TYPE", g, noiseTypeNames(), 0,
                  "The noise source: coloured noises, textures, hums and one-shot transients, or a custom sample you load.");
        b.cont (noise (Noise::pitch), "noise_pitch", "Noise Pitch", "PITCH", g, ParamUnit::percent, linear (0, 1), 0.5f, V,
                "Playback rate of the noise sample. 50% plays it at its original speed; lower is darker, higher is brighter.");
        b.cont (noise (Noise::fine), "noise_fine", "Noise Fine", "FINE", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.0f, V, "Fine adjustment of the noise playback rate.");
        b.cont (noise (Noise::phase), "noise_phase", "Noise Start", "START", g, ParamUnit::percent, linear (0, 1), 0.0f, N, "Start position inside the noise sample.");
        b.cont (noise (Noise::randPhase), "noise_randphase", "Noise Random Start", "RAND", g, ParamUnit::percent, linear (0, 1), 1.0f, N, "Randomises the start position per note.");
        b.toggle (noise (Noise::oneShot), "noise_oneshot", "Noise One-Shot", "1-SHOT", g, false, "Plays the noise sample once instead of looping it. Great for transients.");
        b.toggle (noise (Noise::keyTrack), "noise_keytrack", "Noise Key Track", "KEY", g, false, "When on, the noise playback rate follows the played note.");
        b.toggle (noise (Noise::stereo), "noise_stereo", "Noise Stereo", "STEREO", g, true, "Uses decorrelated left/right noise for a wide stereo image.");
    }

    //------------------------------------------------------------------ Routing
    {
        const juce::String g = "Routing";
        const juce::String srcNames[] = { "Osc A", "Osc B", "Osc C", "Noise", "Sub" };
        const juce::String ids[] = { "route_oscA", "route_oscB", "route_oscC", "route_noise", "route_sub" };
        for (int i = 0; i < 5; ++i)
            b.choice (route ((Route) i), ids[i], srcNames[i] + " Routing", "ROUTE", g, routeDestNames(), 0,
                      "Where " + srcNames[i] + " is sent: Filter 1, Filter 2, both filters, or Direct (bypassing the filters).");
        b.choice (route (Route::filterConfig), "route_filterconfig", "Filter Configuration", "CONFIG", g, filterConfigNames(), 0,
                  "Serial: Filter 1 feeds Filter 2. Parallel: both filters run side by side and are mixed.");
    }

    //------------------------------------------------------------------ Filters
    for (int f = 0; f < kNumFilters; ++f)
    {
        const auto p = "filt" + juce::String (f + 1) + "_";
        const auto g = "Filter " + juce::String (f + 1);
        const auto n = g + " ";
        b.toggle (filter (f, Filter::enable), p + "enable", n + "On", "ON", g, false, "Turns the filter on or off.");
        b.choice (filter (f, Filter::type), p + "type", n + "Type", "TYPE", g, filterTypeNames(), kDefaultFilterType,
                  "Filter type. Right-click the display or use the menu to browse all categories (Normal, Multi, Flanges, Misc).");
        b.cont (filter (f, Filter::cutoff), p + "cutoff", n + "Cutoff", "CUTOFF", g, ParamUnit::hertz, cutoffRange(), maps::cutoffFrom01 (0.5f), V,
                "Cutoff frequency (or tuning frequency for comb/flange types). The most common modulation target.");
        b.cont (filter (f, Filter::reso), p + "reso", n + "Resonance", "RES", g, ParamUnit::percent, linear (0, 1), 0.1f, V,
                "Emphasis around the cutoff. High values self-oscillate on many types.");
        b.cont (filter (f, Filter::drive), p + "drive", n + "Drive", "DRIVE", g, ParamUnit::percent, linear (0, 1), 0.0f, V,
                "Input gain into the filter's saturation stage.");
        b.cont (filter (f, Filter::var), p + "var", n + "Var", "VAR", g, ParamUnit::percent, linear (0, 1), 0.0f, V,
                "Type-specific control: Fat (bass compensation), Morph (multi types), Damp, Spread, Vowel, Gain...");
        b.cont (filter (f, Filter::mix), p + "mix", n + "Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 1.0f, V, "Dry/wet balance of the filter.");
        b.cont (filter (f, Filter::stereo), p + "stereo", n + "Stereo", "STEREO", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.0f, V,
                "Offsets the cutoff of the left and right channels in opposite directions for stereo movement.");
        b.cont (filter (f, Filter::level), p + "level", n + "Level", "LEVEL", g, ParamUnit::percent, linear (0, 2), 1.0f, V, "Output level of the filter.");
        b.cont (filter (f, Filter::keyTrack), p + "keytrack", n + "Key Track", "KEYTRK", g, ParamUnit::percent, linear (0, 1), 0.0f, V,
                "How much the cutoff follows the played note. 100% tracks one octave per octave (relative to C3).");
    }

    //------------------------------------------------------------------ Envelopes
    for (int e = 0; e < kNumEnvelopes; ++e)
    {
        const auto p = "env" + juce::String (e + 1) + "_";
        const auto g = "Env " + juce::String (e + 1);
        const auto n = g + " ";
        const juce::String role = e == 0 ? " Env 1 always controls the amplitude of every voice." : "";
        b.cont (env (e, Env::attack), p + "attack", n + "Attack", "ATTACK", g, ParamUnit::seconds, envTimeRange(), maps::envTimeFrom01 (0.11f), V,
                "Time to rise from zero to full level after a note starts." + role);
        b.cont (env (e, Env::hold), p + "hold", n + "Hold", "HOLD", g, ParamUnit::seconds, envTimeRange(), 0.0f, V, "Time the envelope stays at full level before decaying.");
        b.cont (env (e, Env::decay), p + "decay", n + "Decay", "DECAY", g, ParamUnit::seconds, envTimeRange(), maps::envTimeFrom01 (0.5f), V, "Time to fall from full level to the sustain level.");
        b.cont (env (e, Env::sustain), p + "sustain", n + "Sustain", "SUSTAIN", g, e == 0 ? ParamUnit::gainAsDecibel : ParamUnit::percent, linear (0, 1), 1.0f, V,
                "Level held while the key is down.");
        b.cont (env (e, Env::release), p + "release", n + "Release", "RELEASE", g, ParamUnit::seconds, envTimeRange(), maps::envTimeFrom01 (0.215f), V, "Time to fall to zero after the key is released.");
        b.cont (env (e, Env::aCurve), p + "acurve", n + "Attack Curve", "A CURVE", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.0f, N, "Shape of the attack segment. 0 is linear; drag the curve handles in the display.");
        b.cont (env (e, Env::dCurve), p + "dcurve", n + "Decay Curve", "D CURVE", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.34f, N, "Shape of the decay segment. Positive values fall quickly then slowly (exponential).");
        b.cont (env (e, Env::rCurve), p + "rcurve", n + "Release Curve", "R CURVE", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.34f, N, "Shape of the release segment.");
    }

    //------------------------------------------------------------------ LFOs
    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto p = "lfo" + juce::String (l + 1) + "_";
        const auto g = "LFO " + juce::String (l + 1);
        const auto n = g + " ";
        b.choice (lfo (l, Lfo::type), p + "type", n + "Type", "TYPE", g, lfoTypeNames(), 0,
                  "Shape uses the drawable curve. S&H and Smooth Rnd produce random values. Lorenz and Rossler are chaotic attractors.");
        b.choice (lfo (l, Lfo::mode), p + "mode", n + "Mode", "MODE", g, lfoModeNames(), 0,
                  "Trig restarts the LFO with every note. Env plays the shape once (like an envelope). Free runs continuously for all voices.");
        b.toggle (lfo (l, Lfo::sync), p + "sync", n + "Tempo Sync", "BPM", g, true, "Locks the LFO rate to the host tempo.");
        b.cont (lfo (l, Lfo::rate), p + "rate", n + "Rate", "RATE", g, ParamUnit::hertz, lfoHzRange(), 1.0f, V, "LFO speed in Hz when tempo sync is off.");
        b.choice (lfo (l, Lfo::syncRate), p + "syncrate", n + "Sync Rate", "RATE", g, syncRateNames(), 5, "Length of one LFO cycle when tempo sync is on.", V);
        b.choice (lfo (l, Lfo::syncFeel), p + "syncfeel", n + "Sync Feel", "FEEL", g, syncFeelNames(), 0, "Straight, dotted (x1.5 longer) or triplet (x2/3) timing.");
        b.cont (lfo (l, Lfo::rise), p + "rise", n + "Rise", "RISE", g, ParamUnit::seconds, envTimeRange(), 0.0f, V, "Fade-in time of the LFO after a note starts.");
        b.cont (lfo (l, Lfo::delay), p + "delay", n + "Delay", "DELAY", g, ParamUnit::seconds, envTimeRange(), 0.0f, V, "Waiting time before the LFO starts.");
        b.cont (lfo (l, Lfo::smooth), p + "smooth", n + "Smooth", "SMOOTH", g, ParamUnit::percent, linear (0, 1), 0.0f, V, "Slews the LFO output to round off sharp edges.");
        b.cont (lfo (l, Lfo::phase), p + "phase", n + "Phase", "PHASE", g, ParamUnit::degrees, linear (0, 360), 0.0f, V, "Start phase of the LFO cycle.");
    }

    //------------------------------------------------------------------ Macros
    for (int m = 0; m < kNumMacros; ++m)
        b.cont (macro (m), "macro" + juce::String (m + 1), "Macro " + juce::String (m + 1), "MACRO " + juce::String (m + 1), "Macros",
                ParamUnit::percent, linear (0, 1), 0.0f, N,
                "Macro knob: drag its handle onto any control to make it a performance control. Automate it from your DAW.");

    //------------------------------------------------------------------ Global / voicing
    {
        const juce::String g = "Global";
        b.cont (global (Global::master), "master", "Master Volume", "MASTER", g, ParamUnit::gainAsDecibel, linear (0, 1), 0.7f, G,
                "Final output level of CUREUM.");
        b.integer (global (Global::polyphony), "polyphony", "Polyphony", "POLY", g, ParamUnit::voices, 1, kMaxVoices, 16, N,
                   "Maximum number of notes that can sound at once.");
        b.toggle (global (Global::mono), "mono", "Mono", "MONO", g, false, "Plays one note at a time (last-note priority).");
        b.toggle (global (Global::legato), "legato", "Legato", "LEGATO", g, false, "In mono mode, overlapping notes glide without retriggering envelopes.");
        b.cont (global (Global::portaTime), "porta_time", "Portamento Time", "PORTA", g, ParamUnit::seconds, envTimeRange(), 0.0f, N,
                "Glide time between notes.");
        b.cont (global (Global::portaCurve), "porta_curve", "Portamento Curve", "CURVE", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.0f, N,
                "Shape of the glide: negative starts slowly, positive starts quickly.");
        b.toggle (global (Global::portaAlways), "porta_always", "Always Glide", "ALWAYS", g, false, "Glide even when notes do not overlap.");
        b.toggle (global (Global::portaScaled), "porta_scaled", "Scaled Glide", "SCALED", g, false, "Glide time scales with the interval (constant rate).");
        b.integer (global (Global::bendUp), "bend_up", "Pitch Bend Up", "BEND +", g, ParamUnit::semitones, 0, 48, 2, N, "Pitch-bend range upwards.");
        b.integer (global (Global::bendDown), "bend_down", "Pitch Bend Down", "BEND -", g, ParamUnit::semitones, 0, 48, 2, N, "Pitch-bend range downwards.");
        b.cont (global (Global::tune), "tune", "Master Tune", "TUNE", g, ParamUnit::cents, linear (-100, 100), 0.0f, N, "Global tuning offset in cents.");
        b.integer (global (Global::transpose), "transpose", "Transpose", "TRANS", g, ParamUnit::semitones, -24, 24, 0, N, "Global transposition.");
        b.choice (global (Global::quality), "quality", "Quality", "QUALITY", g, qualityNames(), 0,
                  "Internal oversampling of the voice engine. Higher settings reduce aliasing of warps, sync, FM and filters at the cost of CPU.");
        b.cont (global (Global::velToAmp), "vel_to_amp", "Velocity to Amp", "VEL>AMP", g, ParamUnit::percent, linear (0, 1), 0.0f, N,
                "How strongly note velocity controls loudness (like Serum, the default is 0%).");
        b.toggle (global (Global::mpeEnable), "mpe", "MPE", "MPE", g, false, "Enables MIDI Polyphonic Expression: per-note pitch bend, pressure and slide.");
        b.integer (global (Global::mpeBendRange), "mpe_bend", "MPE Bend Range", "MPE BEND", g, ParamUnit::semitones, 1, 96, 48, N, "Per-note pitch-bend range in MPE mode.");
    }

    //------------------------------------------------------------------ Effects
    const auto fx = [&] (const char* key) { return juce::String ("fx_") + key + "_"; };
    {
        const auto p = fx ("hyper"); const juce::String g = "Hyper/Dimension";
        b.toggle (hyper (Hyper::enable), p + "enable", "Hyper On", "ON", g, false, "Enables Hyper/Dimension: a unison-style chorus plus a stereo widener.");
        b.cont (hyper (Hyper::mix), p + "mix", "Hyper Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 0.5f, G, "Wet level of the Hyper section.");
        b.cont (hyper (Hyper::rate), p + "rate", "Hyper Rate", "RATE", g, ParamUnit::percent, linear (0, 1), 0.4f, G, "Speed of the detuning motion.");
        b.cont (hyper (Hyper::detune), p + "detune", "Hyper Detune", "DETUNE", g, ParamUnit::percent, linear (0, 1), 0.25f, G, "Depth of detuning between the Hyper voices.");
        b.integer (hyper (Hyper::unison), p + "unison", "Hyper Unison", "UNISON", g, ParamUnit::voices, 2, 7, 4, N, "Number of virtual voices (2-7).");
        b.toggle (hyper (Hyper::retrig), p + "retrig", "Hyper Retrig", "RETRIG", g, false, "Restarts the Hyper motion on each new note.");
        b.cont (hyper (Hyper::dimSize), p + "dimsize", "Dimension Size", "SIZE", g, ParamUnit::percent, linear (0, 1), 0.5f, G, "Size of the Dimension stereo expander.");
        b.cont (hyper (Hyper::dimMix), p + "dimmix", "Dimension Mix", "DIM", g, ParamUnit::percent, linear (0, 1), 0.0f, G, "Amount of the Dimension stereo expander.");
    }
    {
        const auto p = fx ("dist"); const juce::String g = "Distortion";
        b.toggle (dist (Dist::enable), p + "enable", "Distortion On", "ON", g, false, "Enables the distortion unit.");
        b.cont (dist (Dist::mix), p + "mix", "Distortion Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Dry/wet balance.");
        b.choice (dist (Dist::mode), p + "mode", "Distortion Mode", "MODE", g, distModeNames(), 0, "Distortion algorithm.");
        b.cont (dist (Dist::drive), p + "drive", "Distortion Drive", "DRIVE", g, ParamUnit::percent, linear (0, 1), 0.25f, G, "Amount of distortion.");
        b.choice (dist (Dist::filterPos), p + "filterpos", "Distortion Filter", "FILTER", g, distFilterPosNames(), 0, "Places a filter before (Pre) or after (Post) the shaper.");
        b.choice (dist (Dist::filterType), p + "filtertype", "Distortion Filter Type", "TYPE", g, distFilterTypeNames(), 0, "Response of the distortion filter.");
        b.cont (dist (Dist::freq), p + "freq", "Distortion Filter Freq", "FREQ", g, ParamUnit::hertz, cutoffRange(), maps::cutoffFrom01 (0.5f), G, "Frequency of the distortion filter.");
        b.cont (dist (Dist::bw), p + "bw", "Distortion Filter Q", "RESO", g, ParamUnit::percent, linear (0, 1), 0.5f, G, "Bandwidth/resonance of the distortion filter.");
    }
    {
        const auto p = fx ("flanger"); const juce::String g = "Flanger";
        b.toggle (flanger (Flanger::enable), p + "enable", "Flanger On", "ON", g, false, "Enables the flanger.");
        b.cont (flanger (Flanger::mix), p + "mix", "Flanger Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Dry/wet balance.");
        b.cont (flanger (Flanger::rate), p + "rate", "Flanger Rate", "RATE", g, ParamUnit::hertz, skewed (0.01f, 20.0f, 1.0f), 0.25f, G, "Speed of the sweep.");
        b.toggle (flanger (Flanger::sync), p + "sync", "Flanger Sync", "BPM", g, false, "Locks the sweep to the host tempo.");
        b.choice (flanger (Flanger::syncRate), p + "syncrate", "Flanger Sync Rate", "RATE", g, syncRateNames(), 3, "Sweep length when synced.");
        b.cont (flanger (Flanger::depth), p + "depth", "Flanger Depth", "DEPTH", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Sweep depth.");
        b.cont (flanger (Flanger::feedback), p + "feedback", "Flanger Feedback", "FEED", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.5f, G, "Resonance of the comb. Negative values invert polarity.");
        b.cont (flanger (Flanger::stereo), p + "stereo", "Flanger Stereo", "STEREO", g, ParamUnit::degrees, linear (0, 360), 180.0f, G, "Phase offset between the left and right sweep.");
    }
    {
        const auto p = fx ("phaser"); const juce::String g = "Phaser";
        b.toggle (phaser (Phaser::enable), p + "enable", "Phaser On", "ON", g, false, "Enables the phaser.");
        b.cont (phaser (Phaser::mix), p + "mix", "Phaser Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Dry/wet balance.");
        b.cont (phaser (Phaser::rate), p + "rate", "Phaser Rate", "RATE", g, ParamUnit::hertz, skewed (0.01f, 20.0f, 1.0f), 0.25f, G, "Speed of the sweep.");
        b.toggle (phaser (Phaser::sync), p + "sync", "Phaser Sync", "BPM", g, false, "Locks the sweep to the host tempo.");
        b.choice (phaser (Phaser::syncRate), p + "syncrate", "Phaser Sync Rate", "RATE", g, syncRateNames(), 3, "Sweep length when synced.");
        b.cont (phaser (Phaser::depth), p + "depth", "Phaser Depth", "DEPTH", g, ParamUnit::percent, linear (0, 1), 0.5f, G, "Sweep depth.");
        b.cont (phaser (Phaser::freq), p + "freq", "Phaser Frequency", "FREQ", g, ParamUnit::hertz, cutoffRange(), maps::cutoffFrom01 (0.6f), G, "Centre frequency of the notches.");
        b.cont (phaser (Phaser::feedback), p + "feedback", "Phaser Feedback", "FEED", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.8f, G, "Resonance of the notches.");
        b.cont (phaser (Phaser::stereo), p + "stereo", "Phaser Stereo", "STEREO", g, ParamUnit::degrees, linear (0, 360), 180.0f, G, "Phase offset between left and right.");
        b.integer (phaser (Phaser::stages), p + "stages", "Phaser Stages", "STAGES", g, ParamUnit::none, 2, 12, 4, N, "Number of allpass stages (more = more notches).");
    }
    {
        const auto p = fx ("chorus"); const juce::String g = "Chorus";
        b.toggle (chorus (Chorus::enable), p + "enable", "Chorus On", "ON", g, false, "Enables the chorus.");
        b.cont (chorus (Chorus::mix), p + "mix", "Chorus Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 0.5f, G, "Dry/wet balance.");
        b.cont (chorus (Chorus::rate), p + "rate", "Chorus Rate", "RATE", g, ParamUnit::hertz, skewed (0.01f, 20.0f, 1.0f), 0.25f, G, "Modulation speed.");
        b.toggle (chorus (Chorus::sync), p + "sync", "Chorus Sync", "BPM", g, false, "Locks the modulation to the host tempo.");
        b.choice (chorus (Chorus::syncRate), p + "syncrate", "Chorus Sync Rate", "RATE", g, syncRateNames(), 3, "Modulation cycle when synced.");
        b.cont (chorus (Chorus::delay1), p + "delay1", "Chorus Delay 1", "DLY 1", g, ParamUnit::milliseconds, skewed (0.5f, 30.0f, 8.0f), 8.0f, G, "Delay time of the first voice.");
        b.cont (chorus (Chorus::delay2), p + "delay2", "Chorus Delay 2", "DLY 2", g, ParamUnit::milliseconds, skewed (0.5f, 30.0f, 8.0f), 12.0f, G, "Delay time of the second voice.");
        b.cont (chorus (Chorus::depth), p + "depth", "Chorus Depth", "DEPTH", g, ParamUnit::milliseconds, skewed (0.0f, 15.0f, 3.0f), 3.0f, G, "Modulation depth in milliseconds.");
        b.cont (chorus (Chorus::feedback), p + "feedback", "Chorus Feedback", "FEED", g, ParamUnit::bipolarPercent, linear (-0.95f, 0.95f), 0.1f, G, "Feedback of the delay voices.");
        b.cont (chorus (Chorus::lpf), p + "lpf", "Chorus Low Pass", "LPF", g, ParamUnit::hertz, cutoffRange(), maps::cutoffFrom01 (0.85f), G, "Low-pass filter on the wet signal.");
    }
    {
        const auto p = fx ("delay"); const juce::String g = "Delay";
        b.toggle (delay (Delay::enable), p + "enable", "Delay On", "ON", g, false, "Enables the stereo delay.");
        b.cont (delay (Delay::mix), p + "mix", "Delay Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 0.3f, G, "Dry/wet balance.");
        b.choice (delay (Delay::mode), p + "mode", "Delay Mode", "MODE", g, delayModeNames(), 0, "Normal stereo, Ping-Pong (bouncing) or Tap->Delay (an extra tap before the echoes).");
        b.toggle (delay (Delay::sync), p + "sync", "Delay Sync", "BPM", g, true, "Locks the delay times to the host tempo.");
        b.cont (delay (Delay::timeL), p + "timel", "Delay Time L", "TIME L", g, ParamUnit::milliseconds, skewed (1.0f, 2000.0f, 300.0f), 375.0f, G, "Left delay time when sync is off.");
        b.cont (delay (Delay::timeR), p + "timer", "Delay Time R", "TIME R", g, ParamUnit::milliseconds, skewed (1.0f, 2000.0f, 300.0f), 375.0f, G, "Right delay time when sync is off.");
        b.choice (delay (Delay::syncL), p + "syncl", "Delay Sync L", "SYNC L", g, syncRateNames(), 6, "Left delay time when synced.");
        b.choice (delay (Delay::syncR), p + "syncr", "Delay Sync R", "SYNC R", g, syncRateNames(), 6, "Right delay time when synced.");
        b.toggle (delay (Delay::link), p + "link", "Delay Link", "LINK", g, true, "Uses the left time for both channels.");
        b.cont (delay (Delay::feedback), p + "feedback", "Delay Feedback", "FEED", g, ParamUnit::percent, linear (0, 1.05f), 0.4f, G, "Amount of signal fed back (number of repeats).");
        b.cont (delay (Delay::freq), p + "freq", "Delay Filter Freq", "FREQ", g, ParamUnit::hertz, cutoffRange(), maps::cutoffFrom01 (0.7f), G, "Centre of the band-pass filter in the feedback path.");
        b.cont (delay (Delay::bw), p + "bw", "Delay Filter Width", "WIDTH", g, ParamUnit::octaves, linear (0.5f, 9.0f), 6.8f, G, "Width of the band-pass filter in octaves.");
    }
    {
        const auto p = fx ("comp"); const juce::String g = "Compressor";
        b.toggle (comp (Comp::enable), p + "enable", "Compressor On", "ON", g, false, "Enables the compressor.");
        b.cont (comp (Comp::mix), p + "mix", "Compressor Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Dry/wet (parallel compression).");
        b.cont (comp (Comp::threshold), p + "threshold", "Compressor Threshold", "THRESH", g, ParamUnit::decibel, linear (-60, 0), -18.0f, G, "Level above which compression starts.");
        b.cont (comp (Comp::ratio), p + "ratio", "Compressor Ratio", "RATIO", g, ParamUnit::ratio, skewed (1.0f, 20.0f, 4.0f), 4.0f, G, "Amount of gain reduction above the threshold.");
        b.cont (comp (Comp::attack), p + "attack", "Compressor Attack", "ATTACK", g, ParamUnit::milliseconds, skewed (0.1f, 500.0f, 20.0f), 10.0f, G, "How fast the compressor reacts.");
        b.cont (comp (Comp::release), p + "release", "Compressor Release", "RELEASE", g, ParamUnit::milliseconds, skewed (5.0f, 2000.0f, 150.0f), 120.0f, G, "How fast the compressor recovers.");
        b.cont (comp (Comp::gain), p + "gain", "Compressor Gain", "GAIN", g, ParamUnit::decibel, linear (0, 30), 0.0f, G, "Make-up gain.");
        b.toggle (comp (Comp::multiband), p + "multiband", "Compressor Multiband", "MULTI", g, false, "Splits into three bands with upward and downward compression (OTT style).");
        b.cont (comp (Comp::lowGain), p + "lowgain", "Compressor Low Band", "LOW", g, ParamUnit::percent, linear (0, 2), 1.0f, G, "Low band output (multiband mode).");
        b.cont (comp (Comp::midGain), p + "midgain", "Compressor Mid Band", "MID", g, ParamUnit::percent, linear (0, 2), 1.0f, G, "Mid band output (multiband mode).");
        b.cont (comp (Comp::highGain), p + "highgain", "Compressor High Band", "HIGH", g, ParamUnit::percent, linear (0, 2), 1.0f, G, "High band output (multiband mode).");
    }
    {
        const auto p = fx ("reverb"); const juce::String g = "Reverb";
        b.toggle (reverb (Reverb::enable), p + "enable", "Reverb On", "ON", g, false, "Enables the reverb.");
        b.cont (reverb (Reverb::mix), p + "mix", "Reverb Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 0.33f, G, "Dry/wet balance.");
        b.choice (reverb (Reverb::mode), p + "mode", "Reverb Mode", "MODE", g, reverbModeNames(), 1, "Plate (dense, bright) or Hall (large, smooth).");
        b.cont (reverb (Reverb::size), p + "size", "Reverb Size", "SIZE", g, ParamUnit::percent, linear (0, 1), 0.5f, G, "Room size and decay time.");
        b.cont (reverb (Reverb::preDelay), p + "predelay", "Reverb Pre-Delay", "PRE", g, ParamUnit::milliseconds, skewed (0.0f, 250.0f, 40.0f), 10.0f, G, "Gap before the reverb tail starts.");
        b.cont (reverb (Reverb::lowCut), p + "lowcut", "Reverb Low Cut", "LOW CUT", g, ParamUnit::percent, linear (0, 1), 0.0f, G, "Removes low frequencies from the tail.");
        b.cont (reverb (Reverb::damp), p + "damp", "Reverb Damping", "DAMP", g, ParamUnit::percent, linear (0, 1), 0.28f, G, "High-frequency absorption over time.");
        b.cont (reverb (Reverb::highCut), p + "highcut", "Reverb High Cut", "HI CUT", g, ParamUnit::percent, linear (0, 1), 0.5f, G, "Removes high frequencies from the tail.");
        b.cont (reverb (Reverb::width), p + "width", "Reverb Width", "WIDTH", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Stereo width of the tail.");
    }
    {
        const auto p = fx ("eq"); const juce::String g = "EQ";
        b.toggle (eq (Eq::enable), p + "enable", "EQ On", "ON", g, false, "Enables the two-band equaliser.");
        b.choice (eq (Eq::lowType), p + "lowtype", "EQ Low Type", "LOW", g, eqLowTypeNames(), 0, "Shape of the low band.");
        b.cont (eq (Eq::lowFreq), p + "lowfreq", "EQ Low Freq", "FREQ", g, ParamUnit::hertz, skewed (20.0f, 20000.0f, 500.0f), 200.0f, G, "Frequency of the low band.");
        b.cont (eq (Eq::lowQ), p + "lowq", "EQ Low Q", "WIDTH", g, ParamUnit::none, skewed (0.1f, 10.0f, 1.0f), 0.707f, G, "Bandwidth of the low band.");
        b.cont (eq (Eq::lowGain), p + "lowgain", "EQ Low Gain", "GAIN", g, ParamUnit::decibel, linear (-24, 24), 0.0f, G, "Boost or cut of the low band.");
        b.choice (eq (Eq::highType), p + "hightype", "EQ High Type", "HIGH", g, eqHighTypeNames(), 0, "Shape of the high band.");
        b.cont (eq (Eq::highFreq), p + "highfreq", "EQ High Freq", "FREQ", g, ParamUnit::hertz, skewed (20.0f, 20000.0f, 500.0f), 4000.0f, G, "Frequency of the high band.");
        b.cont (eq (Eq::highQ), p + "highq", "EQ High Q", "WIDTH", g, ParamUnit::none, skewed (0.1f, 10.0f, 1.0f), 0.707f, G, "Bandwidth of the high band.");
        b.cont (eq (Eq::highGain), p + "highgain", "EQ High Gain", "GAIN", g, ParamUnit::decibel, linear (-24, 24), 0.0f, G, "Boost or cut of the high band.");
    }
    {
        const auto p = fx ("filter"); const juce::String g = "FX Filter";
        b.toggle (fxFilter (FxFilter::enable), p + "enable", "FX Filter On", "ON", g, false, "Enables the post-voice filter (applied to the whole mix).");
        b.cont (fxFilter (FxFilter::mix), p + "mix", "FX Filter Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Dry/wet balance.");
        b.choice (fxFilter (FxFilter::type), p + "type", "FX Filter Type", "TYPE", g, filterTypeNames(), kDefaultFilterType, "Filter type (the same list as the voice filters).");
        b.cont (fxFilter (FxFilter::cutoff), p + "cutoff", "FX Filter Cutoff", "CUTOFF", g, ParamUnit::hertz, cutoffRange(), maps::cutoffFrom01 (0.5f), G, "Cutoff frequency.");
        b.cont (fxFilter (FxFilter::reso), p + "reso", "FX Filter Resonance", "RES", g, ParamUnit::percent, linear (0, 1), 0.0f, G, "Resonance.");
        b.cont (fxFilter (FxFilter::drive), p + "drive", "FX Filter Drive", "DRIVE", g, ParamUnit::percent, linear (0, 1), 0.0f, G, "Saturation before the filter.");
        b.cont (fxFilter (FxFilter::var), p + "var", "FX Filter Var", "VAR", g, ParamUnit::percent, linear (0, 1), 0.0f, G, "Type-specific control.");
        b.cont (fxFilter (FxFilter::stereo), p + "stereo", "FX Filter Stereo", "STEREO", g, ParamUnit::bipolarPercent, linear (-1, 1), 0.0f, G, "Left/right cutoff offset.");
    }
    {
        const auto p = fx ("bode"); const juce::String g = "Bode Shifter";
        b.toggle (bode (Bode::enable), p + "enable", "Bode On", "ON", g, false, "Enables the frequency shifter (shifts every partial by a fixed number of Hz).");
        b.cont (bode (Bode::mix), p + "mix", "Bode Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Dry/wet balance.");
        b.cont (bode (Bode::shift), p + "shift", "Bode Shift", "SHIFT", g, ParamUnit::hertz, bodeRange(), 0.0f, G, "Frequency shift in Hz. Small values give phasing, large values metallic inharmonic tones.");
        b.cont (bode (Bode::feedback), p + "feedback", "Bode Feedback", "FEED", g, ParamUnit::percent, linear (0, 0.95f), 0.0f, G, "Feeds the shifted signal back for barber-pole effects.");
        b.cont (bode (Bode::spread), p + "spread", "Bode Stereo", "SPREAD", g, ParamUnit::percent, linear (0, 1), 0.0f, G, "Shifts left and right in opposite directions.");
    }
    {
        const auto p = fx ("util"); const juce::String g = "Utility";
        b.toggle (util (Util::enable), p + "enable", "Utility On", "ON", g, false, "Enables the utility unit (gain, pan, width, mono bass).");
        b.cont (util (Util::gain), p + "gain", "Utility Gain", "GAIN", g, ParamUnit::decibel, linear (-24, 24), 0.0f, G, "Output gain.");
        b.cont (util (Util::pan), p + "pan", "Utility Pan", "PAN", g, ParamUnit::pan, linear (-1, 1), 0.0f, G, "Balance.");
        b.cont (util (Util::width), p + "width", "Utility Width", "WIDTH", g, ParamUnit::percent, linear (0, 2), 1.0f, G, "Stereo width (0% mono, 200% extra wide).");
        b.toggle (util (Util::monoBass), p + "monobass", "Utility Mono Bass", "MONO LO", g, false, "Makes frequencies below the crossover mono.");
        b.cont (util (Util::monoFreq), p + "monofreq", "Utility Mono Freq", "FREQ", g, ParamUnit::hertz, skewed (20.0f, 500.0f, 120.0f), 120.0f, G, "Crossover for mono bass.");
        b.toggle (util (Util::invertL), p + "invertl", "Utility Invert L", "INV L", g, false, "Inverts the polarity of the left channel.");
        b.toggle (util (Util::invertR), p + "invertr", "Utility Invert R", "INV R", g, false, "Inverts the polarity of the right channel.");
    }
    {
        const auto p = fx ("conv"); const juce::String g = "Convolve";
        b.toggle (conv (Conv::enable), p + "enable", "Convolve On", "ON", g, false, "Enables the convolution unit (impulse-response reverb and resonances).");
        b.cont (conv (Conv::mix), p + "mix", "Convolve Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 0.3f, G, "Dry/wet balance.");
        b.choice (conv (Conv::ir), p + "ir", "Convolve IR", "IR", g, convIrNames(), 0, "Impulse response. Choose Custom IR and load any WAV file.");
        b.cont (conv (Conv::preDelay), p + "predelay", "Convolve Pre-Delay", "PRE", g, ParamUnit::milliseconds, skewed (0.0f, 250.0f, 40.0f), 0.0f, G, "Delay before the impulse response.");
        b.cont (conv (Conv::lowCut), p + "lowcut", "Convolve Low Cut", "LOW CUT", g, ParamUnit::hertz, skewed (20.0f, 2000.0f, 200.0f), 20.0f, G, "High-pass on the wet signal.");
        b.cont (conv (Conv::highCut), p + "highcut", "Convolve High Cut", "HI CUT", g, ParamUnit::hertz, skewed (1000.0f, 20000.0f, 6000.0f), 20000.0f, G, "Low-pass on the wet signal.");
        b.cont (conv (Conv::gain), p + "gain", "Convolve Gain", "GAIN", g, ParamUnit::decibel, linear (-24, 24), 0.0f, G, "Wet gain.");
    }
    for (int s = 0; s < 4; ++s)
    {
        const auto p = fx (("plugin" + juce::String (s + 1)).toRawUTF8());
        const auto g = "Plugin Slot " + juce::String (s + 1);
        b.toggle (plugin (s, Plugin::enable), p + "enable", g + " On", "ON", g, false, "Bypasses or enables the hosted VST3/AU effect in this slot.");
        b.cont (plugin (s, Plugin::mix), p + "mix", g + " Mix", "MIX", g, ParamUnit::percent, linear (0, 1), 1.0f, G, "Dry/wet balance of the hosted plugin.");
    }

    //------------------------------------------------------------------ Mod matrix amounts
    for (int m = 0; m < kNumModSlots; ++m)
        b.cont (mod (m), "mod" + juce::String (m + 1) + "_amount", "Mod " + juce::String (m + 1) + " Amount", "AMOUNT", "Matrix",
                ParamUnit::bipolarPercent, linear (-1, 1), 0.0f, N,
                "Depth of modulation slot " + juce::String (m + 1) + " as a fraction of the destination's full range.");

    jassert ((int) specs.size() == pid::total);

    for (auto& s : specs)
        idToIndex[s.id] = s.index;
}

const ParamRegistry& ParamRegistry::get()
{
    static const ParamRegistry instance;
    return instance;
}

const ParamSpec* ParamRegistry::find (const juce::String& id) const
{
    auto it = idToIndex.find (id);
    return it != idToIndex.end() ? &specs[(size_t) it->second] : nullptr;
}

int ParamRegistry::indexOf (const juce::String& id) const
{
    auto it = idToIndex.find (id);
    return it != idToIndex.end() ? it->second : -1;
}

juce::AudioProcessorValueTreeState::ParameterLayout ParamRegistry::createLayout() const
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (auto& s : specs)
    {
        const juce::ParameterID pidv { s.id, 1 };
        const ParamSpec* sp = &s;

        switch (s.kind)
        {
            case ParamKind::toggle:
                layout.add (std::make_unique<juce::AudioParameterBool> (pidv, s.name, s.defaultValue >= 0.5f,
                    juce::AudioParameterBoolAttributes().withLabel (s.group)));
                break;

            case ParamKind::choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pidv, s.name, s.choices, juce::roundToInt (s.defaultValue),
                    juce::AudioParameterChoiceAttributes().withLabel (s.group)));
                break;

            case ParamKind::integer:
            case ParamKind::continuous:
                layout.add (std::make_unique<juce::AudioParameterFloat> (pidv, s.name, s.range, s.defaultValue,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (s.group)
                        .withStringFromValueFunction ([sp] (float v, int len) { return sp->formatValue (v, len); })));
                break;
        }
    }
    return layout;
}

std::vector<std::pair<juce::String, std::vector<int>>> ParamRegistry::modDestinationGroups() const
{
    std::vector<std::pair<juce::String, std::vector<int>>> groups;
    for (auto& s : specs)
    {
        if (! s.isModulatable())
            continue;
        if (groups.empty() || groups.back().first != s.group)
            groups.push_back ({ s.group, {} });
        groups.back().second.push_back (s.index);
    }
    return groups;
}

} // namespace cureum
