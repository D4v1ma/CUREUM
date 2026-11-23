#include "Theme.h"
#include "CureumBinaryData.h"
#include "../Core/ModSources.h"

namespace cureum
{

namespace
{
    juce::Typeface::Ptr loadFace (int weight)
    {
        switch (weight)
        {
            case 0:  return juce::Typeface::createSystemTypefaceFor (CureumBinary::BarlowRegular_ttf, (size_t) CureumBinary::BarlowRegular_ttfSize);
            case 1:  return juce::Typeface::createSystemTypefaceFor (CureumBinary::BarlowMedium_ttf, (size_t) CureumBinary::BarlowMedium_ttfSize);
            case 2:  return juce::Typeface::createSystemTypefaceFor (CureumBinary::BarlowSemiBold_ttf, (size_t) CureumBinary::BarlowSemiBold_ttfSize);
            default: return juce::Typeface::createSystemTypefaceFor (CureumBinary::BarlowBold_ttf, (size_t) CureumBinary::BarlowBold_ttfSize);
        }
    }

    juce::Typeface::Ptr face (int weight)
    {
        static juce::Typeface::Ptr faces[4] = { loadFace (0), loadFace (1), loadFace (2), loadFace (3) };
        return faces[juce::jlimit (0, 3, weight)];
    }
}

juce::Font theme::font (float height, int weight)
{
    return juce::Font (juce::FontOptions (face (weight)).withHeight (height));
}

void theme::drawPanel (juce::Graphics& g, juce::Rectangle<float> r, bool raised)
{
    juce::ColourGradient grad (raised ? panelRaised : panel, r.getX(), r.getY(),
                               (raised ? panelRaised : panel).darker (0.15f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

juce::Colour theme::sourceColour (int s)
{
    const auto src = (ModSource) s;
    if (src >= ModSource::env1 && src <= ModSource::env4) return juce::Colour (0xffffa24d);
    if (src >= ModSource::lfo1 && src <= ModSource::lfo10) return mod;
    if (src >= ModSource::macro1 && src <= ModSource::macro8) return juce::Colour (0xff3ee0b0);
    return juce::Colour (0xffff6fb5);
}

//==============================================================================
CureumLookAndFeel::CureumLookAndFeel()
{
    for (int i = 0; i < 4; ++i) faces[i] = face (i);
    using namespace theme;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::PopupMenu::backgroundColourId, panelRaised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDeep.withAlpha (0.55f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::ComboBox::backgroundColourId, knobBody);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::TextButton::buttonColourId, knobBody);
    setColour (juce::TextButton::buttonOnColourId, accentDeep);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, background);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accentDeep.withAlpha (0.5f));
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, outlineHi);
    setColour (juce::TooltipWindow::backgroundColourId, panelRaised);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, outlineHi);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, track);
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, panelRaised);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, outlineHi);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, textFaint);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd9dee8));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff141922));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6c7486));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.25f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.7f));
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.4f));
    setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, panelRaised);
    setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, textDim);
    setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff606a7c));
}

juce::Typeface::Ptr CureumLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() || f.getTypefaceName().isEmpty())
        return faces[f.isBold() ? 3 : 1];
    return LookAndFeel_V4::getTypefaceForFont (f);
}

juce::Font CureumLookAndFeel::getPopupMenuFont()                        { return theme::font (14.0f, 1); }
juce::Font CureumLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return theme::font (std::min (13.0f, (float) h * 0.55f), 2); }
juce::Font CureumLookAndFeel::getComboBoxFont (juce::ComboBox& b)      { return theme::font (std::min (13.0f, (float) b.getHeight() * 0.58f), 1); }
juce::Font CureumLookAndFeel::getLabelFont (juce::Label& l)            { return theme::font (std::max (10.0f, l.getFont().getHeight()), 1); }

void CureumLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto base = on ? theme::accentDeep.withAlpha (0.85f) : theme::knobBody;
    if (hi) base = base.brighter (0.12f);
    if (down) base = base.darker (0.1f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? theme::accent.withAlpha (0.7f) : theme::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void CureumLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? juce::Colours::white : (b.isEnabled() ? theme::text : theme::textFaint));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void CureumLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (3.0f);
    g.setColour (b.getToggleState() ? theme::accent : theme::knobBody);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (hi ? theme::outlineHi : theme::outline);
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
        g.setColour (theme::background);
        g.strokePath (tick, juce::PathStrokeType (2.0f));
    }
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f, 1));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
}

void CureumLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (theme::knobBody);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? theme::accent.withAlpha (0.6f) : theme::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (theme::textDim);
    g.fillPath (arrow);
}

void CureumLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void CureumLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (theme::panelRaised);
    g.setColour (theme::outlineHi);
    g.drawRect (0, 0, w, h, 1);
}

void CureumLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                           bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                                           const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (theme::outline);
        g.fillRect (area.reduced (6, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (2, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (theme::accentDeep.withAlpha (0.45f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }
    auto textArea = r.reduced (8, 0);
    if (isTicked)
    {
        g.setColour (theme::accent);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) textArea.getX() + 3.0f, (float) r.getCentreY() }));
    }
    textArea.removeFromLeft (12);
    if (icon != nullptr)
        icon->drawWithin (g, textArea.removeFromLeft (r.getHeight()).toFloat().reduced (3.0f), juce::RectanglePlacement::centred, 1.0f);
    g.setColour (textColour != nullptr ? *textColour : (isActive ? theme::text : theme::textFaint));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (theme::textDim);
        g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight);
    }
    if (hasSubMenu)
    {
        juce::Path p;
        const float x = (float) r.getRight() - 10.0f, y = (float) r.getCentreY();
        p.addTriangle (x - 3.0f, y - 4.0f, x - 3.0f, y + 4.0f, x + 2.0f, y);
        g.setColour (theme::textDim);
        g.fillPath (p);
    }
}

void CureumLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (theme::label (11.0f));
    g.setColour (theme::accent.withAlpha (0.8f));
    g.drawFittedText (name.toUpperCase(), area.reduced (10, 0), juce::Justification::bottomLeft, 1);
}

void CureumLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float> ((float) x + 2, (float) thumbStart, (float) w - 4, (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbStart, (float) y + 2, (float) thumbSize, (float) h - 4);
    g.setColour ((over || down) ? theme::textFaint : theme::outlineHi);
    g.fillRoundedRectangle (thumb, 3.0f);
}

void CureumLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (theme::background);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 5.0f);
}

void CureumLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& t)
{
    g.setColour (t.hasKeyboardFocus (true) ? theme::accent.withAlpha (0.7f) : theme::outline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 5.0f, 1.0f);
}

void CureumLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (theme::panelRaised);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 5.0f);
    g.setColour (theme::outlineHi);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.0f, 1.0f);
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f, 1));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> CureumLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int w = juce::jlimit (60, 320, juce::GlyphArrangement::getStringWidthInt (theme::font (13.0f, 1), tipText) + 18);
    const int lines = 1 + juce::GlyphArrangement::getStringWidthInt (theme::font (13.0f, 1), tipText) / 300;
    const int h = 10 + 17 * lines;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
}

void CureumLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (theme::track);
    g.fillRoundedRectangle (r, 3.0f);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? (float) x + (float) w * 0.5f : (float) x;
    auto fill = juce::Rectangle<float>::leftTopRightBottom (std::min (zero, pos), r.getY(), std::max (zero, pos), r.getBottom());
    g.setColour (s.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (fill, 3.0f);
}

} // namespace cureum
