#include "SerumImport.h"
#include "SerumImportInternal.h"
#include "Cbor.h"
#include "WavetableIO.h"
#include "../Core/Settings.h"
#include "../Core/ValueMaps.h"
#include "../DSP/WavetableFactory.h"
#include <zstd.h>
#include <map>
#include <mutex>

namespace cureum
{

using namespace pid;

namespace serumdetail
{
    //==========================================================================
    // Name matching helpers
    juce::String normaliseName (juce::String s)
    {
        s = s.trim();
        if (s.length() > 1 && s[0] == 'k' && juce::CharacterFunctions::isUpperCase (s[1]))
            s = s.substring (1);
        s = s.replace ("+/-", "plusminus").replace ("+", "plus").replace ("-", "minus");
        juce::String out;
        for (auto c : s.toLowerCase())
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                out << c;
        return out;
    }

    float num (const juce::var& obj, const char* key, float def)
    {
        const auto v = obj[key];
        if (v.isDouble() || v.isInt() || v.isInt64() || v.isBool())
            return (float) (double) v;
        return def;
    }
    bool has (const juce::var& obj, const char* key)
    {
        if (auto* o = obj.getDynamicObject()) return o->hasProperty (key);
        return false;
    }
    juce::String str (const juce::var& obj, const char* key) { return obj[key].toString(); }

    // plainParams is either the string "default" or an object of parameter values.
    juce::var plain (const juce::var& module)
    {
        const auto p = module["plainParams"];
        return p.isObject() ? p : juce::var (new juce::DynamicObject());
    }

    float pct (float v) { return v / 100.0f; }

    //==========================================================================
    // Index of Serum resource files (wavetables, noises) in the configured Serum folders.
    struct SerumLibrary
    {
        std::mutex mutex;
        bool built = false;
        std::multimap<juce::String, juce::File> byName;   // lower-case file name -> file

        void build()
        {
            byName.clear();
            for (auto& root : Settings::get().getSerumFolders())
            {
                juce::File dir (root);
                if (! dir.isDirectory()) continue;
                for (const auto& entry : juce::RangedDirectoryIterator (dir, true, "*.wav;*.WAV;*.aif;*.aiff;*.flac", juce::File::findFiles))
                    byName.emplace (entry.getFile().getFileName().toLowerCase(), entry.getFile());
            }
            built = true;
        }

        juce::File find (const juce::String& relative, bool noise)
        {
            const std::lock_guard<std::mutex> lock (mutex);
            auto rel = relative.replaceCharacter ('\\', '/').trimCharactersAtStart ("/");
            if (rel.isEmpty()) return {};
            for (auto& root : Settings::get().getSerumFolders())
            {
                const juce::File r (root);
                for (auto sub : { noise ? "Noises" : "Tables", "", "Wavetables", "Samples" })
                {
                    auto f = juce::String (sub).isEmpty() ? r.getChildFile (rel) : r.getChildFile (sub).getChildFile (rel);
                    if (f.existsAsFile()) return f;
                }
            }
            if (! built) build();
            const auto fileName = rel.fromLastOccurrenceOf ("/", false, false).toLowerCase();
            const auto folder = rel.upToLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("/", false, false).toLowerCase();
            juce::File best;
            auto range = byName.equal_range (fileName);
            for (auto it = range.first; it != range.second; ++it)
            {
                if (best == juce::File()) best = it->second;
                if (folder.isNotEmpty() && it->second.getParentDirectory().getFileName().toLowerCase() == folder)
                    return it->second;
            }
            return best;
        }
    };

    SerumLibrary& library() { static SerumLibrary lib; return lib; }

    WavetablePtr resolveWavetable (const juce::String& relPath, juce::StringArray& warnings)
    {
        if (relPath.trim().isEmpty())
            return nullptr;
        const auto file = library().find (relPath, false);
        if (file.existsAsFile())
        {
            juce::String err;
            if (auto t = WavetableIO::loadWavetable (file, err))
                return t;
        }
        const auto tableName = relPath.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false);
        const auto match = WavetableFactory::closestMatch (tableName.isNotEmpty() ? tableName : relPath);
        warnings.addIfNotAlreadyThere ("Wavetable \"" + relPath + "\" is not installed; using CUREUM's \"" + match
                                       + "\" instead. Point Settings > Serum folders to your Serum tables to load the original.");
        return WavetableFactory::get (match);
    }

