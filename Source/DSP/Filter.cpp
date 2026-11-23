#include "Filter.h"
#include <juce_dsp/juce_dsp.h>

namespace cureum
{

using namespace dsp;

void MultiFilter::prepare (double sampleRate, double maxSampleRate)
{
    sr = sampleRate;
    lineSize = (int) std::ceil (std::max (sampleRate, maxSampleRate) / 25.0) + 8;
    memory.assign ((size_t) lineSize * 8, 0.0f);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4; ++i)
        {
            auto& l = chan[ch].lines[i];
            l.buf = memory.data() + (size_t) (ch * 4 + i) * (size_t) lineSize;
            l.size = lineSize;
            l.pos = 0;
        }
    reset();
    setParameters (1000.0f, reso, drive, var, 0.0f);
}

void MultiFilter::reset()
{
    for (auto& c : chan)
    {
        for (auto& s : c.svf) s.reset();
        for (auto& o : c.op) o.reset();
        for (auto& a : c.ap) a.reset();
        for (auto& l : c.ladder) l = 0.0f;
        for (auto& b : c.bq) for (auto& v : b) v = 0.0f;
        c.lastOut = c.hold = c.holdPhase = c.ringPhase = c.ringPhase2 = c.smooth = 0.0f;
        for (auto& l : c.lines) l.clear();
    }
}

void MultiFilter::setType (int typeIndex)
{
    typeIndex = juce::jlimit (0, numFilterTypes() - 1, typeIndex);
    if (typeIndex == type)
        return;
    const auto oldAlgo = def.algo;
    type = typeIndex;
    def = filterTypes()[(size_t) type];
    if (def.algo != oldAlgo)
        reset();
}

namespace
{
    inline float tanApprox (float x) noexcept { return std::tan (x); }

    void rbjPeak (float* c, float fc, float q, float A, double sr)
    {
        const float w = kTwoPi * fc / (float) sr;
        const float alpha = std::sin (w) / (2.0f * q);
        const float cw = std::cos (w);
        const float a0 = 1.0f + alpha / A;
        c[0] = (1.0f + alpha * A) / a0; c[1] = -2.0f * cw / a0; c[2] = (1.0f - alpha * A) / a0;
        c[3] = -2.0f * cw / a0; c[4] = (1.0f - alpha / A) / a0;
    }

    void rbjShelf (float* c, float fc, float q, float A, double sr, bool low)
    {
        const float w = kTwoPi * fc / (float) sr;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / (2.0f * q);
        const float sq = 2.0f * std::sqrt (A) * alpha;
        float b0, b1, b2, a0, a1, a2;
        if (low)
        {
            b0 = A * ((A + 1) - (A - 1) * cw + sq); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sq);
            a0 = (A + 1) + (A - 1) * cw + sq;       a1 = -2 * ((A - 1) + (A + 1) * cw);    a2 = (A + 1) + (A - 1) * cw - sq;
        }
        else
        {
            b0 = A * ((A + 1) + (A - 1) * cw + sq); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sq);
            a0 = (A + 1) - (A - 1) * cw + sq;        a1 = 2 * ((A - 1) - (A + 1) * cw);      a2 = (A + 1) - (A - 1) * cw - sq;
        }
        c[0] = b0 / a0; c[1] = b1 / a0; c[2] = b2 / a0; c[3] = a1 / a0; c[4] = a2 / a0;
    }

    inline float biquad (const float* c, float* s, float x) noexcept
    {
        const float y = c[0] * x + s[0];
        s[0] = c[1] * x - c[3] * y + s[1];
        s[1] = c[2] * x - c[4] * y;
        return y;
    }

    // Formant tables (Hz) for A E I O U, three formants.
    constexpr float kFormants[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
    constexpr float kFormantAmp[3] = { 1.0f, 0.55f, 0.3f };
}

