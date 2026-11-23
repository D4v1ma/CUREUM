#pragma once

#include "Oscillators.h"
#include "Filter.h"
#include "Envelope.h"
#include "Lfo.h"
#include "ParamCache.h"
#include "../Core/ModMatrix.h"

namespace cureum
{

// Everything a voice needs to read while rendering one block. Owned by the engine.
struct VoiceContext
{
    const ParamCache* params = nullptr;
    const CompiledMatrix* matrix = nullptr;
    const Wavetable* tables[kNumOscillators] {};
    const LfoShape* lfoShapes[kNumLfos] {};
    const NoiseSample* customNoise = nullptr;
    float globalSources[kNumModSources] {};
    double freeLfoPhase[kNumLfos] {};            // phase at the start of the current render call
    double freeLfoCyclesPerSample[kNumLfos] {};  // at the render (oversampled) rate
    double bpm = 120.0;
    double renderRate = 44100.0;
    int oversample = 1;
    float pitchBendSemis = 0.0f;
};

class Voice
{
public:
    static constexpr int kControlBlock = 32;   // control-rate interval in base-rate samples

    struct StartInfo
    {
        int note = 60;
        float velocity = 1.0f;
        int channel = 1;
        float glideFrom = -1.0f;   // pitch to glide from, < 0 = no glide
        bool retrigger = true;
        uint32_t seed = 1;
        juce::uint64 age = 0;
    };

    void prepare (double renderRate, double maxRenderRate, int voiceIndex);
    void setRenderRate (double renderRate);

    void start (const StartInfo& info, const VoiceContext& ctx);
    void startOrSteal (const StartInfo& info, const VoiceContext& ctx);   // fades out first if sounding
    void glideTo (int note, float velocity, juce::uint64 age, const VoiceContext& ctx);  // legato
    void release (float releaseVelocity);
    void kill();

    void render (float* outL, float* outR, int numSamples, const VoiceContext& ctx, int offsetInCall);

    bool isActive() const noexcept     { return active; }
    bool isReleased() const noexcept   { return ! keyDown; }
    bool isStealing() const noexcept   { return stealing; }
    int getNote() const noexcept       { return pending.valid ? pending.info.note : note; }
    int getChannel() const noexcept    { return channel; }
    juce::uint64 getAge() const noexcept { return age; }
    float getPitch() const noexcept    { return pitch; }
    float getAmplitude() const noexcept { return env[0].current(); }

    void setPolyAftertouch (float v) noexcept { polyAT = v; }
    void setMpeBend (float semis) noexcept    { mpeBend = semis; }
    void setMpePressure (float v) noexcept    { mpePressure = v; }
    void setMpeSlide (float v) noexcept       { mpeSlide = v; }

    const float* sources() const noexcept     { return src; }
    float modOffset (int paramIndex) const noexcept { return mod[paramIndex]; }
    const Envelope& getEnvelope (int e) const noexcept { return env[e]; }
    float lfoPhase (int l) const noexcept     { return lfo[l].currentPhase(); }

    bool keyDown = false;
    bool sustained = false;

private:
    void updateControl (int numSamples, const VoiceContext& ctx, int offsetInCall);
    void renderAudio (float* outL, float* outR, int numSamples);
    float val (const VoiceContext& ctx, int paramIndex) const noexcept
    {
        return ctx.matrix != nullptr && ctx.matrix->modulated[(size_t) paramIndex] ? ctx.params->modulated (paramIndex, mod[paramIndex])
                                                                                    : ctx.params->plain (paramIndex);
    }

    double sr = 44100.0, maxSr = 44100.0;
    int index = 0;
    bool active = false, stealing = false;
    int note = 60, channel = 1;
    float velocity = 1.0f, releaseVel = 0.0f, polyAT = 0.0f, mpeBend = 0.0f, mpePressure = 0.0f, mpeSlide = 0.0f;
    float random1 = 0.0f, random2 = 0.0f;
    juce::uint64 age = 0;

    // glide
    float pitch = 60.0f, glideStart = 60.0f, glideTarget = 60.0f, glideT = 1.0f, glideSeconds = 0.0f, glideCurve = 0.0f;

    struct Pending { bool valid = false; StartInfo info; } pending;
    float fade = 1.0f, fadeStep = 0.0f;

    WavetableOscillator osc[kNumOscillators];
    SubOscillator sub;
    NoiseOscillator noise;
    MultiFilter filter[kNumFilters];
    Envelope env[kNumEnvelopes];
    LfoGen lfo[kNumLfos];

    // per control block
    bool oscRun[kNumOscillators] {}, subRun = false, noiseRun = false, filterOn[kNumFilters] {};
    int route[5] {};
    bool parallel = false;
    float filterMix[kNumFilters] {}, filterLevel[kNumFilters] {};
    float gain[5] {}, gainInc[5] {};        // osc A, B, C, sub, noise output gains (ramped)
    float velGain = 1.0f;

    float src[kNumModSources] {};
    float mod[pid::total] {};
};

} // namespace cureum
