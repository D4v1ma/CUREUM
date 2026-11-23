#pragma once

#include <juce_core/juce_core.h>
#include <array>

// Enumerations and display names shared by the DSP engine, the parameter layout, the UI,
// the help system and the Serum importers. The order of every list is part of the preset
// format, so new entries must only ever be appended.

namespace cureum
{

//==============================================================================
// Oscillator warp modes (two warp slots per wavetable oscillator).
// Indices 0..23 follow the order of the Serum 1 warp menu as closely as it is known so that
// Serum 1 presets map index-for-index; everything after that is a CUREUM / Serum 2 style
// extension.
enum class WarpMode : int
{
    off = 0, sync, bendPlus, bendMinus, bendPlusMinus, pwm, asymPlus, asymMinus, asymPlusMinus,
    flip, mirror, remap1, remap2, remap3, remap4, quantize,
    fmOsc, amOsc, rmOsc, fmNoise, amNoise, rmNoise, fmSub, amSub,
    rmSub, syncWindow, pmOsc, pmSelf, fmA, fmB, fmC, pmA, pmB, pmC,
    squash, fold, phaseDistort, bitCrush,
    count
};

inline const juce::StringArray& warpModeNames()
{
    static const juce::StringArray names {
        "Off", "Sync", "Bend +", "Bend -", "Bend +/-", "PWM", "Asym +", "Asym -", "Asym +/-",
        "Flip", "Mirror", "Remap 1", "Remap 2", "Remap 3", "Remap 4", "Quantize",
        "FM (Osc)", "AM (Osc)", "RM (Osc)", "FM (Noise)", "AM (Noise)", "RM (Noise)", "FM (Sub)", "AM (Sub)",
        "RM (Sub)", "Sync (Window)", "PM (Osc)", "PM (Self)", "FM (A)", "FM (B)", "FM (C)", "PM (A)", "PM (B)", "PM (C)",
        "Squash", "Fold", "Phase Distort", "Bit Crush"
    };
    jassert (names.size() == (int) WarpMode::count);
    return names;
}

//==============================================================================
enum class UnisonMode : int { linear = 0, super, exponential, inverse, random, count };
inline const juce::StringArray& unisonModeNames()
{
    static const juce::StringArray n { "Linear", "Super", "Exp", "Inverse", "Random" };
    return n;
}

enum class UnisonStack : int { off = 0, octave, octaveFifth, twoOctaves, centreOctave, centreFifth, count };
inline const juce::StringArray& unisonStackNames()
{
    static const juce::StringArray n { "Off", "12", "12 + 7", "24", "Centre 12", "Centre 7" };
    return n;
}

//==============================================================================
enum class SubShape : int { sine = 0, roundRect, triangle, saw, square, pulse, count };
inline const juce::StringArray& subShapeNames()
{
    static const juce::StringArray n { "Sine", "Round Rect", "Triangle", "Saw", "Square", "Pulse" };
    return n;
}

//==============================================================================
// Built-in noise sources. The last entry plays the user-loaded sample.
enum class NoiseType : int
{
    white = 0, pink, brown, blue, violet, grey, digital, crackle, vinyl, tapeHiss, wind, rain,
    hum50, hum60, metallic, breath, staticBurst, click, thump, snap, custom, count
};
inline const juce::StringArray& noiseTypeNames()
{
    static const juce::StringArray n {
        "White", "Pink", "Brown", "Blue", "Violet", "Grey", "Digital", "Crackle", "Vinyl", "Tape Hiss",
        "Wind", "Rain", "Hum 50 Hz", "Hum 60 Hz", "Metallic", "Breath", "Static", "Click", "Thump", "Snap",
        "Custom Sample"
    };
    return n;
}

//==============================================================================
enum class RouteDest : int { filter1 = 0, filter2, both, direct, count };
inline const juce::StringArray& routeDestNames()
{
    static const juce::StringArray n { "Filter 1", "Filter 2", "Filter 1+2", "Direct" };
    return n;
}

enum class FilterConfig : int { serial = 0, parallel, count };
inline const juce::StringArray& filterConfigNames()
{
    static const juce::StringArray n { "Serial", "Parallel" };
    return n;
}

//==============================================================================
enum class LfoType : int { shape = 0, sampleHold, smoothRandom, lorenz, rossler, count };
inline const juce::StringArray& lfoTypeNames()
{
    static const juce::StringArray n { "Shape", "S&H", "Smooth Rnd", "Lorenz", "Rossler" };
    return n;
}

enum class LfoMode : int { trig = 0, env, free, count };
inline const juce::StringArray& lfoModeNames()
{
    static const juce::StringArray n { "Trig", "Env", "Free" };
    return n;
}

// Musical divisions used by synced LFOs and tempo-synced effects (in whole notes).
inline const juce::StringArray& syncRateNames()
{
    static const juce::StringArray n { "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128" };
    return n;
}
inline double syncRateWholeNotes (int index)
{
    static constexpr std::array<double, 11> v { 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625, 0.0078125 };
    return v[(size_t) juce::jlimit (0, (int) v.size() - 1, index)];
}

enum class SyncFeel : int { straight = 0, dotted, triplet, count };
inline const juce::StringArray& syncFeelNames()
{
    static const juce::StringArray n { "Straight", "Dotted", "Triplet" };
    return n;
}
inline double syncFeelMultiplier (int feel)
{
    return feel == 1 ? 1.5 : (feel == 2 ? 2.0 / 3.0 : 1.0);
}

//==============================================================================
enum class Quality : int { normal = 0, high, ultra, count };
inline const juce::StringArray& qualityNames()
{
    static const juce::StringArray n { "Normal (1x)", "High (2x)", "Ultra (4x)" };
    return n;
}

//==============================================================================
// Effects
enum class FxType : int
{
    hyper = 0, distortion, flanger, phaser, chorus, delay, compressor, reverb, eq, filter,
    bode, utility, convolve, plugin1, plugin2, plugin3, plugin4, count
};
constexpr int kNumFxTypes = (int) FxType::count;

inline const juce::StringArray& fxTypeNames()
{
    static const juce::StringArray n {
        "Hyper/Dimension", "Distortion", "Flanger", "Phaser", "Chorus", "Delay", "Compressor", "Reverb",
        "EQ", "Filter", "Bode Shifter", "Utility", "Convolve", "Plugin Slot 1", "Plugin Slot 2", "Plugin Slot 3", "Plugin Slot 4"
    };
    return n;
}
inline const char* fxTypeKey (FxType t)
{
    static const char* keys[] = { "hyper", "dist", "flanger", "phaser", "chorus", "delay", "comp", "reverb", "eq",
                                  "filter", "bode", "util", "conv", "plugin1", "plugin2", "plugin3", "plugin4" };
    return keys[(int) t];
}
inline bool isPluginSlot (FxType t) { return t >= FxType::plugin1 && t <= FxType::plugin4; }

enum class DistMode : int
{
    tube = 0, softClip, hardClip, diode1, diode2, linFold, sinFold, zeroSquare, downsample, asym,
    rectify, xShaper, xShaperAsym, sineShaper, stompBox, bitCrush, fuzz, overdrive, count
};
inline const juce::StringArray& distModeNames()
{
    static const juce::StringArray n {
        "Tube", "Soft Clip", "Hard Clip", "Diode 1", "Diode 2", "Linear Fold", "Sine Fold", "Zero-Square",
        "Downsample", "Asymmetric", "Rectify", "X-Shaper", "X-Shaper Asym", "Sine Shaper", "Stomp Box",
        "Bit Crush", "Fuzz", "Overdrive"
    };
    return n;
}

enum class DistFilterPos : int { off = 0, pre, post, count };
inline const juce::StringArray& distFilterPosNames()
{
    static const juce::StringArray n { "Off", "Pre", "Post" };
    return n;
}
enum class DistFilterType : int { lowpass = 0, bandpass, highpass, count };
inline const juce::StringArray& distFilterTypeNames()
{
    static const juce::StringArray n { "Low Pass", "Band Pass", "High Pass" };
    return n;
}

enum class DelayMode : int { normal = 0, pingPong, tapDelay, count };
inline const juce::StringArray& delayModeNames()
{
    static const juce::StringArray n { "Normal", "Ping-Pong", "Tap -> Delay" };
    return n;
}

enum class ReverbMode : int { plate = 0, hall, count };
inline const juce::StringArray& reverbModeNames()
{
    static const juce::StringArray n { "Plate", "Hall" };
    return n;
}

enum class EqLowType : int { shelf = 0, peak, highPass, count };
inline const juce::StringArray& eqLowTypeNames()
{
    static const juce::StringArray n { "Low Shelf", "Peak", "High Pass" };
    return n;
}
enum class EqHighType : int { shelf = 0, peak, lowPass, count };
inline const juce::StringArray& eqHighTypeNames()
{
    static const juce::StringArray n { "High Shelf", "Peak", "Low Pass" };
    return n;
}

enum class ConvIr : int { room = 0, hall, plate, spring, cathedral, metallic, gated, custom, count };
inline const juce::StringArray& convIrNames()
{
    static const juce::StringArray n { "Small Room", "Concert Hall", "Plate", "Spring", "Cathedral", "Metallic Box", "Gated", "Custom IR" };
    return n;
}

} // namespace cureum
