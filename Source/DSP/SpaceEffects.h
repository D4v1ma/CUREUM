#pragma once

#include "Effects.h"

namespace cureum
{

class Reverb : public FxUnit
{
public:
    void prepare (double sr, int maxBlock) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;

private:
    void processHall (float inL, float inR, float& outL, float& outR) noexcept;
    void processPlate (float inL, float inR, float& outL, float& outR) noexcept;

    struct Allpass
    {
        FxDelayLine line; float delay = 100.0f, g = 0.6f;
        inline float process (float x) noexcept { const float d = line.read (delay); const float w = x + g * d; line.push (w); return d - g * w; }
    };

    double sr = 44100.0;
    FxDelayLine pre;
    // hall
    FxDelayLine hall[8];
    float hallLen[8] {}, hallGain[8] {}, hallLp[8] {};
    Allpass hallDiff[4];
    double modPhase = 0.0;
    // plate
    Allpass plateIn[4];
    Allpass tankAp[2], tankAp2[2];
    FxDelayLine tankDelay[2], tankDelay2[2];
    float tankLen[2] {}, tankLen2[2] {}, tankLp[2] {}, tankFb[2] {};
    float bandLp = 0.0f;
    // shared
    float damp = 0.3f, decayGain = 0.8f, lowCutA = 0.0f, highCutA = 0.0f;
    float lc[2] {}, hc[2] {};
    float preDelay = 0.0f, width = 1.0f, sizeScale = 1.0f;
    ReverbMode mode = ReverbMode::hall;
    Ramp mix;
};

class Convolver : public FxUnit
{
public:
    Convolver();
    void prepare (double sr, int maxBlock) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;

    // Message thread: load a built-in IR or a user file.
    void loadBuiltIn (ConvIr type);
    bool loadFile (const juce::File& file);
    const juce::File& getCustomFile() const noexcept { return customFile; }
    ConvIr getLoadedType() const noexcept { return loaded; }

    static juce::AudioBuffer<float> generateIr (ConvIr type, double sampleRate);

private:
    double sr = 44100.0;
    int maxBlock = 512;
    juce::dsp::Convolution convolution;
    juce::AudioBuffer<float> dry, wet;
    FxDelayLine preL, preR;
    float lcS[2] {}, hcS[2] {};
    ConvIr loaded = ConvIr::count;
    juce::File customFile;
    bool prepared = false;
    Ramp mix;
};

} // namespace cureum
