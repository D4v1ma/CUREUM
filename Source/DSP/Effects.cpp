#include "Effects.h"
#include "../Core/ValueMaps.h"

namespace cureum
{

using dsp::fastTanh;
using dsp::kTwoPi;

namespace
{
    inline float lfoSin (double phase) noexcept { return std::sin ((float) (kTwoPi * (phase - std::floor (phase)))); }

    double rateHzFor (const FxContext& ctx, int rateIdx, int syncIdx, int syncRateIdx)
    {
        if (ctx.on (syncIdx))
            return 1.0 / ctx.syncSeconds (ctx.choice (syncRateIdx));
        return (double) ctx.value (rateIdx);
    }
}

//==============================================================================
// Hyper / Dimension
void HyperDimension::prepare (double rate, int)
{
    sr = rate;
    hyperL.allocate ((int) (0.06 * sr));
    hyperR.allocate ((int) (0.06 * sr));
    dimL.allocate ((int) (0.08 * sr));
    dimR.allocate ((int) (0.08 * sr));
    reset();
}

void HyperDimension::reset()
{
    hyperL.clear(); hyperR.clear(); dimL.clear(); dimR.clear();
    for (int i = 0; i < 7; ++i) phase[i] = (double) i / 7.0;
    mix.init = dim.init = false;
}

void HyperDimension::process (float* l, float* r, int n, const FxContext& ctx)
{
    const int voices = juce::jlimit (2, 7, ctx.choice (pid::hyper (pid::Hyper::unison)));
    const float rate = ctx.value (pid::hyper (pid::Hyper::rate));
    const float detune = ctx.value (pid::hyper (pid::Hyper::detune));
    const float size = ctx.value (pid::hyper (pid::Hyper::dimSize));
    mix.set (ctx.value (pid::hyper (pid::Hyper::mix)), n);
    dim.set (ctx.value (pid::hyper (pid::Hyper::dimMix)), n);

    if (ctx.noteStarted && ctx.on (pid::hyper (pid::Hyper::retrig)))
        for (int i = 0; i < voices; ++i) phase[i] = (double) i / (double) voices;

    const double hz = 0.08 * std::pow (80.0, (double) rate);
    const double inc = hz / sr;
    const float depth = (float) (detune * detune * 0.004 * sr);    // up to 4 ms
    const float baseD = (float) (0.006 * sr);
    const float dimScale = 0.4f + size * 1.2f;
    const float dTaps[4] = { 0.0113f, 0.0171f, 0.0137f, 0.0229f };

    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i], inR = r[i];
        hyperL.push (inL);
        hyperR.push (inR);

        float wl = 0.0f, wr = 0.0f;
        for (int v = 0; v < voices; ++v)
        {
            phase[v] += inc * (1.0 + 0.13 * v);
            const float d = baseD + (float) v * 0.0011f * (float) sr + depth * (0.5f + 0.5f * lfoSin (phase[v]));
            const float pan = voices > 1 ? (float) v / (float) (voices - 1) : 0.5f;
            const float sl = hyperL.read (d), sr2 = hyperR.read (d);
            wl += (sl * (1.0f - pan) + sr2 * 0.25f);
            wr += (sr2 * pan + sl * 0.25f);
        }
        const float norm = 1.6f / (float) voices;
        const float m = mix.next();
        float outL = inL + (wl * norm - inL) * m;
        float outR = inR + (wr * norm - inR) * m;

        // Dimension: short cross-panned delays widen the image.
        dimL.push (outL);
        dimR.push (outR);
        const float dm = dim.next();
        if (dm > 0.0001f)
        {
            const float a = dimL.read (dTaps[0] * dimScale * (float) sr) - dimR.read (dTaps[2] * dimScale * (float) sr);
            const float b = dimR.read (dTaps[1] * dimScale * (float) sr) - dimL.read (dTaps[3] * dimScale * (float) sr);
            outL += a * dm * 0.7f;
            outR += b * dm * 0.7f;
        }
        l[i] = outL;
        r[i] = outR;
    }
    for (auto& p : phase) p -= std::floor (p);
}

//==============================================================================
// Distortion
void Distortion::prepare (double rate, int)
{
    sr = rate;
    filter.prepare (sr, sr);
    reset();
}

