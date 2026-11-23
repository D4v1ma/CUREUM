// Serum 2 (.SerumPreset) importer.
#include "SerumImport.h"
#include "SerumImportInternal.h"
#include "Cbor.h"
#include "../Core/ValueMaps.h"
#include <zstd.h>
#include <map>

namespace cureum
{

using namespace pid;
using namespace serumdetail;

namespace
{
    enum class Cv { raw, pct, cutoff01, sqrtAmp, dbToGain, boolean, distMode, filterType, reverbMode, delayMode, pctCentred, degrees };

    struct FxMap { const char* serumName; int param; Cv conv; };

    FxType fxTypeFromKey (const juce::String& key, int typeInt, bool& known)
    {
        known = true;
        const auto k = key.toLowerCase();
        if (k.contains ("dist")) return FxType::distortion;
        if (k.contains ("flang")) return FxType::flanger;
        if (k.contains ("phas")) return FxType::phaser;
        if (k.contains ("chorus")) return FxType::chorus;
        if (k.contains ("delay")) return FxType::delay;
        if (k.contains ("comp")) return FxType::compressor;
        if (k.contains ("reverb") || k.contains ("verb")) return FxType::reverb;
        if (k.contains ("eq")) return FxType::eq;
        if (k.contains ("filter")) return FxType::filter;
        if (k.contains ("hyper") || k.contains ("dim")) return FxType::hyper;
        if (k.contains ("bode") || k.contains ("shift")) return FxType::bode;
        if (k.contains ("util")) return FxType::utility;
        if (k.contains ("conv")) return FxType::convolve;
        static constexpr FxType byInt[] = { FxType::distortion, FxType::flanger, FxType::phaser, FxType::chorus, FxType::delay,
                                            FxType::compressor, FxType::reverb, FxType::eq, FxType::filter, FxType::hyper };
        if (typeInt >= 0 && typeInt < 10) return byInt[typeInt];
        known = false;
        return FxType::count;
    }