    NoiseType noiseFromName (const juce::String& n)
    {
        const auto s = n.toLowerCase();
        struct R { const char* k; NoiseType t; };
        static constexpr R rules[] = {
            { "pink", NoiseType::pink }, { "brown", NoiseType::brown }, { "blue", NoiseType::blue }, { "violet", NoiseType::violet },
            { "grey", NoiseType::grey }, { "gray", NoiseType::grey }, { "vinyl", NoiseType::vinyl }, { "crackl", NoiseType::crackle },
            { "tape", NoiseType::tapeHiss }, { "hiss", NoiseType::tapeHiss }, { "wind", NoiseType::wind }, { "air", NoiseType::wind },
            { "rain", NoiseType::rain }, { "hum", NoiseType::hum60 }, { "metal", NoiseType::metallic }, { "breath", NoiseType::breath },
            { "static", NoiseType::staticBurst }, { "radio", NoiseType::staticBurst }, { "click", NoiseType::click }, { "kick", NoiseType::thump },
            { "thump", NoiseType::thump }, { "snap", NoiseType::snap }, { "snare", NoiseType::snap }, { "digital", NoiseType::digital },
            { "bit", NoiseType::digital }, { "white", NoiseType::white }
        };
        for (auto& r : rules)
            if (s.contains (r.k)) return r.t;
        return NoiseType::white;
    }

    void applyNoiseSample (PresetBuilder& b, const juce::String& relPath)
    {
        if (relPath.trim().isEmpty()) return;
        const auto file = library().find (relPath, true);
        if (file.existsAsFile())
        {
            b.setNoiseFile (file);
            b.setChoice (noise (Noise::type), (int) NoiseType::custom);
            return;
        }
        const auto t = noiseFromName (relPath);
        b.setChoice (noise (Noise::type), (int) t);
        b.warnings.addIfNotAlreadyThere ("Noise sample \"" + relPath + "\" is not installed; using the built-in \"" + noiseTypeNames()[(int) t] + "\" noise.");
    }

    int distModeFromName (const juce::String& n)
    {
        const auto s = normaliseName (n);
        static const std::map<juce::String, DistMode> m {
            { "tube", DistMode::tube }, { "softclip", DistMode::softClip }, { "hardclip", DistMode::hardClip },
            { "diode1", DistMode::diode1 }, { "diode2", DistMode::diode2 }, { "linfold", DistMode::linFold },
            { "linearfold", DistMode::linFold }, { "sinfold", DistMode::sinFold }, { "sinefold", DistMode::sinFold },
            { "zerosquare", DistMode::zeroSquare }, { "zerosqr", DistMode::zeroSquare }, { "downsample", DistMode::downsample },
            { "downsmp", DistMode::downsample }, { "asym", DistMode::asym }, { "rectify", DistMode::rectify }, { "rect", DistMode::rectify },
            { "xshaper", DistMode::xShaper }, { "xshaperasym", DistMode::xShaperAsym }, { "sineshaper", DistMode::sineShaper },
            { "stompbox", DistMode::stompBox }, { "stomp", DistMode::stompBox }, { "bitcrush", DistMode::bitCrush },
            { "fuzz", DistMode::fuzz }, { "overdrive", DistMode::overdrive }
        };
        if (auto it = m.find (s); it != m.end()) return (int) it->second;
        if (s.contains ("diode")) return (int) DistMode::diode1;
        if (s.contains ("fold")) return (int) DistMode::sinFold;
        if (s.contains ("clip")) return (int) DistMode::softClip;
        return (int) DistMode::tube;
    }
}

using namespace serumdetail;

