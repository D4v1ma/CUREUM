#include "PluginHost.h"

namespace cureum
{

PluginHost::PluginHost()
{
    juce::addDefaultFormatsToManager (formatManager);
    load();
}

PluginHost::~PluginHost()
{
    stopTimer();
    scanner.reset();
}

juce::File PluginHost::getDataFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("CUREUM");
    dir.createDirectory();
    return dir;
}

void PluginHost::load()
{
    const auto file = getDataFolder().getChildFile ("KnownPlugins.xml");
    if (auto xml = juce::parseXML (file))
        knownPlugins.recreateFromXml (*xml);
}

void PluginHost::save()
{
    if (auto xml = knownPlugins.createXml())
        xml->writeTo (getDataFolder().getChildFile ("KnownPlugins.xml"));
}

juce::Array<juce::PluginDescription> PluginHost::getEffectPlugins() const
{
    juce::Array<juce::PluginDescription> result;
    for (auto& d : knownPlugins.getTypes())
        if (! d.isInstrument)
            result.add (d);
    std::sort (result.begin(), result.end(), [] (const auto& a, const auto& b)
               { return a.manufacturerName + a.name < b.manufacturerName + b.name; });
    return result;
}

juce::FileSearchPath PluginHost::getDefaultSearchPath() const
{
    juce::FileSearchPath path;
    for (auto* f : formatManager.getFormats())
    {
        const auto p = f->getDefaultLocationsToSearch();
        for (int i = 0; i < p.getNumPaths(); ++i)
            path.addIfNotAlreadyThere (p[i]);
    }
    return path;
}

void PluginHost::startScan (const juce::StringArray& extraFolders)
{
    cancelScan();
    extraScanFolders = extraFolders;
    pendingFormats.clear();
    for (auto* f : formatManager.getFormats())
        pendingFormats.add (f);
    scanProgress = 0.0f;
    startTimer (5);
}

void PluginHost::cancelScan()
{
    stopTimer();
    scanner.reset();
    pendingFormats.clear();
    currentName.clear();
}

void PluginHost::timerCallback()
{
    if (scanner == nullptr)
    {
        if (pendingFormats.isEmpty())
        {
            stopTimer();
            currentName.clear();
            scanProgress = 1.0f;
            save();
            if (onScanUpdate) onScanUpdate();
            return;
        }
        auto* format = pendingFormats.removeAndReturn (0);
        auto path = format->getDefaultLocationsToSearch();
        for (auto& f : extraScanFolders)
            path.addIfNotAlreadyThere (juce::File (f));
        scanner = std::make_unique<juce::PluginDirectoryScanner> (knownPlugins, *format, path, true,
                                                                  getDataFolder().getChildFile ("ScanCrash.txt"), false);
    }

    juce::String name;
    const bool more = scanner->scanNextFile (true, name);
    currentName = name;
    scanProgress = scanner->getProgress();
    if (! more)
    {
        scanner.reset();
        save();
    }
    if (onScanUpdate) onScanUpdate();
}

int PluginHost::addPluginFile (const juce::File& file, juce::String& error)
{
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* f : formatManager.getFormats())
        if (f->fileMightContainThisPluginType (file.getFullPathName()))
            knownPlugins.scanAndAddFile (file.getFullPathName(), false, found, *f);
    if (found.isEmpty())
        error = "No compatible VST3/AU plugin found in " + file.getFileName();
    save();
    return found.size();
}

void PluginHost::createInstance (const juce::PluginDescription& desc, double sampleRate, int blockSize,
                                 std::function<void (std::unique_ptr<juce::AudioPluginInstance>, const juce::String&)> callback)
{
    formatManager.createPluginInstanceAsync (desc, sampleRate > 0 ? sampleRate : 44100.0, blockSize > 0 ? blockSize : 512,
        [cb = std::move (callback)] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& err)
        {
            if (cb) cb (std::move (instance), err);
        });
}

} // namespace cureum
