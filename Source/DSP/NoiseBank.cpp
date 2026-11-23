#include "Oscillators.h"

// Procedurally generated noise and texture sources for the noise oscillator. Everything is
// synthesised at start-up (no sample files are shipped), deterministic, and loop-friendly:
// each looped source is crossfaded at its loop point.

namespace cureum
{

namespace
{
    constexpr double kNoiseRate = 48000.0;

    struct OnePole
    {
        float a = 0.0f, z = 0.0f;
        void setLowpass (float hz) { a = std::exp (-dsp::kTwoPi * hz / (float) kNoiseRate); }
        float lp (float x) { z = x + a * (z - x); return z; }
        float hp (float x) { return x - lp (x); }
    };

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void bandpass (float hz, float q)
        {
            const float w = dsp::kTwoPi * hz / (float) kNoiseRate;
            const float alpha = std::sin (w) / (2.0f * q);
            const float a0 = 1.0f + alpha;
            b0 = alpha / a0; b1 = 0.0f; b2 = -alpha / a0;
            a1 = -2.0f * std::cos (w) / a0; a2 = (1.0f - alpha) / a0;
        }
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct Pink
    {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        float process (float w)
        {
            b0 = 0.99886f * b0 + w * 0.0555179f;
            b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f;
            b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f;
            b5 = -0.7616f * b5 - w * 0.0168980f;
            const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            return out * 0.11f;
        }
    };

    void normalise (std::vector<float>& v, float target = 0.7f)
    {
        float peak = 0.0f;
        for (auto s : v) peak = std::max (peak, std::abs (s));
        if (peak > 1.0e-6f)
            for (auto& s : v) s *= target / peak;
    }

    void crossfadeLoop (std::vector<float>& v, int fade)
    {
        const int n = (int) v.size();
        if (n < fade * 3) return;
        // Blend the tail into the head so the loop point is seamless, then drop the tail.
        for (int i = 0; i < fade; ++i)
        {
            const float t = (float) i / (float) fade;
            v[(size_t) i] = v[(size_t) i] * t + v[(size_t) (n - fade + i)] * (1.0f - t);
        }
        v.resize ((size_t) (n - fade));
    }

    using Gen = std::function<float (dsp::Random&, int, int)>;

    NoiseSample makeLooped (const juce::String& name, double seconds, uint32_t seed, const std::function<void (std::vector<float>&, dsp::Random&)>& fill)
    {
        NoiseSample s;
        s.name = name;
        s.sampleRate = kNoiseRate;
        const int len = (int) (seconds * kNoiseRate);
        const int fade = (int) (0.05 * kNoiseRate);
        for (int ch = 0; ch < 2; ++ch)
        {
            std::vector<float> buf ((size_t) (len + fade), 0.0f);
            dsp::Random rng (seed + (uint32_t) ch * 7919u);
            fill (buf, rng);
            crossfadeLoop (buf, fade);
            normalise (buf);
            (ch == 0 ? s.left : s.right) = std::move (buf);
        }
        return s;
    }

