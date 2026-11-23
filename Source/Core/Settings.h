#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace cureum
{

// Application-wide preferences shared by every CUREUM instance (stored in the user's app-data
// folder, e.g. ~/Library/Application Support/CUREUM/CUREUM.settings on macOS).
class Settings
{
public:
    static Settings& get();

    juce::PropertiesFile& props() { return *file; }

    juce::File getUserFolder() const;                    // ~/Documents/CUREUM
    juce::File getUserPresetFolder() const;
    juce::File getUserWavetableFolder() const;
    juce::File getImportedPresetFolder() const;

    // Folders that contain Serum presets / tables / noises (used to browse Serum libraries and to
    // resolve wavetables referenced by Serum presets).
    juce::StringArray getSerumFolders() const;
    void setSerumFolders (const juce::StringArray& folders);
    static juce::StringArray detectSerumFolders();

    juce::StringArray getFavourites() const;
    bool isFavourite (const juce::String& path) const;
    void toggleFavourite (const juce::String& path);

    float getUiScale() const;
    void setUiScale (float scale);

    bool getBool (const juce::String& key, bool def) const { return file->getBoolValue (key, def); }
    void setBool (const juce::String& key, bool v)         { file->setValue (key, v); file->saveIfNeeded(); }
    juce::String getString (const juce::String& key, const juce::String& def = {}) const { return file->getValue (key, def); }
    void setString (const juce::String& key, const juce::String& v) { file->setValue (key, v); file->saveIfNeeded(); }

private:
    Settings();
    std::unique_ptr<juce::PropertiesFile> file;
};

} // namespace cureum
