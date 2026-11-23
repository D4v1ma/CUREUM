#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace cureum
{

// Process-wide manager for third-party effect plugins (VST3 everywhere, AU on macOS): the format
// manager, the list of known plugins (persisted to disk) and an incremental, UI-friendly scanner.
// Use through juce::SharedResourcePointer<PluginHost>.
class PluginHost : private juce::Timer
{
public:
    PluginHost();
    ~PluginHost() override;

    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }
    juce::KnownPluginList& getKnownPlugins() noexcept           { return knownPlugins; }

    // Effects only (instruments are hidden from the FX rack menus).
    juce::Array<juce::PluginDescription> getEffectPlugins() const;

    // Scans the default plugin folders (plus extra folders) one file per timer tick.
    void startScan (const juce::StringArray& extraFolders = {});
    void cancelScan();
    bool isScanning() const noexcept { return scanner != nullptr; }
    float getScanProgress() const noexcept { return scanProgress; }
    juce::String getCurrentlyScanning() const { return currentName; }
    std::function<void()> onScanUpdate;

    // Adds a single plugin file/bundle the user picked. Returns the number of plugin types found.
    int addPluginFile (const juce::File& file, juce::String& error);

    void createInstance (const juce::PluginDescription& desc, double sampleRate, int blockSize,
                         std::function<void (std::unique_ptr<juce::AudioPluginInstance>, const juce::String&)> callback);

    static juce::File getDataFolder();
    juce::FileSearchPath getDefaultSearchPath() const;

private:
    void timerCallback() override;
    void save();
    void load();

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    juce::Array<juce::AudioPluginFormat*> pendingFormats;
    juce::StringArray extraScanFolders;
    float scanProgress = 0.0f;
    juce::String currentName;
};

} // namespace cureum