void Distortion::reset()
{
    filter.reset();
    hold[0] = hold[1] = 0.0f; holdCount = 0;
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
    mix.init = drv.init = false;
}

float Distortion::shape (DistMode mode, float x, float d) noexcept
{
    const float g = 1.0f + d * d * 40.0f;
    const float v = x * g;
    switch (mode)
    {
        case DistMode::tube:        return fastTanh (v + 0.15f * d) - fastTanh (0.15f * d);
        case DistMode::softClip:    return fastTanh (v);
        case DistMode::hardClip:    return juce::jlimit (-1.0f, 1.0f, v);
        case DistMode::diode1:      return v > 0.0f ? 1.0f - std::exp (-v) : -0.7f * (1.0f - std::exp (v * 0.6f));
        case DistMode::diode2:      return v > 0.0f ? fastTanh (v) : 0.25f * fastTanh (v * 0.5f);
        case DistMode::linFold:
        {
            float t = (v + 1.0f) * 0.25f;
            t -= std::floor (t);
            return 4.0f * std::abs (t - 0.5f) - 1.0f;
        }
        case DistMode::sinFold:     return std::sin (v * dsp::kPi * 0.5f);
        case DistMode::zeroSquare:  return std::copysign (std::min (1.0f, v * v), v);
        case DistMode::downsample:  return x;
        case DistMode::asym:        return v > 0.0f ? fastTanh (v) : fastTanh (v * 2.5f) * 0.6f;
        case DistMode::rectify:     return std::min (1.0f, std::abs (v)) * 2.0f - 1.0f;
        case DistMode::xShaper:
        {
            const float c = juce::jlimit (-1.5f, 1.5f, v);
            return 1.5f * c - 0.5f * c * c * c;
        }
        case DistMode::xShaperAsym:
        {
            const float c = juce::jlimit (-1.2f, 1.2f, v);
            return juce::jlimit (-1.0f, 1.0f, c + 0.4f * c * c - 0.35f * c * c * c);
        }
        case DistMode::sineShaper:  return std::sin (v * dsp::kPi);
        case DistMode::stompBox:    return fastTanh (v * 3.0f + 0.3f) * 0.9f - 0.27f;
        case DistMode::bitCrush:
        {
            const float levels = std::exp2 (16.0f - d * 14.5f);
            return std::round (x * levels) / levels;
        }
        case DistMode::fuzz:        return std::copysign (1.0f - std::exp (-std::abs (v * 2.0f)), v);
        case DistMode::overdrive:   return v / (1.0f + std::abs (v)) * 1.5f;
        case DistMode::count:       break;
    }
    return x;
}

void Distortion::process (float* l, float* r, int n, const FxContext& ctx)
{
    const auto mode = (DistMode) ctx.choice (pid::dist (pid::Dist::mode));
    const auto fpos = (DistFilterPos) ctx.choice (pid::dist (pid::Dist::filterPos));
    mix.set (ctx.value (pid::dist (pid::Dist::mix)), n);
    drv.set (ctx.value (pid::dist (pid::Dist::drive)), n);

    if (fpos != DistFilterPos::off)
    {
        static constexpr int types[] = { 5, 12, 9 };  // Low 12, Band 12, High 12
        filter.setType (types[juce::jlimit (0, 2, ctx.choice (pid::dist (pid::Dist::filterType)))]);
        filter.setParameters (ctx.value (pid::dist (pid::Dist::freq)), ctx.value (pid::dist (pid::Dist::bw)) * 0.85f, 0.0f, 0.0f, 0.0f);
    }

    const bool asymmetric = mode == DistMode::tube || mode == DistMode::diode1 || mode == DistMode::diode2 || mode == DistMode::asym
                         || mode == DistMode::rectify || mode == DistMode::xShaperAsym || mode == DistMode::stompBox;
    const float dcR = 1.0f - (float) (2.0 * dsp::kPiD * 10.0 / sr);

    for (int i = 0; i < n; ++i)
    {
        const float m = mix.next();
        const float d = drv.next();
        float x[2] = { l[i], r[i] };
        float y[2] = { x[0], x[1] };
        if (fpos == DistFilterPos::pre) filter.process (y[0], y[1]);

        if (mode == DistMode::downsample)
        {
            const int period = 1 + (int) (d * d * 63.0f);
            if (++holdCount >= period) { holdCount = 0; hold[0] = y[0]; hold[1] = y[1]; }
            y[0] = hold[0]; y[1] = hold[1];
        }
        else
        {
            for (int c = 0; c < 2; ++c)
            {
                float s = shape (mode, y[c], d);
                if (asymmetric)
                {
                    const float out = s - dcX[c] + dcR * dcY[c];
                    dcX[c] = s; dcY[c] = out;
                    s = out;
                }
                y[c] = s;
            }
        }

        if (fpos == DistFilterPos::post) filter.process (y[0], y[1]);
        l[i] = x[0] + (y[0] - x[0]) * m;
        r[i] = x[1] + (y[1] - x[1]) * m;
    }
}

