#include "Oscillators.h"
#include "../Core/ValueMaps.h"

namespace cureum
{

//==============================================================================
void WavetableOscillator::prepare (double sampleRate)
{
    sr = sampleRate;
    dsp::Random rng (0xC0FFEEu + (uint32_t) osc * 977u);
    for (int u = 0; u < kMaxUnison; ++u)
        unisonRandom[u] = rng.bipolar();
}

void WavetableOscillator::noteOn (float phaseDeg, float randomPhase, uint32_t seed)
{
    dsp::Random rng (seed);
    const float start = phaseDeg / 360.0f;
    for (int u = 0; u < kMaxUnison; ++u)
    {
        float p = start + randomPhase * rng.uniform();
        phase[u] = p - std::floor (p);
        lastOut[u] = 0.0f;
    }
    lastMono = 0.0f;
}

void WavetableOscillator::setup (const Settings& s, int oscIndex)
{
    osc = oscIndex;
    unison = juce::jlimit (1, kMaxUnison, s.unison);
    warp1 = s.warp1;
    warp2 = s.warp2;

    const Wavetable* table = s.table;
    if (table == nullptr)
        return;

    const int numFrames = table->getNumFrames();
    const bool interp = table->interpolatesFrames();

    // Unison positions in [-1, 1]
    float pos[kMaxUnison] {};
    if (unison > 1)
    {
        for (int u = 0; u < unison; ++u)
        {
            const float t = -1.0f + 2.0f * (float) u / (float) (unison - 1);
            const float at = std::abs (t);
            switch (s.uniMode)
            {
                case UnisonMode::super:       pos[u] = std::copysign (std::pow (at, 1.6f), t); break;
                case UnisonMode::exponential: pos[u] = std::copysign (at * at, t); break;
                case UnisonMode::inverse:     pos[u] = std::copysign (1.0f - (1.0f - at) * (1.0f - at), t); break;
                case UnisonMode::random:      pos[u] = u == 0 ? -1.0f : (u == unison - 1 ? 1.0f : unisonRandom[u]); break;
                case UnisonMode::linear:
                case UnisonMode::count:       pos[u] = t; break;
            }
        }
    }

    // Blend weights: centre voice(s) vs. outer voices.
    const float wCentre = s.blend <= 0.75f ? 1.0f : 1.0f - (s.blend - 0.75f) * 2.0f;
    const float wSide   = s.blend >= 0.75f ? 1.0f : s.blend / 0.75f;
    float weights[kMaxUnison] {};
    float sumSq = 0.0f;
    for (int u = 0; u < unison; ++u)
    {
        bool centre = unison == 1;
        if (unison > 1)
            centre = (unison % 2 == 1) ? (u == unison / 2) : (u == unison / 2 || u == unison / 2 - 1);
        weights[u] = centre ? wCentre : wSide;
        sumSq += weights[u] * weights[u];
    }
    const float norm = sumSq > 1.0e-6f ? 1.0f / std::sqrt (sumSq) : 0.0f;
    const float gain = maps::levelToGain (s.level) * norm;

    const float spreadSemis = s.detune * s.detune * 1.5f;
    const float baseInc = (float) (s.freqHz / sr);

    for (int u = 0; u < unison; ++u)
    {
        auto& v = uni[u];

        float stack = 0.0f;
        const bool centre = (unison % 2 == 1) ? (u == unison / 2) : (u == unison / 2 || u == unison / 2 - 1);
        switch (s.uniStack)
        {
            case UnisonStack::octave:       stack = (u % 2 == 1) ? 12.0f : 0.0f; break;
            case UnisonStack::octaveFifth:  stack = (u % 3 == 1) ? 12.0f : ((u % 3 == 2) ? 7.0f : 0.0f); break;
            case UnisonStack::twoOctaves:   stack = (float) (u % 3) * 12.0f; break;
            case UnisonStack::centreOctave: stack = (centre && unison > 1) ? 12.0f : 0.0f; break;
            case UnisonStack::centreFifth:  stack = (centre && unison > 1) ? 7.0f : 0.0f; break;
            case UnisonStack::off:
            case UnisonStack::count:        break;
        }

        const float semis = pos[u] * spreadSemis + stack;
        v.inc = baseInc * dsp::semitonesToRatio (semis);

        // Mip level: account for the extra harmonics that sync produces.
        float effInc = v.inc;
        if (warp1 == WarpMode::sync || warp1 == WarpMode::syncWindow) effInc *= 1.0f + s.warp1Amt * s.warp1Amt * 15.0f;
        if (warp2 == WarpMode::sync || warp2 == WarpMode::syncWindow) effInc *= 1.0f + s.warp2Amt * s.warp2Amt * 15.0f;
        const int level = Wavetable::levelForIncrement (effInc);
        v.size = Wavetable::levelSize (level);

        // Wavetable position per voice
        const float wt = dsp::clamp01 (s.wtPos + s.uniWtPos * pos[u] * 0.5f);
        const float fpos = wt * (float) (numFrames - 1);
        int f0 = (int) fpos;
        float frac = fpos - (float) f0;
        if (! interp) { f0 = juce::roundToInt (fpos); frac = 0.0f; }
        f0 = juce::jlimit (0, numFrames - 1, f0);
        const int f1 = std::min (f0 + 1, numFrames - 1);
        v.t0 = table->levelFrame (level, f0);
        v.t1 = table->levelFrame (level, f1);
        v.frac = f1 == f0 ? 0.0f : frac;

        v.warpA = dsp::clamp01 (s.warp1Amt + s.uniWarp * pos[u] * 0.5f);
        v.warpB = dsp::clamp01 (s.warp2Amt + s.uniWarp * pos[u] * 0.5f);

        // Stereo placement: osc pan plus unison spread.
        const float p = juce::jlimit (-1.0f, 1.0f, s.pan + pos[u] * s.width * (1.0f - std::abs (s.pan)));
        float gl, gr;
        dsp::panGains (p, gl, gr);
        v.gainL = gl * gain * weights[u];
        v.gainR = gr * gain * weights[u];
    }
}

//==============================================================================
const Wavetable& SubOscillator::shapes()
{
    static const WavetablePtr table = []
    {
        constexpr int n = Wavetable::kFrameSize;
        std::vector<float> frames ((size_t) n * (size_t) SubShape::count);
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) i / (float) n;
            const float sine = std::sin (dsp::kTwoPi * x);
            frames[(size_t) i] = sine;                                                                          // sine
            frames[(size_t) (n + i)] = juce::jlimit (-1.0f, 1.0f, sine * 3.0f) * 0.92f;                        // round rect
            frames[(size_t) (2 * n + i)] = x < 0.25f ? x * 4.0f : (x < 0.75f ? 2.0f - x * 4.0f : x * 4.0f - 4.0f); // triangle
            frames[(size_t) (3 * n + i)] = x < 0.5f ? x * 2.0f : x * 2.0f - 2.0f;                               // saw
            frames[(size_t) (4 * n + i)] = x < 0.5f ? 1.0f : -1.0f;                                              // square
            frames[(size_t) (5 * n + i)] = x < 0.2f ? 1.0f : -0.25f;                                             // pulse
        }
        Wavetable::Options o;
        o.interpolateFrames = false;
        return Wavetable::create ("Sub Shapes", std::move (frames), o);
    }();
    return *table;
}

