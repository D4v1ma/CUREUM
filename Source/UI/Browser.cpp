#include "Browser.h"
#include "../Core/Settings.h"

namespace cureum
{

class PresetBrowser::PresetModel : public juce::ListBoxModel
{
public:
    explicit PresetModel (PresetBrowser& b) : owner (b) {}
    int getNumRows() override { return (int) owner.filtered.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) owner.filtered.size()) return;
        const auto& e = owner.presets.getEntries()[(size_t) owner.filtered[(size_t) row]];
        const bool current = owner.presets.indexOfCurrent() == owner.filtered[(size_t) row];
        if (selected) { g.setColour (theme::accentDeep.withAlpha (0.45f)); g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f); }
        else if (row % 2 == 1) { g.setColour (theme::panelRaised.withAlpha (0.5f)); g.fillRect (0, 0, w, h); }
        if (e.isFavourite) drawIcon (g, IconButton::Icon::star, { 8.0f, (float) h * 0.5f - 5.0f, 10.0f, 10.0f }, theme::gold);
        g.setFont (theme::value (13.5f));
        g.setColour (current ? theme::accent : theme::text);
        g.drawText (e.name, 26, 0, w / 2, h, juce::Justification::centredLeft);
        g.setFont (theme::value (11.5f));
        g.setColour (theme::textDim);
        g.drawText (e.folder, w / 2 + 30, 0, w / 2 - 120, h, juce::Justification::centredLeft);
        const auto badge = juce::Rectangle<float> ((float) w - 80.0f, (float) h * 0.5f - 8.0f, 70.0f, 16.0f);
        const auto c = e.format == "CUREUM" ? theme::accent : (e.format == "Serum 2" ? theme::mod : theme::gold);
        g.setColour (c.withAlpha (0.15f));
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (c);
        g.setFont (theme::label (9.5f));
        g.drawText (e.format.toUpperCase(), badge, juce::Justification::centred);
    }
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { owner.loadSelected(); }
    void returnKeyPressed (int) override { owner.loadSelected(); }
    void selectedRowsChanged (int) override { owner.repaint(); }
    PresetBrowser& owner;
};

class PresetBrowser::FolderModel : public juce::ListBoxModel
{
public:
    explicit FolderModel (PresetBrowser& b) : owner (b) {}
    int getNumRows() override { return owner.folders.size() + 1; }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        const auto name = row == 0 ? juce::String ("All presets") : owner.folders[row - 1];
        const int depth = row == 0 ? 0 : name.retainCharacters ("/").length();
        if (selected) { g.setColour (theme::accent.withAlpha (0.14f)); g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f); }
        g.setColour (selected ? theme::text : theme::textDim);
        g.setFont (depth == 0 ? theme::label (12.0f) : theme::value (12.5f));
        g.drawText (row == 0 ? name : name.fromLastOccurrenceOf ("/", false, false), 10 + depth * 12, 0, w - 12, h, juce::Justification::centredLeft);
    }
    void selectedRowsChanged (int row) override
    {
        owner.folderFilter = row <= 0 ? juce::String() : owner.folders[row - 1];
        owner.applyFilter();
    }
    PresetBrowser& owner;
};