//==============================================================================
// Flanger
void Flanger::prepare (double rate, int)
{
    sr = rate;
    for (auto& d : line) d.allocate ((int) (0.03 * sr));
    reset();
}

void Flanger::reset() { for (auto& d : line) d.clear(); fb[0] = fb[1] = 0.0f; mix.init = false; }

void Flanger::process (float* l, float* r, int n, const FxContext& ctx)
{
    const double hz = rateHzFor (ctx, pid::flanger (pid::Flanger::rate), pid::flanger (pid::Flanger::sync), pid::flanger (pid::Flanger::syncRate));
    const float depth = ctx.value (pid::flanger (pid::Flanger::depth));
    const float feedback = ctx.value (pid::flanger (pid::Flanger::feedback)) * 0.95f;
    const double stereo = ctx.value (pid::flanger (pid::Flanger::stereo)) / 360.0;
    mix.set (ctx.value (pid::flanger (pid::Flanger::mix)), n);
    const double inc = hz / sr;
    const float minD = (float) (0.0005 * sr), range = (float) (0.007 * sr) * depth;

    for (int i = 0; i < n; ++i)
    {
        phase += inc;
        const float m = mix.next();
        float* ch[2] = { l + i, r + i };
        for (int c = 0; c < 2; ++c)
        {
            const float lfo = 0.5f + 0.5f * lfoSin (phase + (c == 1 ? stereo : 0.0));
            const float d = minD + range * lfo;
            const float in = *ch[c];
            const float delayed = line[c].read (d);
            line[c].push (in + fb[c] * feedback);
            fb[c] = delayed;
            *ch[c] = in + (0.5f * (in + delayed) - in) * m * 1.0f;
        }
    }
    phase -= std::floor (phase);
}

//==============================================================================
// Phaser
void Phaser::prepare (double rate, int) { sr = rate; reset(); }
void Phaser::reset()
{
    for (auto& a : x1) for (auto& v : a) v = 0.0f;
    for (auto& a : y1) for (auto& v : a) v = 0.0f;
    fb[0] = fb[1] = 0.0f;
    mix.init = false;
}

void Phaser::process (float* l, float* r, int n, const FxContext& ctx)
{
    const double hz = rateHzFor (ctx, pid::phaser (pid::Phaser::rate), pid::phaser (pid::Phaser::sync), pid::phaser (pid::Phaser::syncRate));
    const float depth = ctx.value (pid::phaser (pid::Phaser::depth));
    const float centre = ctx.value (pid::phaser (pid::Phaser::freq));
    const float feedback = ctx.value (pid::phaser (pid::Phaser::feedback)) * 0.92f;
    const double stereo = ctx.value (pid::phaser (pid::Phaser::stereo)) / 360.0;
    const int stages = juce::jlimit (2, 12, ctx.choice (pid::phaser (pid::Phaser::stages)));
    mix.set (ctx.value (pid::phaser (pid::Phaser::mix)), n);
    const double inc = hz / sr;

    for (int i = 0; i < n; ++i)
    {
        const float m = mix.next();
        float* ch[2] = { l + i, r + i };
        for (int c = 0; c < 2; ++c)
        {
            // Recompute the allpass coefficient every 8 samples (cheap and smooth enough).
            const float lfo = lfoSin (phase + (c == 1 ? stereo : 0.0));
            const float f = juce::jlimit (20.0f, (float) sr * 0.45f, centre * std::exp2 (lfo * depth * 2.5f));
            const float t = std::tan (dsp::kPi * f / (float) sr);
            const float a = (t - 1.0f) / (t + 1.0f);
            const float in = *ch[c];
            float y = in + fb[c] * feedback;
            for (int s = 0; s < stages; ++s)
            {
                const float out = a * y + x1[c][s] - a * y1[c][s];
                x1[c][s] = y; y1[c][s] = out;
                y = out;
            }
            fb[c] = dsp::flushDenormal (y);
            *ch[c] = in + (0.5f * (in + y) - in) * m;
        }
        phase += inc;
    }
    phase -= std::floor (phase);
}

