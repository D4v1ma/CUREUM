#include "WavetableFactory.h"
#include "../Core/DspMath.h"
#include <map>
#include <mutex>

namespace cureum
{

namespace
{
    constexpr int N = Wavetable::kFrameSize;
    constexpr int kHarm = 512;
    using dsp::kPi;
    using dsp::kTwoPi;

    using SampleFn = std::function<float (float frame01, float x01)>;
    using HarmFn = std::function<float (float frame01, int harmonic)>;   // amplitude of sine harmonic

    std::vector<float> fromFormula (int frames, const SampleFn& fn)
    {
        std::vector<float> data ((size_t) frames * N);
        for (int f = 0; f < frames; ++f)
        {
            const float fp = frames > 1 ? (float) f / (float) (frames - 1) : 0.0f;
            for (int i = 0; i < N; ++i)
                data[(size_t) f * N + (size_t) i] = fn (fp, (float) i / (float) N);
        }
        return data;
    }

    std::vector<float> fromHarmonics (int frames, const HarmFn& fn, bool alternatePhase = false)
    {
        std::vector<float> data ((size_t) frames * N);
        std::vector<float> mags (kHarm), phases (kHarm, 0.0f);
        for (int f = 0; f < frames; ++f)
        {
            const float fp = frames > 1 ? (float) f / (float) (frames - 1) : 0.0f;
            for (int h = 1; h <= kHarm; ++h)
            {
                mags[(size_t) h - 1] = fn (fp, h);
                phases[(size_t) h - 1] = alternatePhase && (h % 2 == 0) ? kPi : 0.0f;
            }
            Wavetable::framesFromHarmonics (mags.data(), phases.data(), kHarm, data.data() + (size_t) f * N);
        }
        return data;
    }

    float saw (float x)      { return x < 0.5f ? 2.0f * x : 2.0f * x - 2.0f; }
    float square (float x)   { return x < 0.5f ? 1.0f : -1.0f; }
    float tri (float x)      { return x < 0.25f ? 4.0f * x : (x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f); }
    float pulse (float x, float w) { return x < w ? 1.0f : -1.0f; }

    float formantGain (float hz, int vowel)
    {
        static constexpr float F[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
        static constexpr float A[3] = { 1.0f, 0.5f, 0.25f };
        float g = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            const float bw = 80.0f + F[vowel][k] * 0.08f;
            const float d = (hz - F[vowel][k]) / bw;
            g += A[k] / (1.0f + d * d);
        }
        return g;
    }

    float vowelMorph (float hz, float t)   // t in [0,1] across A E I O U
    {
        const float p = dsp::clamp01 (t) * 4.0f;
        const int v0 = std::min (4, (int) p);
        const int v1 = std::min (4, v0 + 1);
        const float f = p - (float) v0;
        return formantGain (hz, v0) * (1.0f - f) + formantGain (hz, v1) * f;
    }

