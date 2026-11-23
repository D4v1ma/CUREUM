#include "SpaceEffects.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace cureum
{

using dsp::kTwoPi;

//==============================================================================
// Reverb
void ReverbFx::prepare (double rate, int)
{
    sr = rate;
    const double scale = sr / 44100.0;
    pre.allocate ((int) (0.3 * sr));
    for (auto& h : hall) h.allocate ((int) (0.25 * sr));
    const float diffLens[4] = { 142, 107, 379, 277 };
    for (int i = 0; i < 4; ++i)
    {
        hallDiff[i].line.allocate ((int) (500 * scale));
        hallDiff[i].delay = (float) (diffLens[i] * scale * 0.7);
        hallDiff[i].g = 0.6f;
        plateIn[i].line.allocate ((int) (600 * scale));
        plateIn[i].delay = (float) (diffLens[i] * scale);
        plateIn[i].g = i < 2 ? 0.75f : 0.625f;
    }
    for (int i = 0; i < 2; ++i)
    {
        tankAp[i].line.allocate ((int) (2500 * scale));
        tankAp2[i].line.allocate ((int) (4000 * scale));
        tankDelay[i].allocate ((int) (9000 * scale));
        tankDelay2[i].allocate ((int) (8000 * scale));
    }
    reset();
}

void ReverbFx::reset()
{
    pre.clear();
    for (auto& h : hall) h.clear();
    for (auto& a : hallDiff) a.line.clear();
    for (auto& a : plateIn) a.line.clear();
    for (int i = 0; i < 2; ++i) { tankAp[i].line.clear(); tankAp2[i].line.clear(); tankDelay[i].clear(); tankDelay2[i].clear(); tankLp[i] = tankFb[i] = 0.0f; }
    for (auto& v : hallLp) v = 0.0f;
    lc[0] = lc[1] = hc[0] = hc[1] = bandLp = 0.0f;
    mix.init = false;
}

void ReverbFx::processHall (float inL, float inR, float& outL, float& outR) noexcept
{
    float in = 0.5f * (inL + inR);
    for (auto& d : hallDiff) in = d.process (in);

    modPhase += 0.6 / sr;
    if (modPhase >= 1.0) modPhase -= 1.0;
    const float mod = std::sin ((float) (kTwoPi * modPhase)) * (float) (0.0008 * sr);

    float d[8];
    for (int i = 0; i < 8; ++i)
    {
        const float len = hallLen[i] + (i < 4 ? mod * (i % 2 ? 1.0f : -1.0f) : 0.0f);
        const float x = hall[i].read (len);
        hallLp[i] = x + damp * (hallLp[i] - x);
        d[i] = hallLp[i] * hallGain[i];
    }
    float sum = 0.0f;
    for (float v : d) sum += v;
    sum *= 0.25f; // Householder: x - 2/N * sum
    for (int i = 0; i < 8; ++i)
        hall[i].push (d[i] - sum + in * ((i & 1) ? -0.35f : 0.35f));

    outL = (d[0] + d[2] + d[4] + d[6]) * 0.6f;
    outR = (d[1] + d[3] + d[5] + d[7]) * 0.6f;
}

void ReverbFx::processPlate (float inL, float inR, float& outL, float& outR) noexcept
{
    float in = 0.5f * (inL + inR);
    bandLp = in + 0.3f * (bandLp - in);
    in = bandLp;
    for (auto& a : plateIn) in = a.process (in);

    modPhase += 0.9 / sr;
    if (modPhase >= 1.0) modPhase -= 1.0;
    const float mod = std::sin ((float) (kTwoPi * modPhase)) * (float) (0.0005 * sr);

    float tankOut[2];
    for (int b = 0; b < 2; ++b)
    {
        const float fromOther = tankFb[1 - b];
        float x = in + fromOther * decayGain;
        tankAp[b].delay = std::max (10.0f, (float) ((b == 0 ? 672.0 : 908.0) * sr / 29761.0) * sizeScale + (b == 0 ? mod : -mod));
        tankAp[b].g = -0.7f;
        x = tankAp[b].process (x);
        tankDelay[b].push (x);
        float y = tankDelay[b].read (tankLen[b]);
        tankLp[b] = y + damp * (tankLp[b] - y);
        y = tankLp[b] * decayGain;
        tankAp2[b].delay = (float) ((b == 0 ? 1800.0 : 2656.0) * sr / 29761.0) * sizeScale;
        tankAp2[b].g = 0.5f;
        y = tankAp2[b].process (y);
        tankDelay2[b].push (y);
        tankFb[b] = tankDelay2[b].read (tankLen2[b]);
        tankOut[b] = y;
    }
    outL = (tankDelay[0].read (tankLen[0] * 0.3f) + tankDelay2[1].read (tankLen2[1] * 0.6f) + tankOut[0]) * 0.55f;
    outR = (tankDelay[1].read (tankLen[1] * 0.35f) + tankDelay2[0].read (tankLen2[0] * 0.55f) + tankOut[1]) * 0.55f;
}

void ReverbFx::process (float* l, float* r, int n, const FxContext& ctx)
{
    mode = (ReverbMode) ctx.choice (pid::reverb (pid::Reverb::mode));
    const float size = ctx.value (pid::reverb (pid::Reverb::size));
    preDelay = (float) (ctx.value (pid::reverb (pid::Reverb::preDelay)) * 0.001 * sr);
    damp = 0.05f + ctx.value (pid::reverb (pid::Reverb::damp)) * 0.9f;
    width = ctx.value (pid::reverb (pid::Reverb::width));
    const float lowCut = ctx.value (pid::reverb (pid::Reverb::lowCut));
    const float highCut = ctx.value (pid::reverb (pid::Reverb::highCut));
    lowCutA = std::exp (-kTwoPi * (20.0f * std::pow (40.0f, lowCut)) / (float) sr);
    highCutA = std::exp (-kTwoPi * (20000.0f * std::pow (0.05f, highCut)) / (float) sr);
    mix.set (ctx.value (pid::reverb (pid::Reverb::mix)), n);

    sizeScale = 0.5f + size * 1.0f;
    if (mode == ReverbMode::hall)
    {
        const float base[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        const float rt60 = 0.4f + size * size * 9.0f;
        for (int i = 0; i < 8; ++i)
        {
            hallLen[i] = std::min ((float) hall[i].capacity() - 2.0f, base[i] * 0.001f * (float) sr * (0.6f + size * 1.6f));
            hallGain[i] = std::pow (10.0f, -3.0f * hallLen[i] / ((float) sr * rt60));
        }
    }
    else
    {
        decayGain = 0.35f + size * 0.6f;
        tankLen[0] = (float) (4453.0 * sr / 29761.0) * sizeScale;
        tankLen[1] = (float) (4217.0 * sr / 29761.0) * sizeScale;
        tankLen2[0] = (float) (3720.0 * sr / 29761.0) * sizeScale;
        tankLen2[1] = (float) (3163.0 * sr / 29761.0) * sizeScale;
    }

    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i], inR = r[i];
        pre.push (0.5f * (inL + inR));
        const float p = preDelay > 1.0f ? pre.read (preDelay) : 0.5f * (inL + inR);
        float wl, wr;
        if (mode == ReverbMode::hall) processHall (p, p, wl, wr);
        else                          processPlate (p, p, wl, wr);

        // tone: high cut then low cut on the wet signal
        hc[0] = wl + highCutA * (hc[0] - wl); hc[1] = wr + highCutA * (hc[1] - wr);
        lc[0] = hc[0] + lowCutA * (lc[0] - hc[0]); lc[1] = hc[1] + lowCutA * (lc[1] - hc[1]);
        wl = hc[0] - lc[0] + (lowCut < 0.001f ? lc[0] : 0.0f);
        wr = hc[1] - lc[1] + (lowCut < 0.001f ? lc[1] : 0.0f);

        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
        const float m = mix.next();
        l[i] = inL * (1.0f - m) + (mid + side) * m;
        r[i] = inR * (1.0f - m) + (mid - side) * m;
    }
}

