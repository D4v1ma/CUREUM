#pragma once

#include "Displays.h"

namespace cureum
{

// Rounded section with a title strip and an optional power toggle.
class SectionPanel : public juce::Component
{
public:
    SectionPanel (EditorServices& s, const juce::String& title, int enableParam = -1);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> getContentArea() const;
    juce::Rectangle<int> getHeaderArea() const;
    virtual void layoutContent (juce::Rectangle<int>) {}
protected:
    EditorServices& services;
    CureumProcessor& proc;
    juce::String title;
    std::unique_ptr<ParamToggle> power;
    juce::Colour titleColour = theme::text;
};

juce::PopupMenu buildWarpMenu (int current);
juce::PopupMenu buildFilterTypeMenu (int current);

//==============================================================================
class OscPanel : public SectionPanel
{
public:
    OscPanel (EditorServices& s, int osc);
    void layoutContent (juce::Rectangle<int>) override;
    void paint (juce::Graphics&) override;
    std::function<void (int osc)> onEditWavetable;
private:
    void showTableMenu();
    void stepTable (int dir);
    void showAdvanced();
    void importTable();
    void exportTable();
    const int osc;
    WavetableView view;
    juce::TextButton tableButton;
    IconButton prev { IconButton::Icon::left, "Previous wavetable", "Loads the previous factory wavetable." };
    IconButton next { IconButton::Icon::right, "Next wavetable", "Loads the next factory wavetable." };
    IconButton edit { IconButton::Icon::edit, "Edit wavetable", "Opens the wavetable editor: draw, generate with formulas, edit harmonics, import audio." };
    IconButton gear { IconButton::Icon::gear, "Unison and tracking", "Unison mode, stack, warp/position spread and key tracking." };
    ParamMenu warp1, warp2;
    std::vector<std::unique_ptr<ModKnob>> knobs;
    std::unique_ptr<juce::FileChooser> chooser;
};

class SubPanel : public SectionPanel
{
public:
    explicit SubPanel (EditorServices& s);
    void layoutContent (juce::Rectangle<int>) override;
private:
    ParamMenu shape, route;
    ModKnob octave, semi, level, pan;
};

class NoisePanel : public SectionPanel
{
public:
    explicit NoisePanel (EditorServices& s);
    void layoutContent (juce::Rectangle<int>) override;
private:
    void loadSample();
    ParamMenu type, route;
    ModKnob pitch, fine, level, pan;
    ParamToggle key, oneShot, stereo;
    std::unique_ptr<juce::FileChooser> chooser;
};

class FilterPanel : public SectionPanel
{
public:
    explicit FilterPanel (EditorServices& s);
    void layoutContent (juce::Rectangle<int>) override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refreshLabels();
private:
    void selectFilter (int f);
    juce::Rectangle<int> tabArea (int f) const;
    int current = 0;
    FilterView view;
    std::unique_ptr<ParamToggle> filterPower[kNumFilters];
    std::unique_ptr<ParamMenu> type[kNumFilters];
    std::vector<std::unique_ptr<ModKnob>> knobs[kNumFilters];
    std::unique_ptr<ParamMenu> routes[5];
    std::unique_ptr<ParamMenu> config;
    int lastTypes[kNumFilters] { -1, -1 };
};

// ENV 1-4 and LFO 1-10 with draggable tabs.
class ModulationPanel : public SectionPanel, public Refreshable
{
public:
    explicit ModulationPanel (EditorServices& s);
    void layoutContent (juce::Rectangle<int>) override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    void select (int tab);
private:
    class SourceTab;
    int selected = 0;   // 0..3 envs, 4..13 lfos
    std::vector<std::unique_ptr<SourceTab>> tabs;
    EnvelopeView envView;
    LfoEditor lfoEditor;
    std::vector<std::unique_ptr<ModKnob>> envKnobs[kNumEnvelopes];
    struct LfoWidgets
    {
        std::unique_ptr<ModKnob> rate, syncRate, rise, delay, smooth, phase;
        std::unique_ptr<ParamToggle> sync;
        std::unique_ptr<ParamMenu> type, mode, feel;
    } lfoW[kNumLfos];
    bool lastSync[kNumLfos] {};
};

class MacroPanel : public SectionPanel
{
public:
    explicit MacroPanel (EditorServices& s);
    void layoutContent (juce::Rectangle<int>) override;
    void refreshNames();
private:
    std::vector<std::unique_ptr<ModKnob>> knobs;
    std::vector<std::unique_ptr<juce::Label>> names;
    std::vector<std::unique_ptr<ModSourceHandle>> handles;
    std::vector<std::unique_ptr<ModSourceHandle>> extraSources;
};

class VoicingPanel : public SectionPanel
{
public:
    explicit VoicingPanel (EditorServices& s);
    void layoutContent (juce::Rectangle<int>) override;
private:
    ModKnob poly, porta, curve;
    ParamToggle mono, legato, always, scaled;
};

} // namespace cureum
