#include "SynthEngine.h"

namespace cureum
{

using namespace pid;

void SynthEngine::prepare (double sampleRate, int maxBlockSize)
{
    baseRate = sampleRate;
    maxBlock = std::max (16, maxBlockSize);
    osL.assign ((size_t) maxBlock * 4, 0.0f);
    osR.assign ((size_t) maxBlock * 4, 0.0f);
    for (int i = 0; i < kMaxVoices; ++i)
        voices[(size_t) i].prepare (baseRate * oversample, baseRate * 4.0, i);
    decimator.reset();
    monoCount = 0;
}

void SynthEngine::setOversampling (int factor)
{
    factor = factor >= 4 ? 4 : (factor >= 2 ? 2 : 1);
    if (factor == oversample)
        return;
    oversample = factor;
    for (auto& v : voices)
        v.setRenderRate (baseRate * oversample);
    decimator.reset();
}

int SynthEngine::numActiveVoices() const noexcept
{
    int n = 0;
    for (auto& v : voices) n += v.isActive() ? 1 : 0;
    return n;
}

const Voice* SynthEngine::newestVoice() const noexcept
{
    const Voice* best = nullptr;
    for (auto& v : voices)
        if (v.isActive() && (best == nullptr || v.getAge() > best->getAge()))
            best = &v;
    return best;
}

Voice* SynthEngine::findVoiceFor (int maxPoly)
{
    int active = 0;
    for (auto& v : voices) active += v.isActive() ? 1 : 0;

    if (active < maxPoly)
        for (auto& v : voices)
            if (! v.isActive())
                return &v;

    // Steal: oldest released voice first, then the oldest held voice.
    Voice* victim = nullptr;
    for (auto& v : voices)
        if (v.isActive() && v.isReleased() && ! v.sustained && (victim == nullptr || v.getAge() < victim->getAge()))
            victim = &v;
    if (victim == nullptr)
        for (auto& v : voices)
            if (v.isActive() && (victim == nullptr || v.getAge() < victim->getAge()))
                victim = &v;
    return victim;
}

void SynthEngine::noteOn (int note, float velocity, int channel, const VoiceContext& ctx)
{
    const auto& P = *ctx.params;
    const bool mono = P.on (global (Global::mono));
    const bool portaAlways = P.on (global (Global::portaAlways));
    const float porta = P.plain (global (Global::portaTime));

    if (mono)
    {
        // remove a duplicate, then push
        for (int i = 0; i < monoCount; ++i)
            if (monoStack[(size_t) i] == note)
            {
                std::copy (monoStack.begin() + i + 1, monoStack.begin() + monoCount, monoStack.begin() + i);
                --monoCount;
                break;
            }
        if (monoCount < (int) monoStack.size())
            monoStack[(size_t) monoCount++] = note;

        // Mono uses voice 0; release any other voices left over from poly mode.
        for (size_t i = 1; i < voices.size(); ++i)
            if (voices[i].isActive() && voices[i].keyDown) voices[i].release (0.0f);

        auto& v = voices[0];
        const bool otherHeld = monoCount > 1 && v.isActive() && v.keyDown;
        monoVelocity = velocity;
        if (otherHeld && P.on (global (Global::legato)))
            v.glideTo (note, velocity, ++ageCounter, ctx);
        else
        {
            Voice::StartInfo info;
            info.note = note; info.velocity = velocity; info.channel = channel;
            info.seed = ++seedCounter * 747796405u; info.age = ++ageCounter;
            const bool glide = porta > 0.0005f && (portaAlways || otherHeld);
            info.glideFrom = glide ? (v.isActive() ? v.getPitch() : lastPitch) : -1.0f;
            if (v.isActive())
                v.startOrSteal (info, ctx);
            else
                v.start (info, ctx);
        }
        lastPitch = (float) note;
        return;
    }

    const int poly = juce::jlimit (1, kMaxVoices, P.choice (global (Global::polyphony)));
    bool anyHeld = false;
    for (auto& v : voices) anyHeld = anyHeld || (v.isActive() && v.keyDown);

    if (auto* v = findVoiceFor (poly))
    {
        Voice::StartInfo info;
        info.note = note; info.velocity = velocity; info.channel = channel;
        info.seed = ++seedCounter * 747796405u; info.age = ++ageCounter;
        const bool glide = porta > 0.0005f && lastPitch >= 0.0f && (portaAlways || anyHeld);
        info.glideFrom = glide ? lastPitch : -1.0f;
        v->startOrSteal (info, ctx);
    }
    lastPitch = (float) note;
}

void SynthEngine::noteOff (int note, float velocity, int channel, const VoiceContext& ctx)
{
    const auto& P = *ctx.params;
    if (P.on (global (Global::mono)))
    {
        for (int i = 0; i < monoCount; ++i)
            if (monoStack[(size_t) i] == note)
            {
                std::copy (monoStack.begin() + i + 1, monoStack.begin() + monoCount, monoStack.begin() + i);
                --monoCount;
                break;
            }
        auto& v = voices[0];
        if (! v.isActive() || v.getNote() != note)
            return;
        if (monoCount > 0)
        {
            const int back = monoStack[(size_t) monoCount - 1];
            if (P.on (global (Global::legato)))
                v.glideTo (back, monoVelocity, ++ageCounter, ctx);
            else
            {
                Voice::StartInfo info;
                info.note = back; info.velocity = monoVelocity; info.channel = channel;
                info.seed = ++seedCounter * 747796405u; info.age = ++ageCounter;
                info.glideFrom = P.plain (global (Global::portaTime)) > 0.0005f ? v.getPitch() : -1.0f;
                v.startOrSteal (info, ctx);
            }
            lastPitch = (float) back;
            return;
        }
        if (sustainDown) { v.keyDown = false; v.sustained = true; }
        else v.release (velocity);
        return;
    }

    for (auto& v : voices)
    {
        if (v.isActive() && v.keyDown && v.getNote() == note && (channel <= 0 || v.getChannel() == channel || ! P.on (global (Global::mpeEnable))))
        {
            if (sustainDown) { v.keyDown = false; v.sustained = true; }
            else v.release (velocity);
        }
    }
}

void SynthEngine::setSustain (bool down)
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.isActive() && v.sustained)
                v.release (0.0f);
}

