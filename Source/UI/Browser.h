#pragma once

#include "Widgets.h"
#include "../Presets/PresetManager.h"

namespace cureum
{

class PresetBrowser : public juce::Component, private juce::TextEditor::Listener
{
public:
    PresetBrowser (EditorServices& s, PresetManager& pm);
    ~PresetBrowser() override;
    void open();
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void()> onClose;
    void importFile();
    void importBank (bool folder);

private:
    class PresetModel;
    class FolderModel;
    void textEditorTextChanged (juce::TextEditor&) override;
    void applyFilter();
    void loadSelected();
    const PresetEntry* selectedEntry() const;

    EditorServices& services;
    PresetManager& presets;
    std::unique_ptr<PresetModel> presetModel;
    std::unique_ptr<FolderModel> folderModel;
    juce::ListBox presetList, folderList;
    juce::TextEditor search;
    juce::ToggleButton favouritesOnly { "Favourites only" };
    juce::TextButton loadButton { "Load" }, favButton { "Favourite" }, revealButton { "Show file" },
                     importFileButton { "Open preset file..." }, importBankButton { "Import bank / pack..." },
                     importFolderButton { "Import folder..." }, rescanButton { "Rescan" };
    IconButton close { IconButton::Icon::close, "Close browser", "Closes the preset browser (Esc)." };
    std::vector<int> filtered;
    juce::StringArray folders;
    juce::String folderFilter;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace cureum