//==============================================================================
// Convolution
Convolver::Convolver() : convolution (juce::dsp::Convolution::NonUniform { 512 }) {}

void Convolver::prepare (double rate, int block)
{
    sr = rate;
    maxBlock = std::max (16, block);
    convolution.prepare ({ sr, (juce::uint32) maxBlock, 2 });
    dry.setSize (2, maxBlock);
    wet.setSize (2, maxBlock);
    preL.allocate ((int) (0.3 * sr));
    preR.allocate ((int) (0.3 * sr));
    prepared = true;
    const auto type = loaded;
    loaded = ConvIr::count;
    if (type == ConvIr::custom && customFile.existsAsFile()) loadFile (customFile);
    else loadBuiltIn (type == ConvIr::count ? ConvIr::room : type);
    reset();
}

void Convolver::reset()
{
    convolution.reset();
    preL.clear(); preR.clear();
    lcS[0] = lcS[1] = hcS[0] = hcS[1] = 0.0f;
    mix.init = false;
}

juce::AudioBuffer<float> Convolver::generateIr (ConvIr type, double rate)
{
    struct Spec { float seconds, decay, brightness, density; };
    Spec s { 1.0f, 3.0f, 0.5f, 1.0f };
    switch (type)
    {
        case ConvIr::room:      s = { 0.6f, 9.0f, 0.55f, 1.0f }; break;
        case ConvIr::hall:      s = { 3.0f, 2.2f, 0.45f, 1.0f }; break;
        case ConvIr::plate:     s = { 2.2f, 2.8f, 0.85f, 1.0f }; break;
        case ConvIr::spring:    s = { 2.0f, 2.5f, 0.6f, 0.2f }; break;
        case ConvIr::cathedral: s = { 6.0f, 0.9f, 0.3f, 1.0f }; break;
        case ConvIr::metallic:  s = { 1.4f, 3.5f, 0.9f, 0.05f }; break;
        case ConvIr::gated:     s = { 0.45f, 0.0f, 0.6f, 1.0f }; break;
        case ConvIr::custom:
        case ConvIr::count:     break;
    }
    const int len = (int) (s.seconds * rate);
    juce::AudioBuffer<float> ir (2, len);
    for (int ch = 0; ch < 2; ++ch)
    {
        dsp::Random rng (1234u + (uint32_t) type * 31u + (uint32_t) ch * 7u);
        float lp = 0.0f;
        auto* d = ir.getWritePointer (ch);
        const float lpA = std::exp (-kTwoPi * (1500.0f + s.brightness * 14000.0f) / (float) rate);
        for (int i = 0; i < len; ++i)
        {
            const float t = (float) i / (float) rate;
            float env = type == ConvIr::gated ? (t < s.seconds * 0.85f ? 1.0f : std::max (0.0f, 1.0f - (t - s.seconds * 0.85f) / (s.seconds * 0.15f)))
                                              : std::exp (-s.decay * t);
            if (t < 0.004f) env *= t / 0.004f;
            float x = rng.bipolar();
            if (s.density < 1.0f && rng.uniform() > s.density) x *= 0.05f;
            // gradual darkening of the tail
            const float a = lpA + (1.0f - lpA) * std::min (0.95f, t * 0.35f);
            lp = x + a * (lp - x);
            d[i] = lp * env;
        }
        if (type == ConvIr::metallic)
        {
            const float freqs[] = { 431.0f, 1277.0f, 2213.0f, 3517.0f, 5113.0f };
            for (int i = 0; i < len; ++i)
            {
                const float t = (float) i / (float) rate;
                float v = 0.0f;
                for (float f : freqs) v += std::sin (kTwoPi * f * t * (ch ? 1.003f : 1.0f)) * std::exp (-t * (2.0f + f * 0.0008f));
                d[i] = d[i] * 0.4f + v * 0.15f;
            }
        }
        if (type == ConvIr::spring)
        {
            // repeating dispersive chirps
            const int period = (int) (0.032 * rate);
            for (int i = period; i < len; ++i)
                d[i] += d[i - period] * 0.55f;
        }
    }
    ir.applyGain (1.0f / std::max (1.0e-3f, ir.getMagnitude (0, len)));
    return ir;
}

