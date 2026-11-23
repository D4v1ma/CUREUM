#pragma once

#include "PluginProcessor.h"
#include "../UI/Pages.h"
#include "../UI/Browser.h"
#include "../UI/HelpView.h"
#include "../UI/WavetableEditor.h"

namespace cureum
{

class CureumEditor : public juce::AudioProcessorEditor,
                     public EditorServices,
                     public juce::DragAndDropContainer,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    explicit CureumEditor (CureumProcessor&);
    ~CureumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // EditorServices
    CureumProcessor& getProcessor() override { return proc; }
    void showStatus (const juce::String& text, bool isMessage = false) override;
    void openHelp (const juce::String& topicId, const juce::String& focusText = {}) override;
    void showMatrix() override { selectPage (2); }
    juce::DragAndDropContainer& getDragContainer() override { return *this; }
    bool isHelpModeActive() const override { return helpMode; }

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    static constexpr int kBaseWidth = 1180, kBaseHeight = 780;

private:
    class MainView;
    class Header;
    class OscPage;
    class HelpModeOverlay;

    void timerCallback() override;
    void selectPage (int index);
    void setScale (float s);
    void toggleHelpMode();
    void closeOverlays();
    void showBrowser();
    void showWavetableEditor (int osc);
    void savePresetDialog();
    void showMainMenu();
    void loadFile (const juce::File& f);

    CureumProcessor& proc;
    CureumLookAndFeel lnf;
    PresetManager presets;
    std::unique_ptr<MainView> view;
    float scale = 1.0f;
    bool helpMode = false;
    int lastPresetVersion = -1;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { nullptr, 900 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CureumEditor)
};

} // namespace cureum
