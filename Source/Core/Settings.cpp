#include "Settings.h"

namespace cureum
{

Settings& Settings::get()
{
    static Settings instance;
    return instance;
}

Settings::Settings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "CUREUM";
    o.filenameSuffix = ".settings";
    o.folderName = "CUREUM";
    o.osxLibrarySubFolder = "Application Support";
    o.commonToAllUsers = false;
    o.ignoreCaseOfKeyNames = true;
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    file = std::make_unique<juce::PropertiesFile> (o);

    if (! file->containsKey ("serumFolders"))
        setSerumFolders (detectSerumFolders());
}

juce::File Settings::getUserFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("CUREUM");
    dir.createDirectory();
    return dir;
}

juce::File Settings::getUserPresetFolder() const
{
    const auto custom = file->getValue ("presetFolder");
    auto dir = custom.isNotEmpty() ? juce::File (custom) : getUserFolder().getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::File Settings::getUserWavetableFolder() const
{
    auto dir = getUserFolder().getChildFile ("Wavetables");
    dir.createDirectory();
    return dir;
}

juce::File Settings::getImportedPresetFolder() const
{
    auto dir = getUserPresetFolder().getChildFile ("Imported");
    dir.createDirectory();
    return dir;
}

juce::StringArray Settings::detectSerumFolders()
{
    juce::StringArray found;
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const juce::File candidates[] = {
        docs.getChildFile ("Xfer/Serum Presets"),
        docs.getChildFile ("Xfer/Serum 2 Presets"),
        home.getChildFile ("Library/Audio/Presets/Xfer Records/Serum Presets"),
        home.getChildFile ("Library/Audio/Presets/Xfer Records/Serum 2 Presets"),
        juce::File ("/Library/Audio/Presets/Xfer Records/Serum Presets"),
        juce::File ("/Library/Audio/Presets/Xfer Records/Serum 2 Presets"),
        juce::File ("C:\\Users\\Public\\Documents\\Xfer\\Serum Presets"),
    };
    for (auto& c : candidates)
        if (c.isDirectory())
            found.addIfNotAlreadyThere (c.getFullPathName());
    return found;
}

juce::StringArray Settings::getSerumFolders() const
{
    return juce::StringArray::fromLines (file->getValue ("serumFolders"));
}

void Settings::setSerumFolders (const juce::StringArray& folders)
{
    juce::StringArray clean;
    for (auto& f : folders)
        if (f.trim().isNotEmpty())
            clean.addIfNotAlreadyThere (f.trim());
    file->setValue ("serumFolders", clean.joinIntoString ("\n"));
    file->saveIfNeeded();
}

juce::StringArray Settings::getFavourites() const
{
    return juce::StringArray::fromLines (file->getValue ("favourites"));
}

bool Settings::isFavourite (const juce::String& path) const
{
    return getFavourites().contains (path);
}

void Settings::toggleFavourite (const juce::String& path)
{
    auto f = getFavourites();
    if (f.contains (path)) f.removeString (path);
    else f.add (path);
    f.removeEmptyStrings();
    file->setValue ("favourites", f.joinIntoString ("\n"));
    file->saveIfNeeded();
}

float Settings::getUiScale() const     { return (float) file->getDoubleValue ("uiScale", 1.0); }
void Settings::setUiScale (float s)    { file->setValue ("uiScale", (double) s); file->saveIfNeeded(); }

} // namespace cureum
