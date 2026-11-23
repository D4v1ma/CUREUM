#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace cureum
{

// The algorithm family used to realise a filter type. The engine implements one processor per
// family; the per-type parameters (a, b, c) select poles, response, polarity and variants.
enum class FilterAlgo : int
{
    svf,          // a = response (FilterResp), b = poles (1..4)
    ladder,       // "MG" style transistor ladder, a = poles (1..4)
    multi,        // morphing state-variable filter: a = MultiShape, b = poles (2 or 4, 1 = 6 dB LH)
    comb,         // a = polarity (+1/-1), b = damping (0 none, 1 LP6, 2 HP6, 3 LP+HP)
    flange,       // same parameters as comb, feed-forward + feedback structure
    phaser,       // a = allpass stages, b = polarity, c = damping
    freqPhaser,   // a = stages (spread phaser)
    eq,           // a = EqShape
    ringMod,      // a = 1 or 2 carriers
    sampleHold,   // a = polarity (+1 normal, -1 smoothed)
    combs,        // four tuned combs
    allpasses,    // diffusion allpass chain
    reverb,       // small feedback-delay-network
    french,       // diode-style ladder with per-stage saturation
    german,       // transistor ladder with per-stage saturation
    addBass,      // low-pass with low boost
    formant,      // a = vowel set (1..3)
    bandReject,
    distComb,     // a = variant (1/2), b = post filter (0 LP, 1 BP)
    scream,       // a = 0 LP, 1 BP
    diode,        // acid-style diode ladder
    ms20,         // a = 0 LP, 1 HP (Sallen-Key style with diode feedback)
    vowel,        // continuously morphing vowel filter
    ladderHigh    // 24 dB high-pass ladder
};

enum class FilterResp : int { lowpass = 0, highpass, bandpass, peak, notch };

// For the "multi" family. Var morphs between the listed responses.
enum class MultiShape : int { LH, LB, LP, LN, HB, HP, HN, BP, PP, PN, NN, LBH, LPH, LNH, BPN };

enum class EqShape : int { lowShelf6, lowShelf12, band12, highShelf6, highShelf12 };

struct FilterTypeDef
{
    const char* name;
    const char* category;
    FilterAlgo algo;
    int a = 0, b = 0, c = 0;
    const char* varLabel = "Var";   // what the Var/Fat knob does for this type
};

inline const std::vector<FilterTypeDef>& filterTypes()
{
    using A = FilterAlgo;
    using R = FilterResp;
    using M = MultiShape;
    static const std::vector<FilterTypeDef> types {
        // ---- Normal (0..17)
        { "MG Low 6",  "Normal", A::ladder, 1, 0, 0, "Fat" },
        { "MG Low 12", "Normal", A::ladder, 2, 0, 0, "Fat" },
        { "MG Low 18", "Normal", A::ladder, 3, 0, 0, "Fat" },
        { "MG Low 24", "Normal", A::ladder, 4, 0, 0, "Fat" },
        { "Low 6",     "Normal", A::svf, (int) R::lowpass, 1, 0, "Fat" },
        { "Low 12",    "Normal", A::svf, (int) R::lowpass, 2, 0, "Fat" },
        { "Low 18",    "Normal", A::svf, (int) R::lowpass, 3, 0, "Fat" },
        { "Low 24",    "Normal", A::svf, (int) R::lowpass, 4, 0, "Fat" },
        { "High 6",    "Normal", A::svf, (int) R::highpass, 1, 0, "Fat" },
        { "High 12",   "Normal", A::svf, (int) R::highpass, 2, 0, "Fat" },
        { "High 18",   "Normal", A::svf, (int) R::highpass, 3, 0, "Fat" },
        { "High 24",   "Normal", A::svf, (int) R::highpass, 4, 0, "Fat" },
        { "Band 12",   "Normal", A::svf, (int) R::bandpass, 2, 0, "Fat" },
        { "Band 24",   "Normal", A::svf, (int) R::bandpass, 4, 0, "Fat" },
        { "Peak 12",   "Normal", A::svf, (int) R::peak, 2, 0, "Fat" },
        { "Peak 24",   "Normal", A::svf, (int) R::peak, 4, 0, "Fat" },
        { "Notch 12",  "Normal", A::svf, (int) R::notch, 2, 0, "Fat" },
        { "Notch 24",  "Normal", A::svf, (int) R::notch, 4, 0, "Fat" },
        // ---- Multi (18..37): Var morphs between the responses in the name
        { "LH 6",      "Multi", A::multi, (int) M::LH, 1, 0, "Morph" },
        { "LH 12",     "Multi", A::multi, (int) M::LH, 2, 0, "Morph" },
        { "LB 12",     "Multi", A::multi, (int) M::LB, 2, 0, "Morph" },
        { "LP 12",     "Multi", A::multi, (int) M::LP, 2, 0, "Morph" },
        { "LN 12",     "Multi", A::multi, (int) M::LN, 2, 0, "Morph" },
        { "HB 12",     "Multi", A::multi, (int) M::HB, 2, 0, "Morph" },
        { "HP 12",     "Multi", A::multi, (int) M::HP, 2, 0, "Morph" },
        { "HN 12",     "Multi", A::multi, (int) M::HN, 2, 0, "Morph" },
        { "BP 12",     "Multi", A::multi, (int) M::BP, 2, 0, "Morph" },
        { "PP 12",     "Multi", A::multi, (int) M::PP, 2, 0, "Spread" },
        { "PN 12",     "Multi", A::multi, (int) M::PN, 2, 0, "Morph" },
        { "NN 12",     "Multi", A::multi, (int) M::NN, 2, 0, "Spread" },
        { "L/B/H 12",  "Multi", A::multi, (int) M::LBH, 2, 0, "Morph" },
        { "L/B/H 24",  "Multi", A::multi, (int) M::LBH, 4, 0, "Morph" },
        { "L/P/H 12",  "Multi", A::multi, (int) M::LPH, 2, 0, "Morph" },
        { "L/P/H 24",  "Multi", A::multi, (int) M::LPH, 4, 0, "Morph" },
        { "L/N/H 12",  "Multi", A::multi, (int) M::LNH, 2, 0, "Morph" },
        { "L/N/H 24",  "Multi", A::multi, (int) M::LNH, 4, 0, "Morph" },
        { "B/P/N 12",  "Multi", A::multi, (int) M::BPN, 2, 0, "Morph" },
        { "B/P/N 24",  "Multi", A::multi, (int) M::BPN, 4, 0, "Morph" },
        // ---- Flanges (38..70)
        { "Cmb +",     "Flanges", A::comb, 1, 0, 0, "Damp" },
        { "Cmb -",     "Flanges", A::comb, -1, 0, 0, "Damp" },
        { "Cmb L6+",   "Flanges", A::comb, 1, 1, 0, "Cutoff" },
        { "Cmb L6-",   "Flanges", A::comb, -1, 1, 0, "Cutoff" },
        { "Cmb H6+",   "Flanges", A::comb, 1, 2, 0, "Cutoff" },
        { "Cmb H6-",   "Flanges", A::comb, -1, 2, 0, "Cutoff" },
        { "Cmb HL6+",  "Flanges", A::comb, 1, 3, 0, "Width" },
        { "Cmb HL6-",  "Flanges", A::comb, -1, 3, 0, "Width" },
        { "Flg +",     "Flanges", A::flange, 1, 0, 0, "Damp" },
        { "Flg -",     "Flanges", A::flange, -1, 0, 0, "Damp" },
        { "Flg L6+",   "Flanges", A::flange, 1, 1, 0, "Cutoff" },
        { "Flg L6-",   "Flanges", A::flange, -1, 1, 0, "Cutoff" },
        { "Flg H6+",   "Flanges", A::flange, 1, 2, 0, "Cutoff" },
        { "Flg H6-",   "Flanges", A::flange, -1, 2, 0, "Cutoff" },
        { "Flg HL6+",  "Flanges", A::flange, 1, 3, 0, "Width" },
        { "Flg HL6-",  "Flanges", A::flange, -1, 3, 0, "Width" },
        { "Phs 12+",   "Flanges", A::phaser, 2, 1, 0, "Spread" },
        { "Phs 12-",   "Flanges", A::phaser, 2, -1, 0, "Spread" },
        { "Phs 24+",   "Flanges", A::phaser, 4, 1, 0, "Spread" },
        { "Phs 24-",   "Flanges", A::phaser, 4, -1, 0, "Spread" },
        { "Phs 36+",   "Flanges", A::phaser, 6, 1, 0, "Spread" },
        { "Phs 36-",   "Flanges", A::phaser, 6, -1, 0, "Spread" },
        { "Phs 48+",   "Flanges", A::phaser, 8, 1, 0, "Spread" },
        { "Phs 48-",   "Flanges", A::phaser, 8, -1, 0, "Spread" },
        { "Phs 48L6+", "Flanges", A::phaser, 8, 1, 1, "Spread" },
        { "Phs 48L6-", "Flanges", A::phaser, 8, -1, 1, "Spread" },
        { "Phs 48H6+", "Flanges", A::phaser, 8, 1, 2, "Spread" },
        { "Phs 48H6-", "Flanges", A::phaser, 8, -1, 2, "Spread" },
        { "Phs 120+",  "Flanges", A::phaser, 20, 1, 0, "Spread" },
        { "Phs 120-",  "Flanges", A::phaser, 20, -1, 0, "Spread" },
        { "FPhs 4",    "Flanges", A::freqPhaser, 4, 0, 0, "Spread" },
        { "FPhs 8",    "Flanges", A::freqPhaser, 8, 0, 0, "Spread" },
        { "FPhs 12",   "Flanges", A::freqPhaser, 12, 0, 0, "Spread" },
        // ---- Misc (71..95)
        { "Low EQ 6",  "Misc", A::eq, (int) EqShape::lowShelf6, 0, 0, "Gain" },
        { "Low EQ 12", "Misc", A::eq, (int) EqShape::lowShelf12, 0, 0, "Gain" },
        { "Band EQ 12","Misc", A::eq, (int) EqShape::band12, 0, 0, "Gain" },
        { "High EQ 6", "Misc", A::eq, (int) EqShape::highShelf6, 0, 0, "Gain" },
        { "High EQ 12","Misc", A::eq, (int) EqShape::highShelf12, 0, 0, "Gain" },
        { "Ring Mod",  "Misc", A::ringMod, 1, 0, 0, "Mix" },
        { "Ring Modx2","Misc", A::ringMod, 2, 0, 0, "Spread" },
        { "SampHold",  "Misc", A::sampleHold, 1, 0, 0, "Smooth" },
        { "SampHold-", "Misc", A::sampleHold, -1, 0, 0, "Smooth" },
        { "Combs",     "Misc", A::combs, 0, 0, 0, "Spread" },
        { "Allpasses", "Misc", A::allpasses, 0, 0, 0, "Spread" },
        { "Reverb",    "Misc", A::reverb, 0, 0, 0, "Damp" },
        { "French LP", "Misc", A::french, 0, 0, 0, "Fat" },
        { "German LP", "Misc", A::german, 0, 0, 0, "Fat" },
        { "Add Bass",  "Misc", A::addBass, 0, 0, 0, "Bass" },
        { "Formant-I", "Misc", A::formant, 1, 0, 0, "Vowel" },
        { "Formant-II","Misc", A::formant, 2, 0, 0, "Vowel" },
        { "Formant-III","Misc", A::formant, 3, 0, 0, "Vowel" },
        { "Bandreject","Misc", A::bandReject, 0, 0, 0, "Width" },
        { "Dist.Comb 1 LP", "Misc", A::distComb, 1, 0, 0, "Tone" },
        { "Dist.Comb 1 BP", "Misc", A::distComb, 1, 1, 0, "Tone" },
        { "Dist.Comb 2 LP", "Misc", A::distComb, 2, 0, 0, "Tone" },
        { "Dist.Comb 2 BP", "Misc", A::distComb, 2, 1, 0, "Tone" },
        { "Scream LP", "Misc", A::scream, 0, 0, 0, "Scream" },
        { "Scream BP", "Misc", A::scream, 1, 0, 0, "Scream" },
        // ---- CUREUM extras (96..)
        { "Diode LP",  "CUREUM", A::diode, 0, 0, 0, "Squelch" },
        { "MS20 LP",   "CUREUM", A::ms20, 0, 0, 0, "Bite" },
        { "MS20 HP",   "CUREUM", A::ms20, 1, 0, 0, "Bite" },
        { "Vowel Morph","CUREUM", A::vowel, 0, 0, 0, "Vowel" },
        { "MG High 24","CUREUM", A::ladderHigh, 4, 0, 0, "Fat" },
    };
    return types;
}

inline int numFilterTypes() { return (int) filterTypes().size(); }

inline juce::StringArray filterTypeNames()
{
    juce::StringArray names;
    for (auto& t : filterTypes())
        names.add (t.name);
    return names;
}

constexpr int kDefaultFilterType = 1; // MG Low 12

} // namespace cureum