//==============================================================================
int SerumImport::filterTypeFromSerumName (const juce::String& name)
{
    static const std::map<juce::String, int> exact = []
    {
        std::map<juce::String, int> m;
        const auto& t = filterTypes();
        for (int i = 0; i < (int) t.size(); ++i)
            m[normaliseName (t[(size_t) i].name)] = i;
        return m;
    }();
    const auto s = normaliseName (name);
    if (s.isEmpty() || s == "none" || s == "off")
        return -1;
    if (auto it = exact.find (s); it != exact.end())
        return it->second;

    // Fuzzy fallback by keywords and slope.
    auto slopeIndex = [&s] (int base6, int base12, int base18, int base24)
    {
        if (s.contains ("24")) return base24;
        if (s.contains ("18")) return base18;
        if (s.contains ("6") && ! s.contains ("12")) return base6;
        return base12;
    };
    auto find = [] (const char* n) { auto names = filterTypeNames(); return std::max (0, names.indexOf (n)); };
    if (s.contains ("mg") || s.contains ("ladder") || s.contains ("moog"))
        return slopeIndex (find ("MG Low 6"), find ("MG Low 12"), find ("MG Low 18"), find ("MG Low 24"));
    if (s.contains ("diode") || s.contains ("acid")) return find ("Diode LP");
    if (s.contains ("french")) return find ("French LP");
    if (s.contains ("german")) return find ("German LP");
    if (s.contains ("scream")) return s.contains ("bp") ? find ("Scream BP") : find ("Scream LP");
    if (s.contains ("formant") || s.contains ("vowel")) return find ("Formant-I");
    if (s.contains ("comb") || s.contains ("cmb")) return s.contains ("minus") ? find ("Cmb -") : find ("Cmb +");
    if (s.contains ("flg") || s.contains ("flang")) return s.contains ("minus") ? find ("Flg -") : find ("Flg +");
    if (s.contains ("phs") || s.contains ("phase")) return slopeIndex (find ("Phs 12+"), find ("Phs 12+"), find ("Phs 24+"), find ("Phs 24+"));
    if (s.contains ("ring")) return find ("Ring Mod");
    if (s.contains ("samp") || s.contains ("hold")) return find ("SampHold");
    if (s.contains ("reverb")) return find ("Reverb");
    if (s.contains ("reject")) return find ("Bandreject");
    if (s.contains ("bass")) return find ("Add Bass");
    if (s.contains ("eq")) return find ("Band EQ 12");
    if (s.contains ("notch")) return slopeIndex (find ("Notch 12"), find ("Notch 12"), find ("Notch 24"), find ("Notch 24"));
    if (s.contains ("peak")) return slopeIndex (find ("Peak 12"), find ("Peak 12"), find ("Peak 24"), find ("Peak 24"));
    if (s.contains ("band") || s.startsWith ("bp")) return slopeIndex (find ("Band 12"), find ("Band 12"), find ("Band 24"), find ("Band 24"));
    if (s.contains ("high") || s.startsWith ("hp")) return slopeIndex (find ("High 6"), find ("High 12"), find ("High 18"), find ("High 24"));
    if (s.contains ("low") || s.startsWith ("lp")) return slopeIndex (find ("Low 6"), find ("Low 12"), find ("Low 18"), find ("Low 24"));
    return kDefaultFilterType;
}

WarpMode SerumImport::warpFromSerumName (const juce::String& name)
{
    const auto s = normaliseName (name);
    static const std::map<juce::String, WarpMode> m {
        { "none", WarpMode::off }, { "off", WarpMode::off }, { "sync", WarpMode::sync }, { "syncwin", WarpMode::syncWindow },
        { "syncwindow", WarpMode::syncWindow }, { "windowsync", WarpMode::syncWindow }, { "bendplus", WarpMode::bendPlus },
        { "bendminus", WarpMode::bendMinus }, { "bendplusminus", WarpMode::bendPlusMinus }, { "bendpm", WarpMode::bendPlusMinus },
        { "pwm", WarpMode::pwm }, { "asymplus", WarpMode::asymPlus }, { "asymminus", WarpMode::asymMinus },
        { "asymplusminus", WarpMode::asymPlusMinus }, { "flip", WarpMode::flip }, { "mirror", WarpMode::mirror },
        { "remap1", WarpMode::remap1 }, { "remap2", WarpMode::remap2 }, { "remap3", WarpMode::remap3 }, { "remap4", WarpMode::remap4 },
        { "quantize", WarpMode::quantize }, { "quant", WarpMode::quantize }, { "fmosc", WarpMode::fmOsc }, { "amosc", WarpMode::amOsc },
        { "rmosc", WarpMode::rmOsc }, { "fmnoise", WarpMode::fmNoise }, { "amnoise", WarpMode::amNoise }, { "rmnoise", WarpMode::rmNoise },
        { "fmsub", WarpMode::fmSub }, { "amsub", WarpMode::amSub }, { "rmsub", WarpMode::rmSub }, { "pdosc", WarpMode::pmOsc },
        { "pmosc", WarpMode::pmOsc }, { "pmself", WarpMode::pmSelf }, { "pd", WarpMode::phaseDistort }, { "phasedistortion", WarpMode::phaseDistort },
        { "squash", WarpMode::squash }, { "fold", WarpMode::fold }, { "bitcrush", WarpMode::bitCrush }, { "fma", WarpMode::fmA },
        { "fmb", WarpMode::fmB }, { "fmc", WarpMode::fmC }
    };
    if (auto it = m.find (s); it != m.end()) return it->second;
    if (s.contains ("sync")) return s.contains ("win") ? WarpMode::syncWindow : WarpMode::sync;
    if (s.contains ("bend")) return WarpMode::bendPlusMinus;
    if (s.contains ("asym")) return WarpMode::asymPlusMinus;
    if (s.startsWith ("fm")) return s.contains ("noise") ? WarpMode::fmNoise : (s.contains ("sub") ? WarpMode::fmSub : WarpMode::fmOsc);
    if (s.startsWith ("am")) return s.contains ("noise") ? WarpMode::amNoise : (s.contains ("sub") ? WarpMode::amSub : WarpMode::amOsc);
    if (s.startsWith ("rm")) return s.contains ("noise") ? WarpMode::rmNoise : (s.contains ("sub") ? WarpMode::rmSub : WarpMode::rmOsc);
    if (s.startsWith ("pd") || s.startsWith ("pm")) return WarpMode::pmOsc;
    if (s.contains ("remap")) return WarpMode::remap1;
    if (s.contains ("pwm")) return WarpMode::pwm;
    return WarpMode::off;
}