    NoiseSample makeOneShot (const juce::String& name, double seconds, uint32_t seed, const std::function<void (std::vector<float>&, dsp::Random&)>& fill)
    {
        NoiseSample s;
        s.name = name;
        s.sampleRate = kNoiseRate;
        s.oneShot = true;
        const int len = (int) (seconds * kNoiseRate);
        for (int ch = 0; ch < 2; ++ch)
        {
            std::vector<float> buf ((size_t) len, 0.0f);
            dsp::Random rng (seed + (uint32_t) ch * 104729u);
            fill (buf, rng);
            normalise (buf, 0.9f);
            (ch == 0 ? s.left : s.right) = std::move (buf);
        }
        return s;
    }
}

NoiseBank::NoiseBank()
{
    constexpr double len = 3.0;
    const auto T = [] (double s) { return (int) (s * kNoiseRate); };

    samples.resize ((size_t) NoiseType::count);
    auto set = [this] (NoiseType t, NoiseSample s) { samples[(size_t) t] = std::move (s); };

    set (NoiseType::white, makeLooped ("White", len, 1, [] (auto& b, auto& r) { for (auto& x : b) x = r.bipolar(); }));

    set (NoiseType::pink, makeLooped ("Pink", len, 2, [] (auto& b, auto& r) { Pink p; for (auto& x : b) x = p.process (r.bipolar()); }));

    set (NoiseType::brown, makeLooped ("Brown", len, 3, [] (auto& b, auto& r)
    {
        float z = 0.0f;
        for (auto& x : b) { z = 0.997f * z + 0.06f * r.bipolar(); x = z; }
    }));

    set (NoiseType::blue, makeLooped ("Blue", len, 4, [] (auto& b, auto& r)
    {
        Pink p; float prev = 0.0f;
        for (auto& x : b) { const float v = p.process (r.bipolar()); x = v - prev; prev = v; }
    }));

    set (NoiseType::violet, makeLooped ("Violet", len, 5, [] (auto& b, auto& r)
    {
        float prev = 0.0f;
        for (auto& x : b) { const float v = r.bipolar(); x = v - prev; prev = v; }
    }));

    set (NoiseType::grey, makeLooped ("Grey", len, 6, [] (auto& b, auto& r)
    {
        Pink p; OnePole hp; hp.setLowpass (2500.0f); OnePole lp; lp.setLowpass (180.0f);
        for (auto& x : b) { const float w = p.process (r.bipolar()); x = w + 0.6f * hp.hp (w) + 1.5f * lp.lp (w); }
    }));

    set (NoiseType::digital, makeLooped ("Digital", len, 7, [] (auto& b, auto& r)
    {
        float held = 0.0f; int count = 0, period = 4;
        for (auto& x : b)
        {
            if (--count <= 0) { held = std::round (r.bipolar() * 7.0f) / 7.0f; period = 1 + (int) (r.uniform() * 24.0f); count = period; }
            x = held;
        }
    }));

    set (NoiseType::crackle, makeLooped ("Crackle", len, 8, [] (auto& b, auto& r)
    {
        float env = 0.0f, sign = 1.0f;
        for (auto& x : b)
        {
            if (r.uniform() < 0.0009f) { env = 0.3f + r.uniform() * 0.7f; sign = r.bipolar() > 0 ? 1.0f : -1.0f; }
            x = env * sign * (0.6f + 0.4f * r.bipolar());
            env *= 0.93f;
        }
    }));

    set (NoiseType::vinyl, makeLooped ("Vinyl", len, 9, [] (auto& b, auto& r)
    {
        Pink p; OnePole hp; hp.setLowpass (1200.0f); OnePole rumble; rumble.setLowpass (35.0f);
        float env = 0.0f;
        for (auto& x : b)
        {
            if (r.uniform() < 0.0004f) env = 0.4f + r.uniform() * 0.6f;
            const float pop = env * r.bipolar();
            env *= 0.9f;
            x = 0.08f * hp.hp (p.process (r.bipolar())) + pop + 0.25f * rumble.lp (r.bipolar() * 4.0f);
        }
    }));

    set (NoiseType::tapeHiss, makeLooped ("Tape Hiss", len, 10, [] (auto& b, auto& r)
    {
        Pink p; OnePole hp; hp.setLowpass (3000.0f); OnePole lp; lp.setLowpass (14000.0f);
        int i = 0;
        for (auto& x : b)
        {
            const float wobble = 1.0f + 0.1f * std::sin ((float) i++ * 0.00023f);
            x = lp.lp (hp.hp (p.process (r.bipolar()))) * wobble;
        }
    }));

    set (NoiseType::wind, makeLooped ("Wind", 4.0, 11, [] (auto& b, auto& r)
    {
        Biquad bp; float target = 600.0f, freq = 600.0f, amp = 0.5f, ampT = 0.5f; int n = 0;
        for (auto& x : b)
        {
            if (n-- <= 0) { target = 250.0f + r.uniform() * 1400.0f; ampT = 0.25f + r.uniform() * 0.75f; n = 6000 + (int) (r.uniform() * 12000.0f); }
            freq += (target - freq) * 0.00008f;
            amp += (ampT - amp) * 0.00006f;
            bp.bandpass (freq, 2.5f);
            x = bp.process (r.bipolar()) * amp;
        }
    }));

    set (NoiseType::rain, makeLooped ("Rain", len, 12, [] (auto& b, auto& r)
    {
        Pink p; OnePole hp; hp.setLowpass (900.0f);
        Biquad drop; float env = 0.0f;
        for (auto& x : b)
        {
            if (r.uniform() < 0.0035f) { env = 0.3f + 0.7f * r.uniform(); drop.bandpass (1500.0f + r.uniform() * 5000.0f, 6.0f); }
            const float d = drop.process (r.bipolar()) * env;
            env *= 0.996f;
            x = 0.25f * hp.hp (p.process (r.bipolar())) + d * 3.0f;
        }
    }));

    auto hum = [] (float f0)
    {
        return [f0] (std::vector<float>& b, dsp::Random& r)
        {
            for (size_t i = 0; i < b.size(); ++i)
            {
                const float t = (float) i / (float) kNoiseRate;
                float v = 0.0f;
                for (int h = 1; h <= 9; ++h)
                    v += std::sin (dsp::kTwoPi * f0 * (float) h * t + (float) h) / (float) (h * (h % 2 ? 1 : 2));
                b[i] = v + 0.01f * r.bipolar();
            }
        };
    };
    set (NoiseType::hum50, makeLooped ("Hum 50 Hz", 1.0, 13, hum (50.0f)));
    set (NoiseType::hum60, makeLooped ("Hum 60 Hz", 1.0, 14, hum (60.0f)));

    set (NoiseType::metallic, makeLooped ("Metallic", len, 15, [] (auto& b, auto& r)
    {
        const int delays[] = { 113, 167, 229, 317 };
        std::vector<std::vector<float>> lines;
        for (int d : delays) lines.emplace_back ((size_t) d, 0.0f);
        int idx[4] = {};
        for (auto& x : b)
        {
            const float in = r.bipolar() * 0.05f;
            float out = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                auto& line = lines[(size_t) k];
                const float y = line[(size_t) idx[k]];
                line[(size_t) idx[k]] = in + y * 0.985f;
                idx[k] = (idx[k] + 1) % (int) line.size();
                out += y;
            }
            x = out;
        }
    }));