void SubOscillator::setup (float freqHz, SubShape shape, float level, float pan)
{
    inc = (float) (freqHz / sr);
    const int lvl = Wavetable::levelForIncrement (inc);
    size = Wavetable::levelSize (lvl);
    table = shapes().levelFrame (lvl, juce::jlimit (0, (int) SubShape::count - 1, (int) shape));
    float gl, gr;
    dsp::panGains (pan, gl, gr);
    const float g = maps::levelToGain (level);
    gainL = gl * g;
    gainR = gr * g;
}

//==============================================================================
void NoiseOscillator::noteOn (const NoiseSample* src, float start, float randomStart, uint32_t seed)
{
    sample = src;
    finished = false;
    if (sample == nullptr || sample->length() < 2) { pos = 0.0; return; }
    dsp::Random rng (seed * 2654435761u + 17u);
    double p = (double) start + (double) randomStart * (double) rng.uniform();
    p -= std::floor (p);
    pos = p * (double) (sample->length() - 1);
    if (sample->oneShot && randomStart <= 0.0f)
        pos = (double) start * (double) (sample->length() - 1);
}

void NoiseOscillator::setup (const NoiseSample* src, float rate, float level, float pan, bool shouldOneShot, bool isStereo)
{
    if (src != sample)
    {
        sample = src;
        pos = 0.0;
        finished = false;
    }
    oneShot = shouldOneShot || (sample != nullptr && sample->oneShot);
    stereo = isStereo;
    inc = sample != nullptr ? (double) rate * sample->sampleRate / sr : 0.0;
    float gl, gr;
    dsp::panGains (pan, gl, gr);
    const float g = maps::levelToGain (level);
    gainL = gl * g;
    gainR = gr * g;
}

} // namespace cureum
