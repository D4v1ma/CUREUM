// Serum 1 (.fxp / .fxb) importer, bank/archive handling and the generic file entry points.
#include "SerumImport.h"
#include "SerumImportInternal.h"
#include "../Core/ValueMaps.h"
#include "../Core/Settings.h"
#include "../DSP/WavetableFactory.h"
#include <zstd.h>

namespace cureum
{

using namespace pid;
using namespace serumdetail;

namespace
{
    // Layout of the decompressed Serum 1 parameter blob.
    constexpr size_t kParamOffset   = 0x3460;
    constexpr int    kNumS1Params   = 299;
    constexpr size_t kOscAPath      = 0x3c08;
    constexpr size_t kOscBPath      = 0x3e08;
    constexpr size_t kNoisePath     = 0x4008;
    constexpr size_t kPathLength    = 512;
    constexpr size_t kPresetName    = 0x4972;
    constexpr size_t kAuthor        = 0x49a0;
    constexpr size_t kDescription   = 0x49d0;
    constexpr size_t kMacroNames    = 0x4a60;
    constexpr size_t kMatrixOffset  = 0x0000;
    constexpr size_t kMatrixStride  = 40;
    constexpr int    kMatrixSlots   = 16;

    juce::String readString (const juce::MemoryBlock& blob, size_t offset, size_t maxLen)
    {
        if (offset >= blob.getSize()) return {};
        maxLen = std::min (maxLen, blob.getSize() - offset);
        auto* c = static_cast<const char*> (blob.getData()) + offset;
        size_t n = 0;
        while (n < maxLen && c[n] != 0) ++n;
        juce::String s = juce::String::fromUTF8 (c, (int) n);
        // Reject garbage (uninitialised buffers in some presets)
        for (auto ch : s)
            if (ch < 32 && ch != '\t') return {};
        return s.trim();
    }

    bool inflate (const void* data, size_t size, juce::MemoryBlock& out)
    {
        juce::MemoryInputStream in (data, size, false);
        juce::GZIPDecompressorInputStream z (&in, false, juce::GZIPDecompressorInputStream::zlibFormat);
        juce::MemoryOutputStream mo (out, false);
        constexpr juce::int64 limit = (juce::int64) 64 << 20;
        char buffer[16384];
        juce::int64 total = 0;
        while (! z.isExhausted())
        {
            const int n = z.read (buffer, (int) sizeof (buffer));
            if (n <= 0) break;
            mo.write (buffer, (size_t) n);
            total += n;
            if (total > limit) return false;
        }
        mo.flush();
        return total > 0;
    }

    // Filter-type parameter: normalised index / (count - 1), where the count depends on the Serum
    // version that saved the preset (89 or 96 types have been seen in real presets).
    int serum1FilterType (float v)
    {
        if (v <= 0.0f) return 0;
        int best = 0;
        float bestErr = 1.0f;
        for (int count : { 96, 89, 98, 100, 84 })
        {
            const float k = v * (float) (count - 1);
            const float err = std::abs (k - std::round (k));
            if (err < bestErr - 1.0e-4f) { bestErr = err; best = juce::roundToInt (k); }
        }
        return juce::jlimit (0, numFilterTypes() - 1, best);
    }

    float envTime (float v)  { return maps::envTimeFrom01 (v); }
    float bip (float v)      { return (v - 0.5f) * 2.0f; }
    float fxRateHz (float v) { return 0.01f + 20.0f * v * v * v; }