void MultiFilter::computeCoeffs (Coeffs& c, float fc) const noexcept
{
    const float nyq = (float) sr * 0.47f;
    fc = juce::jlimit (8.0f, nyq, fc);
    c.fc = fc;
    c.g = tanApprox (kPi * fc / (float) sr);
    c.G = c.g / (1.0f + c.g);
    c.k = 2.0f * (1.0f - 0.985f * std::pow (reso, 0.8f));
    c.delaySamples = juce::jlimit (2.0f, (float) lineSize - 4.0f, (float) sr / fc);

    auto gFor = [this, nyq] (float f) { return tanApprox (kPi * juce::jlimit (8.0f, nyq, f) / (float) sr); };

    switch (def.algo)
    {
        case FilterAlgo::multi:
        {
            const float spread = 1.0f + var * 2.0f;
            c.g2 = gFor (fc / spread);
            c.g3 = gFor (fc * spread);
            break;
        }
        case FilterAlgo::bandReject:
        {
            const float w = 1.2f + var * 1.5f;
            c.g2 = gFor (fc / w);
            c.g3 = gFor (fc * w);
            break;
        }
        case FilterAlgo::comb:
        case FilterAlgo::flange:
        {
            // damping filters (one-pole G values)
            float lpHz = 20000.0f, hpHz = 10.0f;
            if (def.b == 0)      lpHz = 18000.0f * std::pow (0.02f, var);
            else if (def.b == 1) lpHz = 20000.0f * std::pow (0.01f, var);
            else if (def.b == 2) hpHz = 10.0f * std::pow (500.0f, var);
            else                 { lpHz = 20000.0f * std::pow (0.05f, var); hpHz = 20.0f * std::pow (60.0f, var); }
            const float gl = gFor (lpHz), gh = gFor (hpHz);
            c.g2 = gl / (1.0f + gl);
            c.g3 = gh / (1.0f + gh);
            break;
        }
        case FilterAlgo::phaser:
        case FilterAlgo::freqPhaser:
        {
            const int n = juce::jlimit (1, 24, def.a);
            for (int i = 0; i < n; ++i)
            {
                const float t = n > 1 ? (float) i / (float) (n - 1) : 0.5f;
                const float f = def.algo == FilterAlgo::phaser ? fc * std::exp2 ((t - 0.5f) * var * 4.0f)
                                                               : fc * std::pow (1.0f + var * 3.0f, t * 2.0f);
                const float tg = gFor (f);
                c.apA[i] = (tg - 1.0f) / (tg + 1.0f);
            }
            const float gl = gFor (fc * 2.0f), gh = gFor (fc * 0.5f);
            c.g2 = gl / (1.0f + gl);
            c.g3 = gh / (1.0f + gh);
            break;
        }
        case FilterAlgo::eq:
        {
            const float A = std::pow (10.0f, (var - 0.5f) * 48.0f / 40.0f);
            const float q = 0.4f + reso * 6.0f;
            const auto shape = (EqShape) def.a;
            if (shape == EqShape::lowShelf12)       rbjShelf (c.bq, fc, std::min (q, 2.0f), A, sr, true);
            else if (shape == EqShape::highShelf12) rbjShelf (c.bq, fc, std::min (q, 2.0f), A, sr, false);
            else if (shape == EqShape::band12)      rbjPeak (c.bq, fc, q, A, sr);
            c.g2 = A * A; // linear gain for first-order shelves
            break;
        }
        case FilterAlgo::formant:
        case FilterAlgo::vowel:
        {
            const float shift = fc / 425.0f;
            const float pos = var * 4.0f;
            const int v0 = juce::jlimit (0, 4, (int) pos);
            const int v1 = std::min (4, v0 + 1);
            const float t = pos - (float) v0;
            float scale[3] = { 1.0f, 1.0f, 1.0f };
            if (def.a == 2) { scale[0] = 1.18f; scale[1] = 1.2f; scale[2] = 1.16f; }
            if (def.a == 3) { scale[0] = 0.75f; scale[1] = 1.45f; scale[2] = 1.6f; }
            for (int f = 0; f < 3; ++f)
            {
                const float hz = lerp (kFormants[v0][f], kFormants[v1][f], t) * scale[f] * shift;
                c.apA[f] = gFor (hz);
            }
            break;
        }
        case FilterAlgo::distComb:
        {
            c.g2 = gFor (fc * (1.0f + var * 7.0f));
            break;
        }
        case FilterAlgo::diode:
        {
            const float gh = gFor (160.0f);
            c.g3 = gh / (1.0f + gh);
            break;
        }
        case FilterAlgo::allpasses:
        case FilterAlgo::combs:
        case FilterAlgo::reverb:
        {
            const float gl = gFor (20000.0f * std::pow (0.03f, var));
            c.g2 = gl / (1.0f + gl);
            break;
        }
        default: break;
    }
}