//==============================================================================
// Chorus
void Chorus::prepare (double rate, int)
{
    sr = rate;
    for (auto& d : line) d.allocate ((int) (0.06 * sr));
    reset();
}
void Chorus::reset() { for (auto& d : line) d.clear(); lp[0] = lp[1] = fb[0] = fb[1] = 0.0f; mix.init = false; }

void Chorus::process (float* l, float* r, int n, const FxContext& ctx)
{
    const double hz = rateHzFor (ctx, pid::chorus (pid::Chorus::rate), pid::chorus (pid::Chorus::sync), pid::chorus (pid::Chorus::syncRate));
    const float d1 = ctx.value (pid::chorus (pid::Chorus::delay1)) * 0.001f * (float) sr;
    const float d2 = ctx.value (pid::chorus (pid::Chorus::delay2)) * 0.001f * (float) sr;
    const float depth = ctx.value (pid::chorus (pid::Chorus::depth)) * 0.001f * (float) sr;
    const float feedback = ctx.value (pid::chorus (pid::Chorus::feedback));
    const float lpHz = ctx.value (pid::chorus (pid::Chorus::lpf));
    const float lpA = std::exp (-kTwoPi * lpHz / (float) sr);
    mix.set (ctx.value (pid::chorus (pid::Chorus::mix)), n);
    const double inc = hz / sr;

    for (int i = 0; i < n; ++i)
    {
        phase += inc;
        const float m = mix.next();
        float* ch[2] = { l + i, r + i };
        for (int c = 0; c < 2; ++c)
        {
            const double off = c == 0 ? 0.0 : 0.25;
            const float m1 = 0.5f + 0.5f * lfoSin (phase + off);
            const float m2 = 0.5f + 0.5f * lfoSin (phase + off + 0.5);
            const float in = *ch[c];
            const float a = line[c].read (std::max (1.0f, d1 + depth * m1));
            const float b = line[c].read (std::max (1.0f, d2 + depth * m2));
            float wet = 0.5f * (a + b);
            line[c].push (in + fb[c] * feedback);
            fb[c] = wet;
            lp[c] = wet + lpA * (lp[c] - wet);
            wet = lp[c];
            *ch[c] = in + (wet - in) * m;
        }
    }
    phase -= std::floor (phase);
}

//==============================================================================
// Stereo delay
void StereoDelay::prepare (double rate, int)
{
    sr = rate;
    for (auto& d : line) d.allocate ((int) (4.5 * sr));
    reset();
}
void StereoDelay::reset() { for (auto& d : line) d.clear(); lpS[0] = lpS[1] = hpS[0] = hpS[1] = 0.0f; mix.init = false; first = true; }