    const std::vector<FxMap>& fxParamMap (FxType t)
    {
        static const std::map<FxType, std::vector<FxMap>> maps {
            { FxType::hyper, { { "kParamWet", hyper (Hyper::mix), Cv::pct }, { "kParamRate", hyper (Hyper::rate), Cv::pct },
                               { "kParamDetune", hyper (Hyper::detune), Cv::pct }, { "kParamUnison", hyper (Hyper::unison), Cv::raw },
                               { "kParamRetrig", hyper (Hyper::retrig), Cv::boolean }, { "kParamDimESize", hyper (Hyper::dimSize), Cv::pct },
                               { "kParamDimEWet", hyper (Hyper::dimMix), Cv::pct }, { "kParamDimEMix", hyper (Hyper::dimMix), Cv::pct } } },
            { FxType::distortion, { { "kParamWet", dist (Dist::mix), Cv::pct }, { "kParamDrive", dist (Dist::drive), Cv::pct },
                                    { "kParamMode", dist (Dist::mode), Cv::distMode }, { "kParamFreq", dist (Dist::freq), Cv::cutoff01 },
                                    { "kParamBW", dist (Dist::bw), Cv::pct }, { "kParamQ", dist (Dist::bw), Cv::pct } } },
            { FxType::flanger, { { "kParamWet", flanger (Flanger::mix), Cv::pct }, { "kParamRate", flanger (Flanger::rate), Cv::raw },
                                 { "kParamDepth", flanger (Flanger::depth), Cv::pct }, { "kParamFeedback", flanger (Flanger::feedback), Cv::pct },
                                 { "kParamStereo", flanger (Flanger::stereo), Cv::degrees } } },
            { FxType::phaser, { { "kParamWet", phaser (Phaser::mix), Cv::pct }, { "kParamRate", phaser (Phaser::rate), Cv::raw },
                                { "kParamDepth", phaser (Phaser::depth), Cv::pct }, { "kParamFreq", phaser (Phaser::freq), Cv::cutoff01 },
                                { "kParamFeedback", phaser (Phaser::feedback), Cv::pct }, { "kParamStereo", phaser (Phaser::stereo), Cv::degrees } } },
            { FxType::chorus, { { "kParamWet", chorus (Chorus::mix), Cv::pct }, { "kParamRate", chorus (Chorus::rate), Cv::raw },
                                { "kParamDelay1", chorus (Chorus::delay1), Cv::raw }, { "kParamDelay2", chorus (Chorus::delay2), Cv::raw },
                                { "kParamDepth", chorus (Chorus::depth), Cv::raw }, { "kParamFeedback", chorus (Chorus::feedback), Cv::pct },
                                { "kParamLPF", chorus (Chorus::lpf), Cv::cutoff01 }, { "kParamFilter", chorus (Chorus::lpf), Cv::cutoff01 } } },
            { FxType::delay, { { "kParamWet", delay (Delay::mix), Cv::pct }, { "kParamFeedback", delay (Delay::feedback), Cv::pct },
                               { "kParamTimeL", delay (Delay::timeL), Cv::raw }, { "kParamTimeR", delay (Delay::timeR), Cv::raw },
                               { "kParamDelayL", delay (Delay::timeL), Cv::raw }, { "kParamDelayR", delay (Delay::timeR), Cv::raw },
                               { "kParamLink", delay (Delay::link), Cv::boolean }, { "kParamFreq", delay (Delay::freq), Cv::cutoff01 },
                               { "kParamBW", delay (Delay::bw), Cv::raw }, { "kParamMode", delay (Delay::mode), Cv::delayMode } } },
            { FxType::compressor, { { "kParamWet", comp (Comp::mix), Cv::pct }, { "kParamThreshold", comp (Comp::threshold), Cv::raw },
                                    { "kParamRatio", comp (Comp::ratio), Cv::raw }, { "kParamAttack", comp (Comp::attack), Cv::raw },
                                    { "kParamRelease", comp (Comp::release), Cv::raw }, { "kParamMakeup", comp (Comp::gain), Cv::raw },
                                    { "kParamGain", comp (Comp::gain), Cv::raw }, { "kParamMultiband", comp (Comp::multiband), Cv::boolean },
                                    { "kParamGain0", comp (Comp::lowGain), Cv::dbToGain }, { "kParamGain1", comp (Comp::midGain), Cv::dbToGain },
                                    { "kParamGain2", comp (Comp::highGain), Cv::dbToGain } } },
            { FxType::reverb, { { "kParamWet", reverb (Reverb::mix), Cv::pct }, { "kParamSize", reverb (Reverb::size), Cv::pct },
                                { "kParamPreDelay", reverb (Reverb::preDelay), Cv::raw }, { "kParamLowCut", reverb (Reverb::lowCut), Cv::pct },
                                { "kParamDamp", reverb (Reverb::damp), Cv::pct }, { "kParamHighCut", reverb (Reverb::highCut), Cv::pct },
                                { "kParamWidth", reverb (Reverb::width), Cv::pct }, { "kParamMode", reverb (Reverb::mode), Cv::reverbMode },
                                { "kParamType", reverb (Reverb::mode), Cv::reverbMode } } },
            { FxType::eq, { { "kParamFreqL", eq (Eq::lowFreq), Cv::raw }, { "kParamFreq0", eq (Eq::lowFreq), Cv::raw },
                            { "kParamQL", eq (Eq::lowQ), Cv::raw }, { "kParamGainL", eq (Eq::lowGain), Cv::raw },
                            { "kParamGain0", eq (Eq::lowGain), Cv::raw }, { "kParamFreqH", eq (Eq::highFreq), Cv::raw },
                            { "kParamFreq1", eq (Eq::highFreq), Cv::raw }, { "kParamQH", eq (Eq::highQ), Cv::raw },
                            { "kParamGainH", eq (Eq::highGain), Cv::raw }, { "kParamGain1", eq (Eq::highGain), Cv::raw } } },
            { FxType::filter, { { "kParamWet", fxFilter (FxFilter::mix), Cv::pct }, { "kParamType", fxFilter (FxFilter::type), Cv::filterType },
                                { "kParamFreq", fxFilter (FxFilter::cutoff), Cv::cutoff01 }, { "kParamReso", fxFilter (FxFilter::reso), Cv::pct },
                                { "kParamDrive", fxFilter (FxFilter::drive), Cv::pct }, { "kParamVar", fxFilter (FxFilter::var), Cv::pct },
                                { "kParamStereo", fxFilter (FxFilter::stereo), Cv::pctCentred } } },
            { FxType::bode, { { "kParamWet", bode (Bode::mix), Cv::pct }, { "kParamShift", bode (Bode::shift), Cv::raw },
                              { "kParamFreq", bode (Bode::shift), Cv::raw }, { "kParamFeedback", bode (Bode::feedback), Cv::pct } } },
            { FxType::utility, { { "kParamGain", util (Util::gain), Cv::raw }, { "kParamPan", util (Util::pan), Cv::pctCentred },
                                 { "kParamWidth", util (Util::width), Cv::pct } } },
            { FxType::convolve, { { "kParamWet", conv (Conv::mix), Cv::pct }, { "kParamPreDelay", conv (Conv::preDelay), Cv::raw } } },
        };
        static const std::vector<FxMap> empty;
        if (auto it = maps.find (t); it != maps.end()) return it->second;
        return empty;
    }

