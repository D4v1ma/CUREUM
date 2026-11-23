#pragma once

#include "../DSP/Effects.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace cureum
{

// An effect slot that hosts an external VST3 (or AU on macOS) plugin inside CUREUM's FX rack.
// The plugin instance is created on the message thread and handed to the audio thread under a
// spin lock that the audio thread only ever try-locks (it never waits).
class PluginSlot : public FxUnit
{
public:
    explicit PluginSlot (int slotIndex) : index (slotIndex) {}
    ~PluginSlot() override;

    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* l, float* r, int n, const FxContext& ctx) override;
    int getLatencySamples() const override;

    // ---- message thread
    void setInstance (std::unique_ptr<juce::AudioPluginInstance> newInstance);
    void clear();
    bool isLoaded() const;
    juce::AudioPluginInstance* getInstance() const noexcept { return instance.get(); }   // message thread
    juce::String getPluginName() const;
    juce::PluginDescription getDescription() const;

    juce::ValueTree saveState() const;                          // description + plugin state
    static bool readState (const juce::ValueTree& tree, juce::PluginDescription& desc, juce::MemoryBlock& state);

    std::function<void()> onChange;   // called on the message thread after load/unload

private:
    void prepareInstance (juce::AudioPluginInstance& p);
    std::shared_ptr<juce::AudioPluginInstance> getShared() const;

    const int index;
    // Guards the instance pointer only. The audio thread try-locks it, so nothing slow may run
    // while it is held: callers copy the shared_ptr and talk to the plugin outside the lock. The
    // shared ownership keeps an instance alive while a host saves state from a worker thread.
    mutable juce::SpinLock lock;
    std::shared_ptr<juce::AudioPluginInstance> instance;
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;
    double sr = 44100.0;
    int maxBlock = 512;
    Ramp mix;
};

} // namespace cureum