void StereoDelay::process (float* l, float* r, int n, const FxContext& ctx)
{
    const auto mode = (DelayMode) ctx.choice (pid::delay (pid::Delay::mode));
    const bool link = ctx.on (pid::delay (pid::Delay::link));
    double tl, tr;
    if (ctx.on (pid::delay (pid::Delay::sync)))
    {
        tl = ctx.syncSeconds (ctx.choice (pid::delay (pid::Delay::syncL)));
        tr = link ? tl : ctx.syncSeconds (ctx.choice (pid::delay (pid::Delay::syncR)));
    }
    else
    {
        tl = ctx.value (pid::delay (pid::Delay::timeL)) * 0.001;
        tr = link ? tl : ctx.value (pid::delay (pid::Delay::timeR)) * 0.001;
    }
    const float maxD = (float) line[0].capacity() - 2.0f;
    const float targetL = juce::jlimit (1.0f, maxD, (float) (tl * sr));
    const float targetR = juce::jlimit (1.0f, maxD, (float) (tr * sr));
    if (first) { timeL = targetL; timeR = targetR; first = false; }

    const float feedback = juce::jlimit (0.0f, 1.05f, ctx.value (pid::delay (pid::Delay::feedback)));
    const float centre = ctx.value (pid::delay (pid::Delay::freq));
    const float bw = ctx.value (pid::delay (pid::Delay::bw));
    const float loHz = juce::jlimit (20.0f, 20000.0f, centre / std::exp2 (bw * 0.5f));
    const float hiHz = juce::jlimit (40.0f, 20000.0f, centre * std::exp2 (bw * 0.5f));
    const float lpA = std::exp (-kTwoPi * hiHz / (float) sr);
    const float hpA = std::exp (-kTwoPi * loHz / (float) sr);
    mix.set (ctx.value (pid::delay (pid::Delay::mix)), n);
    const float glide = 1.0f - std::exp (-1.0f / (0.05f * (float) sr));

    for (int i = 0; i < n; ++i)
    {
        timeL += (targetL - timeL) * glide;
        timeR += (targetR - timeR) * glide;
        const float inL = l[i], inR = r[i];
        float dl = line[0].read (timeL);
        float dr = line[1].read (timeR);

        // band-pass (HP + LP one-poles) and soft saturation in the feedback path
        auto tone = [&] (int c, float x)
        {
            lpS[c] = x + lpA * (lpS[c] - x);
            hpS[c] = lpS[c] + hpA * (hpS[c] - lpS[c]);
            return fastTanh ((lpS[c] - hpS[c]) * 0.9f) / 0.9f;
        };
        const float fl = tone (0, dl), fr = tone (1, dr);

        switch (mode)
        {
            case DelayMode::pingPong:
                line[0].push (0.5f * (inL + inR) + fr * feedback);
                line[1].push (fl * feedback);
                break;
            case DelayMode::tapDelay:
                line[0].push (inL + fl * feedback);
                line[1].push (inR + dl * 0.6f + fr * feedback);
                break;
            case DelayMode::normal:
            case DelayMode::count:
                line[0].push (inL + fl * feedback);
                line[1].push (inR + fr * feedback);
                break;
        }
        const float m = mix.next();
        l[i] = inL + (dl - inL) * m;
        r[i] = inR + (dr - inR) * m;
    }
}

//==============================================================================
// Compressor (single band or OTT-style three-band)
void Compressor::Biquad::setLR (bool high, float f, double rate)
{
    const float w = kTwoPi * f / (float) rate;
    const float c = std::cos (w), s = std::sin (w);
    const float alpha = s / (2.0f * 0.70710678f);
    const float a0 = 1.0f + alpha;
    if (high) { b0 = (1.0f + c) * 0.5f / a0; b1 = -(1.0f + c) / a0; b2 = b0; }
    else      { b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0;  b2 = b0; }
    a1 = -2.0f * c / a0;
    a2 = (1.0f - alpha) / a0;
}

void Compressor::prepare (double rate, int)
{
    sr = rate;
    lo1a.setLR (false, 150.0f, sr); lo1b.setLR (false, 150.0f, sr);
    hi1a.setLR (true, 150.0f, sr);  hi1b.setLR (true, 150.0f, sr);
    lo2a.setLR (false, 2500.0f, sr); lo2b.setLR (false, 2500.0f, sr);
    hi2a.setLR (true, 2500.0f, sr);  hi2b.setLR (true, 2500.0f, sr);
    reset();
}

void Compressor::reset()
{
    for (auto* b : { &lo1a, &lo1b, &hi1a, &hi1b, &lo2a, &lo2b, &hi2a, &hi2b }) b->reset();
    for (int i = 0; i < 3; ++i) { env[i] = -100.0f; gainSm[i] = 1.0f; }
    mix.init = false;
}

float Compressor::compressSample (int band, float levelDb, float thr, float ratio, bool upward) noexcept
{
    // envelope follower in dB
    const float coef = levelDb > env[band] ? attCoef : relCoef;
    env[band] = levelDb + coef * (env[band] - levelDb);
    const float e = env[band];
    constexpr float knee = 6.0f;
    float gr = 0.0f;
    const float over = e - thr;
    if (over > knee * 0.5f)       gr = over * (1.0f / ratio - 1.0f);
    else if (over > -knee * 0.5f) { const float x = over + knee * 0.5f; gr = (1.0f / ratio - 1.0f) * x * x / (2.0f * knee); }

    if (upward)
    {
        const float lowThr = thr - 24.0f;
        if (e < lowThr && e > -70.0f)
            gr += std::min (18.0f, (lowThr - e) * 0.6f);
    }
    return gr;
}

