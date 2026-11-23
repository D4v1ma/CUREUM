#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "../Core/Params.h"
#include "../Core/ModMatrix.h"
#include "../Core/AtomicResource.h"
#include "../DSP/SynthEngine.h"
#include "../DSP/FxRack.h"
#include "../DSP/WavetableFactory.h"
#include "../Host/PluginHost.h"

namespace cureum
{

struct PresetInfo
{
    juce::String name { "Init" }, author, description, category;
    juce::StringArray tags;
    juce::String sourceFile;    // the file this preset was loaded from (for next/prev browsing)
};

class CureumProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    CureumProcessor();
    ~CureumProcessor() override;

    //== AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "CUREUM"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;   // amp release + time-based FX
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presetInfo.name; }
    void changeProgramName (int, const juce::String& n) override { presetInfo.name = n; }
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //== State (message thread)
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    PresetInfo presetInfo;

    juce::ValueTree createStateTree (bool includePlugins = true) const;
    void loadStateTree (const juce::ValueTree& tree, bool restorePlugins = true);
    void resetToInit();

    // Parameters
    float getPlain (int index) const;
    void setPlain (int index, float plainValue);                 // notifies host
    juce::RangedAudioParameter* getParam (int index) const;

    // Wavetables
    WavetablePtr getWavetable (int osc) const;
    void setWavetable (int osc, WavetablePtr table);

    // LFO shapes
    const LfoShape& getLfoShape (int lfo) const;
    void setLfoShape (int lfo, const LfoShape& shape);

    // Modulation matrix
    const ModSlots& getModSlots() const noexcept { return modSlots; }
    void setModSlot (int index, const ModSlot& slot);
    void setModSlots (const ModSlots& slots);
    int addModulation (ModSource source, int destParam, float amount = 0.5f, bool bipolar = false);  // returns slot or -1
    void removeModSlot (int index);
    std::vector<int> slotsForDestination (int destParam) const;
    int findFreeModSlot() const;

    // FX rack
    FxOrder getFxOrder() const;
    void setFxOrder (const FxOrder& order);
    FxRack& getFxRack() noexcept { return fxRack; }
    void loadPluginIntoSlot (int slot, const juce::PluginDescription& desc, const juce::MemoryBlock* stateToRestore = nullptr);
    void clearPluginSlot (int slot);
    PluginHost& getPluginHost() { return *pluginHost; }
    void setConvolverIr (ConvIr type, const juce::File& customFile = {});

    // Noise
    bool loadCustomNoise (const juce::File& file, juce::String& error);
    juce::String getCustomNoiseName() const;

    // Macros
    juce::String getMacroName (int m) const { return macroNames[(size_t) juce::jlimit (0, kNumMacros - 1, m)]; }
    void setMacroName (int m, const juce::String& name);

    // Change counters polled by the UI (cheap, thread-safe)
    std::atomic<int> structureVersion { 0 };   // wavetables, matrix, fx order, shapes, macro names
    std::atomic<int> presetVersion { 0 };      // a different preset was loaded
    void bumpStructure() { ++structureVersion; }

    // A message for the user raised outside the editor (for example a hosted effect that could
    // not be loaded when a project was reopened). Message thread only; the editor shows each
    // notice once in its status bar.
    void postNotice (const juce::String& text) { noticeText = text; ++noticeVersion; }
    juce::String noticeText;
    int noticeVersion = 0, noticeSeenVersion = 0;

    //== UI feedback written by the audio thread
    struct Feedback
    {
        std::array<std::atomic<float>, (size_t) pid::total> modNorm {};   // modulated normalised value (newest voice)
        std::array<std::atomic<float>, (size_t) kNumLfos> lfoPhase {}, lfoValue {};
        std::array<std::atomic<float>, (size_t) kNumEnvelopes> envValue {};
        std::array<std::atomic<int>, (size_t) kNumEnvelopes> envStage {};
        std::atomic<float> peakL { 0.0f }, peakR { 0.0f }, compGr { 0.0f };
        std::atomic<int> activeVoices { 0 };
        std::atomic<float> cpuLoad { 0.0f };
        std::atomic<bool> anyVoice { false };
        static constexpr int scopeSize = 2048;
        std::array<float, scopeSize> scope {};
        std::atomic<int> scopePos { 0 };
    } feedback;

    const CompiledMatrix* getCompiledMatrix() const { return compiledMatrix.get(); }
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    double getCurrentBpm() const noexcept { return currentBpm.load(); }

private:
    juce::AudioProcessorEditor* createEditor() override;
    void timerCallback() override;
    void publishMatrix();
    void handleMidi (const juce::MidiMessage& m, VoiceContext& ctx);
    void renderRange (juce::AudioBuffer<float>& buffer, int start, int num, VoiceContext& ctx);
    void updateFeedback (const VoiceContext& ctx);
    void restorePluginSlots (const juce::ValueTree& tree);

    ParamCache params;
    SynthEngine engine;
    FxRack fxRack;
    juce::SharedResourcePointer<PluginHost> pluginHost;

    std::array<WavetablePtr, kNumOscillators> wavetables;
    std::array<AtomicResource<Wavetable>, kNumOscillators> wavetableSlots;
    std::array<LfoShape, kNumLfos> lfoShapes;
    std::array<AtomicResource<LfoShape>, kNumLfos> lfoShapeSlots;
    ModSlots modSlots;
    AtomicResource<CompiledMatrix> compiledMatrix;
    AtomicResource<FxOrder> fxOrder;
    AtomicResource<NoiseSample> customNoise;
    juce::File customNoiseFile;
    std::array<juce::String, (size_t) kNumMacros> macroNames;
    juce::CriticalSection stateLock;

    std::vector<float> globalMod;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    std::atomic<double> currentBpm { 120.0 };
    std::array<double, (size_t) kNumLfos> freeLfoPhase {};
    float pitchBendValue = 0.0f, pitchBendSmoothed = 0.0f;
    float modWheel = 0.0f, aftertouch = 0.0f;
    std::array<float, 17> mpeChannelBend {};
    bool noteStartedThisBlock = false;
    int lastConvIr = -1;
    int lastLatency = 0;
    juce::AudioBuffer<float> fxBuffer;
    Ramp masterGain;

    void initResources();

    JUCE_DECLARE_WEAK_REFERENCEABLE (CureumProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CureumProcessor)
};

} // namespace cureum