    float convertValue (Cv cv, const juce::var& v, int param)
    {
        const float x = (float) (double) v;
        switch (cv)
        {
            case Cv::raw:        return x;
            case Cv::pct:        return x / 100.0f;
            case Cv::cutoff01:   return x <= 1.0f ? maps::cutoffFrom01 (x) : x;
            case Cv::sqrtAmp:    return std::sqrt (std::max (0.0f, x));
            case Cv::dbToGain:   return juce::jlimit (0.0f, 2.0f, juce::Decibels::decibelsToGain (x));
            case Cv::boolean:    return x > 0.5f ? 1.0f : 0.0f;
            case Cv::pctCentred: return juce::jlimit (-1.0f, 1.0f, (x - 50.0f) / 50.0f);
            case Cv::degrees:    return x;
            case Cv::distMode:   return (float) distModeFromName (v.toString());
            case Cv::filterType: { const int t = SerumImport::filterTypeFromSerumName (v.toString()); return (float) (t < 0 ? kDefaultFilterType : t); }
            case Cv::reverbMode: return v.toString().containsIgnoreCase ("plate") ? 0.0f : 1.0f;
            case Cv::delayMode:
            {
                const auto s = v.toString().toLowerCase();
                return s.contains ("ping") ? 1.0f : (s.contains ("tap") ? 2.0f : 0.0f);
            }
        }
        juce::ignoreUnused (param);
        return x;
    }

    // Maps a Serum 2 modulation destination to a CUREUM parameter index (or -1).
    int destinationFor (const juce::String& type, int id, const juce::String& p, const PresetBuilder& b)
    {
        auto pick = [&p] (std::initializer_list<std::pair<const char*, int>> list) -> int
        {
            for (auto& e : list) if (p == e.first) return e.second;
            return -1;
        };
        if (type == "Oscillator")
        {
            if (id >= 0 && id < 3)
                return pick ({ { "kParamVolume", osc (id, Osc::level) }, { "kParamPan", osc (id, Osc::pan) }, { "kParamOctave", osc (id, Osc::octave) },
                               { "kParamPitch", osc (id, Osc::semi) }, { "kParamFine", osc (id, Osc::fine) }, { "kParamCoarsePit", osc (id, Osc::coarse) },
                               { "kParamDetune", osc (id, Osc::detune) }, { "kParamDetuneWid", osc (id, Osc::width) }, { "kParamBlend", osc (id, Osc::blend) } });
            if (id == 3) return pick ({ { "kParamVolume", noise (Noise::level) }, { "kParamPan", noise (Noise::pan) } });
            if (id == 4) return pick ({ { "kParamVolume", sub (Sub::level) }, { "kParamPan", sub (Sub::pan) }, { "kParamOctave", sub (Sub::octave) }, { "kParamCoarsePit", sub (Sub::coarse) } });
        }
        if (type == "WTOsc" && id >= 0 && id < 3)
            return pick ({ { "kParamWarp", osc (id, Osc::warp1Amt) }, { "kParamWarp2", osc (id, Osc::warp2Amt) }, { "kParamTablePos", osc (id, Osc::wtPos) },
                           { "kParamInitialPhase", osc (id, Osc::phase) } });
        if (type == "NoiseOsc") return pick ({ { "kParamColor", noise (Noise::pitch) }, { "kParamFine", noise (Noise::fine) } });
        if (type == "VoiceFilter" && id >= 0 && id < 2)
            return pick ({ { "kParamFreq", filter (id, Filter::cutoff) }, { "kParamReso", filter (id, Filter::reso) }, { "kParamDrive", filter (id, Filter::drive) },
                           { "kParamVar", filter (id, Filter::var) }, { "kParamWet", filter (id, Filter::mix) }, { "kParamStereo", filter (id, Filter::stereo) },
                           { "kParamLevelOut", filter (id, Filter::level) }, { "kParamKeyTrack", filter (id, Filter::keyTrack) } });
        if (type == "Env" && id >= 0 && id < 4)
            return pick ({ { "kParamAttack", env (id, Env::attack) }, { "kParamHold", env (id, Env::hold) }, { "kParamDecay", env (id, Env::decay) },
                           { "kParamSustain", env (id, Env::sustain) }, { "kParamRelease", env (id, Env::release) } });
        if (type == "LFO" && id >= 0 && id < kNumLfos)
        {
            const bool synced = b.get (lfo (id, Lfo::sync)) > 0.5f;
            return pick ({ { "kParamRate", synced ? lfo (id, Lfo::syncRate) : lfo (id, Lfo::rate) }, { "kParamRise", lfo (id, Lfo::rise) },
                           { "kParamDelay", lfo (id, Lfo::delay) }, { "kParamSmooth", lfo (id, Lfo::smooth) }, { "kParamPhase", lfo (id, Lfo::phase) } });
        }
        if (type == "Global") return pick ({ { "kParamMasterVolume", global (Global::master) } });
        if (type.startsWith ("FX"))
        {
            bool known = false;
            const auto t = fxTypeFromKey (type, -1, known);
            if (known)
                for (auto& m : fxParamMap (t))
                    if (p == m.serumName && ParamRegistry::get().spec (m.param).isModulatable())
                        return m.param;
        }
        return -1;
    }