    void applyParams (PresetBuilder& b, const float* P, int count)
    {
        auto p = [&] (int i, float def) { return i < count && std::isfinite (P[i]) ? juce::jlimit (0.0f, 1.0f, P[i]) : def; };

        b.set (global (Global::master), p (0, 0.7f));

        // Oscillators A (0) and B (1); osc C stays off.
        for (int o = 0; o < 2; ++o)
        {
            const int base = o == 0 ? 1 : 14;
            b.set (osc (o, Osc::level), p (base + 0, 0.75f));
            b.set (osc (o, Osc::pan), bip (p (base + 1, 0.5f)));
            b.set (osc (o, Osc::octave), std::round (p (base + 2, 0.5f) * 8.0f - 4.0f));
            b.set (osc (o, Osc::semi), std::round (p (base + 3, 0.5f) * 24.0f - 12.0f));
            b.set (osc (o, Osc::fine), p (base + 4, 0.5f) * 200.0f - 100.0f);
            b.set (osc (o, Osc::unison), 1.0f + std::round (p (base + 5, 0.0f) * 15.0f));
            b.set (osc (o, Osc::detune), p (base + 6, 0.5f));
            b.set (osc (o, Osc::blend), p (base + 7, 0.75f));
            b.set (osc (o, Osc::warp1Amt), p (base + 8, 0.0f));
            b.set (osc (o, Osc::coarse), bip (p (base + 9, 0.5f)) * 48.0f);
            b.set (osc (o, Osc::wtPos), p (base + 10, 0.0f));
            b.set (osc (o, Osc::randPhase), p (base + 11, 1.0f));
            b.set (osc (o, Osc::phase), p (base + 12, 0.5f) * 360.0f);
            b.setBool (osc (o, Osc::enable), p (212 + o, o == 0 ? 1.0f : 0.0f) > 0.5f);
            b.setChoice (osc (o, Osc::warp1Mode), juce::jlimit (0, 23, juce::roundToInt (p (168 + o, 0.0f) * 23.0f)));
            b.setBool (osc (o, Osc::keyTrack), p (164 + o, 1.0f) > 0.5f);
            b.set (osc (o, Osc::width), p (172 + o, 1.0f));
            b.set (osc (o, Osc::uniWarp), bip (p (174 + o, 0.5f)));
            b.set (osc (o, Osc::uniWtPos), bip (p (176 + o, 0.5f)));
            b.setChoice (osc (o, Osc::uniStack), juce::jlimit (0, 5, juce::roundToInt (p (178 + o, 0.0f) * 5.0f)));
        }
        b.setBool (osc (2, Osc::enable), false);

        // Noise
        b.set (noise (Noise::level), p (27, 0.25f));
        b.set (noise (Noise::pitch), p (28, 0.5f));
        b.set (noise (Noise::fine), bip (p (29, 0.5f)));
        b.set (noise (Noise::pan), bip (p (30, 0.5f)));
        b.set (noise (Noise::randPhase), p (31, 0.0f));
        b.set (noise (Noise::phase), p (32, 0.0f));
        b.setBool (noise (Noise::enable), p (214, 0.0f) > 0.5f);

        // Sub
        b.set (sub (Sub::level), p (33, 0.75f));
        b.set (sub (Sub::pan), bip (p (34, 0.5f)));
        b.setChoice (sub (Sub::shape), juce::jlimit (0, 5, juce::roundToInt (p (170, 0.0f) * 5.0f)));
        b.set (sub (Sub::octave), std::round (p (171, 0.5f) * 8.0f - 4.0f));
        b.setBool (sub (Sub::enable), p (215, 0.0f) > 0.5f);

        // Envelopes (Serum 1 has three)
        const int envBase[3][5] = { { 35, 36, 37, 38, 39 }, { 51, 52, 53, 54, 55 }, { 56, 57, 58, 59, 60 } };
        const int curveBase[3] = { 71, 74, 77 };
        for (int e = 0; e < 3; ++e)
        {
            b.set (pid::env (e, Env::attack), envTime (p (envBase[e][0], 0.11f)));
            b.set (pid::env (e, Env::hold), envTime (p (envBase[e][1], 0.0f)));
            b.set (pid::env (e, Env::decay), envTime (p (envBase[e][2], 0.5f)));
            b.set (pid::env (e, Env::sustain), p (envBase[e][3], 1.0f));
            b.set (pid::env (e, Env::release), envTime (p (envBase[e][4], 0.215f)));
            b.set (pid::env (e, Env::aCurve), p (curveBase[e] + 0, 0.5f) * 2.0f - 1.0f);
            b.set (pid::env (e, Env::dCurve), p (curveBase[e] + 1, 0.67f) * 2.0f - 1.0f);
            b.set (pid::env (e, Env::rCurve), p (curveBase[e] + 2, 0.67f) * 2.0f - 1.0f);
        }

        // Filter + routing
        b.setChoice (filter (0, Filter::type), serum1FilterType (p (44, 0.0113636f)));
        b.set (filter (0, Filter::cutoff), maps::cutoffFrom01 (p (45, 0.5f)));
        b.set (filter (0, Filter::reso), p (46, 0.1f));
        b.set (filter (0, Filter::drive), p (47, 0.0f));
        b.set (filter (0, Filter::var), p (48, 0.0f));
        b.set (filter (0, Filter::mix), p (49, 1.0f));
        b.set (filter (0, Filter::stereo), bip (p (50, 0.5f)));
        b.setBool (filter (0, Filter::enable), p (216, 0.0f) > 0.5f);
        const Route routes[] = { Route::oscA, Route::oscB, Route::noise, Route::sub };
        for (int i = 0; i < 4; ++i)
            b.setChoice (route (routes[i]), p (40 + i, i == 0 ? 1.0f : 0.0f) > 0.5f ? (int) RouteDest::filter1 : (int) RouteDest::direct);

        // LFOs 1-8 (Serum's LFO rates are tempo synced by default)
        const int rateIdx[8] = { 61, 62, 63, 64, 260, 261, 262, 263 };
        const int smoothIdx[8] = { 223, 224, 225, 226, 264, 265, 266, 267 };
        for (int l = 0; l < 8; ++l)
        {
            b.setBool (lfo (l, Lfo::sync), true);
            b.setChoice (lfo (l, Lfo::syncRate), juce::jlimit (0, 10, juce::roundToInt (p (rateIdx[l], 0.5f) * 10.0f)));
            b.set (lfo (l, Lfo::smooth), p (smoothIdx[l], 0.0f));
            b.set (lfo (l, Lfo::rise), envTime (p (273 + l, 0.0f)));
            b.set (lfo (l, Lfo::delay), envTime (p (281 + l, 0.0f)));
            b.setChoice (lfo (l, Lfo::mode), (int) LfoMode::free);
        }
        // Chaos 1/2 -> LFO 9/10 as chaotic attractors
        b.setChoice (lfo (8, Lfo::type), (int) LfoType::lorenz);
        b.setChoice (lfo (9, Lfo::type), (int) LfoType::rossler);
        for (int c = 0; c < 2; ++c)
        {
            b.setBool (lfo (8 + c, Lfo::sync), false);
            b.set (lfo (8 + c, Lfo::rate), maps::lfoHzFrom01 (p (69 + c, 0.251189f)));
            b.setChoice (lfo (8 + c, Lfo::mode), (int) LfoMode::free);
        }

        // Voicing
        b.set (global (Global::portaTime), envTime (p (65, 0.0f)));
        b.set (global (Global::portaCurve), bip (p (66, 0.5f)));
        b.set (global (Global::bendUp), std::abs (std::round (p (166, 0.5417f) * 48.0f - 24.0f)));
        b.set (global (Global::bendDown), std::abs (std::round (p (167, 0.4583f) * 48.0f - 24.0f)));

        // Macros
        for (int m = 0; m < 4; ++m)
            b.set (macro (m), p (218 + m, 0.0f));

        // Effects: enables follow the Serum 1 rack order (Dist, Flg, Phs, Cho, Dly, Comp, Rev, EQ, Fil, Hyp)
        const FxType enableOrder[] = { FxType::distortion, FxType::flanger, FxType::phaser, FxType::chorus, FxType::delay,
                                       FxType::compressor, FxType::reverb, FxType::eq, FxType::filter, FxType::hyper };
        for (int i = 0; i < 10; ++i)
            b.setBool (pid::fxEnable (enableOrder[i]), p (154 + i, 0.0f) > 0.5f);

        b.set (reverb (Reverb::mix), p (81, 0.33f));
        b.set (reverb (Reverb::size), p (82, 0.33f));
        b.set (reverb (Reverb::preDelay), p (83, 0.12f) * 250.0f);
        b.set (reverb (Reverb::lowCut), p (84, 0.0f));
        b.set (reverb (Reverb::damp), p (85, 0.28f));
        b.set (reverb (Reverb::highCut), p (86, 0.5f));
        b.set (reverb (Reverb::width), p (87, 1.0f));

        b.set (eq (Eq::lowFreq), 20.0f * std::pow (1000.0f, p (88, 0.333f)));
        b.set (eq (Eq::highFreq), 20.0f * std::pow (1000.0f, p (89, 0.666f)));
        b.set (eq (Eq::lowQ), 0.1f * std::pow (10.0f, p (90, 0.6f) * 2.0f));
        b.set (eq (Eq::highQ), 0.1f * std::pow (10.0f, p (91, 0.6f) * 2.0f));
        b.set (eq (Eq::lowGain), p (92, 0.5f) * 48.0f - 24.0f);
        b.set (eq (Eq::highGain), p (93, 0.5f) * 48.0f - 24.0f);
        b.setChoice (eq (Eq::lowType), juce::roundToInt (p (94, 0.0f) * 2.0f));
        b.setChoice (eq (Eq::highType), juce::roundToInt (p (95, 0.0f) * 2.0f));

        b.set (dist (Dist::mix), p (96, 1.0f));
        b.set (dist (Dist::drive), p (97, 0.25f));
        b.setChoice (dist (Dist::filterType), juce::roundToInt (p (98, 0.0f) * 2.0f));
        b.setChoice (dist (Dist::mode), juce::jlimit (0, (int) DistMode::count - 1, juce::roundToInt (p (99, 0.0f) * 15.0f)));
        b.set (dist (Dist::freq), maps::cutoffFrom01 (p (100, 0.5f)));
        b.set (dist (Dist::bw), p (101, 0.5f));
        b.setChoice (dist (Dist::filterPos), juce::roundToInt (p (102, 0.0f) * 2.0f));

        b.set (flanger (Flanger::mix), p (103, 1.0f));
        b.setBool (flanger (Flanger::sync), p (104, 0.0f) > 0.5f);
        b.set (flanger (Flanger::rate), fxRateHz (p (105, 0.25f)));
        b.set (flanger (Flanger::depth), p (106, 1.0f));
        b.set (flanger (Flanger::feedback), p (107, 0.5f));
        b.set (flanger (Flanger::stereo), p (108, 0.5f) * 360.0f);

        b.set (phaser (Phaser::mix), p (109, 1.0f));
        b.setBool (phaser (Phaser::sync), p (110, 0.0f) > 0.5f);
        b.set (phaser (Phaser::rate), fxRateHz (p (111, 0.25f)));
        b.set (phaser (Phaser::depth), p (112, 0.5f));
        b.set (phaser (Phaser::freq), maps::cutoffFrom01 (p (113, 0.5f)));
        b.set (phaser (Phaser::feedback), p (114, 0.8f));
        b.set (phaser (Phaser::stereo), p (115, 0.5f) * 360.0f);

        b.set (chorus (Chorus::mix), p (116, 0.5f));
        b.setBool (chorus (Chorus::sync), p (117, 0.0f) > 0.5f);
        b.set (chorus (Chorus::rate), fxRateHz (p (118, 0.25f)));
        b.set (chorus (Chorus::delay1), 0.5f + p (119, 0.5f) * 19.5f);
        b.set (chorus (Chorus::delay2), 0.5f + p (120, 0.0f) * 19.5f);
        b.set (chorus (Chorus::depth), p (121, 1.0f) * 6.0f);
        b.set (chorus (Chorus::feedback), p (122, 0.1f) * 0.95f);
        b.set (chorus (Chorus::lpf), maps::cutoffFrom01 (p (123, 0.5f)));

        b.set (delay (Delay::mix), p (124, 0.3f));
        b.set (delay (Delay::freq), maps::cutoffFrom01 (p (125, 0.5f)));
        b.set (delay (Delay::bw), 0.8f + p (126, 0.81f) * 7.4f);
        b.setBool (delay (Delay::sync), p (127, 1.0f) > 0.5f);
        b.setBool (delay (Delay::link), p (128, 0.0f) > 0.5f);
        b.setChoice (delay (Delay::syncL), juce::jlimit (0, 10, juce::roundToInt (p (129, 0.625f) * 10.0f)));
        b.setChoice (delay (Delay::syncR), juce::jlimit (0, 10, juce::roundToInt (p (130, 0.625f) * 10.0f)));
        b.set (delay (Delay::timeL), 1.0f + p (129, 0.625f) * 1999.0f);
        b.set (delay (Delay::timeR), 1.0f + p (130, 0.625f) * 1999.0f);
        b.setChoice (delay (Delay::mode), juce::roundToInt (p (131, 0.0f) * 2.0f));
        b.set (delay (Delay::feedback), p (132, 0.4f));

        b.set (comp (Comp::threshold), (p (135, 0.5f) - 1.0f) * 60.0f);
        b.set (comp (Comp::ratio), 1.0f + p (136, 0.75f) * p (136, 0.75f) * 8.0f);
        b.set (comp (Comp::attack), 0.1f + p (137, 0.3f) * p (137, 0.3f) * 500.0f);
        b.set (comp (Comp::release), 5.0f + p (138, 0.3f) * p (138, 0.3f) * 1995.0f);
        b.set (comp (Comp::gain), p (139, 0.0f) * 30.0f);
        b.setBool (comp (Comp::multiband), p (140, 0.0f) > 0.5f);
        b.set (comp (Comp::mix), p (269, 1.0f));
        b.set (comp (Comp::lowGain), p (270, 0.5f) * 2.0f);
        b.set (comp (Comp::midGain), p (271, 0.5f) * 2.0f);
        b.set (comp (Comp::highGain), p (272, 0.5f) * 2.0f);

        b.set (fxFilter (FxFilter::mix), p (141, 1.0f));
        b.setChoice (fxFilter (FxFilter::type), serum1FilterType (p (142, 0.0f)));
        b.set (fxFilter (FxFilter::cutoff), maps::cutoffFrom01 (p (143, 0.5f)));
        b.set (fxFilter (FxFilter::reso), p (144, 0.0f));
        b.set (fxFilter (FxFilter::drive), p (145, 0.0f));
        b.set (fxFilter (FxFilter::var), p (146, 0.0f));
        b.set (fxFilter (FxFilter::stereo), bip (p (268, 0.5f)));

        b.set (hyper (Hyper::mix), p (147, 0.5f));
        b.set (hyper (Hyper::rate), p (148, 0.4f));
        b.set (hyper (Hyper::detune), p (149, 0.25f));
        b.set (hyper (Hyper::unison), (float) juce::jlimit (2, 7, juce::roundToInt (p (150, 0.571429f) * 7.0f)));
        b.setBool (hyper (Hyper::retrig), p (151, 0.0f) > 0.5f);
        b.set (hyper (Hyper::dimSize), p (152, 0.5f));
        b.set (hyper (Hyper::dimMix), p (153, 0.0f));

        b.setFxOrder (FxOrder::defaultOrder());
    }
}

int SerumImport::serum1ParamToCureum (int sp, int& module)
{
    module = 0;
    if (sp == 0) return global (Global::master);
    if (sp >= 1 && sp <= 26)
    {
        const int o = sp <= 13 ? 0 : 1;
        const int k = sp - (o == 0 ? 1 : 14);
        static const Osc map[] = { Osc::level, Osc::pan, Osc::octave, Osc::semi, Osc::fine, Osc::unison, Osc::detune, Osc::blend,
                                   Osc::warp1Amt, Osc::coarse, Osc::wtPos, Osc::randPhase, Osc::phase };
        module = o;
        return osc (o, map[k]);
    }
    switch (sp)
    {
        case 27: return noise (Noise::level);   case 28: return noise (Noise::pitch);
        case 29: return noise (Noise::fine);    case 30: return noise (Noise::pan);
        case 33: return sub (Sub::level);       case 34: return sub (Sub::pan);
        case 35: return env (0, Env::attack);   case 36: return env (0, Env::hold);   case 37: return env (0, Env::decay);
        case 38: return env (0, Env::sustain);  case 39: return env (0, Env::release);
        case 45: return filter (0, Filter::cutoff); case 46: return filter (0, Filter::reso); case 47: return filter (0, Filter::drive);
        case 48: return filter (0, Filter::var);    case 49: return filter (0, Filter::mix);  case 50: return filter (0, Filter::stereo);
        case 51: return env (1, Env::attack);   case 52: return env (1, Env::hold);   case 53: return env (1, Env::decay);
        case 54: return env (1, Env::sustain);  case 55: return env (1, Env::release);
        case 56: return env (2, Env::attack);   case 57: return env (2, Env::hold);   case 58: return env (2, Env::decay);
        case 59: return env (2, Env::sustain);  case 60: return env (2, Env::release);
        case 61: case 62: case 63: case 64: return lfo (sp - 61, Lfo::syncRate);
        case 69: return lfo (8, Lfo::rate);     case 70: return lfo (9, Lfo::rate);
        case 81: return reverb (Reverb::mix);   case 82: return reverb (Reverb::size); case 83: return reverb (Reverb::preDelay);
        case 84: return reverb (Reverb::lowCut);case 85: return reverb (Reverb::damp); case 86: return reverb (Reverb::highCut);
        case 87: return reverb (Reverb::width);
        case 88: return eq (Eq::lowFreq);       case 89: return eq (Eq::highFreq);     case 90: return eq (Eq::lowQ);
        case 91: return eq (Eq::highQ);         case 92: return eq (Eq::lowGain);      case 93: return eq (Eq::highGain);
        case 96: return dist (Dist::mix);       case 97: return dist (Dist::drive);    case 100: return dist (Dist::freq);
        case 101: return dist (Dist::bw);
        case 103: return flanger (Flanger::mix);case 105: return flanger (Flanger::rate); case 106: return flanger (Flanger::depth);
        case 107: return flanger (Flanger::feedback); case 108: return flanger (Flanger::stereo);
        case 109: return phaser (Phaser::mix);  case 111: return phaser (Phaser::rate); case 112: return phaser (Phaser::depth);
        case 113: return phaser (Phaser::freq); case 114: return phaser (Phaser::feedback); case 115: return phaser (Phaser::stereo);
        case 116: return chorus (Chorus::mix);  case 118: return chorus (Chorus::rate); case 119: return chorus (Chorus::delay1);
        case 120: return chorus (Chorus::delay2); case 121: return chorus (Chorus::depth); case 122: return chorus (Chorus::feedback);
        case 123: return chorus (Chorus::lpf);
        case 124: return delay (Delay::mix);    case 125: return delay (Delay::freq);  case 126: return delay (Delay::bw);
        case 129: return delay (Delay::timeL);  case 130: return delay (Delay::timeR); case 132: return delay (Delay::feedback);
        case 135: return comp (Comp::threshold);case 136: return comp (Comp::ratio);   case 137: return comp (Comp::attack);
        case 138: return comp (Comp::release);  case 139: return comp (Comp::gain);
        case 141: return fxFilter (FxFilter::mix); case 143: return fxFilter (FxFilter::cutoff); case 144: return fxFilter (FxFilter::reso);
        case 145: return fxFilter (FxFilter::drive); case 146: return fxFilter (FxFilter::var);
        case 147: return hyper (Hyper::mix);    case 148: return hyper (Hyper::rate);  case 149: return hyper (Hyper::detune);
        case 152: return hyper (Hyper::dimSize);case 153: return hyper (Hyper::dimMix);
        case 174: return osc (0, Osc::uniWarp); case 175: return osc (1, Osc::uniWarp);
        case 176: return osc (0, Osc::uniWtPos);case 177: return osc (1, Osc::uniWtPos);
        case 223: case 224: case 225: case 226: return lfo (sp - 223, Lfo::smooth);
        case 260: case 261: case 262: case 263: return lfo (sp - 256, Lfo::syncRate);
        case 264: case 265: case 266: case 267: return lfo (sp - 260, Lfo::smooth);
        case 268: return fxFilter (FxFilter::stereo);
        case 269: return comp (Comp::mix);      case 270: return comp (Comp::lowGain); case 271: return comp (Comp::midGain);
        case 272: return comp (Comp::highGain);
        default: break;
    }
    if (sp >= 273 && sp <= 280) return lfo (sp - 273, Lfo::rise);
    if (sp >= 281 && sp <= 288) return lfo (sp - 281, Lfo::delay);
    if (sp == 80) { module = 99; return -2; }   // master tune: handled specially
    return -1;
}

ImportedPreset SerumImport::importFxp (const void* data, size_t size, const juce::String& fileName)
{
    ImportedPreset result;
    result.sourceFormat = "Serum 1 (FXP)";
    auto* p = static_cast<const juce::uint8*> (data);
    if (size < 60 || std::memcmp (p, "CcnK", 4) != 0)
    {
        result.error = "Not a VST preset (missing CcnK header).";
        return result;
    }
    const bool opaque = std::memcmp (p + 8, "FPCh", 4) == 0;
    const bool params = std::memcmp (p + 8, "FxCk", 4) == 0;
    if (! opaque && ! params)
    {
        result.error = "Unsupported FXP type.";
        return result;
    }

    PresetBuilder b;
    b.category = "Serum 1 Import";
    b.name = readString (juce::MemoryBlock (p + 28, 28), 0, 28);
    if (std::memcmp (p + 16, "XfsX", 4) != 0)
        b.warnings.add ("This FXP was not saved by Serum (plug-in id " + juce::String::fromUTF8 (reinterpret_cast<const char*> (p + 16), 4)
                        + "); CUREUM interprets it as a Serum preset anyway.");

    if (params)
    {
        const int n = (int) juce::ByteOrder::bigEndianInt (p + 24);
        std::vector<float> values ((size_t) std::max (0, n));
        for (int i = 0; i < n && 56 + (size_t) i * 4 + 4 <= size; ++i)
        {
            const auto bits = juce::ByteOrder::bigEndianInt (p + 56 + (size_t) i * 4);
            std::memcpy (&values[(size_t) i], &bits, 4);
        }
        applyParams (b, values.data(), (int) values.size());
    }
    else
    {
        const juce::uint32 chunkSize = juce::ByteOrder::bigEndianInt (p + 56);
        if (60 + (size_t) chunkSize > size || chunkSize < 8)
        {
            result.error = "The preset chunk is truncated.";
            return result;
        }
        const auto* chunk = p + 60;
        // The last four bytes hold the compressed size of the parameter blob; a second zlib stream
        // (embedded wavetable frames) may follow it.
        const juce::uint32 firstLen = juce::ByteOrder::littleEndianInt (chunk + chunkSize - 4);
        const bool trailerValid = firstLen > 8 && firstLen <= chunkSize - 4;

        juce::MemoryBlock blob, frames;
        if (! inflate (chunk, trailerValid ? firstLen : chunkSize, blob) || blob.getSize() < kParamOffset + 4 * 80)
        {
            result.error = "Could not decompress the Serum preset data.";
            return result;
        }
        if (trailerValid && chunkSize - 4 > firstLen + 8)
            inflate (chunk + firstLen, chunkSize - 4 - firstLen, frames);

        const int count = (int) std::min<size_t> (kNumS1Params, (blob.getSize() - kParamOffset) / 4);
        std::vector<float> values ((size_t) count);
        std::memcpy (values.data(), static_cast<const char*> (blob.getData()) + kParamOffset, (size_t) count * 4);
        applyParams (b, values.data(), count);

        // Names and descriptive text
        const auto n = readString (blob, kPresetName, 46);
        if (n.isNotEmpty()) b.name = n;
        b.author = readString (blob, kAuthor, 48);
        b.description = readString (blob, kDescription, 136);
        for (int m = 0; m < 4; ++m)
        {
            const auto mn = readString (blob, kMacroNames + (size_t) m * 32, 32);
            if (mn.isNotEmpty()) b.setMacroName (m, mn);
        }

        // Wavetables: referenced by path, or embedded as raw 2048-sample frames.
        const auto pathA = readString (blob, kOscAPath, kPathLength);
        const auto pathB = readString (blob, kOscBPath, kPathLength);
        const int embeddedFrames = (int) (frames.getSize() / (sizeof (float) * Wavetable::kFrameSize));
        int framesA = 0, framesB = 0;
        if (embeddedFrames > 0)
        {
            if (pathA.isEmpty() && pathB.isNotEmpty()) framesA = embeddedFrames;
            else if (pathA.isNotEmpty() && pathB.isEmpty()) framesB = embeddedFrames;
            else if (pathA.isEmpty() && pathB.isEmpty()) { framesA = (embeddedFrames + 1) / 2; framesB = embeddedFrames - framesA; }
        }
        const float* frameData = static_cast<const float*> (frames.getData());
        auto embedded = [&] (int start, int count2, const juce::String& nm) -> WavetablePtr
        {
            if (count2 <= 0) return nullptr;
            std::vector<float> f (frameData + (size_t) start * Wavetable::kFrameSize, frameData + (size_t) (start + count2) * Wavetable::kFrameSize);
            Wavetable::Options o;
            o.normalise = true;
            return Wavetable::create (nm, std::move (f), o);
        };
        if (pathA.isNotEmpty()) { if (auto t = resolveWavetable (pathA, b.warnings)) b.setWavetable (0, t); }
        else if (auto t = embedded (0, framesA, b.name + " A")) b.setWavetable (0, t);
        else b.setWavetable (0, WavetableFactory::get ("Saw"));
        if (pathB.isNotEmpty()) { if (auto t = resolveWavetable (pathB, b.warnings)) b.setWavetable (1, t); }
        else if (auto t = embedded (framesA, framesB, b.name + " B")) b.setWavetable (1, t);
        else b.setWavetable (1, WavetableFactory::get ("Saw"));

        const auto noisePath = readString (blob, kNoisePath, kPathLength);
        if (noisePath.isNotEmpty()) applyNoiseSample (b, noisePath);

        // Modulation matrix: 16 records of 40 bytes at the start of the blob.
        int skipped = 0;
        auto* raw = static_cast<const juce::uint8*> (blob.getData());
        for (int s = 0; s < kMatrixSlots; ++s)
        {
            const auto* rec = raw + kMatrixOffset + (size_t) s * kMatrixStride;
            float amount, output;
            std::memcpy (&amount, rec + 4, 4);
            std::memcpy (&output, rec + 8, 4);
            const int src = juce::ByteOrder::littleEndianShort (rec + 0x14);
            const int aux = juce::ByteOrder::littleEndianShort (rec + 0x16);
            const int dst = juce::ByteOrder::littleEndianShort (rec + 0x1a);
            if (src == 0 || ! std::isfinite (amount) || std::abs (amount) > 1.0001f || src > 64 || dst > 400)
                continue;
            const auto source = serum1Source (src);
            const auto auxSource = serum1Source (aux);
            int module = 0;
            const int dest = serum1ParamToCureum (dst, module);
            if (source == ModSource::none) { ++skipped; continue; }
            const float out = std::isfinite (output) ? juce::jlimit (0.0f, 1.0f, output) : 1.0f;
            if (dest == -2)
            {
                // Master tune: modulate the pitch of every oscillator (assumes a +/-1 semitone range).
                for (int o = 0; o < 2; ++o)
                    b.addModulation (source, osc (o, Osc::coarse), amount / 48.0f, false, auxSource, 0.0f, out);
                b.addModulation (source, sub (Sub::coarse), amount / 12.0f, false, auxSource, 0.0f, out);
                continue;
            }
            if (dest < 0 || ! ParamRegistry::get().spec (dest).isModulatable()) { ++skipped; continue; }
            b.addModulation (source, dest, amount, false, auxSource, 0.0f, out);
        }
        if (skipped > 0)
            b.warnings.add (juce::String (skipped) + " Serum 1 matrix routing(s) target controls CUREUM cannot modulate and were skipped.");
        b.warnings.addIfNotAlreadyThere ("Serum 1 LFO shapes are not imported (CUREUM uses its default triangle).");
    }

    if (b.name.isEmpty()) b.name = juce::File::createFileWithoutCheckingPath (fileName).getFileNameWithoutExtension();
    result.name = b.name;
    result.state = b.build();
    result.warnings = b.warnings;
    return result;
}

std::vector<ImportedPreset> SerumImport::importFxb (const void* data, size_t size, const juce::String& fileName)
{
    std::vector<ImportedPreset> out;
    auto* p = static_cast<const juce::uint8*> (data);
    if (size < 160 || std::memcmp (p, "CcnK", 4) != 0)
        return out;
    const int numPrograms = (int) juce::ByteOrder::bigEndianInt (p + 24);
    const size_t base = 156;

    if (std::memcmp (p + 8, "FxBk", 4) == 0)
    {
        size_t off = base;
        for (int i = 0; i < numPrograms && off + 60 <= size; ++i)
        {
            if (std::memcmp (p + off, "CcnK", 4) != 0) break;
            const size_t len = (size_t) juce::ByteOrder::bigEndianInt (p + off + 4) + 8;
            if (off + len > size) break;
            auto r = importFxp (p + off, len, fileName + " #" + juce::String (i + 1));
            if (r.ok()) out.push_back (std::move (r));
            off += len;
        }
        return out;
    }

    // Opaque bank chunk: look for embedded FXP programs first, then for raw parameter blobs.
    const size_t chunkSize = juce::ByteOrder::bigEndianInt (p + base);
    const size_t start = base + 4;
    const size_t end = std::min (size, start + chunkSize);
    for (size_t off = start; off + 60 < end; ++off)
    {
        if (std::memcmp (p + off, "CcnK", 4) == 0 && std::memcmp (p + off + 8, "FPCh", 4) == 0)
        {
            const size_t len = (size_t) juce::ByteOrder::bigEndianInt (p + off + 4) + 8;
            if (off + len <= end)
            {
                auto r = importFxp (p + off, len, fileName);
                if (r.ok()) out.push_back (std::move (r));
                off += len - 1;
            }
        }
    }
    return out;
}

ImportedPreset SerumImport::importFile (const juce::File& file)
{
    ImportedPreset r;
    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb))
    {
        r.error = "Could not read " + file.getFullPathName();
        return r;
    }
    switch (detect (mb.getData(), mb.getSize()))
    {
        case Kind::serum2:    return importSerum2 (mb.getData(), mb.getSize(), file.getFileName());
        case Kind::serum1Fxp: return importFxp (mb.getData(), mb.getSize(), file.getFileName());
        case Kind::serum1Fxb:
        {
            auto all = importFxb (mb.getData(), mb.getSize(), file.getFileName());
            if (! all.empty()) return all.front();
            r.error = "The bank contains no readable presets.";
            return r;
        }
        case Kind::cureum:
        {
            r.state = StateFormat::readPresetData (mb.getData(), mb.getSize());
            r.sourceFormat = "CUREUM";
            r.name = r.state.getChildWithName ("Meta").getProperty ("name").toString();
            if (! r.state.isValid()) r.error = "The CUREUM preset is damaged.";
            return r;
        }
        case Kind::archive:
        case Kind::unknown: break;
    }
    r.error = "Unsupported preset format: " + file.getFileName();
    return r;
}

