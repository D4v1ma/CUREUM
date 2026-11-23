#pragma once

#include "Voice.h"
#include "Oversampling.h"
#include <array>

namespace cureum
{

class SynthEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void setOversampling (int factor);
    int getOversampling() const noexcept { return oversample; }
    double getBaseRate() const noexcept   { return baseRate; }

    // MIDI handling (audio thread)
    void noteOn (int note, float velocity, int channel, const VoiceContext& ctx);
    void noteOff (int note, float velocity, int channel, const VoiceContext& ctx);
    void setSustain (bool down);
    void allNotesOff();
    void allSoundOff();
    void polyAftertouch (int note, int channel, float value);
    void mpeBend (int channel, float semis);
    void mpePressure (int channel, float value);
    void mpeSlide (int channel, float value);

    // Renders numSamples at the base sample rate and ADDS the result into outL/outR.
    void render (float* outL, float* outR, int numSamples, VoiceContext& ctx);

    int numActiveVoices() const noexcept;
    const Voice* newestVoice() const noexcept;
    const std::array<Voice, kMaxVoices>& getVoices() const noexcept { return voices; }

    // Modulation of global (non-voice) destinations, e.g. effect parameters. Per-voice sources are
    // taken from the most recently started voice, like Serum does.
    void computeGlobalModulation (const VoiceContext& ctx, float* modOut) const noexcept;

private:
    Voice* findVoiceFor (int maxPoly);
    void retriggerMono (int note, float velocity, int channel, const VoiceContext& ctx, bool glideFromCurrent);

    std::array<Voice, kMaxVoices> voices;
    std::vector<float> osL, osR;
    Decimator decimator;
    double baseRate = 44100.0;
    int oversample = 1;
    int maxBlock = 512;
    juce::uint64 ageCounter = 0;
    uint32_t seedCounter = 1;
    float lastPitch = -1.0f;
    bool sustainDown = false;

    std::array<int, 128> monoStack {};
    int monoCount = 0;
    float monoVelocity = 1.0f;
};

} // namespace cureum