    void importOscillators (PresetBuilder& b, const juce::var& root)
    {
        for (int o = 0; o < kNumOscillators; ++o)
        {
            const auto m = root[juce::Identifier ("Oscillator" + juce::String (o))];
            if (! m.isObject()) continue;
            const auto pp = plain (m);
            if (has (pp, "kParamEnable"))   b.setBool (osc (o, Osc::enable), num (pp, "kParamEnable", 0) > 0.5f);
            if (has (pp, "kParamVolume"))   b.set (osc (o, Osc::level), std::sqrt (std::max (0.0f, num (pp, "kParamVolume", 0.5625f))));
            if (has (pp, "kParamPan"))      b.set (osc (o, Osc::pan), num (pp, "kParamPan", 0) / 100.0f);
            if (has (pp, "kParamOctave"))   b.set (osc (o, Osc::octave), num (pp, "kParamOctave", 0));
            if (has (pp, "kParamPitch"))    b.set (osc (o, Osc::semi), num (pp, "kParamPitch", 0));
            if (has (pp, "kParamFine"))     b.set (osc (o, Osc::fine), num (pp, "kParamFine", 0));
            if (has (pp, "kParamCoarsePit"))b.set (osc (o, Osc::coarse), num (pp, "kParamCoarsePit", 0));
            if (has (pp, "kParamDetune"))   { const float d = num (pp, "kParamDetune", 0.25f); b.set (osc (o, Osc::detune), d <= 1.0f ? std::sqrt (std::max (0.0f, d)) : d / 100.0f); }
            if (has (pp, "kParamDetuneWid"))b.set (osc (o, Osc::width), pct (num (pp, "kParamDetuneWid", 100)));
            if (has (pp, "kParamBlend"))    b.set (osc (o, Osc::blend), pct (num (pp, "kParamBlend", 75)));
            if (has (pp, "kParamUnison"))   b.set (osc (o, Osc::unison), num (pp, "kParamUnison", 1));

            const auto wt = m[juce::Identifier ("WTOsc" + juce::String (o))];
            if (wt.isObject())
            {
                const auto pw = plain (wt);
                if (has (pw, "kParamTablePos"))     b.set (osc (o, Osc::wtPos), juce::jlimit (0.0f, 1.0f, (num (pw, "kParamTablePos", 1) - 1.0f) / 255.0f));
                if (has (pw, "kParamWarp"))         { const float w = num (pw, "kParamWarp", 0); b.set (osc (o, Osc::warp1Amt), w <= 1.0f ? w : w / 100.0f); }
                if (has (pw, "kParamWarp2"))        { const float w = num (pw, "kParamWarp2", 0); b.set (osc (o, Osc::warp2Amt), w <= 1.0f ? w : w / 100.0f); }
                if (has (pw, "kParamWarpMenu"))     b.setChoice (osc (o, Osc::warp1Mode), (int) SerumImport::warpFromSerumName (str (pw, "kParamWarpMenu")));
                if (has (pw, "kParamWarpMenu2"))    b.setChoice (osc (o, Osc::warp2Mode), (int) SerumImport::warpFromSerumName (str (pw, "kParamWarpMenu2")));
                if (has (pw, "kParamInitialPhase")) b.set (osc (o, Osc::phase), num (pw, "kParamInitialPhase", 180));
                if (has (pw, "kParamRandomPhase"))  b.set (osc (o, Osc::randPhase), pct (num (pw, "kParamRandomPhase", 100)));
                const auto path = wt["relativePathToWT"].toString();
                if (path.isNotEmpty())
                    if (auto t = resolveWavetable (path, b.warnings))
                        b.setWavetable (o, t);
            }
            for (auto modeKey : { "SampleOsc", "GranularOsc", "SpectralOsc", "MultiSampleOsc" })
            {
                const auto sm = m[juce::Identifier (juce::String (modeKey) + juce::String (o))];
                if (sm.isObject() && sm["plainParams"].isObject() && b.get (osc (o, Osc::enable)) > 0.5f)
                    b.warnings.addIfNotAlreadyThere ("Osc " + oscLetter (o) + " has Serum 2 " + juce::String (modeKey).dropLastCharacters (3)
                                                     + " settings; CUREUM imports the wavetable engine only.");
            }
        }

        // Noise (Oscillator3)
        if (const auto m = root["Oscillator3"]; m.isObject())
        {
            const auto pp = plain (m);
            if (has (pp, "kParamEnable")) b.setBool (noise (Noise::enable), num (pp, "kParamEnable", 0) > 0.5f);
            if (has (pp, "kParamVolume")) b.set (noise (Noise::level), std::sqrt (std::max (0.0f, num (pp, "kParamVolume", 0.0625f))));
            if (has (pp, "kParamPan"))    b.set (noise (Noise::pan), num (pp, "kParamPan", 0) / 100.0f);
            const auto n = m["NoiseOsc3"];
            if (n.isObject())
            {
                const auto pn = plain (n);
                if (has (pn, "kParamColor"))        { const float c = num (pn, "kParamColor", 0.5f); b.set (noise (Noise::pitch), c <= 1.0f ? c : c / 100.0f); }
                if (has (pn, "kParamFine"))         b.set (noise (Noise::fine), juce::jlimit (-1.0f, 1.0f, num (pn, "kParamFine", 0) / 100.0f));
                if (has (pn, "kParamInitialPhase")) b.set (noise (Noise::phase), pct (num (pn, "kParamInitialPhase", 0)));
                if (has (pn, "kParamRandomPhase"))  b.set (noise (Noise::randPhase), pct (num (pn, "kParamRandomPhase", 100)));
                if (has (pn, "kParamOneShot"))      b.setBool (noise (Noise::oneShot), num (pn, "kParamOneShot", 0) > 0.5f);
                applyNoiseSample (b, n["relativePathToNoiseSample"].toString());
            }
        }

        // Sub (Oscillator4)
        if (const auto m = root["Oscillator4"]; m.isObject())
        {
            const auto pp = plain (m);
            if (has (pp, "kParamEnable"))   b.setBool (sub (Sub::enable), num (pp, "kParamEnable", 0) > 0.5f);
            if (has (pp, "kParamVolume"))   b.set (sub (Sub::level), std::sqrt (std::max (0.0f, num (pp, "kParamVolume", 0.5625f))));
            if (has (pp, "kParamPan"))      b.set (sub (Sub::pan), num (pp, "kParamPan", 0) / 100.0f);
            if (has (pp, "kParamOctave"))   b.set (sub (Sub::octave), num (pp, "kParamOctave", 0));
            if (has (pp, "kParamCoarsePit"))b.set (sub (Sub::coarse), num (pp, "kParamCoarsePit", 0));
            const auto s = m["SubOsc4"];
            if (s.isObject())
            {
                const auto ps = plain (s);
                if (has (ps, "kParamInitialPhase")) b.set (sub (Sub::phase), num (ps, "kParamInitialPhase", 0));
                if (has (ps, "kParamShape"))
                {
                    const auto shape = normaliseName (str (ps, "kParamShape"));
                    SubShape sh = SubShape::sine;
                    if (shape.contains ("round")) sh = SubShape::roundRect;
                    else if (shape.contains ("tri")) sh = SubShape::triangle;
                    else if (shape.contains ("saw")) sh = SubShape::saw;
                    else if (shape.contains ("sq")) sh = SubShape::square;
                    else if (shape.contains ("pulse")) sh = SubShape::pulse;
                    b.setChoice (sub (Sub::shape), (int) sh);
                }
            }
        }
    }