void SynthEngine::allNotesOff()
{
    monoCount = 0;
    for (auto& v : voices)
        if (v.isActive()) v.release (0.0f);
}

void SynthEngine::allSoundOff()
{
    monoCount = 0;
    for (auto& v : voices) v.kill();
    decimator.reset();
}

void SynthEngine::polyAftertouch (int note, int channel, float value)
{
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note && (channel <= 0 || v.getChannel() == channel))
            v.setPolyAftertouch (value);
}

void SynthEngine::mpeBend (int channel, float semis)
{
    for (auto& v : voices)
        if (v.isActive() && v.getChannel() == channel) v.setMpeBend (semis);
}

void SynthEngine::mpePressure (int channel, float value)
{
    for (auto& v : voices)
        if (v.isActive() && v.getChannel() == channel) v.setMpePressure (value);
}

void SynthEngine::mpeSlide (int channel, float value)
{
    for (auto& v : voices)
        if (v.isActive() && v.getChannel() == channel) v.setMpeSlide (value);
}

void SynthEngine::render (float* outL, float* outR, int numSamples, VoiceContext& ctx)
{
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        const int nOS = n * oversample;
        float* l = osL.data();
        float* r = osR.data();
        std::fill (l, l + nOS, 0.0f);
        std::fill (r, r + nOS, 0.0f);

        ctx.oversample = oversample;
        ctx.renderRate = baseRate * oversample;

        for (auto& v : voices)
            if (v.isActive())
                v.render (l, r, nOS, ctx, 0);

        for (int i = 0; i < kNumLfos; ++i)
            ctx.freeLfoPhase[i] += ctx.freeLfoCyclesPerSample[i] * nOS;

        if (oversample == 1)
        {
            for (int i = 0; i < n; ++i) { outL[done + i] += l[i]; outR[done + i] += r[i]; }
        }
        else
        {
            // decimate in place (the output region of the scratch buffer is never read again)
            decimator.process (l, l, n, oversample, 0);
            decimator.process (r, r, n, oversample, 1);
            for (int i = 0; i < n; ++i) { outL[done + i] += l[i]; outR[done + i] += r[i]; }
        }
        done += n;
    }
}

void SynthEngine::computeGlobalModulation (const VoiceContext& ctx, float* modOut) const noexcept
{
    if (ctx.matrix == nullptr || ctx.matrix->globalRoutes.empty())
        return;

    float sources[kNumModSources] {};
    if (const auto* v = newestVoice())
        std::copy (v->sources(), v->sources() + kNumModSources, sources);
    for (auto s : { ModSource::modWheel, ModSource::pitchBend, ModSource::aftertouch, ModSource::constant })
        sources[(int) s] = ctx.globalSources[(int) s];
    for (int m = 0; m < kNumMacros; ++m)
        sources[(int) macroSource (m)] = ctx.globalSources[(int) macroSource (m)];

    for (int d : ctx.matrix->globalDests)
        modOut[d] = 0.0f;
    for (const auto& r : ctx.matrix->globalRoutes)
        modOut[r.dest] += CompiledMatrix::evaluate (r, sources) * ctx.params->plain (pid::mod (r.slot));
}

} // namespace cureum