void Compressor::process (float* l, float* r, int n, const FxContext& ctx)
{
    const float thr = ctx.value (pid::comp (pid::Comp::threshold));
    const float ratio = std::max (1.0f, ctx.value (pid::comp (pid::Comp::ratio)));
    const float makeup = juce::Decibels::decibelsToGain (ctx.value (pid::comp (pid::Comp::gain)));
    const bool multi = ctx.on (pid::comp (pid::Comp::multiband));
    attCoef = std::exp (-1.0f / (std::max (0.05f, ctx.value (pid::comp (pid::Comp::attack))) * 0.001f * (float) sr));
    relCoef = std::exp (-1.0f / (std::max (1.0f, ctx.value (pid::comp (pid::Comp::release))) * 0.001f * (float) sr));
    const float bandGain[3] = { ctx.value (pid::comp (pid::Comp::lowGain)), ctx.value (pid::comp (pid::Comp::midGain)), ctx.value (pid::comp (pid::Comp::highGain)) };
    mix.set (ctx.value (pid::comp (pid::Comp::mix)), n);
    float maxGr = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i], inR = r[i];
        float outL, outR;
        if (! multi)
        {
            const float level = juce::Decibels::gainToDecibels (std::max (std::abs (inL), std::abs (inR)), -100.0f);
            const float gr = compressSample (0, level, thr, ratio, false);
            const float g = juce::Decibels::decibelsToGain (gr);
            maxGr = std::min (maxGr, gr);
            outL = inL * g * makeup;
            outR = inR * g * makeup;
        }
        else
        {
            float bl[3], br[3];
            float x[2] = { inL, inR };
            for (int c = 0; c < 2; ++c)
            {
                const float lo = lo1b.process (c, lo1a.process (c, x[c]));
                const float rest = hi1b.process (c, hi1a.process (c, x[c]));
                const float mid = lo2b.process (c, lo2a.process (c, rest));
                const float hi = hi2b.process (c, hi2a.process (c, rest));
                (c == 0 ? bl : br)[0] = lo;
                (c == 0 ? bl : br)[1] = -mid;   // LR4 sections invert the mid band; restore polarity
                (c == 0 ? bl : br)[2] = hi;
            }
            outL = outR = 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                const float level = juce::Decibels::gainToDecibels (std::max (std::abs (bl[b]), std::abs (br[b])), -100.0f);
                const float gr = compressSample (b, level, thr, ratio, true);
                const float g = juce::Decibels::decibelsToGain (gr) * bandGain[b];
                maxGr = std::min (maxGr, gr);
                outL += bl[b] * g;
                outR += br[b] * g;
            }
            outL *= makeup; outR *= makeup;
        }
        const float m = mix.next();
        l[i] = inL + (outL - inL) * m;
        r[i] = inR + (outR - inR) * m;
    }
    grDisplay = maxGr;
}

//==============================================================================
// EQ
namespace
{
    void bqPeak (float* c, float f, float q, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * f / (float) sr, alpha = std::sin (w) / (2.0f * q), cw = std::cos (w);
        const float a0 = 1.0f + alpha / A;
        c[0] = (1.0f + alpha * A) / a0; c[1] = -2.0f * cw / a0; c[2] = (1.0f - alpha * A) / a0; c[3] = -2.0f * cw / a0; c[4] = (1.0f - alpha / A) / a0;
    }
    void bqShelf (float* c, float f, float q, float gainDb, double sr, bool low)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * f / (float) sr, cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / (2.0f * std::min (q, 2.0f)), sq = 2.0f * std::sqrt (A) * alpha;
        float b0, b1, b2, a0, a1, a2;
        if (low) { b0 = A * ((A + 1) - (A - 1) * cw + sq); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sq);
                   a0 = (A + 1) + (A - 1) * cw + sq; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - sq; }
        else     { b0 = A * ((A + 1) + (A - 1) * cw + sq); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sq);
                   a0 = (A + 1) - (A - 1) * cw + sq; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - sq; }
        c[0] = b0 / a0; c[1] = b1 / a0; c[2] = b2 / a0; c[3] = a1 / a0; c[4] = a2 / a0;
    }
    void bqPass (float* c, float f, float q, double sr, bool high)
    {
        const float w = kTwoPi * f / (float) sr, cw = std::cos (w), alpha = std::sin (w) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        if (high) { c[0] = (1.0f + cw) * 0.5f / a0; c[1] = -(1.0f + cw) / a0; c[2] = c[0]; }
        else      { c[0] = (1.0f - cw) * 0.5f / a0; c[1] = (1.0f - cw) / a0;  c[2] = c[0]; }
        c[3] = -2.0f * cw / a0; c[4] = (1.0f - alpha) / a0;
    }
    inline float bqRun (const float* c, float* z, float x) noexcept
    {
        const float y = c[0] * x + z[0];
        z[0] = c[1] * x - c[3] * y + z[1];
        z[1] = c[2] * x - c[4] * y;
        return y;
    }
}