    void importFilters (PresetBuilder& b, const juce::var& root)
    {
        for (int f = 0; f < kNumFilters; ++f)
        {
            const auto m = root[juce::Identifier ("VoiceFilter" + juce::String (f))];
            if (! m.isObject()) continue;
            const auto pp = plain (m);
            if (has (pp, "kParamEnable")) b.setBool (filter (f, Filter::enable), num (pp, "kParamEnable", 0) > 0.5f);
            if (has (pp, "kParamType"))
            {
                const int t = SerumImport::filterTypeFromSerumName (str (pp, "kParamType"));
                if (t >= 0) b.setChoice (filter (f, Filter::type), t);
                else b.setBool (filter (f, Filter::enable), false);
            }
            if (has (pp, "kParamFreq"))     b.set (filter (f, Filter::cutoff), maps::cutoffFrom01 (num (pp, "kParamFreq", 0.5f)));
            if (has (pp, "kParamReso"))     b.set (filter (f, Filter::reso), pct (num (pp, "kParamReso", 10)));
            if (has (pp, "kParamDrive"))    b.set (filter (f, Filter::drive), pct (num (pp, "kParamDrive", 0)));
            if (has (pp, "kParamVar"))      b.set (filter (f, Filter::var), pct (num (pp, "kParamVar", 0)));
            if (has (pp, "kParamWet"))      b.set (filter (f, Filter::mix), pct (num (pp, "kParamWet", 100)));
            if (has (pp, "kParamStereo"))   b.set (filter (f, Filter::stereo), (num (pp, "kParamStereo", 50) - 50.0f) / 50.0f);
            if (has (pp, "kParamLevelOut")) b.set (filter (f, Filter::level), num (pp, "kParamLevelOut", 1));
            if (has (pp, "kParamKeyTrack")) b.set (filter (f, Filter::keyTrack), pct (num (pp, "kParamKeyTrack", 0)));
        }

        // Per-source routing (RoutingSlot0..4 = Osc A, B, C, Noise, Sub).
        for (int i = 0; i < 5; ++i)
        {
            const auto m = root[juce::Identifier ("RoutingSlot" + juce::String (i))];
            if (! m.isObject()) continue;
            const auto pp = plain (m);
            if (! has (pp, "kParamRoutingDest")) continue;
            const auto d = normaliseName (str (pp, "kParamRoutingDest"));
            RouteDest r = RouteDest::filter1;
            if (d.contains ("direct") || d.contains ("main") || d.contains ("bus")) r = RouteDest::direct;
            else if (d.contains ("both") || d.contains ("filter12") || d.contains ("filters")) r = RouteDest::both;
            else if (d.contains ("filter2")) r = RouteDest::filter2;
            if (d.contains ("bus"))
                b.warnings.addIfNotAlreadyThere ("Serum 2 FX bus routing is mapped to Direct (CUREUM has one FX rack).");
            b.setChoice (route ((Route) i), (int) r);
        }
    }