PresetBrowser::PresetBrowser (EditorServices& s, PresetManager& pm) : services (s), presets (pm)
{
    presetModel = std::make_unique<PresetModel> (*this);
    folderModel = std::make_unique<FolderModel> (*this);
    presetList.setModel (presetModel.get());
    folderList.setModel (folderModel.get());
    presetList.setRowHeight (26);
    folderList.setRowHeight (24);
    for (auto* l : { &presetList, &folderList })
    {
        l->setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        l->getViewport()->setScrollBarThickness (8);
        addAndMakeVisible (*l);
    }
    search.setTextToShowWhenEmpty ("Search presets...", theme::textFaint);
    search.setFont (theme::value (14.0f));
    search.addListener (this);
    addAndMakeVisible (search);
    addAndMakeVisible (favouritesOnly);
    favouritesOnly.onClick = [this] { applyFilter(); };
    for (auto* b : { &loadButton, &favButton, &revealButton, &importFileButton, &importBankButton, &importFolderButton, &rescanButton })
        addAndMakeVisible (b);
    addAndMakeVisible (close);
    close.onClick = [this] { if (onClose) onClose(); };
    loadButton.onClick = [this] { loadSelected(); };
    favButton.onClick = [this]
    {
        if (auto* e = selectedEntry())
        {
            Settings::get().toggleFavourite (e->source == PresetEntry::Source::factory ? "factory:" + e->name : e->file.getFullPathName());
            const int row = presetList.getSelectedRow();
            presets.rescan();
            applyFilter();
            presetList.selectRow (row);
        }
    };
    revealButton.onClick = [this] { if (auto* e = selectedEntry()) if (e->file.existsAsFile()) e->file.revealToUser(); };
    importFileButton.onClick = [this] { importFile(); };
    importBankButton.onClick = [this] { importBank (false); };
    importFolderButton.onClick = [this] { importBank (true); };
    rescanButton.onClick = [this] { presets.rescan(); applyFilter(); };
    setWantsKeyboardFocus (true);
}

PresetBrowser::~PresetBrowser()
{
    presetList.setModel (nullptr);
    folderList.setModel (nullptr);
}

void PresetBrowser::open()
{
    presets.rescan();
    folders.clear();
    for (auto& e : presets.getEntries())
    {
        // add the folder and all its parents
        auto parts = juce::StringArray::fromTokens (e.folder, "/", "");
        juce::String path;
        for (auto& p : parts)
        {
            path = path.isEmpty() ? p : path + "/" + p;
            folders.addIfNotAlreadyThere (path);
        }
    }
    folders.sortNatural();
    folderList.updateContent();
    applyFilter();
    const int cur = presets.indexOfCurrent();
    for (int i = 0; i < (int) filtered.size(); ++i)
        if (filtered[(size_t) i] == cur) { presetList.selectRow (i); presetList.scrollToEnsureRowIsOnscreen (i); }
    search.grabKeyboardFocus();
}

void PresetBrowser::applyFilter()
{
    filtered.clear();
    const auto words = juce::StringArray::fromTokens (search.getText().toLowerCase(), " ", "");
    const auto& all = presets.getEntries();
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& e = all[(size_t) i];
        if (favouritesOnly.getToggleState() && ! e.isFavourite) continue;
        if (folderFilter.isNotEmpty() && ! (e.folder == folderFilter || e.folder.startsWith (folderFilter + "/"))) continue;
        bool match = true;
        const auto hay = (e.name + " " + e.folder + " " + e.format).toLowerCase();
        for (auto& w : words) if (! hay.contains (w)) { match = false; break; }
        if (match) filtered.push_back (i);
    }
    presetList.updateContent();
    repaint();
}

void PresetBrowser::textEditorTextChanged (juce::TextEditor&) { applyFilter(); }

const PresetEntry* PresetBrowser::selectedEntry() const
{
    const int row = presetList.getSelectedRow();
    if (row < 0 || row >= (int) filtered.size()) return nullptr;
    return &presets.getEntries()[(size_t) filtered[(size_t) row]];
}

void PresetBrowser::loadSelected()
{
    if (auto* e = selectedEntry())
    {
        juce::String msg;
        presets.load (*e, msg);
        if (! presets.lastWarnings.isEmpty())
            msg << "  -  " << presets.lastWarnings.size() << " import note(s): " << presets.lastWarnings[0];
        services.showStatus (msg, true);
        presetList.repaint();
    }
}

void PresetBrowser::importFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                   "*.cureum;*.SerumPreset;*.fxp;*.fxb");
    juce::Component::SafePointer<PresetBrowser> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        juce::String msg;
        safe->presets.loadFile (fc.getResult(), msg);
        if (! safe->presets.lastWarnings.isEmpty()) msg << "  -  " << safe->presets.lastWarnings.size() << " import note(s): " << safe->presets.lastWarnings[0];
        safe->services.showStatus (msg, true);
    });
}