    set (NoiseType::breath, makeLooped ("Breath", 4.0, 16, [] (auto& b, auto& r)
    {
        Biquad f1, f2, f3; f1.bandpass (700.0f, 3.0f); f2.bandpass (1220.0f, 4.0f); f3.bandpass (2600.0f, 5.0f);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const float t = (float) i / (float) kNoiseRate;
            const float env = 0.55f + 0.45f * std::sin (dsp::kTwoPi * 0.5f * t);
            const float w = r.bipolar();
            b[i] = (f1.process (w) + 0.7f * f2.process (w) + 0.4f * f3.process (w)) * env;
        }
    }));

    set (NoiseType::staticBurst, makeLooped ("Static", len, 17, [] (auto& b, auto& r)
    {
        int count = 0; bool on = true;
        for (auto& x : b)
        {
            if (--count <= 0) { on = r.uniform() < 0.6f; count = 200 + (int) (r.uniform() * 4000.0f); }
            x = on ? r.bipolar() * (0.4f + 0.6f * r.uniform()) : r.bipolar() * 0.05f;
        }
    }));

    set (NoiseType::click, makeOneShot ("Click", 0.03, 18, [T] (auto& b, auto& r)
    {
        const int n = std::min ((int) b.size(), T (0.004));
        for (int i = 0; i < n; ++i)
            b[(size_t) i] = r.bipolar() * std::exp (-(float) i / 25.0f) + (i < 3 ? 1.0f : 0.0f);
    }));

    set (NoiseType::thump, makeOneShot ("Thump", 0.4, 19, [] (auto& b, auto&)
    {
        float ph = 0.0f;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const float t = (float) i / (float) kNoiseRate;
            const float f = 45.0f + 140.0f * std::exp (-t * 30.0f);
            ph += f / (float) kNoiseRate;
            b[i] = std::sin (dsp::kTwoPi * ph) * std::exp (-t * 9.0f);
        }
    }));

    set (NoiseType::snap, makeOneShot ("Snap", 0.25, 20, [] (auto& b, auto& r)
    {
        Biquad bp; bp.bandpass (2200.0f, 1.2f);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const float t = (float) i / (float) kNoiseRate;
            b[i] = bp.process (r.bipolar()) * std::exp (-t * 28.0f) + 0.3f * std::sin (dsp::kTwoPi * 190.0f * t) * std::exp (-t * 40.0f);
        }
    }));

    // "Custom" is filled at run time by the processor; keep a silent placeholder.
    NoiseSample silent;
    silent.name = "Custom Sample";
    silent.left.assign (1024, 0.0f);
    silent.right.assign (1024, 0.0f);
    set (NoiseType::custom, std::move (silent));
}

const NoiseBank& NoiseBank::get()
{
    static const NoiseBank bank;
    return bank;
}

const NoiseSample& NoiseBank::sample (NoiseType t) const
{
    return samples[(size_t) juce::jlimit (0, (int) samples.size() - 1, (int) t)];
}

} // namespace cureum