    void importEnvelopesLfosMacros (PresetBuilder& b, const juce::var& root)
    {
        for (int e = 0; e < kNumEnvelopes; ++e)
        {
            const auto m = root[juce::Identifier ("Env" + juce::String (e))];
            if (! m.isObject()) continue;
            const auto pp = plain (m);
            if (has (pp, "kParamAttack"))  b.set (env (e, Env::attack), num (pp, "kParamAttack", 0.0005f));
            if (has (pp, "kParamHold"))    b.set (env (e, Env::hold), num (pp, "kParamHold", 0));
            if (has (pp, "kParamDecay"))   b.set (env (e, Env::decay), num (pp, "kParamDecay", 1));
            if (has (pp, "kParamRelease")) b.set (env (e, Env::release), num (pp, "kParamRelease", 0.015f));
            if (has (pp, "kParamSustain"))
            {
                float s = num (pp, "kParamSustain", 1);
                if (s < 0.0f) s = juce::Decibels::decibelsToGain (s);
                else if (s > 1.0001f) s /= 100.0f;
                b.set (env (e, Env::sustain), s);
            }
            if (has (pp, "kParamCurve1")) b.set (env (e, Env::aCurve), (num (pp, "kParamCurve1", 50) - 50.0f) / 50.0f);
            if (has (pp, "kParamCurve2")) b.set (env (e, Env::dCurve), (num (pp, "kParamCurve2", 66.6f) - 50.0f) / 50.0f);
            if (has (pp, "kParamCurve3")) b.set (env (e, Env::rCurve), (num (pp, "kParamCurve3", 66.6f) - 50.0f) / 50.0f);
        }

        for (int l = 0; l < kNumLfos; ++l)
        {
            const auto m = root[juce::Identifier ("LFO" + juce::String (l))];
            if (! m.isObject()) continue;
            const auto pp = plain (m);
            if (has (pp, "kParamType"))
            {
                const auto t = normaliseName (str (pp, "kParamType"));
                LfoType type = LfoType::shape;
                if (t.contains ("lorenz")) type = LfoType::lorenz;
                else if (t.contains ("rossler") || t.contains ("roessler")) type = LfoType::rossler;
                else if (t.contains ("sh") || t.contains ("sample") || t.contains ("random")) type = LfoType::sampleHold;
                b.setChoice (lfo (l, Lfo::type), (int) type);
            }
            if (has (pp, "kParamMode"))
            {
                const auto md = normaliseName (str (pp, "kParamMode"));
                b.setChoice (lfo (l, Lfo::mode), (int) (md.contains ("env") ? LfoMode::env : (md.contains ("free") || md.contains ("off") ? LfoMode::free : LfoMode::trig)));
            }
            const bool sync = ! has (pp, "kParamBeatSync") || num (pp, "kParamBeatSync", 1) > 0.5f;
            b.setBool (lfo (l, Lfo::sync), sync);
            if (has (pp, "kParamRate"))
            {
                const float r = num (pp, "kParamRate", 0.5f);
                if (sync) b.setChoice (lfo (l, Lfo::syncRate), juce::jlimit (0, 10, juce::roundToInt (juce::jlimit (0.0f, 1.0f, r) * 10.0f)));
                else      b.set (lfo (l, Lfo::rate), r * (num (pp, "kParamRate10x", 0) > 0.5f ? 10.0f : 1.0f));
            }
            const bool dotted = num (pp, "kParamDotted", 0) > 0.5f;
            const bool triplets = num (pp, "kParamTriplets", 0) > 0.5f;
            SyncFeel feel = SyncFeel::straight;
            if (dotted && ! triplets) feel = SyncFeel::dotted;
            else if (triplets && ! dotted) feel = SyncFeel::triplet;
            b.setChoice (lfo (l, Lfo::syncFeel), (int) feel);
            if (has (pp, "kParamRise"))   b.set (lfo (l, Lfo::rise), num (pp, "kParamRise", 0));
            if (has (pp, "kParamDelay"))  b.set (lfo (l, Lfo::delay), num (pp, "kParamDelay", 0));
            if (has (pp, "kParamSmooth")) b.set (lfo (l, Lfo::smooth), pct (num (pp, "kParamSmooth", 0)));
            if (has (pp, "kParamPhase"))  b.set (lfo (l, Lfo::phase), num (pp, "kParamPhase", 0));

            const auto curve = m["curveData"];
            const auto* xs = curve["xVals"].getArray();
            const auto* ys = curve["yVals"].getArray();
            const auto* cs = curve["curveVals"].getArray();
            if (xs != nullptr && ys != nullptr && xs->size() >= 2 && xs->size() == ys->size())
            {
                std::vector<LfoPoint> pts;
                for (int i = 0; i < xs->size(); ++i)
                {
                    LfoPoint pt;
                    pt.x = (float) (double) xs->getReference (i);
                    pt.y = (float) (double) ys->getReference (i);
                    pt.curve = (cs != nullptr && i < cs->size()) ? (float) (double) cs->getReference (i) : 0.5f;
                    pts.push_back (pt);
                }
                b.setLfoShape (l, LfoShape (pts, "Imported"));
            }
        }

        for (int m = 0; m < kNumMacros; ++m)
        {
            const auto mac = root[juce::Identifier ("Macro" + juce::String (m))];
            if (! mac.isObject()) continue;
            const auto pp = plain (mac);
            if (has (pp, "kParamValue")) b.set (macro (m), pct (num (pp, "kParamValue", 0)));
            const auto n = mac["name"].toString();
            if (n.isNotEmpty()) b.setMacroName (m, n);
        }
    }