std::vector<ImportedPreset> SerumImport::importBank (const juce::File& source, const juce::File& extractFolder, std::function<bool (float)> progress)
{
    std::vector<ImportedPreset> out;
    auto addAll = [&out] (std::vector<ImportedPreset> v) { for (auto& r : v) out.push_back (std::move (r)); };

    if (source.isDirectory())
    {
        juce::Array<juce::File> files;
        for (const auto& e : juce::RangedDirectoryIterator (source, true, "*", juce::File::findFiles))
            if (isImportableExtension (e.getFile().getFileExtension().toLowerCase()))
                files.add (e.getFile());
        for (int i = 0; i < files.size(); ++i)
        {
            if (progress && ! progress ((float) i / (float) files.size())) break;
            const auto ext = files[i].getFileExtension().toLowerCase();
            if (ext == ".fxb" || ext == ".zip" || ext == ".serumpack")
                addAll (importBank (files[i], extractFolder, {}));
            else
            {
                auto r = importFile (files[i]);
                if (r.ok()) out.push_back (std::move (r));
            }
        }
        return out;
    }

    juce::MemoryBlock mb;
    if (! source.loadFileAsData (mb))
        return out;
    const auto kind = detect (mb.getData(), mb.getSize());

    if (kind == Kind::serum1Fxb)
        return importFxb (mb.getData(), mb.getSize(), source.getFileName());

    if (kind == Kind::archive)
    {
        // Extract everything (presets, wavetables, noises) so referenced files can be found, then
        // import the presets with the extracted folder temporarily added to the Serum folders.
        juce::ZipFile zip (source);
        extractFolder.createDirectory();
        zip.uncompressTo (extractFolder, true);
        auto folders = Settings::get().getSerumFolders();
        const auto original = folders;
        folders.addIfNotAlreadyThere (extractFolder.getFullPathName());
        Settings::get().setSerumFolders (folders);
        rescanSerumFolders();
        juce::Array<juce::File> files;
        for (const auto& e : juce::RangedDirectoryIterator (extractFolder, true, "*", juce::File::findFiles))
        {
            const auto ext = e.getFile().getFileExtension().toLowerCase();
            if (ext == ".serumpreset" || ext == ".fxp" || ext == ".fxb" || ext == ".cureum")
                files.add (e.getFile());
        }
        for (int i = 0; i < files.size(); ++i)
        {
            if (progress && ! progress ((float) i / (float) std::max (1, files.size()))) break;
            if (files[i].hasFileExtension ("fxb")) addAll (importBank (files[i], extractFolder, {}));
            else { auto r = importFile (files[i]); if (r.ok()) out.push_back (std::move (r)); }
        }
        Settings::get().setSerumFolders (original);
        rescanSerumFolders();
        return out;
    }

    // Unknown container (e.g. a non-zip .SerumPack): scan for embedded Serum 2 presets.
    auto* p = static_cast<const juce::uint8*> (mb.getData());
    const size_t n = mb.getSize();
    for (size_t off = 0; off + 32 < n; ++off)
    {
        if (p[off] != 'X' || std::memcmp (p + off, "XferJson\0", 9) != 0)
            continue;
        const juce::uint32 jlen = juce::ByteOrder::littleEndianInt (p + off + 9);
        const size_t zOff = off + 17 + jlen + 8;
        if (zOff >= n) break;
        const size_t frame = ZSTD_findFrameCompressedSize (p + zOff, n - zOff);
        if (ZSTD_isError (frame)) continue;
        auto r = importSerum2 (p + off, zOff + frame - off, source.getFileName());
        if (r.ok()) out.push_back (std::move (r));
        off = zOff + frame - 1;
        if (progress && ! progress ((float) off / (float) n)) break;
    }
    return out;
}

} // namespace cureum
