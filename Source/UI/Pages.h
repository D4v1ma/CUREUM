#pragma once

#include "SynthPanels.h"

namespace cureum
{

// Horizontal bipolar bar bound to a parameter (used for matrix amounts).
class ParamBar : public ParamWidget
{
public:
    ParamBar (EditorServices& s, int paramIndex) : ParamWidget (s, paramIndex) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    float startNorm = 0.0f;
};

//==============================================================================
class FxPage : public juce::Component, public Refreshable
{
public:
    explicit FxPage (EditorServices& s);
    ~FxPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    void rebuild();
    void showAddMenu();
    void showPluginMenu (int slot);

private:
    class RackList;
    class UnitPanel;
    EditorServices& services;
    CureumProcessor& proc;
    ScopeView scope;
    std::unique_ptr<RackList> rack;
    juce::TextButton addButton { "+  ADD EFFECT" };
    juce::Viewport viewport;
    juce::Component stack;
    std::vector<std::unique_ptr<UnitPanel>> units;
    juce::String lastOrder;
    int lastStructure = -1;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<juce::DocumentWindow>> pluginWindows;
    friend class UnitPanel;
    void openPluginEditor (int slot);
};

//==============================================================================
class MatrixPage : public juce::Component, public Refreshable
{
public:
    explicit MatrixPage (EditorServices& s);
    ~MatrixPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
private:
    class Row;
    EditorServices& services;
    CureumProcessor& proc;
    juce::Viewport viewport;
    juce::Component rows;
    std::vector<std::unique_ptr<Row>> rowComps;
    juce::ToggleButton activeOnly { "Show used slots only" };
    juce::TextButton clearAll { "Clear all" };
    int lastStructure = -1;
    void layoutRows();
};

//==============================================================================
class GlobalPage : public juce::Component, public Refreshable
{
public:
    explicit GlobalPage (EditorServices& s);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    std::function<void (float)> onZoom;
private:
    void refreshFolders();
    EditorServices& services;
    CureumProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> widgets;
    std::vector<juce::Component*> tuneRow;
    juce::Label presetFolderLabel, serumFoldersLabel, pluginStatus, aboutText;
    juce::TextButton changePresetFolder { "Change..." }, openPresetFolder { "Open" };
    juce::TextButton addSerumFolder { "Add folder..." }, removeSerumFolder { "Remove last" }, detectSerum { "Auto-detect" };
    juce::TextButton scanPlugins { "Scan for plugins" }, addPluginFile { "Add plugin file..." };
    juce::ComboBox zoom;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace cureum
