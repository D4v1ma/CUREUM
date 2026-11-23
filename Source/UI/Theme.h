#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace cureum
{

// Design tokens and fonts for the CUREUM interface.
namespace theme
{
    inline const juce::Colour background  { 0xff0a0d13 };
    inline const juce::Colour panel       { 0xff11161f };
    inline const juce::Colour panelRaised { 0xff161c27 };
    inline const juce::Colour header      { 0xff0e1219 };
    inline const juce::Colour outline     { 0xff232c3b };
    inline const juce::Colour outlineHi   { 0xff33405a };
    inline const juce::Colour track       { 0xff252e40 };
    inline const juce::Colour knobBody    { 0xff1b2231 };
    inline const juce::Colour text        { 0xffe6ecf5 };
    inline const juce::Colour textDim     { 0xff8d99ae };
    inline const juce::Colour textFaint   { 0xff566176 };
    inline const juce::Colour accent      { 0xff2fd6f2 };   // values
    inline const juce::Colour accentDeep  { 0xff1590b8 };
    inline const juce::Colour mod         { 0xffa070ff };   // modulation
    inline const juce::Colour modDeep     { 0xff6b45d6 };
    inline const juce::Colour warn        { 0xffff5d73 };
    inline const juce::Colour good        { 0xff3ee08f };
    inline const juce::Colour gold        { 0xffffc85a };

    juce::Font font (float height, int weight = 1);   // 0 regular, 1 medium, 2 semibold, 3 bold
    inline juce::Font label (float h = 11.0f)  { return font (h, 2); }
    inline juce::Font value (float h = 11.5f)  { return font (h, 1); }
    inline juce::Font title (float h = 13.0f)  { return font (h, 3); }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, bool raised = false);
    juce::Colour sourceColour (int modSourceIndex);
}

class CureumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CureumLookAndFeel();

    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getPopupMenuBorderSize() override { return 4; }

private:
    juce::Typeface::Ptr faces[4];
};

} // namespace cureum