// Serum 1 modulation-matrix source numbering (derived from real presets: e.g. presets described
// as "mod wheel to ..." route source 1; filter envelopes use source 3 = Env 2).
ModSource SerumImport::serum1Source (int id)
{
    switch (id)
    {
        case 1:  return ModSource::modWheel;
        case 2:  return ModSource::env1;
        case 3:  return ModSource::env2;
        case 4:  return ModSource::env3;
        case 5:  return ModSource::lfo1;
        case 6:  return ModSource::lfo2;
        case 7:  return ModSource::lfo3;
        case 8:  return ModSource::lfo4;
        case 9:  return ModSource::lfo5;
        case 10: return ModSource::lfo6;
        case 11: return ModSource::lfo7;
        case 12: return ModSource::lfo8;
        case 13: return ModSource::velocity;
        case 14: return ModSource::note;
        case 15: return ModSource::aftertouch;
        case 16: return ModSource::pitchBend;
        case 17: return ModSource::macro1;
        case 18: return ModSource::macro2;
        case 19: return ModSource::macro3;
        case 20: return ModSource::macro4;
        case 21: return ModSource::random1;
        case 22: return ModSource::random2;
        case 23: return ModSource::lfo9;   // Chaos 1 (CUREUM LFO 9, Lorenz)
        case 24: return ModSource::lfo10;  // Chaos 2 (CUREUM LFO 10, Rossler)
        default: return ModSource::none;
    }
}

// Serum 2 source ids ([type, aux]). Mod wheel, envelopes and LFOs are documented by community
// research; the remaining ids are best guesses.
ModSource SerumImport::serum2Source (int id, int aux)
{
    if (id == 1) return ModSource::modWheel;
    if (id >= 2 && id <= 5) return envSource (id - 2);
    if (id >= 6 && id <= 15) return lfoSource (id - 6);
    if (id == 16) return macroSource (juce::jlimit (0, kNumMacros - 1, aux));
    if (id >= 17 && id <= 23) return macroSource (id - 16);
    switch (id)
    {
        case 24: return ModSource::velocity;
        case 25: return ModSource::note;
        case 26: return ModSource::aftertouch;
        case 27: return ModSource::polyAftertouch;
        case 28: return ModSource::pitchBend;
        case 29: return ModSource::random1;
        case 30: return ModSource::random2;
        default: return ModSource::none;
    }
}

juce::File SerumImport::findSerumResource (const juce::String& relativePath, bool isNoise)
{
    return library().find (relativePath, isNoise);
}

void SerumImport::rescanSerumFolders()
{
    const std::lock_guard<std::mutex> lock (library().mutex);
    library().built = false;
}

bool SerumImport::isImportableExtension (const juce::String& ext)
{
    return ext == ".serumpreset" || ext == ".fxp" || ext == ".fxb" || ext == ".cureum" || ext == ".zip" || ext == ".serumpack";
}

SerumImport::Kind SerumImport::detect (const void* data, size_t size)
{
    auto* p = static_cast<const char*> (data);
    if (size >= 9 && std::memcmp (p, "XferJson", 8) == 0) return Kind::serum2;
    if (size >= 12 && std::memcmp (p, "CcnK", 4) == 0)
    {
        if (std::memcmp (p + 8, "FPCh", 4) == 0 || std::memcmp (p + 8, "FxCk", 4) == 0) return Kind::serum1Fxp;
        if (std::memcmp (p + 8, "FBCh", 4) == 0 || std::memcmp (p + 8, "FxBk", 4) == 0) return Kind::serum1Fxb;
    }
    if (size >= 4 && std::memcmp (p, "PK\x03\x04", 4) == 0) return Kind::archive;
    if (size >= 2 && (juce::uint8) p[0] == 0x1f && (juce::uint8) p[1] == 0x8b) return Kind::cureum;
    if (size >= 7 && juce::String (p, 64).contains ("<CUREUM")) return Kind::cureum;
    return Kind::unknown;
}

} // namespace cureum
