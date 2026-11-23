#include "PresetManager.h"
#include "../Plugin/PluginProcessor.h"
#include "../Core/Settings.h"

namespace cureum
{

PresetManager::PresetManager (CureumProcessor& p) : processor (p)
{
    rescan();
}

void PresetManager::rescan()
{
    entries.clear();
    const auto& settings = Settings::get();

    for (int i = 0; i < getNumFactoryPresets(); ++i)
    {
        PresetEntry e;
        e.source = PresetEntry::Source::factory;
        e.factoryIndex = i;
        e.name = getFactoryName (i);
        e.folder = "Factory/" + getFactoryCategory (i);
        e.format = "CUREUM";
        e.isFavourite = settings.isFavourite ("factory:" + e.name);
        entries.push_back (e);
    }

    const auto userRoot = settings.getUserPresetFolder();
    for (const auto& it : juce::RangedDirectoryIterator (userRoot, true, "*.cureum;*.SerumPreset;*.fxp", juce::File::findFiles))
    {
        PresetEntry e;
        e.source = PresetEntry::Source::user;
        e.file = it.getFile();
        e.name = e.file.getFileNameWithoutExtension();
        const auto rel = e.file.getParentDirectory().getRelativePathFrom (userRoot).replaceCharacter ('\\', '/');
        e.folder = rel == "." ? juce::String ("User") : "User/" + rel;
        e.format = e.file.hasFileExtension ("cureum") ? "CUREUM" : (e.file.hasFileExtension ("fxp") ? "Serum 1" : "Serum 2");
        e.isFavourite = settings.isFavourite (e.file.getFullPathName());
        entries.push_back (e);
    }

    // Serum libraries (read-only, converted on load)
    for (auto& rootPath : settings.getSerumFolders())
    {
        const juce::File root (rootPath);
        if (! root.isDirectory()) continue;
        int count = 0;
        for (const auto& it : juce::RangedDirectoryIterator (root, true, "*.SerumPreset;*.fxp", juce::File::findFiles))
        {
            if (++count > 20000) break;
            PresetEntry e;
            e.source = PresetEntry::Source::serum;
            e.file = it.getFile();
            e.name = e.file.getFileNameWithoutExtension();
            const auto rel = e.file.getParentDirectory().getRelativePathFrom (root).replaceCharacter ('\\', '/');
            e.folder = root.getFileName() + (rel == "." ? juce::String() : "/" + rel);
            e.format = e.file.hasFileExtension ("fxp") ? "Serum 1" : "Serum 2";
            e.isFavourite = settings.isFavourite (e.file.getFullPathName());
            entries.push_back (e);
        }
    }
}

bool PresetManager::load (const PresetEntry& entry, juce::String& message)
{
    lastWarnings.clear();
    if (entry.source == PresetEntry::Source::factory)
    {
        processor.loadStateTree (createFactoryPreset (entry.factoryIndex), false);
        processor.updatePresetInfo ([&] (PresetInfo& info) { info.sourceFile = "factory:" + juce::String (entry.factoryIndex); });
        message = "Loaded " + entry.name;
        return true;
    }
    return loadFile (entry.file, message);
}

bool PresetManager::loadFile (const juce::File& file, juce::String& message)
{
    lastWarnings.clear();
    auto r = SerumImport::importFile (file);
    if (! r.ok())
    {
        message = r.error;
        return false;
    }
    processor.loadStateTree (r.state, r.sourceFormat == "CUREUM");
    processor.updatePresetInfo ([&] (PresetInfo& info) { info.sourceFile = file.getFullPathName(); });
    lastWarnings = r.warnings;
    message = "Loaded " + r.name + (r.sourceFormat != "CUREUM" ? " (converted from " + r.sourceFormat + ")" : juce::String());
    return true;
}

bool PresetManager::saveCurrent (const juce::String& name, const juce::String& subFolder, juce::String& message)
{
    const auto clean = juce::File::createLegalFileName (name.trim().isEmpty() ? juce::String ("Untitled") : name.trim());
    auto folder = Settings::get().getUserPresetFolder();
    if (subFolder.isNotEmpty())
        folder = folder.getChildFile (juce::File::createLegalFileName (subFolder));
    const auto file = folder.getChildFile (clean + StateFormat::kPresetExtension);
    processor.updatePresetInfo ([&] (PresetInfo& info) { info.name = name.trim().isEmpty() ? clean : name.trim(); });
    if (! StateFormat::writePresetFile (processor.createStateTree (true), file))
    {
        message = "Could not write " + file.getFullPathName();
        return false;
    }
    processor.updatePresetInfo ([&] (PresetInfo& info) { info.sourceFile = file.getFullPathName(); });
    message = "Saved " + file.getFileName();
    rescan();
    return true;
}

void PresetManager::initPreset()
{
    processor.resetToInit();
}

int PresetManager::indexOfCurrent() const
{
    const auto& src = processor.presetInfo.sourceFile;
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        if ((e.source == PresetEntry::Source::factory && src == "factory:" + juce::String (e.factoryIndex))
            || (e.file != juce::File() && e.file.getFullPathName() == src))
            return i;
    }
    return -1;
}

void PresetManager::step (int direction, juce::String& message)
{
    if (entries.empty()) return;
    int i = indexOfCurrent();
    i = i < 0 ? 0 : (i + direction + (int) entries.size()) % (int) entries.size();
    load (entries[(size_t) i], message);
}

int PresetManager::importBank (const juce::File& source, juce::String& message, std::function<bool (float)> progress)
{
    const auto bankName = juce::File::createLegalFileName (source.getFileNameWithoutExtension());
    const auto dest = Settings::get().getImportedPresetFolder().getChildFile (bankName);
    dest.createDirectory();
    auto presets = SerumImport::importBank (source, dest.getChildFile ("_resources"), std::move (progress));
    int written = 0;
    juce::StringArray used;
    for (auto& p : presets)
    {
        auto n = juce::File::createLegalFileName (p.name.isNotEmpty() ? p.name : "Preset");
        auto candidate = n;
        for (int k = 2; used.contains (candidate); ++k) candidate = n + " " + juce::String (k);
        used.add (candidate);
        if (StateFormat::writePresetFile (p.state, dest.getChildFile (candidate + StateFormat::kPresetExtension)))
            ++written;
    }
    message = written > 0 ? "Imported " + juce::String (written) + " presets into User/Imported/" + bankName
                          : "No Serum presets were found in " + source.getFileName();
    rescan();
    return written;
}

} // namespace cureum
