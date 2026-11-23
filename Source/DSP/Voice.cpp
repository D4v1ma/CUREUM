#include "Voice.h"
#include "../Core/ValueMaps.h"

namespace cureum
{

using namespace pid;

void Voice::prepare (double renderRate, double maxRenderRate, int voiceIndex)
{
    index = voiceIndex;
    maxSr = maxRenderRate;
    for (auto& f : filter)
        f.prepare (renderRate, maxRenderRate);
    setRenderRate (renderRate);
}

void Voice::setRenderRate (double rate)
{
    sr = rate;
    for (auto& o : osc) o.prepare (rate);
    sub.prepare (rate);
    noise.prepare (rate);
    for (auto& f : filter) f.setSampleRate (rate);
    for (auto& e : env) e.setSampleRate (rate);
    fadeStep = 1.0f / (float) (0.0015 * rate);
}

static LfoGen::Settings lfoSettings (const ParamCache& p, int l)
{
    LfoGen::Settings s;
    s.type = (LfoType) p.choice (pid::lfo (l, Lfo::type));
    s.mode = (LfoMode) p.choice (pid::lfo (l, Lfo::mode));
    s.rise = p.plain (pid::lfo (l, Lfo::rise));
    s.delay = p.plain (pid::lfo (l, Lfo::delay));
    s.smooth = p.plain (pid::lfo (l, Lfo::smooth));
    s.phaseOffset = p.plain (pid::lfo (l, Lfo::phase)) / 360.0f;
    return s;
}

static const NoiseSample* noiseSampleFor (const ParamCache& p, const VoiceContext& ctx)
{
    const auto type = (NoiseType) p.choice (noise (Noise::type));
    if (type == NoiseType::custom && ctx.customNoise != nullptr)
        return ctx.customNoise;
    return &NoiseBank::get().sample (type);
}

void Voice::start (const StartInfo& info, const VoiceContext& ctx)
{
    const auto& P = *ctx.params;
    active = true;
    stealing = false;
    pending.valid = false;
    fade = 1.0f;
    note = info.note;
    velocity = info.velocity;
    channel = info.channel;
    age = info.age;
    keyDown = true;
    sustained = false;
    releaseVel = polyAT = mpeBend = mpePressure = mpeSlide = 0.0f;

    dsp::Random rng (info.seed);
    random1 = rng.uniform();
    random2 = rng.bipolar();

    glideTarget = (float) note;
    const float porta = P.plain (global (Global::portaTime));
    if (info.glideFrom >= 0.0f && porta > 0.0005f && std::abs (info.glideFrom - glideTarget) > 0.001f)
    {
        glideStart = info.glideFrom;
        glideT = 0.0f;
        glideSeconds = porta * (P.on (global (Global::portaScaled)) ? std::max (0.08f, std::abs (glideTarget - glideStart) / 12.0f) : 1.0f);
        glideCurve = P.plain (global (Global::portaCurve));
        pitch = glideStart;
    }
    else
    {
        pitch = glideStart = glideTarget;
        glideT = 1.0f;
    }

    std::fill (std::begin (mod), std::end (mod), 0.0f);

    for (int o = 0; o < kNumOscillators; ++o)
        osc[o].noteOn (P.plain (pid::osc (o, Osc::phase)), P.plain (pid::osc (o, Osc::randPhase)), rng.next());
    sub.noteOn (P.plain (pid::sub (Sub::phase)));
    noise.noteOn (noiseSampleFor (P, ctx), P.plain (pid::noise (Noise::phase)), P.plain (pid::noise (Noise::randPhase)), rng.next());

    for (auto& f : filter) f.reset();

    for (int e = 0; e < kNumEnvelopes; ++e)
    {
        env[e].setParameters (P.plain (pid::env (e, Env::attack)), P.plain (pid::env (e, Env::hold)), P.plain (pid::env (e, Env::decay)),
                              P.plain (pid::env (e, Env::sustain)), P.plain (pid::env (e, Env::release)),
                              P.plain (pid::env (e, Env::aCurve)), P.plain (pid::env (e, Env::dCurve)), P.plain (pid::env (e, Env::rCurve)));
        env[e].noteOn (false);
    }

    for (int l = 0; l < kNumLfos; ++l)
        lfo[l].noteOn (lfoSettings (P, l), rng.next());

    for (auto& g : gain) g = -1.0f;   // snap on the first control block
}

void Voice::startOrSteal (const StartInfo& info, const VoiceContext& ctx)
{
    if (active && env[0].current() > 0.0005f)
    {
        stealing = true;
        pending.valid = true;
        pending.info = info;
        return;
    }
    start (info, ctx);
}

void Voice::glideTo (int newNote, float vel, juce::uint64 newAge, const VoiceContext& ctx)
{
    const auto& P = *ctx.params;
    const float porta = P.plain (global (Global::portaTime));
    note = newNote;
    velocity = vel;
    age = newAge;
    keyDown = true;
    sustained = false;
    glideTarget = (float) newNote;
    if (porta > 0.0005f)
    {
        glideStart = pitch;
        glideT = 0.0f;
        glideSeconds = porta * (P.on (global (Global::portaScaled)) ? std::max (0.08f, std::abs (glideTarget - glideStart) / 12.0f) : 1.0f);
        glideCurve = P.plain (global (Global::portaCurve));
    }
    else
    {
        pitch = glideStart = glideTarget;
        glideT = 1.0f;
    }
}

void Voice::release (float releaseVelocity)
{
    keyDown = false;
    sustained = false;
    releaseVel = releaseVelocity;
    for (auto& e : env) e.noteOff();
}

void Voice::kill()
{
    active = false;
    stealing = false;
    pending.valid = false;
    for (auto& e : env) e.reset();
}

void Voice::render (float* outL, float* outR, int numSamples, const VoiceContext& ctx, int offsetInCall)
{
    int done = 0;
    const int block = kControlBlock * std::max (1, ctx.oversample);
    while (done < numSamples && active)
    {
        const int n = std::min (block, numSamples - done);
        updateControl (n, ctx, offsetInCall + done);
        renderAudio (outL + done, outR + done, n);
        done += n;

        if (stealing && fade <= 0.0f && pending.valid)
        {
            const auto info = pending.info;
            start (info, ctx);
        }
    }
}

void Voice::updateControl (int n, const VoiceContext& ctx, int offsetInCall)
{
    const auto& P = *ctx.params;
    const float seconds = (float) ((double) n / sr);

    // ---- glide
    if (glideT < 1.0f)
    {
        glideT = std::min (1.0f, glideT + seconds / std::max (1.0e-4f, glideSeconds));
        pitch = glideStart + (glideTarget - glideStart) * dsp::shapeCurve (glideT, glideCurve);
    }
    else
        pitch = glideTarget;

    // ---- modulation sources
    for (int e = 0; e < kNumEnvelopes; ++e)
        src[(int) envSource (e)] = env[e].current();
    src[(int) ModSource::velocity] = velocity;
    src[(int) ModSource::note] = juce::jlimit (0.0f, 1.0f, pitch / 127.0f);
    src[(int) ModSource::polyAftertouch] = polyAT;
    src[(int) ModSource::random1] = random1;
    src[(int) ModSource::random2] = random2;
    src[(int) ModSource::gate] = keyDown || sustained ? 1.0f : 0.0f;
    src[(int) ModSource::releaseVelocity] = releaseVel;
    src[(int) ModSource::mpeSlide] = mpeSlide;
    src[(int) ModSource::mpePressure] = mpePressure;
    for (auto s : { ModSource::modWheel, ModSource::pitchBend, ModSource::aftertouch, ModSource::constant })
        src[(int) s] = ctx.globalSources[(int) s];
    for (int m = 0; m < kNumMacros; ++m)
        src[(int) macroSource (m)] = ctx.globalSources[(int) macroSource (m)];

    for (int l = 0; l < kNumLfos; ++l)
    {
        LfoGen::Settings st;
        st.type = (LfoType) P.choice (pid::lfo (l, Lfo::type));
        st.mode = (LfoMode) P.choice (pid::lfo (l, Lfo::mode));
        st.rise = val (ctx, pid::lfo (l, Lfo::rise));
        st.delay = val (ctx, pid::lfo (l, Lfo::delay));
        st.smooth = val (ctx, pid::lfo (l, Lfo::smooth));
        st.phaseOffset = val (ctx, pid::lfo (l, Lfo::phase)) / 360.0f;

        float rateHz;
        if (P.on (pid::lfo (l, Lfo::sync)))
        {
            const int idx = juce::roundToInt (val (ctx, pid::lfo (l, Lfo::syncRate)));
            const double beats = 4.0 * syncRateWholeNotes (idx) * syncFeelMultiplier (P.choice (pid::lfo (l, Lfo::syncFeel)));
            rateHz = (float) (ctx.bpm / 60.0 / beats);
        }
        else
            rateHz = val (ctx, pid::lfo (l, Lfo::rate));

        const double gp = st.mode == LfoMode::free ? ctx.freeLfoPhase[l] + ctx.freeLfoCyclesPerSample[l] * offsetInCall : -1.0;
        src[(int) lfoSource (l)] = lfo[l].advance (st, ctx.lfoShapes[l], (double) rateHz * seconds, seconds, rateHz, gp);
    }

    // ---- matrix
    if (ctx.matrix != nullptr)
    {
        for (int d : ctx.matrix->voiceDests)
            mod[d] = 0.0f;
        for (const auto& r : ctx.matrix->voiceRoutes)
            mod[r.dest] += CompiledMatrix::evaluate (r, src) * P.plain (pid::mod (r.slot));
    }

    // ---- pitch
    const float transpose = P.plain (global (Global::transpose));
    const float tuneRatio = dsp::semitonesToRatio (P.plain (global (Global::tune)) * 0.01f);
    const float played = pitch + transpose + ctx.pitchBendSemis + mpeBend;
    const bool first = gain[0] < 0.0f;
    auto setGain = [&] (int i, float target)
    {
        if (first) { gain[i] = target; gainInc[i] = 0.0f; }
        else gainInc[i] = (target - gain[i]) / (float) n;
    };

    // Which modules feed cross-modulation warps.
    bool needOsc[kNumOscillators] {};
    bool needSub = false, needNoise = false;
    WarpMode warps[kNumOscillators][2];
    for (int o = 0; o < kNumOscillators; ++o)
    {
        warps[o][0] = (WarpMode) P.choice (pid::osc (o, Osc::warp1Mode));
        warps[o][1] = (WarpMode) P.choice (pid::osc (o, Osc::warp2Mode));
        if (! P.on (pid::osc (o, Osc::enable)))
            continue;
        const int partner = o == 0 ? 1 : 0;
        for (auto m : warps[o])
        {
            switch (m)
            {
                case WarpMode::fmOsc: case WarpMode::amOsc: case WarpMode::rmOsc: case WarpMode::pmOsc: needOsc[partner] = true; break;
                case WarpMode::fmA: case WarpMode::pmA: needOsc[0] = true; break;
                case WarpMode::fmB: case WarpMode::pmB: needOsc[1] = true; break;
                case WarpMode::fmC: case WarpMode::pmC: needOsc[2] = true; break;
                case WarpMode::fmNoise: case WarpMode::amNoise: case WarpMode::rmNoise: needNoise = true; break;
                case WarpMode::fmSub: case WarpMode::amSub: case WarpMode::rmSub: needSub = true; break;
                default: break;
            }
        }
    }

    for (int o = 0; o < kNumOscillators; ++o)
    {
        const bool en = P.on (pid::osc (o, Osc::enable));
        oscRun[o] = (en || needOsc[o]) && ctx.tables[o] != nullptr;
        float target = 0.0f;
        if (oscRun[o])
        {
            WavetableOscillator::Settings s;
            s.table = ctx.tables[o];
            const float base = P.on (pid::osc (o, Osc::keyTrack)) ? played : 60.0f + transpose;
            const float semis = val (ctx, pid::osc (o, Osc::octave)) * 12.0f + val (ctx, pid::osc (o, Osc::semi))
                              + val (ctx, pid::osc (o, Osc::fine)) * 0.01f + val (ctx, pid::osc (o, Osc::coarse));
            s.freqHz = juce::jlimit (0.01f, (float) sr * 0.5f, dsp::midiToHz (base + semis) * tuneRatio);
            s.level = 1.0f;
            s.pan = val (ctx, pid::osc (o, Osc::pan));
            s.wtPos = val (ctx, pid::osc (o, Osc::wtPos));
            s.warp1 = warps[o][0];
            s.warp2 = warps[o][1];
            s.warp1Amt = val (ctx, pid::osc (o, Osc::warp1Amt));
            s.warp2Amt = val (ctx, pid::osc (o, Osc::warp2Amt));
            s.unison = P.choice (pid::osc (o, Osc::unison));
            s.detune = val (ctx, pid::osc (o, Osc::detune));
            s.blend = val (ctx, pid::osc (o, Osc::blend));
            s.width = val (ctx, pid::osc (o, Osc::width));
            s.uniMode = (UnisonMode) P.choice (pid::osc (o, Osc::uniMode));
            s.uniStack = (UnisonStack) P.choice (pid::osc (o, Osc::uniStack));
            s.uniWarp = val (ctx, pid::osc (o, Osc::uniWarp));
            s.uniWtPos = val (ctx, pid::osc (o, Osc::uniWtPos));
            osc[o].setup (s, o);
            if (en) target = maps::levelToGain (val (ctx, pid::osc (o, Osc::level)));
        }
        setGain (o, target);
    }

    // ---- sub
    {
        const bool en = P.on (pid::sub (Sub::enable));
        subRun = en || needSub;
        float target = 0.0f;
        if (subRun)
        {
            const float base = P.on (pid::sub (Sub::keyTrack)) ? played : 60.0f + transpose;
            const float semis = val (ctx, pid::sub (Sub::octave)) * 12.0f + val (ctx, pid::sub (Sub::coarse));
            sub.setup (juce::jlimit (0.01f, (float) sr * 0.5f, dsp::midiToHz (base + semis) * tuneRatio),
                       (SubShape) P.choice (pid::sub (Sub::shape)), 1.0f, val (ctx, pid::sub (Sub::pan)));
            if (en) target = maps::levelToGain (val (ctx, pid::sub (Sub::level)));
        }
        setGain (3, target);
    }

    // ---- noise
    {
        const bool en = P.on (pid::noise (Noise::enable));
        noiseRun = en || needNoise;
        float target = 0.0f;
        if (noiseRun)
        {
            float rate = std::exp2 ((val (ctx, pid::noise (Noise::pitch)) - 0.5f) * 8.0f + val (ctx, pid::noise (Noise::fine)) * 0.5f);
            if (P.on (pid::noise (Noise::keyTrack)))
                rate *= dsp::semitonesToRatio (played - 60.0f);
            noise.setup (noiseSampleFor (P, ctx), rate, 1.0f, val (ctx, pid::noise (Noise::pan)),
                         P.on (pid::noise (Noise::oneShot)), P.on (pid::noise (Noise::stereo)));
            if (en) target = maps::levelToGain (val (ctx, pid::noise (Noise::level)));
        }
        setGain (4, target);
    }

    // ---- routing and filters
    for (int i = 0; i < 5; ++i)
        route[i] = P.choice (pid::route ((Route) i));
    parallel = P.choice (pid::route (Route::filterConfig)) == (int) FilterConfig::parallel;

    const float keyOffset = (pitch + transpose - 60.0f) / 12.0f;
    for (int f = 0; f < kNumFilters; ++f)
    {
        filterOn[f] = P.on (pid::filter (f, Filter::enable));
        if (! filterOn[f])
            continue;
        filter[f].setType (P.choice (pid::filter (f, Filter::type)));
        const float cutoff = val (ctx, pid::filter (f, Filter::cutoff)) * std::exp2 (keyOffset * val (ctx, pid::filter (f, Filter::keyTrack)));
        filter[f].setParameters (cutoff, val (ctx, pid::filter (f, Filter::reso)), val (ctx, pid::filter (f, Filter::drive)),
                                 val (ctx, pid::filter (f, Filter::var)), val (ctx, pid::filter (f, Filter::stereo)));
        filterMix[f] = val (ctx, pid::filter (f, Filter::mix));
        filterLevel[f] = val (ctx, pid::filter (f, Filter::level));
    }

    // ---- envelopes
    for (int e = 0; e < kNumEnvelopes; ++e)
        env[e].setParameters (val (ctx, pid::env (e, Env::attack)), val (ctx, pid::env (e, Env::hold)), val (ctx, pid::env (e, Env::decay)),
                              val (ctx, pid::env (e, Env::sustain)), val (ctx, pid::env (e, Env::release)),
                              P.plain (pid::env (e, Env::aCurve)), P.plain (pid::env (e, Env::dCurve)), P.plain (pid::env (e, Env::rCurve)));

    const float velAmt = P.plain (global (Global::velToAmp));
    velGain = 1.0f - velAmt * (1.0f - velocity);
}

void Voice::renderAudio (float* outL, float* outR, int n)
{
    CrossModSignals x;
    for (int o = 0; o < kNumOscillators; ++o)
    {
        x.oscOut[o] = osc[o].rawOutputs();
        x.oscUnison[o] = oscRun[o] ? osc[o].numUnison() : 0;
    }

    for (int i = 0; i < n; ++i)
    {
        float nL = 0.0f, nR = 0.0f, sL = 0.0f, sR = 0.0f;
        x.noise = noiseRun ? noise.renderSample (nL, nR) : 0.0f;
        x.sub = subRun ? sub.renderSample (sL, sR) : 0.0f;

        float oL[kNumOscillators] {}, oR[kNumOscillators] {};
        for (int o = 0; o < kNumOscillators; ++o)
            if (oscRun[o])
                osc[o].renderSample (oL[o], oR[o], x);

        for (int g = 0; g < 5; ++g)
            gain[g] += gainInc[g];

        float f1L = 0.0f, f1R = 0.0f, f2L = 0.0f, f2R = 0.0f, dL = 0.0f, dR = 0.0f;
        auto add = [&] (int dest, float L, float R) noexcept
        {
            switch (dest)
            {
                case 0: f1L += L; f1R += R; break;
                case 1: f2L += L; f2R += R; break;
                case 2: f1L += L; f1R += R; if (parallel) { f2L += L; f2R += R; } break;
                default: dL += L; dR += R; break;
            }
        };
        add (route[0], oL[0] * gain[0], oR[0] * gain[0]);
        add (route[1], oL[1] * gain[1], oR[1] * gain[1]);
        add (route[2], oL[2] * gain[2], oR[2] * gain[2]);
        add (route[3], nL * gain[4], nR * gain[4]);
        add (route[4], sL * gain[3], sR * gain[3]);

        auto runFilter = [this] (int f, float& L, float& R) noexcept
        {
            if (! filterOn[f]) return;
            float l = L, r = R;
            filter[f].process (l, r);
            L = (L + (l - L) * filterMix[f]) * filterLevel[f];
            R = (R + (r - R) * filterMix[f]) * filterLevel[f];
        };

        float yL, yR;
        if (! parallel)
        {
            runFilter (0, f1L, f1R);
            f2L += f1L; f2R += f1R;
            runFilter (1, f2L, f2R);
            yL = f2L + dL; yR = f2R + dR;
        }
        else
        {
            runFilter (0, f1L, f1R);
            runFilter (1, f2L, f2R);
            yL = f1L + f2L + dL; yR = f1R + f2R + dR;
        }

        const float amp = env[0].next();
        env[1].next(); env[2].next(); env[3].next();
        float g = amp * velGain;
        if (stealing)
        {
            fade = std::max (0.0f, fade - fadeStep);
            g *= fade;
        }
        outL[i] += yL * g;
        outR[i] += yR * g;
    }

    if (! env[0].isActive() && ! stealing)
        active = false;
}

} // namespace cureum