    void importFx (PresetBuilder& b, const juce::var& root)
    {
        FxOrder order;
        std::array<bool, (size_t) kNumFxTypes> used {};
        for (int rack = 0; rack < 3; ++rack)
        {
            const auto r = root[juce::Identifier ("FXRack" + juce::String (rack))];
            const auto* list = r["FX"].getArray();
            if (list == nullptr) continue;
            for (const auto& entry : *list)
            {
                auto* obj = entry.getDynamicObject();
                if (obj == nullptr) continue;
                const int typeInt = (int) entry["type"];
                juce::String key;
                juce::var params;
                for (auto& prop : obj->getProperties())
                    if (prop.name.toString().startsWith ("FX") && prop.value.isObject())
                    {
                        key = prop.name.toString();
                        params = prop.value;
                        break;
                    }
                bool known = false;
                const auto type = fxTypeFromKey (key, typeInt, known);
                if (! known)
                {
                    b.warnings.addIfNotAlreadyThere ("Serum 2 effect \"" + (key.isNotEmpty() ? key : juce::String (typeInt)) + "\" is not available in CUREUM and was skipped.");
                    continue;
                }
                if (used[(size_t) type])
                {
                    b.warnings.addIfNotAlreadyThere ("Duplicate " + fxTypeNames()[(int) type] + " units were merged (CUREUM has one of each effect).");
                    continue;
                }
                used[(size_t) type] = true;
                order.units.push_back (type);
                if (rack > 0)
                    b.warnings.addIfNotAlreadyThere ("Effects from Serum 2 bus racks were appended to the main FX rack.");

                const auto pp = plain (params);
                b.setBool (pid::fxEnable (type), ! has (pp, "kParamEnable") || num (pp, "kParamEnable", 1) > 0.5f);
                for (auto& mp : fxParamMap (type))
                    if (has (pp, mp.serumName))
                        b.set (mp.param, convertValue (mp.conv, pp[mp.serumName], mp.param));
                if (type == FxType::compressor && (has (pp, "kParamGain0") || has (pp, "kParamGain2")))
                    b.setBool (comp (Comp::multiband), true);
            }
        }
        // Keep the remaining effects available (disabled) after the imported ones.
        for (auto t : FxOrder::defaultOrder().units)
            if (! order.contains (t)) order.units.push_back (t);
        b.setFxOrder (order);
    }

    void importGlobal (PresetBuilder& b, const juce::var& root)
    {
        const auto pp = plain (root["Global0"]);
        if (has (pp, "kParamMasterVolume")) b.set (global (Global::master), std::sqrt (std::max (0.0f, num (pp, "kParamMasterVolume", 0.49f))));
        if (has (pp, "kParamMonoToggle"))   b.setBool (global (Global::mono), num (pp, "kParamMonoToggle", 0) > 0.5f);
        if (has (pp, "kParamPolyCount"))    b.set (global (Global::polyphony), num (pp, "kParamPolyCount", 16));
        if (has (pp, "kParamLegato"))       b.setBool (global (Global::legato), num (pp, "kParamLegato", 0) > 0.5f);
        for (auto k : { "kParamPortamento", "kParamPortaTime", "kParamGlide" })
            if (has (pp, k)) b.set (global (Global::portaTime), num (pp, k, 0));
        if (has (pp, "kParamPortaCurve"))   b.set (global (Global::portaCurve), num (pp, "kParamPortaCurve", 0) / 100.0f);
        if (has (pp, "kParamPortaAlways"))  b.setBool (global (Global::portaAlways), num (pp, "kParamPortaAlways", 0) > 0.5f);
        if (has (pp, "kParamBendUp"))       b.set (global (Global::bendUp), std::abs (num (pp, "kParamBendUp", 2)));
        if (has (pp, "kParamBendDown"))     b.set (global (Global::bendDown), std::abs (num (pp, "kParamBendDown", 2)));
        if (has (pp, "kParamTranspose"))    b.set (global (Global::transpose), num (pp, "kParamTranspose", 0));
        if ((bool) root["mpeEnabled"])      b.setBool (global (Global::mpeEnable), true);
        if (root["mpePitchBendRange"].isDouble() || root["mpePitchBendRange"].isInt() || root["mpePitchBendRange"].isInt64())
            b.set (global (Global::mpeBendRange), (float) (double) root["mpePitchBendRange"]);
    }

