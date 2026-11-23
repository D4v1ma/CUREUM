#pragma once

#include "Theme.h"
#include "../Plugin/PluginProcessor.h"

namespace cureum
{

// Services the editor provides to every widget.
class EditorServices
{
public:
    virtual ~EditorServices() = default;
    virtual CureumProcessor& getProcessor() = 0;
    virtual void showStatus (const juce::String& text, bool isMessage = false) = 0;
    virtual void openHelp (const juce::String& topicId, const juce::String& focusText = {}) = 0;
    virtual void showMatrix() = 0;
    virtual juce::DragAndDropContainer& getDragContainer() = 0;
    virtual bool isHelpModeActive() const = 0;
};

// Anything that can describe itself in the status bar / help mode.
class HelpTextProvider
{
public:
    virtual ~HelpTextProvider() = default;
    virtual juce::String getHelpTitle() const = 0;
    virtual juce::String getHelpBody() const = 0;
    virtual juce::String getHelpTopic() const { return "overview"; }
};

// Anything the editor's UI timer should poll.
class Refreshable
{
public:
    virtual ~Refreshable() = default;
    virtual void refresh() = 0;
};

// Help topic for a parameter group (used by right-click "Help" and the What's-this mode).
juce::String helpTopicForGroup (const juce::String& group);

// Base class for widgets bound to a parameter.
class ParamWidget : public juce::Component, public HelpTextProvider, public juce::SettableTooltipClient, public Refreshable
{
public:
    ParamWidget (EditorServices& s, int paramIndex);
    ~ParamWidget() override;

    int getParamIndex() const noexcept { return index; }
    const ParamSpec& spec() const { return ParamRegistry::get().spec (index); }
    void refresh() override;    // called by the editor's UI timer

    juce::String getHelpTitle() const override;
    juce::String getHelpBody() const override;
    juce::String getHelpTopic() const override { return helpTopicForGroup (spec().group); }

    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

protected:
    float getNorm() const;
    float getPlain() const;
    void setNorm (float n);
    void setPlainValue (float v);
    void beginGesture();
    void endGesture();
    void addHelpAndResetItems (juce::PopupMenu& m, int baseId);
    bool handleCommonMenuResult (int result, int baseId);

    EditorServices& services;
    CureumProcessor& proc;
    const int index;
    juce::RangedAudioParameter* param = nullptr;
    float lastNorm = -1.0f;
    bool hovering = false;
};

//==============================================================================
class ModKnob : public ParamWidget, public juce::DragAndDropTarget
{
public:
    enum class Style { normal, small, large };
    ModKnob (EditorServices& s, int paramIndex, Style style = Style::normal, const juce::String& labelText = {});

    void refresh() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void setLabel (const juce::String& l) { label = l; repaint(); }
    juce::String getHelpBody() const override;

private:
    juce::Rectangle<float> knobArea() const;
    void showMenu();
    void startTextEntry();

    Style style;
    juce::String label;
    struct ModRange { float from, to; juce::Colour colour; int slot; };
    std::vector<ModRange> ranges;
    float liveMod = -1.0f;
    bool dropHighlight = false, dragging = false, modDrag = false;
    int modDragSlot = -1;
    float dragStartNorm = 0.0f, dragStartAmount = 0.0f;
    juce::Point<float> dragStart;
    std::unique_ptr<juce::TextEditor> entry;
    int lastStructure = -1;
};

//==============================================================================
class ParamToggle : public ParamWidget
{
public:
    enum class Style { power, pill, led };
    ParamToggle (EditorServices& s, int paramIndex, Style style = Style::pill, const juce::String& text = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    Style style;
    juce::String text;
};

//==============================================================================
class ParamMenu : public ParamWidget
{
public:
    using MenuBuilder = std::function<void (juce::PopupMenu&, int currentIndex)>;   // item ids must be index + 1
    ParamMenu (EditorServices& s, int paramIndex, MenuBuilder customBuilder = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setPrefix (const juce::String& p) { prefix = p; repaint(); }
    std::function<void (int itemId)> onExtraItem;   // ids >= 5000 are custom actions
private:
    MenuBuilder builder;
    juce::String prefix;
};

//==============================================================================
// A draggable chip representing a modulation source.
class ModSourceHandle : public juce::Component, public HelpTextProvider, public juce::SettableTooltipClient
{
public:
    ModSourceHandle (EditorServices& s, ModSource source, const juce::String& text = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getHelpTitle() const override;
    juce::String getHelpBody() const override;
    juce::String getHelpTopic() const override { return "modulation"; }
    ModSource getSource() const noexcept { return source; }
    void setText (const juce::String& t) { text = t; repaint(); }
    bool compact = false;
private:
    EditorServices& services;
    ModSource source;
    juce::String text;
    bool hovering = false;
};

// Small icon button (drawn vector icons).
class IconButton : public juce::Button, public HelpTextProvider
{
public:
    enum class Icon { left, right, save, menu, help, whatsThis, browse, edit, close, plus, gear, folder, star, power, dice };
    IconButton (Icon i, const juce::String& title, const juce::String& body);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::String getHelpTitle() const override { return helpTitle; }
    juce::String getHelpBody() const override { return helpBody; }
    Icon icon;
    juce::String helpTitle, helpBody;
    bool active = false;
};

void drawIcon (juce::Graphics& g, IconButton::Icon icon, juce::Rectangle<float> r, juce::Colour c);

// Draws a small-caps section title.
void drawSectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Colour c = theme::textDim);

// Lays out a row of components with equal width inside r.
void layoutRow (juce::Rectangle<int> r, std::initializer_list<juce::Component*> comps, int gap = 4);

} // namespace cureum