void MultiFilter::setParameters (float cutoffHz, float newReso, float newDrive, float newVar, float stereo)
{
    reso = clamp01 (newReso);
    drive = clamp01 (newDrive);
    var = clamp01 (newVar);
    inGain = 1.0f + drive * drive * 15.0f;
    outGain = 1.0f / std::sqrt (inGain);
    const float s = juce::jlimit (-1.0f, 1.0f, stereo);
    computeCoeffs (coeffs[0], cutoffHz * std::exp2 (-s));
    computeCoeffs (coeffs[1], cutoffHz * std::exp2 (s));
}

void MultiFilter::process (float& l, float& r) noexcept
{
    l = processChannel (0, l);
    r = processChannel (1, r);
}

float MultiFilter::processChannel (int ch, float x) noexcept
{
    auto& c = coeffs[ch];
    auto& st = chan[ch];
    const bool driven = drive > 0.001f;

    auto drv = [&] (float v) noexcept { return driven ? fastTanh (v * inGain) * outGain * 1.4f : v; };

    switch (def.algo)
    {
        case FilterAlgo::svf:
        {
            const auto resp = (FilterResp) def.a;
            const int poles = def.b;
            float in = drv (x);
            float lp, bp, hp, y = in;
            if (poles == 1)
            {
                lp = st.op[0].lp (in, c.G);
                y = resp == FilterResp::highpass ? in - lp : lp;
            }
            else
            {
                if (poles == 3)
                    in = resp == FilterResp::highpass ? st.op[0].hp (in, c.G) : st.op[0].lp (in, c.G);
                const int stages = poles == 4 ? 2 : 1;
                y = in;
                for (int s = 0; s < stages; ++s)
                {
                    const float k = (stages == 2 && s == 0) ? std::max (c.k, 1.2f) : c.k;
                    st.svf[s].tick (y, c.g, k, lp, bp, hp);
                    switch (resp)
                    {
                        case FilterResp::lowpass:  y = lp; break;
                        case FilterResp::highpass: y = hp; break;
                        case FilterResp::bandpass: y = bp * k; break;
                        case FilterResp::peak:     y = y + 3.0f * reso * bp * k; break;
                        case FilterResp::notch:    y = lp + hp; break;
                    }
                }
            }
            if (resp == FilterResp::lowpass || resp == FilterResp::highpass)
                y *= 1.0f + var * (0.4f + reso * 1.6f);
            return y;
        }

        case FilterAlgo::ladder:
        {
            const float g = c.g, G = c.G;
            const float k = reso * 3.98f;
            float* s = st.ladder;
            const float inv = 1.0f / (1.0f + g);
            const float S = G * G * G * s[0] * inv + G * G * s[1] * inv + G * s[2] * inv + s[3] * inv;
            const float G4 = G * G * G * G;
            const float y4p = (G4 * x + S) / (1.0f + k * G4);
            const float u = fastTanh ((x - k * y4p) * inGain * 0.8f) * (driven ? outGain * 1.25f : 1.25f);
            float v = (u - s[0]) * G;  const float y1 = v + s[0]; s[0] = y1 + v;
            v = (y1 - s[1]) * G;       const float y2 = v + s[1]; s[1] = y2 + v;
            v = (y2 - s[2]) * G;       const float y3 = v + s[2]; s[2] = y3 + v;
            v = (y3 - s[3]) * G;       const float y4 = v + s[3]; s[3] = y4 + v;
            const float taps[4] = { y1, y2, y3, y4 };
            return taps[juce::jlimit (0, 3, def.a - 1)] * (1.0f + k * (0.22f + var * 0.6f));
        }

        case FilterAlgo::ladderHigh:
        {
            const float k = reso * 3.6f;
            const float u = fastTanh ((drv (x) - k * st.lastOut) * 0.9f);
            float y = st.op[0].hp (u, c.G);
            y = st.op[1].hp (y, c.G);
            y = st.op[2].hp (y, c.G);
            y = st.op[3].hp (y, c.G);
            st.lastOut = y;
            return y * (1.0f + k * (0.2f + var * 0.5f));
        }

        case FilterAlgo::multi:
        {
            const auto shape = (MultiShape) def.a;
            const float t = var;
            auto stage = [&] (int idx, float in) -> float
            {
                float lp, bp, hp;
                if (def.b == 1)
                {
                    lp = st.op[idx].lp (in, c.G);
                    return lerp (lp, in - lp, t);
                }
                if (shape == MultiShape::PP)
                {
                    float l1, b1, h1, l2, b2, h2;
                    st.svf[idx * 2].tick (in, c.g2, c.k, l1, b1, h1);
                    st.svf[idx * 2 + 1].tick (in, c.g3, c.k, l2, b2, h2);
                    return in + 1.5f * reso * c.k * (b1 + b2) + 0.5f * c.k * (b1 + b2);
                }
                if (shape == MultiShape::NN)
                {
                    float l1, b1, h1, l2, b2, h2;
                    st.svf[idx * 2].tick (in, c.g2, c.k, l1, b1, h1);
                    const float n1 = l1 + h1;
                    st.svf[idx * 2 + 1].tick (n1, c.g3, c.k, l2, b2, h2);
                    return l2 + h2;
                }
                st.svf[idx].tick (in, c.g, c.k, lp, bp, hp);
                const float bn = bp * c.k;
                const float pk = in + 3.0f * reso * bn;
                const float nt = lp + hp;
                auto m3 = [t] (float a, float b, float d) { return t < 0.5f ? lerp (a, b, t * 2.0f) : lerp (b, d, t * 2.0f - 1.0f); };
                switch (shape)
                {
                    case MultiShape::LH:  return lerp (lp, hp, t);
                    case MultiShape::LB:  return lerp (lp, bn, t);
                    case MultiShape::LP:  return lerp (lp, pk, t);
                    case MultiShape::LN:  return lerp (lp, nt, t);
                    case MultiShape::HB:  return lerp (hp, bn, t);
                    case MultiShape::HP:  return lerp (hp, pk, t);
                    case MultiShape::HN:  return lerp (hp, nt, t);
                    case MultiShape::BP:  return lerp (bn, pk, t);
                    case MultiShape::PN:  return lerp (pk, nt, t);
                    case MultiShape::LBH: return m3 (lp, bn, hp);
                    case MultiShape::LPH: return m3 (lp, pk, hp);
                    case MultiShape::LNH: return m3 (lp, nt, hp);
                    case MultiShape::BPN: return m3 (bn, pk, nt);
                    case MultiShape::PP:
                    case MultiShape::NN:  break;
                }
                return lp;
            };
            float y = stage (0, drv (x));
            if (def.b == 4)
                y = stage (1, y);
            return y;
        }

        case FilterAlgo::comb:
        case FilterAlgo::flange:
        {
            auto& line = st.lines[0];
            const float d = line.read (c.delaySamples);
            float damped = d;
            if (def.b == 0 || def.b == 1 || def.b == 3) damped = st.op[0].lp (damped, c.g2);
            if (def.b == 2 || def.b == 3)               damped = st.op[1].hp (damped, c.g3);
            const float sign = def.a < 0 ? -1.0f : 1.0f;
            const float fb = sign * reso * 0.985f;
            const float in = drv (x);
            const float w = in + fb * damped;
            line.write (fastTanh (w * 0.5f) * 2.0f);
            if (def.algo == FilterAlgo::comb)
                return w * (1.0f - reso * 0.55f);
            return 0.5f * (in + sign * damped) * (1.0f - reso * 0.3f);
        }

        case FilterAlgo::phaser:
        case FilterAlgo::freqPhaser:
        {
            const int n = juce::jlimit (1, 24, def.a);
            const float sign = (def.algo == FilterAlgo::phaser && def.b < 0) ? -1.0f : 1.0f;
            const float in = drv (x);
            float y = in + st.lastOut * reso * 0.9f * sign;
            for (int i = 0; i < n; ++i)
                y = st.ap[i].process (y, c.apA[i]);
            st.lastOut = flushDenormal (y);
            float out = 0.5f * (in + sign * y);
            if (def.c == 1) out = st.op[0].lp (out, c.g2);
            if (def.c == 2) out = st.op[1].hp (out, c.g3);
            return out;
        }

        case FilterAlgo::eq:
        {
            const auto shape = (EqShape) def.a;
            const float in = drv (x);
            if (shape == EqShape::lowShelf6)  { const float lp = st.op[0].lp (in, c.G); return in + (c.g2 - 1.0f) * lp; }
            if (shape == EqShape::highShelf6) { const float lp = st.op[0].lp (in, c.G); return in + (c.g2 - 1.0f) * (in - lp); }
            return biquad (c.bq, st.bq[0], in);
        }

        case FilterAlgo::ringMod:
        {
            st.ringPhase += c.fc / (float) sr;
            st.ringPhase -= std::floor (st.ringPhase);
            float carrier = fastSin01 (st.ringPhase);
            const float in = drv (x);
            if (def.a == 1)
            {
                carrier = lerp (carrier, fastTanh (carrier * 5.0f), var);
                return lerp (in * carrier, in, reso * 0.5f);
            }
            st.ringPhase2 += c.fc * (1.5f + var * 2.5f) / (float) sr;
            st.ringPhase2 -= std::floor (st.ringPhase2);
            return lerp (in * 0.5f * (carrier + fastSin01 (st.ringPhase2)), in, reso * 0.5f);
        }

        case FilterAlgo::sampleHold:
        {
            st.holdPhase += c.fc / (float) sr;
            if (st.holdPhase >= 1.0f)
            {
                st.holdPhase -= std::floor (st.holdPhase);
                st.hold = drv (x) + reso * 0.9f * (st.hold - st.smooth);
            }
            const float amount = def.a < 0 ? 0.5f + var * 0.499f : var * 0.995f;
            st.smooth = st.hold + amount * (st.smooth - st.hold);
            return def.a < 0 ? st.smooth : lerp (st.hold, st.smooth, var);
        }

        case FilterAlgo::combs:
        {
            const float in = drv (x);
            const float ratios[4] = { 1.0f, 1.0f + 0.5f * var, 1.0f + var, 1.0f + 2.0f * var };
            float out = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                auto& line = st.lines[i];
                const float d = line.read (c.delaySamples / ratios[i]);
                const float w = in + reso * 0.95f * d;
                line.write (w);
                out += w;
            }
            return st.op[0].lp (out * 0.25f * (1.0f - reso * 0.6f), c.g2);
        }

        case FilterAlgo::allpasses:
        {
            float y = drv (x);
            const float lens[4] = { 1.0f, 0.731f, 0.535f, 0.379f };
            const float g = reso * 0.85f;
            for (int i = 0; i < 4; ++i)
            {
                auto& line = st.lines[i];
                const float d = line.read (std::max (2.0f, c.delaySamples * lens[i] * (1.0f - var * 0.5f)));
                const float w = y + g * d;
                line.write (w);
                y = d - g * w;
            }
            return y;
        }

        case FilterAlgo::reverb:
        {
            const float in = drv (x) * 0.5f;
            const float lens[4] = { 1.0f, 1.17f, 1.38f, 1.53f };
            const float fbk = 0.5f + reso * 0.48f;
            float d[4];
            for (int i = 0; i < 4; ++i)
                d[i] = st.op[i].lp (st.lines[i].read (std::min ((float) lineSize - 4.0f, c.delaySamples * lens[i] * 4.0f)), c.g2);
            const float h0 = d[0] + d[1] + d[2] + d[3];
            const float h1 = d[0] - d[1] + d[2] - d[3];
            const float h2 = d[0] + d[1] - d[2] - d[3];
            const float h3 = d[0] - d[1] - d[2] + d[3];
            st.lines[0].write (in + fbk * 0.5f * h0);
            st.lines[1].write (in + fbk * 0.5f * h1);
            st.lines[2].write (in + fbk * 0.5f * h2);
            st.lines[3].write (in + fbk * 0.5f * h3);
            return (d[0] + d[1] + d[2] + d[3]) * 0.5f;
        }

        case FilterAlgo::french:
        case FilterAlgo::german:
        case FilterAlgo::diode:
        {
            const float G = c.G;
            float k = reso * (def.algo == FilterAlgo::german ? 4.4f : 4.0f);
            float fbSig = st.lastOut;
            if (def.algo == FilterAlgo::diode)
            {
                k *= 1.0f + var * 0.6f;
                fbSig = st.op[4].hp (st.lastOut, c.g3);
            }
            const float u = fastTanh ((x * inGain * 0.9f) - k * (def.algo == FilterAlgo::german ? fastTanh (fbSig) : fbSig));
            float y = st.op[0].lp (u, G);
            y = st.op[1].lp (def.algo == FilterAlgo::german ? y : fastTanh (y), G);
            y = st.op[2].lp (def.algo == FilterAlgo::german ? y : fastTanh (y * 1.1f), G);
            y = st.op[3].lp (fastTanh (y), G);
            st.lastOut = flushDenormal (y);
            const float fat = def.algo == FilterAlgo::diode ? 0.35f : 0.25f + var * 0.6f;
            return y * (1.0f + k * fat) * (driven ? outGain * 1.3f : 1.0f);
        }

        case FilterAlgo::addBass:
        {
            float lp, bp, hp;
            st.svf[0].tick (x, c.g, std::max (c.k, 0.5f), lp, bp, hp);
            return drv (x + (0.5f + var * 2.5f) * lp) * 0.7f;
        }

        case FilterAlgo::formant:
        case FilterAlgo::vowel:
        {
            const float in = drv (x);
            const float q = 4.0f + reso * 16.0f;
            const float k = 1.0f / q;
            float out = 0.0f;
            for (int f = 0; f < 3; ++f)
            {
                float lp, bp, hp;
                st.svf[f].tick (in, c.apA[f], k, lp, bp, hp);
                out += bp * k * kFormantAmp[f];
            }
            return out * (def.algo == FilterAlgo::vowel ? 2.2f : 2.0f);
        }

        case FilterAlgo::bandReject:
        {
            float l1, b1, h1, l2, b2, h2;
            st.svf[0].tick (drv (x), c.g2, std::max (c.k, 0.1f), l1, b1, h1);
            st.svf[1].tick (l1 + h1, c.g3, std::max (c.k, 0.1f), l2, b2, h2);
            return l2 + h2;
        }

        case FilterAlgo::distComb:
        {
            auto& line = st.lines[0];
            const float d = line.read (c.delaySamples);
            const float shaped = def.a == 1 ? fastTanh (d * (1.5f + drive * 5.0f)) : std::sin (d * (2.0f + drive * 6.0f)) * 0.8f;
            const float w = x + reso * 0.97f * shaped;
            line.write (w);
            float lp, bp, hp;
            st.svf[0].tick (w, c.g2, 0.9f, lp, bp, hp);
            return (def.b == 0 ? lp : bp) * 0.8f;
        }

        case FilterAlgo::scream:
        {
            float lp, bp, hp;
            const float in = fastTanh (x * inGain);
            st.svf[0].tick (in, c.g, std::max (0.02f, c.k * (1.0f - var * 0.7f)), lp, bp, hp);
            st.svf[0].ic1 = fastTanh (st.svf[0].ic1 * (1.0f + var * 3.0f)) * 0.95f;
            return (def.a == 0 ? lp : bp * c.k * 2.0f) * outGain;
        }

        case FilterAlgo::ms20:
        {
            float lp, bp, hp;
            st.svf[0].tick (drv (x), c.g, std::max (0.03f, c.k), lp, bp, hp);
            const float bite = 1.0f + var * 2.5f;
            st.svf[0].ic1 = fastTanh (st.svf[0].ic1 * bite) / bite * (1.0f + var * 0.4f);
            return def.a == 0 ? lp : hp;
        }
    }
    return x;
}