void Convolver::loadBuiltIn (ConvIr type)
{
    if (! prepared || type == loaded || type == ConvIr::custom || type == ConvIr::count)
        return;
    auto ir = generateIr (type, sr);
    convolution.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    loaded = type;
}

bool Convolver::loadFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;
    customFile = file;
    if (! prepared)
    {
        loaded = ConvIr::custom;
        return true;
    }
    convolution.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes,
                                     (size_t) (sr * 12.0), juce::dsp::Convolution::Normalise::yes);
    loaded = ConvIr::custom;
    return true;
}

void Convolver::process (float* l, float* r, int n, const FxContext& ctx)
{
    if (! prepared || n > maxBlock)
        return;
    const float preDelay = (float) (ctx.value (pid::conv (pid::Conv::preDelay)) * 0.001 * sr);
    const float lcA = std::exp (-kTwoPi * ctx.value (pid::conv (pid::Conv::lowCut)) / (float) sr);
    const float hcA = std::exp (-kTwoPi * ctx.value (pid::conv (pid::Conv::highCut)) / (float) sr);
    const float g = juce::Decibels::decibelsToGain (ctx.value (pid::conv (pid::Conv::gain)));
    mix.set (ctx.value (pid::conv (pid::Conv::mix)), n);

    auto* wl = wet.getWritePointer (0);
    auto* wr = wet.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        preL.push (l[i]);
        preR.push (r[i]);
        wl[i] = preDelay > 0.5f ? preL.read (preDelay) : l[i];
        wr[i] = preDelay > 0.5f ? preR.read (preDelay) : r[i];
    }

    juce::dsp::AudioBlock<float> block (wet.getArrayOfWritePointers(), 2, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> pc (block);
    convolution.process (pc);

    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < 2; ++c)
        {
            float x = (c == 0 ? wl : wr)[i];
            hcS[c] = x + hcA * (hcS[c] - x);
            lcS[c] = hcS[c] + lcA * (lcS[c] - hcS[c]);
            (c == 0 ? wl : wr)[i] = (hcS[c] - lcS[c]) * g;
        }
        const float m = mix.next();
        l[i] = l[i] * (1.0f - m) + wl[i] * m;
        r[i] = r[i] * (1.0f - m) + wr[i] * m;
    }
}

} // namespace cureum