    std::vector<float> generate (const juce::String& name)
    {
        if (name == "Basic Shapes")
        {
            // Frame order matches Serum's "Basic Shapes" so imported WT positions land on the same shapes.
            return fromFormula (7, [] (float fp, float x)
            {
                const int f = juce::roundToInt (fp * 6.0f);
                switch (f)
                {
                    case 0: return std::sin (kTwoPi * x);
                    case 1: return saw (x);
                    case 2: return tri (x);
                    case 3: return square (x);
                    case 4: return pulse (x, 0.25f);
                    case 5: return pulse (x, 0.125f);
                    default: return std::abs (std::sin (kPi * x)) * 2.0f - 1.27f;
                }
            });
        }
        if (name == "Sine")     return fromFormula (1, [] (float, float x) { return std::sin (kTwoPi * x); });
        if (name == "Saw")      return fromFormula (1, [] (float, float x) { return saw (x); });
        if (name == "Square")   return fromFormula (1, [] (float, float x) { return square (x); });
        if (name == "Triangle") return fromFormula (1, [] (float, float x) { return tri (x); });
        if (name == "PWM")      return fromFormula (64, [] (float fp, float x) { return pulse (x, 0.5f - fp * 0.47f); });
        if (name == "Harmonic Sweep")
            return fromHarmonics (64, [] (float fp, int h) { const float top = 1.0f + fp * fp * 120.0f; return h <= (int) top ? 1.0f / (float) h : 0.0f; });
        if (name == "Odd Sweep")
            return fromHarmonics (64, [] (float fp, int h) { const float top = 1.0f + fp * fp * 120.0f; return (h % 2 == 1 && h <= (int) top) ? 1.0f / (float) h : 0.0f; });
        if (name == "Saw Bright")
            return fromHarmonics (32, [] (float fp, int h) { return 1.0f / std::pow ((float) h, 3.0f - fp * 2.0f); });
        if (name == "Formant Vowels")
            return fromHarmonics (64, [] (float fp, int h) { return vowelMorph (110.0f * (float) h, fp) / std::sqrt ((float) h); });
        if (name == "Vocal Choir")
            return fromHarmonics (64, [] (float fp, int h)
            {
                const float hz = 220.0f * (float) h;
                const float g = vowelMorph (hz, 0.75f + 0.25f * std::sin (fp * kPi)) * std::exp (-(float) h * 0.06f);
                return g / (float) h;
            });
        if (name == "Sync Sweep")
            return fromFormula (64, [] (float fp, float x) { const float r = 1.0f + fp * 7.0f; const float p = x * r; return saw (p - std::floor (p)) * (1.0f - x * 0.3f); });
        if (name == "FM Sweep")
            return fromFormula (64, [] (float fp, float x) { return std::sin (kTwoPi * x + fp * 6.0f * std::sin (kTwoPi * 2.0f * x)); });
        if (name == "FM Bells")
            return fromFormula (64, [] (float fp, float x) { return std::sin (kTwoPi * 2.0f * x + (0.5f + fp * 4.5f) * std::sin (kTwoPi * 7.0f * x)); });
        if (name == "Wavefold")
            return fromFormula (64, [] (float fp, float x) { return std::sin ((1.0f + fp * 9.0f) * std::sin (kTwoPi * x)); });
        if (name == "Triangle Fold")
            return fromFormula (32, [] (float fp, float x)
            {
                float v = tri (x) * (1.0f + fp * 6.0f);
                float t = (v + 1.0f) * 0.25f; t -= std::floor (t);
                return 4.0f * std::abs (t - 0.5f) - 1.0f;
            });
        if (name == "Square Fold")
            return fromFormula (32, [] (float fp, float x) { return std::sin (square (x) * std::sin (kTwoPi * x) * (1.0f + fp * 8.0f)); });
        if (name == "Chebyshev")
            return fromFormula (32, [] (float fp, float x)
            {
                const float c = std::cos (kTwoPi * x);
                const float order = 1.0f + fp * 15.0f;
                return std::cos (order * std::acos (juce::jlimit (-1.0f, 1.0f, c)));
            });
        if (name == "Digital Steps")
            return fromFormula (32, [] (float fp, float x) { const float levels = std::exp2 (7.0f - fp * 6.0f); return std::round (saw (x) * levels) / levels; });
        if (name == "Bitcrush Sine")
            return fromFormula (32, [] (float fp, float x) { const float levels = std::exp2 (6.0f - fp * 5.0f); return std::round (std::sin (kTwoPi * x) * levels) / levels; });
        if (name == "Spectral Comb")
            return fromHarmonics (64, [] (float fp, int h) { const float c = 0.5f + 0.5f * std::cos ((float) h * (0.3f + fp * 1.2f)); return c * c / std::sqrt ((float) h); });
        if (name == "Growl")
            return fromHarmonics (64, [] (float fp, int h)
            {
                const float hz = 55.0f * (float) h;
                return (vowelMorph (hz, fp) * 1.5f + 0.2f) / std::pow ((float) h, 0.7f) * (h % 3 == 0 ? 0.6f : 1.0f);
            });
        if (name == "Reese")
            return fromFormula (64, [] (float fp, float x) { float y = x + fp * 0.5f; y -= std::floor (y); return 0.5f * (saw (x) + saw (y)); });
        if (name == "Organ")
            return fromHarmonics (16, [] (float fp, int h)
            {
                static constexpr int bars[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                const int set = juce::roundToInt (fp * 15.0f);
                for (int b = 0; b < 9; ++b)
                    if (bars[b] == h)
                        return ((set >> (b % 4)) & 1) || b < 2 ? 1.0f / (1.0f + (float) b * 0.35f) : 0.15f;
                return 0.0f;
            });
        if (name == "Phase Distortion")
            return fromFormula (64, [] (float fp, float x)
            {
                const float k = 1.0f + fp * 15.0f;
                return std::cos (kTwoPi * k * x) * (1.0f - x) * 0.9f + 0.05f;
            });
        if (name == "Noise Cycles")
        {
            dsp::Random rng (4242u);
            std::vector<std::vector<float>> amps (32, std::vector<float> (kHarm));
            for (auto& a : amps) for (int h = 0; h < kHarm; ++h) a[(size_t) h] = rng.uniform() / std::sqrt ((float) (h + 1));
            return fromHarmonics (32, [amps] (float fp, int h) { return amps[(size_t) juce::roundToInt (fp * 31.0f)][(size_t) h - 1]; });
        }
        if (name == "Hollow")
            return fromHarmonics (32, [] (float fp, int h) { const int skip = 2 + juce::roundToInt (fp * 6.0f); return (h % skip == 0) ? 0.0f : 1.0f / (float) h; });
        if (name == "Sine Stack")
            return fromHarmonics (32, [] (float fp, int h)
            {
                const int maxOct = juce::roundToInt (fp * 5.0f);
                for (int o = 0; o <= maxOct; ++o) if (h == (1 << o)) return 1.0f / (1.0f + (float) o * 0.5f);
                return 0.0f;
            });
        if (name == "Supersaw Morph")
            return fromHarmonics (64, [] (float fp, int h) { return (1.0f / (float) h) * (1.0f + fp * 0.8f * std::sin ((float) h * 0.9f)); });
        return {};
    }
}

const juce::StringArray& WavetableFactory::names()
{
    static const juce::StringArray n {
        "Basic Shapes", "Sine", "Saw", "Square", "Triangle", "PWM", "Harmonic Sweep", "Odd Sweep", "Saw Bright",
        "Supersaw Morph", "Formant Vowels", "Vocal Choir", "Sync Sweep", "FM Sweep", "FM Bells", "Wavefold", "Triangle Fold",
        "Square Fold", "Chebyshev", "Digital Steps", "Bitcrush Sine", "Spectral Comb", "Growl", "Reese", "Organ",
        "Phase Distortion", "Noise Cycles", "Hollow", "Sine Stack"
    };
    return n;
}

WavetablePtr WavetableFactory::get (const juce::String& name)
{
    static std::mutex mutex;
    static std::map<juce::String, WavetablePtr> cache;
    const std::lock_guard<std::mutex> lock (mutex);
    if (auto it = cache.find (name); it != cache.end())
        return it->second;
    if (! names().contains (name))
        return nullptr;
    auto frames = generate (name);
    if (frames.empty())
        return nullptr;
    Wavetable::Options opts;
    opts.normalise = true;
    auto table = std::const_pointer_cast<Wavetable> (Wavetable::create (name, std::move (frames), opts));
    table->isFactory = true;
    table->sourcePath = "factory:" + name;
    cache[name] = table;
    return table;
}

juce::String WavetableFactory::closestMatch (const juce::String& foreign)
{
    const auto n = foreign.toLowerCase();
    for (auto& candidate : names())
        if (n.contains (candidate.toLowerCase()))
            return candidate;
    struct Rule { const char* key; const char* table; };
    static constexpr Rule rules[] = {
        { "default", "Basic Shapes" }, { "basic", "Basic Shapes" }, { "pwm", "PWM" }, { "pulse", "PWM" },
        { "sync", "Sync Sweep" }, { "fm", "FM Sweep" }, { "bell", "FM Bells" }, { "vowel", "Formant Vowels" },
        { "formant", "Formant Vowels" }, { "vox", "Vocal Choir" }, { "vocal", "Vocal Choir" }, { "choir", "Vocal Choir" },
        { "organ", "Organ" }, { "fold", "Wavefold" }, { "growl", "Growl" }, { "reese", "Reese" }, { "saw", "Saw Bright" },
        { "square", "Square" }, { "sqr", "Square" }, { "sine", "Sine" }, { "tri", "Triangle" }, { "digital", "Digital Steps" },
        { "bit", "Digital Steps" }, { "harm", "Harmonic Sweep" }, { "additive", "Harmonic Sweep" }, { "noise", "Noise Cycles" },
        { "comb", "Spectral Comb" }, { "cheby", "Chebyshev" }, { "pd", "Phase Distortion" }, { "analog", "Saw Bright" },
        { "j106", "Saw Bright" }, { "jno", "Saw Bright" }, { "arp", "PWM" }, { "super", "Supersaw Morph" }
    };
    for (auto& r : rules)
        if (n.contains (r.key))
            return r.table;
    return "Basic Shapes";
}

} // namespace cureum