    void importMatrix (PresetBuilder& b, const juce::var& root)
    {
        int skipped = 0;
        for (int i = 0; i < 64; ++i)
        {
            const auto m = root[juce::Identifier ("ModSlot" + juce::String (i))];
            if (! m.isObject()) continue;
            const auto* src = m["source"].getArray();
            if (src == nullptr || src->isEmpty()) continue;
            const int sid = (int) (*src)[0];
            const int aux = src->size() > 1 ? (int) (*src)[1] : 0;
            const auto source = SerumImport::serum2Source (sid, aux);
            const int dest = destinationFor (m["destModuleTypeString"].toString(), (int) m["destModuleID"], m["destModuleParamName"].toString(), b);
            if (source == ModSource::none || dest < 0) { ++skipped; continue; }
            const auto pp = plain (m);
            const float amount = juce::jlimit (-1.0f, 1.0f, num (pp, "kParamAmount", 0) / 100.0f);
            const bool bipolar = num (pp, "kParamBipolar", 0) > 0.5f;
            const float output = has (pp, "kParamOutput") ? pct (num (pp, "kParamOutput", 100)) : 1.0f;
            ModSource auxSrc = ModSource::none;
            if (const auto* a = m["auxSource"].getArray(); a != nullptr && ! a->isEmpty())
                auxSrc = SerumImport::serum2Source ((int) (*a)[0], a->size() > 1 ? (int) (*a)[1] : 0);
            b.addModulation (source, dest, amount, bipolar, auxSrc, 0.0f, output);
        }
        if (skipped > 0)
            b.warnings.add (juce::String (skipped) + " modulation routing(s) use sources or destinations CUREUM cannot map and were skipped.");
    }
}

ImportedPreset SerumImport::importSerum2 (const void* data, size_t size, const juce::String& fileName)
{
    ImportedPreset result;
    result.sourceFormat = "Serum 2";
    auto* p = static_cast<const juce::uint8*> (data);
    if (size < 25 || std::memcmp (p, "XferJson\0", 9) != 0)
    {
        result.error = "Not a Serum 2 preset (missing XferJson header).";
        return result;
    }

    size_t off = 9;
    const juce::uint32 jsonLen = juce::ByteOrder::littleEndianInt (p + off);
    off += 8;
    if (off + jsonLen + 8 > size)
    {
        result.error = "The preset header is truncated.";
        return result;
    }
    const auto meta = juce::JSON::parse (juce::String::fromUTF8 (reinterpret_cast<const char*> (p + off), (int) jsonLen));
    off += jsonLen;
    const juce::uint32 cborLen = juce::ByteOrder::littleEndianInt (p + off);
    off += 8;

    const void* z = p + off;
    const size_t zSize = size - off;
    size_t capacity = cborLen;
    if (capacity == 0)
    {
        const auto cs = ZSTD_getFrameContentSize (z, zSize);
        capacity = (cs == ZSTD_CONTENTSIZE_ERROR || cs == ZSTD_CONTENTSIZE_UNKNOWN) ? (size_t) 16 << 20 : (size_t) cs;
    }
    if (capacity > ((size_t) 64 << 20))
    {
        result.error = "The preset payload is unreasonably large.";
        return result;
    }
    std::vector<char> cbor (capacity + 16);
    const size_t got = ZSTD_decompress (cbor.data(), cbor.size(), z, zSize);
    if (ZSTD_isError (got))
    {
        result.error = juce::String ("Could not decompress the preset: ") + ZSTD_getErrorName (got);
        return result;
    }

    juce::String err;
    const auto root = CborDecoder::decode (cbor.data(), got, err);
    if (! root.isObject())
    {
        result.error = "Could not decode the preset data: " + err;
        return result;
    }

    PresetBuilder b;
    b.name = meta["presetName"].toString();
    if (b.name.isEmpty()) b.name = root["presetName"].toString();
    if (b.name.isEmpty()) b.name = juce::File::createFileWithoutCheckingPath (fileName).getFileNameWithoutExtension();
    b.author = meta["presetAuthor"].toString();
    if (b.author.isEmpty()) b.author = root["presetAuthor"].toString();
    b.description = meta["presetDescription"].toString();
    if (const auto* tags = meta["tags"].getArray())
        for (auto& t : *tags) b.tags.add (t.toString());
    b.category = "Serum 2 Import";

    importOscillators (b, root);
    importFilters (b, root);
    importEnvelopesLfosMacros (b, root);
    importFx (b, root);
    importGlobal (b, root);
    importMatrix (b, root);

    result.name = b.name;
    result.state = b.build();
    result.warnings = b.warnings;
    return result;
}

} // namespace cureum