void MultiFilter::measureResponse (int typeIndex, float cutoffHz, float r, float d, float v, double sampleRate,
                                   const std::vector<float>& freqs, std::vector<float>& magnitudes)
{
    constexpr int order = 13;
    constexpr int n = 1 << order;
    MultiFilter f;
    f.prepare (sampleRate, sampleRate);
    f.setType (typeIndex);
    f.setParameters (cutoffHz, r, d, v, 0.0f);

    std::vector<float> data ((size_t) n * 2, 0.0f);
    constexpr float amp = 0.05f;
    for (int i = 0; i < n; ++i)
    {
        float l = i == 0 ? amp : 0.0f, rr = l;
        f.process (l, rr);
        data[(size_t) i] = std::isfinite (l) ? l / amp : 0.0f;
    }
    juce::dsp::FFT fft (order);
    fft.performFrequencyOnlyForwardTransform (data.data(), true);

    magnitudes.resize (freqs.size());
    for (size_t i = 0; i < freqs.size(); ++i)
    {
        const float bin = freqs[i] / (float) sampleRate * (float) n;
        const int b0 = juce::jlimit (0, n / 2 - 1, (int) bin);
        const float t = juce::jlimit (0.0f, 1.0f, bin - (float) b0);
        magnitudes[i] = data[(size_t) b0] + t * (data[(size_t) b0 + 1] - data[(size_t) b0]);
    }
}

} // namespace cureum
