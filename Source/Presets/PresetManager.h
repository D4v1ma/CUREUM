#pragma once

#include "SerumImport.h"

namespace cureum
{

class CureumProcessor;

struct PresetEntry
{
    enum class Source { factory, user, serum };
    Source source = Source::user;
    juce::File file;
    int factoryIndex = -1;
    juce::String name, folder, format;
    bool isFavourite = false;
};

// Browses, loads, saves and imports presets. Lives on the message thread (owned by the editor).
class PresetManager
{
public:
    explicit PresetManager (CureumProcessor& p);

    void rescan();
    const std::vector<PresetEntry>& getEntries() const noexcept { return entries; }

    bool load (const PresetEntry& entry, juce::String& message);
    bool loadFile (const juce::File& file, juce::String& message);
    bool saveCurrent (const juce::String& name, const juce::String& subFolder, juce::String& message);
    void initPreset();
    void step (int direction, juce::String& message);   // previous / next in the current list
    int indexOfCurrent() const;

    // Converts a bank / archive / folder of Serum presets into CUREUM presets inside
    // <user presets>/Imported/<bank name>. Returns the number of presets converted.
    int importBank (const juce::File& source, juce::String& message, std::function<bool (float)> progress = {});

    juce::StringArray lastWarnings;

    // Factory presets are generated in code.
    static int getNumFactoryPresets();
    static juce::String getFactoryName (int index);
    static juce::String getFactoryCategory (int index);
    static juce::ValueTree createFactoryPreset (int index);

private:
    CureumProcessor& processor;
    std::vector<PresetEntry> entries;
};

} // namespace cureum