void Equalizer::prepare (double rate, int) { sr = rate; reset(); }
void Equalizer::reset() { for (auto* b : { &low, &high }) for (auto& z : b->z) z[0] = z[1] = 0.0f; }

void Equalizer::process (float* l, float* r, int n, const FxContext& ctx)
{
    const float nyq = (float) sr * 0.45f;
    const float lf = juce::jlimit (20.0f, nyq, ctx.value (pid::eq (pid::Eq::lowFreq)));
    const float hf = juce::jlimit (20.0f, nyq, ctx.value (pid::eq (pid::Eq::highFreq)));
    const float lq = ctx.value (pid::eq (pid::Eq::lowQ)), hq = ctx.value (pid::eq (pid::Eq::highQ));
    const float lg = ctx.value (pid::eq (pid::Eq::lowGain)), hg = ctx.value (pid::eq (pid::Eq::highGain));
    switch ((EqLowType) ctx.choice (pid::eq (pid::Eq::lowType)))
    {
        case EqLowType::shelf:    bqShelf (low.c, lf, lq, lg, sr, true); break;
        case EqLowType::peak:     bqPeak (low.c, lf, lq, lg, sr); break;
        case EqLowType::highPass: bqPass (low.c, lf, lq, sr, true); break;
        case EqLowType::count:    break;
    }
    switch ((EqHighType) ctx.choice (pid::eq (pid::Eq::highType)))
    {
        case EqHighType::shelf:   bqShelf (high.c, hf, hq, hg, sr, false); break;
        case EqHighType::peak:    bqPeak (high.c, hf, hq, hg, sr); break;
        case EqHighType::lowPass: bqPass (high.c, hf, hq, sr, false); break;
        case EqHighType::count:   break;
    }
    for (int i = 0; i < n; ++i)
    {
        l[i] = bqRun (high.c, high.z[0], bqRun (low.c, low.z[0], l[i]));
        r[i] = bqRun (high.c, high.z[1], bqRun (low.c, low.z[1], r[i]));
    }
}

//==============================================================================
// FX filter
void FxFilterUnit::prepare (double rate, int) { filter.prepare (rate, rate); reset(); }
void FxFilterUnit::reset() { filter.reset(); mix.init = false; }

void FxFilterUnit::process (float* l, float* r, int n, const FxContext& ctx)
{
    filter.setType (ctx.choice (pid::fxFilter (pid::FxFilter::type)));
    filter.setParameters (ctx.value (pid::fxFilter (pid::FxFilter::cutoff)), ctx.value (pid::fxFilter (pid::FxFilter::reso)),
                          ctx.value (pid::fxFilter (pid::FxFilter::drive)), ctx.value (pid::fxFilter (pid::FxFilter::var)), ctx.value (pid::fxFilter (pid::FxFilter::stereo)));
    mix.set (ctx.value (pid::fxFilter (pid::FxFilter::mix)), n);
    for (int i = 0; i < n; ++i)
    {
        float a = l[i], b = r[i];
        filter.process (a, b);
        const float m = mix.next();
        l[i] += (a - l[i]) * m;
        r[i] += (b - r[i]) * m;
    }
}