void PresetBrowser::importBank (bool folder)
{
    chooser = std::make_unique<juce::FileChooser> (folder ? "Import a folder of presets" : "Import a bank or preset pack",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                   folder ? juce::String() : juce::String ("*.fxb;*.zip;*.SerumPack"));
    juce::Component::SafePointer<PresetBrowser> safe (this);
    const int flags = juce::FileBrowserComponent::openMode | (folder ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles);
    chooser->launchAsync (flags, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        safe->services.showStatus ("Importing " + fc.getResult().getFileName() + "...", true);
        juce::String msg;
        safe->presets.importBank (fc.getResult(), msg);
        safe->services.showStatus (msg, true);
        safe->open();
    });
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (theme::background.withAlpha (0.97f));
    auto r = getLocalBounds().reduced (12);
    theme::drawPanel (g, r.toFloat());
    drawSectionTitle (g, "PRESET BROWSER", r.reduced (14, 0).removeFromTop (40), theme::text);
    auto info = r.reduced (12).removeFromRight (250).withTrimmedTop (48);
    theme::drawPanel (g, info.toFloat(), true);
    auto ir = info.reduced (12);
    if (auto* e = selectedEntry())
    {
        g.setColour (theme::text);
        g.setFont (theme::title (17.0f));
        g.drawFittedText (e->name, ir.removeFromTop (44), juce::Justification::topLeft, 2);
        g.setFont (theme::value (12.5f));
        g.setColour (theme::textDim);
        g.drawText ("Folder: " + e->folder, ir.removeFromTop (20), juce::Justification::centredLeft);
        g.drawText ("Format: " + e->format, ir.removeFromTop (20), juce::Justification::centredLeft);
        if (e->format != "CUREUM")
        {
            ir.removeFromTop (8);
            g.setColour (theme::textFaint);
            g.drawFittedText ("Serum presets are converted when loaded. Add your Serum folders on the GLOBAL page so the original wavetables are found.",
                              ir.removeFromTop (70), juce::Justification::topLeft, 5);
        }
    }
    else
    {
        g.setColour (theme::textFaint);
        g.setFont (theme::value (13.0f));
        g.drawFittedText ("Select a preset to see its details. Double-click or press Enter to load it.", ir.removeFromTop (60), juce::Justification::topLeft, 3);
    }
    g.setColour (theme::textFaint);
    g.setFont (theme::label (10.5f));
    g.drawText (juce::String (filtered.size()) + " presets", r.reduced (14, 8).removeFromBottom (16), juce::Justification::centredLeft);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (12).reduced (12);
    auto top = r.removeFromTop (36);
    close.setBounds (top.removeFromRight (28).withSizeKeepingCentre (26, 26));
    top.removeFromRight (8);
    favouritesOnly.setBounds (top.removeFromRight (140));
    top.removeFromRight (8);
    top.removeFromLeft (160);
    search.setBounds (top.removeFromLeft (std::min (360, top.getWidth())).reduced (0, 4));
    r.removeFromTop (12);
    r.removeFromBottom (20);
    auto info = r.removeFromRight (250).reduced (12);
    auto buttons = info.removeFromBottom (7 * 32);
    for (auto* b : { &loadButton, &favButton, &revealButton, &importFileButton, &importBankButton, &importFolderButton, &rescanButton })
    {
        b->setBounds (buttons.removeFromTop (28));
        buttons.removeFromTop (4);
    }
    r.removeFromRight (12);
    folderList.setBounds (r.removeFromLeft (220));
    r.removeFromLeft (12);
    presetList.setBounds (r);
}

bool PresetBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    if (k == juce::KeyPress::returnKey) { loadSelected(); return true; }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey)
    {
        const int row = juce::jlimit (0, std::max (0, (int) filtered.size() - 1), presetList.getSelectedRow() + (k == juce::KeyPress::downKey ? 1 : -1));
        presetList.selectRow (row);
        return true;
    }
    return false;
}

} // namespace cureum