//==============================================================================
// Bode frequency shifter
void BodeShifter::Hilbert::process (float x, float& resultI, float& resultQ) noexcept
{
    static constexpr float a1[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
    static constexpr float a2[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
    float i = x, q = x;
    for (int s = 0; s < 4; ++s)
    {
        const float k1 = a1[s] * a1[s];
        const float outI = k1 * (i + yi[s][1]) - xi[s][1];
        xi[s][1] = xi[s][0]; xi[s][0] = i; yi[s][1] = yi[s][0]; yi[s][0] = outI;
        i = outI;
        const float k2 = a2[s] * a2[s];
        const float outQ2 = k2 * (q + yq[s][1]) - xq[s][1];
        xq[s][1] = xq[s][0]; xq[s][0] = q; yq[s][1] = yq[s][0]; yq[s][0] = outQ2;
        q = outQ2;
    }
    resultI = i;
    resultQ = qDelay;
    qDelay = q;
}

void BodeShifter::prepare (double rate, int) { sr = rate; reset(); }
void BodeShifter::reset() { h[0] = Hilbert(); h[1] = Hilbert(); phase[0] = phase[1] = 0.0; fb[0] = fb[1] = 0.0f; mix.init = false; }

void BodeShifter::process (float* l, float* r, int n, const FxContext& ctx)
{
    const float shift = ctx.value (pid::bode (pid::Bode::shift));
    const float feedback = ctx.value (pid::bode (pid::Bode::feedback));
    const float spread = ctx.value (pid::bode (pid::Bode::spread));
    mix.set (ctx.value (pid::bode (pid::Bode::mix)), n);
    const double inc[2] = { shift / sr, shift * (1.0 - 2.0 * spread) / sr };

    for (int i = 0; i < n; ++i)
    {
        const float m = mix.next();
        float* ch[2] = { l + i, r + i };
        for (int c = 0; c < 2; ++c)
        {
            const float in = *ch[c];
            float I, Q;
            h[c].process (in + fb[c] * feedback, I, Q);
            phase[c] += inc[c];
            phase[c] -= std::floor (phase[c]);
            const float ph = (float) (kTwoPi * phase[c]);
            const float y = I * std::cos (ph) - Q * std::sin (ph);
            fb[c] = dsp::flushDenormal (y);
            *ch[c] = in + (y - in) * m;
        }
    }
}

//==============================================================================
// Utility
void Utility::prepare (double rate, int) { sr = rate; reset(); }
void Utility::reset() { lp1[0] = lp1[1] = lp2[0] = lp2[1] = 0.0f; gain.init = false; }

void Utility::process (float* l, float* r, int n, const FxContext& ctx)
{
    gain.set (juce::Decibels::decibelsToGain (ctx.value (pid::util (pid::Util::gain))), n);
    const float pan = ctx.value (pid::util (pid::Util::pan));
    const float width = ctx.value (pid::util (pid::Util::width));
    const bool monoBass = ctx.on (pid::util (pid::Util::monoBass));
    const float a = std::exp (-kTwoPi * ctx.value (pid::util (pid::Util::monoFreq)) / (float) sr);
    const float invL = ctx.on (pid::util (pid::Util::invertL)) ? -1.0f : 1.0f;
    const float invR = ctx.on (pid::util (pid::Util::invertR)) ? -1.0f : 1.0f;
    const float panL = pan > 0.0f ? 1.0f - pan : 1.0f;
    const float panR = pan < 0.0f ? 1.0f + pan : 1.0f;

    for (int i = 0; i < n; ++i)
    {
        float L = l[i] * invL, R = r[i] * invR;
        if (monoBass)
        {
            // two cascaded one-poles: low part summed to mono, high part keeps its stereo image
            lp1[0] = L + a * (lp1[0] - L); lp2[0] = lp1[0] + a * (lp2[0] - lp1[0]);
            lp1[1] = R + a * (lp1[1] - R); lp2[1] = lp1[1] + a * (lp2[1] - lp1[1]);
            const float lowMono = 0.5f * (lp2[0] + lp2[1]);
            L = L - lp2[0] + lowMono;
            R = R - lp2[1] + lowMono;
        }
        const float mid = 0.5f * (L + R), side = 0.5f * (L - R) * width;
        const float g = gain.next();
        l[i] = (mid + side) * panL * g;
        r[i] = (mid - side) * panR * g;
    }
}

} // namespace cureum
