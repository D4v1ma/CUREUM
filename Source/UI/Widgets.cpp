#include "Widgets.h"

namespace cureum
{

juce::String helpTopicForGroup (const juce::String& g)
{
    if (g.startsWith ("Osc")) return "oscillators";
    if (g == "Sub" || g == "Noise") return "sub-noise";
    if (g == "Routing" || g.startsWith ("Filter")) return "filters";
    if (g.startsWith ("Env")) return "envelopes";
    if (g.startsWith ("LFO")) return "lfos";
    if (g == "Macros") return "macros";
    if (g == "Global") return "voicing";
    if (g == "Matrix") return "matrix";
    if (g.startsWith ("Plugin")) return "plugins";
    return "effects";
}

//==============================================================================
ParamWidget::ParamWidget (EditorServices& s, int i) : services (s), proc (s.getProcessor()), index (i)
{
    param = proc.getParam (i);
    jassert (param != nullptr);
}

ParamWidget::~ParamWidget() = default;

float ParamWidget::getNorm() const   { return param != nullptr ? param->getValue() : 0.0f; }
float ParamWidget::getPlain() const  { return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f; }
void ParamWidget::setNorm (float n)  { if (param != nullptr) param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n)); }
void ParamWidget::setPlainValue (float v) { if (param != nullptr) setNorm (param->convertTo0to1 (v)); }
void ParamWidget::beginGesture()     { if (param != nullptr) param->beginChangeGesture(); }
void ParamWidget::endGesture()       { if (param != nullptr) param->endChangeGesture(); }

void ParamWidget::refresh()
{
    const float n = getNorm();
    if (n != lastNorm)
    {
        lastNorm = n;
        repaint();
    }
}

juce::String ParamWidget::getHelpTitle() const
{
    return spec().name + "  " + spec().formatValue (getPlain());
}

juce::String ParamWidget::getHelpBody() const { return spec().help; }

void ParamWidget::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    services.showStatus (getHelpTitle() + "  -  " + getHelpBody());
    repaint();
}

void ParamWidget::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void ParamWidget::addHelpAndResetItems (juce::PopupMenu& m, int baseId)
{
    m.addItem (baseId + 1, "Reset to default");
    m.addSeparator();
    m.addItem (baseId + 2, "Help: " + spec().name);
}

bool ParamWidget::handleCommonMenuResult (int result, int baseId)
{
    if (result == baseId + 1)
    {
        beginGesture();
        setNorm (param->getDefaultValue());
        endGesture();
        return true;
    }
    if (result == baseId + 2)
    {
        services.openHelp (getHelpTopic(), spec().name);
        return true;
    }
    return false;
}

//==============================================================================
ModKnob::ModKnob (EditorServices& s, int i, Style st, const juce::String& labelText)
    : ParamWidget (s, i), style (st), label (labelText.isNotEmpty() ? labelText : ParamRegistry::get().spec (i).shortName)
{
    setRepaintsOnMouseActivity (false);
}

juce::Rectangle<float> ModKnob::knobArea() const
{
    auto r = getLocalBounds().toFloat();
    const float labelH = style == Style::small ? 12.0f : 14.0f;
    r.removeFromBottom (labelH);
    const float d = std::min (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (d, d);
}

void ModKnob::refresh()
{
    const float n = getNorm();
    bool changed = n != lastNorm;
    lastNorm = n;

    std::vector<ModRange> newRanges;
    const auto& slots = proc.getModSlots();
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const auto& sl = slots[(size_t) i];
        if (sl.dest != index || sl.source == ModSource::none || ! sl.enabled)
            continue;
        const float amt = proc.getPlain (pid::mod (i)) * sl.output;
        float from = n, to = n + amt;
        if (sl.bipolar || isNaturallyBipolar (sl.source)) { from = n - std::abs (amt); to = n + std::abs (amt); }
        newRanges.push_back ({ juce::jlimit (0.0f, 1.0f, std::min (from, to)), juce::jlimit (0.0f, 1.0f, std::max (from, to)),
                               theme::sourceColour ((int) sl.source), i });
    }
    if (newRanges.size() != ranges.size()) changed = true;
    else
        for (size_t k = 0; k < ranges.size(); ++k)
            if (std::abs (ranges[k].from - newRanges[k].from) > 1.0e-4f || std::abs (ranges[k].to - newRanges[k].to) > 1.0e-4f)
                changed = true;
    ranges = std::move (newRanges);

    float live = -1.0f;
    if (const auto* m = proc.getCompiledMatrix(); m != nullptr && m->modulated[(size_t) index]
        && (proc.feedback.anyVoice.load() || spec().modScope == ModScope::global))
        live = proc.feedback.modNorm[(size_t) index].load();
    if (std::abs (live - liveMod) > 0.002f) { liveMod = live; changed = true; }

    if (changed) repaint();
}

void ModKnob::paint (juce::Graphics& g)
{
    const auto& s = spec();
    auto area = knobArea();
    const auto c = area.getCentre();
    const float radius = area.getWidth() * 0.5f - 4.0f;
    const float start = -2.35619f, end = 2.35619f;
    auto angleFor = [&] (float n) { return start + juce::jlimit (0.0f, 1.0f, n) * (end - start); };
    const float thick = style == Style::small ? 2.5f : (style == Style::large ? 4.0f : 3.2f);
    const bool bipolar = s.range.start < 0.0f && s.range.end > 0.0f;
    const float origin = bipolar ? s.toNormalised (0.0f) : 0.0f;
    const float n = getNorm();

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, start, end, true);
    g.setColour (theme::track);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc with soft glow
    juce::Path val;
    val.addCentredArc (c.x, c.y, radius, radius, 0.0f, angleFor (origin), angleFor (n), true);
    g.setColour (theme::accent.withAlpha (0.18f));
    g.strokePath (val, juce::PathStrokeType (thick + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (theme::accent);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // modulation ranges
    float ringR = radius + thick + 1.5f;
    for (auto& rg : ranges)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, angleFor (rg.from), angleFor (rg.to), true);
        g.setColour (rg.colour.withAlpha (0.9f));
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        ringR += 2.5f;
    }

    // body
    const float bodyR = radius * (style == Style::small ? 0.66f : 0.7f);
    juce::ColourGradient grad (theme::knobBody.brighter (0.25f), c.x, c.y - bodyR, theme::knobBody.darker (0.3f), c.x, c.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c));
    g.setColour (hovering || dragging ? theme::outlineHi.brighter (0.2f) : theme::outline);
    g.drawEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c), 1.0f);

    const float a = angleFor (n);
    const juce::Point<float> dir (std::sin (a), -std::cos (a));
    g.setColour (theme::text);
    g.drawLine ({ c + dir * (bodyR * 0.25f), c + dir * (bodyR * 0.92f) }, 2.0f);

    if (liveMod >= 0.0f)
    {
        const float la = angleFor (liveMod);
        const auto p = c + juce::Point<float> (std::sin (la), -std::cos (la)) * radius;
        g.setColour (theme::mod.brighter (0.4f));
        g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f).withCentre (p));
    }

    if (dropHighlight)
    {
        g.setColour (theme::mod.withAlpha (0.35f));
        g.fillEllipse (area.reduced (1.0f));
        g.setColour (theme::mod);
        g.drawEllipse (area.reduced (1.0f), 2.0f);
    }

    // label / value
    auto textArea = getLocalBounds().toFloat().removeFromBottom (style == Style::small ? 12.0f : 14.0f);
    const bool showValue = dragging || hovering;
    g.setFont (showValue ? theme::value (style == Style::small ? 10.5f : 11.5f) : theme::label (style == Style::small ? 9.5f : 10.5f));
    g.setColour (showValue ? theme::accent : theme::textDim);
    g.drawFittedText (showValue ? s.formatValue (getPlain()) : label, textArea.toNearestInt(), juce::Justification::centred, 1);
}

void ModKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    dragStart = e.position;
    dragStartNorm = getNorm();
    const auto area = knobArea();
    const float dist = e.position.getDistanceFrom (area.getCentre());
    modDrag = ! ranges.empty() && (e.mods.isAltDown() || dist > area.getWidth() * 0.5f - 3.0f);
    if (modDrag)
    {
        modDragSlot = modDragSlot >= 0 && std::any_of (ranges.begin(), ranges.end(), [this] (auto& r) { return r.slot == modDragSlot; })
                        ? modDragSlot : ranges.back().slot;
        dragStartAmount = proc.getPlain (pid::mod (modDragSlot));
        if (auto* p = proc.getParam (pid::mod (modDragSlot))) p->beginChangeGesture();
    }
    else
        beginGesture();
    dragging = true;
    repaint();
}

void ModKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const float delta = (dragStart.y - e.position.y) + (e.position.x - dragStart.x) * 0.25f;
    const float sens = e.mods.isShiftDown() ? 0.0008f : 0.005f;
    if (modDrag && modDragSlot >= 0)
    {
        const float amount = juce::jlimit (-1.0f, 1.0f, dragStartAmount + delta * sens);
        proc.setPlain (pid::mod (modDragSlot), amount);
        services.showStatus ("Modulation " + modSourceNames()[(int) proc.getModSlots()[(size_t) modDragSlot].source] + " -> " + spec().name
                             + ": " + juce::String (amount * 100.0f, 1) + "%");
        refresh();
        return;
    }
    float n = dragStartNorm + delta * sens;
    if (spec().kind == ParamKind::integer || spec().kind == ParamKind::choice)
        n = spec().toNormalised (std::round (spec().fromNormalised (n)));
    setNorm (n);
    services.showStatus (getHelpTitle());
    repaint();
}

void ModKnob::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    if (modDrag && modDragSlot >= 0)
    {
        if (auto* p = proc.getParam (pid::mod (modDragSlot))) p->endChangeGesture();
    }
    else
        endGesture();
    dragging = false;
    modDrag = false;
    repaint();
}

void ModKnob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    beginGesture();
    setNorm (param->getDefaultValue());
    endGesture();
}

void ModKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto& s = spec();
    const float dir = (w.deltaY + w.deltaX) > 0 ? 1.0f : -1.0f;
    if (s.kind == ParamKind::integer || s.kind == ParamKind::choice)
        setPlainValue (getPlain() + dir);
    else
        setNorm (getNorm() + dir * (e.mods.isShiftDown() ? 0.002f : 0.01f));
    services.showStatus (getHelpTitle());
}

bool ModKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("modsrc:") && spec().isModulatable();
}

void ModKnob::itemDragEnter (const SourceDetails&) { dropHighlight = true; repaint(); }
void ModKnob::itemDragExit (const SourceDetails&)  { dropHighlight = false; repaint(); }

void ModKnob::itemDropped (const SourceDetails& d)
{
    dropHighlight = false;
    const auto src = (ModSource) d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
    const bool bip = isNaturallyBipolar (src);
    const int slot = proc.addModulation (src, index, bip ? 0.25f : 0.35f, bip);
    if (slot >= 0)
    {
        modDragSlot = slot;
        services.showStatus ("Added " + modSourceNames()[(int) src] + " -> " + spec().name
                             + ". Drag the outer ring (or Alt-drag) to set the depth.", true);
    }
    else
        services.showStatus ("All 64 modulation slots are in use.", true);
    refresh();
    repaint();
}

juce::String ModKnob::getHelpBody() const
{
    auto t = spec().help;
    if (spec().isModulatable())
        t << "  Drag a modulation source here; drag the coloured ring (or Alt-drag) to change its depth.";
    return t;
}

void ModKnob::showMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader (spec().name);
    m.addItem (1, "Reset to default");
    m.addItem (2, "Enter value...");
    if (! ranges.empty())
    {
        m.addSeparator();
        m.addSectionHeader ("Modulation");
        for (auto& r : ranges)
        {
            const auto& sl = proc.getModSlots()[(size_t) r.slot];
            juce::PopupMenu sub;
            sub.addItem (1000 + r.slot, "Adjust depth with the ring", true, modDragSlot == r.slot);
            sub.addItem (2000 + r.slot, "Bipolar", true, sl.bipolar);
            sub.addItem (3000 + r.slot, "Invert depth");
            sub.addItem (4000 + r.slot, "Remove");
            m.addSubMenu (modSourceNames()[(int) sl.source] + "   " + juce::String (proc.getPlain (pid::mod (r.slot)) * 100.0f, 1) + "%", sub);
        }
        m.addItem (5, "Show in Matrix");
    }
    m.addSeparator();
    m.addItem (6, "Help: " + spec().name);

    juce::Component::SafePointer<ModKnob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& k = *safe;
        if (r == 1) { k.beginGesture(); k.setNorm (k.param->getDefaultValue()); k.endGesture(); }
        else if (r == 2) k.startTextEntry();
        else if (r == 5) k.services.showMatrix();
        else if (r == 6) k.services.openHelp (k.getHelpTopic(), k.spec().name);
        else if (r >= 1000 && r < 2000) k.modDragSlot = r - 1000;
        else if (r >= 2000 && r < 3000) { auto s = k.proc.getModSlots()[(size_t) (r - 2000)]; s.bipolar = ! s.bipolar; k.proc.setModSlot (r - 2000, s); }
        else if (r >= 3000 && r < 4000) k.proc.setPlain (pid::mod (r - 3000), -k.proc.getPlain (pid::mod (r - 3000)));
        else if (r >= 4000 && r < 5000) k.proc.removeModSlot (r - 4000);
        k.refresh();
        k.repaint();
    });
}

void ModKnob::startTextEntry()
{
    entry = std::make_unique<juce::TextEditor>();
    addAndMakeVisible (*entry);
    entry->setBounds (getLocalBounds().removeFromBottom (18).withSizeKeepingCentre (std::max (50, getWidth()), 18));
    entry->setFont (theme::value (12.0f));
    entry->setJustification (juce::Justification::centred);
    entry->setText (spec().formatValue (getPlain()), false);
    entry->selectAll();
    entry->grabKeyboardFocus();
    auto finish = [this] (bool apply)
    {
        if (entry == nullptr) return;
        if (apply)
        {
            const auto& s = spec();
            auto t = entry->getText().trim().toLowerCase();
            float v = t.getFloatValue();
            if (s.kind == ParamKind::choice || s.kind == ParamKind::toggle)
            {
                const int idx = s.choices.indexOf (entry->getText().trim(), true);
                v = idx >= 0 ? (float) idx : v;
            }
            else
            {
                if (s.unit == ParamUnit::hertz && (t.endsWith ("k") || t.contains ("khz"))) v *= 1000.0f;
                if (s.unit == ParamUnit::seconds && t.endsWith ("ms")) v /= 1000.0f;
                if ((s.unit == ParamUnit::percent || s.unit == ParamUnit::bipolarPercent || s.unit == ParamUnit::frames || s.unit == ParamUnit::gainAsDecibel)
                    && (t.contains ("%") || std::abs (v) > 1.0f))
                    v /= 100.0f;
                if (s.unit == ParamUnit::pan) { if (t.startsWith ("l")) v = -std::abs (v) / 100.0f; else if (t.startsWith ("r")) v = std::abs (v) / 100.0f; else if (t == "c") v = 0.0f; }
            }
            beginGesture();
            setPlainValue (v);
            endGesture();
        }
        auto* e = entry.release();
        juce::MessageManager::callAsync ([e] { delete e; });
        repaint();
    };
    entry->onReturnKey = [finish] { finish (true); };
    entry->onEscapeKey = [finish] { finish (false); };
    entry->onFocusLost = [finish] { finish (true); };
}

//==============================================================================
ParamToggle::ParamToggle (EditorServices& s, int i, Style st, const juce::String& t)
    : ParamWidget (s, i), style (st), text (t.isNotEmpty() ? t : ParamRegistry::get().spec (i).shortName) {}

void ParamToggle::paint (juce::Graphics& g)
{
    const bool on = getNorm() >= 0.5f;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (style == Style::power)
    {
        const float d = std::min (r.getWidth(), r.getHeight()) - 2.0f;
        auto c = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        if (on)
        {
            g.setColour (theme::accent.withAlpha (0.25f));
            g.fillEllipse (c.expanded (2.0f));
        }
        g.setColour (on ? theme::accent : theme::knobBody);
        g.fillEllipse (c);
        g.setColour (on ? theme::accent.brighter (0.3f) : theme::outlineHi);
        g.drawEllipse (c, 1.0f);
        drawIcon (g, IconButton::Icon::power, c.reduced (d * 0.22f), on ? theme::background : theme::textDim);
        return;
    }
    if (style == Style::led)
    {
        auto dot = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
        g.setColour (on ? theme::accent : theme::track);
        g.fillEllipse (dot);
        g.setColour (hovering ? theme::text : theme::textDim);
        g.setFont (theme::label (10.5f));
        g.drawFittedText (text, r.toNearestInt().withTrimmedLeft (2), juce::Justification::centredLeft, 1);
        return;
    }
    g.setColour (on ? theme::accentDeep.withAlpha (0.9f) : theme::knobBody);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? theme::accent.withAlpha (0.8f) : (hovering ? theme::outlineHi : theme::outline));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? juce::Colours::white : theme::textDim);
    g.setFont (theme::label (10.5f));
    g.drawFittedText (text, getLocalBounds().reduced (2, 0), juce::Justification::centred, 1);
}

void ParamToggle::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        addHelpAndResetItems (m, 0);
        juce::Component::SafePointer<ParamToggle> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) { if (safe != nullptr) safe->handleCommonMenuResult (r, 0); });
        return;
    }
    beginGesture();
    setNorm (getNorm() >= 0.5f ? 0.0f : 1.0f);
    endGesture();
    services.showStatus (getHelpTitle());
    repaint();
}

//==============================================================================
ParamMenu::ParamMenu (EditorServices& s, int i, MenuBuilder b) : ParamWidget (s, i), builder (std::move (b)) {}

void ParamMenu::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::knobBody);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (hovering ? theme::outlineHi : theme::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    auto t = getLocalBounds().reduced (7, 0);
    juce::Path arrow;
    const float ax = (float) getWidth() - 10.0f, ay = (float) getHeight() * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (theme::textDim);
    g.fillPath (arrow);
    t.removeFromRight (12);
    if (prefix.isNotEmpty())
    {
        g.setFont (theme::label (10.0f));
        g.setColour (theme::textFaint);
        const int w = juce::GlyphArrangement::getStringWidthInt (theme::label (10.0f), prefix) + 6;
        g.drawText (prefix, t.removeFromLeft (w), juce::Justification::centredLeft);
    }
    g.setFont (theme::value (std::min (12.5f, (float) getHeight() * 0.56f)));
    g.setColour (theme::text);
    g.drawFittedText (spec().formatValue (getPlain()), t, juce::Justification::centredLeft, 1);
}

void ParamMenu::mouseDown (const juce::MouseEvent& e)
{
    juce::PopupMenu m;
    const int current = juce::roundToInt (getPlain());
    if (e.mods.isPopupMenu())
    {
        addHelpAndResetItems (m, 100000);
    }
    else if (builder)
        builder (m, current);
    else
        for (int i = 0; i < spec().choices.size(); ++i)
            m.addItem (i + 1, spec().choices[i], true, i == current);

    juce::Component::SafePointer<ParamMenu> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (safe->handleCommonMenuResult (r, 100000)) return;
        if (r >= 5000) { if (safe->onExtraItem) safe->onExtraItem (r); return; }
        safe->beginGesture();
        safe->setPlainValue ((float) (r - 1));
        safe->endGesture();
        safe->repaint();
    });
}

void ParamMenu::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float dir = (w.deltaY + w.deltaX) > 0 ? -1.0f : 1.0f;
    beginGesture();
    setPlainValue (juce::jlimit (spec().range.start, spec().range.end, getPlain() + dir));
    endGesture();
}

//==============================================================================
ModSourceHandle::ModSourceHandle (EditorServices& s, ModSource src, const juce::String& t)
    : services (s), source (src), text (t.isNotEmpty() ? t : modSourceNames()[(int) src])
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ModSourceHandle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto c = theme::sourceColour ((int) source);
    g.setColour (c.withAlpha (hovering ? 0.28f : 0.16f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (c.withAlpha (hovering ? 0.9f : 0.55f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    auto icon = r.removeFromLeft (std::min (r.getHeight(), 16.0f)).reduced (3.0f);
    // four-way drag arrows
    juce::Path p;
    const auto cc = icon.getCentre();
    const float s = icon.getWidth() * 0.5f;
    p.startNewSubPath (cc.x - s, cc.y); p.lineTo (cc.x + s, cc.y);
    p.startNewSubPath (cc.x, cc.y - s); p.lineTo (cc.x, cc.y + s);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.4f));
    for (auto d : { juce::Point<float> (1, 0), juce::Point<float> (-1, 0), juce::Point<float> (0, 1), juce::Point<float> (0, -1) })
    {
        juce::Path a;
        const auto tip = cc + d * s;
        const juce::Point<float> perp (-d.y, d.x);
        a.addTriangle (tip, tip - d * 3.0f + perp * 2.2f, tip - d * 3.0f - perp * 2.2f);
        g.fillPath (a);
    }
    if (! compact)
    {
        g.setColour (theme::text);
        g.setFont (theme::label (10.5f));
        g.drawFittedText (text, r.toNearestInt().withTrimmedRight (2), juce::Justification::centredLeft, 1);
    }
}

void ModSourceHandle::mouseDown (const juce::MouseEvent&) {}

void ModSourceHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    auto& dc = services.getDragContainer();
    if (! dc.isDragAndDropActive())
    {
        juce::Image img (juce::Image::ARGB, 90, 22, true);
        {
            juce::Graphics g (img);
            const auto c = theme::sourceColour ((int) source);
            g.setColour (c.withAlpha (0.85f));
            g.fillRoundedRectangle (0.0f, 0.0f, 90.0f, 22.0f, 5.0f);
            g.setColour (theme::background);
            g.setFont (theme::label (12.0f));
            g.drawFittedText (modSourceNames()[(int) source], 0, 0, 90, 22, juce::Justification::centred, 1);
        }
        dc.startDragging ("modsrc:" + juce::String ((int) source), this, juce::ScaledImage (img), true);
    }
}

void ModSourceHandle::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    services.showStatus (getHelpTitle() + "  -  " + getHelpBody());
    repaint();
}

void ModSourceHandle::mouseExit (const juce::MouseEvent&) { hovering = false; repaint(); }

juce::String ModSourceHandle::getHelpTitle() const { return modSourceNames()[(int) source] + " (modulation source)"; }
juce::String ModSourceHandle::getHelpBody() const
{
    return modSourceDescription (source) + " Drag this handle onto any knob to modulate it.";
}

//==============================================================================
IconButton::IconButton (Icon i, const juce::String& t, const juce::String& b)
    : juce::Button (t), icon (i), helpTitle (t), helpBody (b)
{
    setTooltip (t);
}

void IconButton::paintButton (juce::Graphics& g, bool hi, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (hi || active)
    {
        g.setColour ((active ? theme::accentDeep : theme::knobBody).withAlpha (down ? 0.9f : 0.7f));
        g.fillRoundedRectangle (r, 5.0f);
    }
    drawIcon (g, icon, r.reduced (std::min (r.getWidth(), r.getHeight()) * 0.26f), active ? juce::Colours::white : (hi ? theme::text : theme::textDim));
}

void drawIcon (juce::Graphics& g, IconButton::Icon icon, juce::Rectangle<float> r, juce::Colour c)
{
    using I = IconButton::Icon;
    g.setColour (c);
    juce::Path p;
    const auto ctr = r.getCentre();
    const float w = r.getWidth(), h = r.getHeight();
    const juce::PathStrokeType stroke (std::max (1.4f, w * 0.12f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    switch (icon)
    {
        case I::left:  p.addTriangle (r.getRight() - w * 0.2f, r.getY(), r.getRight() - w * 0.2f, r.getBottom(), r.getX() + w * 0.2f, ctr.y); g.fillPath (p); break;
        case I::right: p.addTriangle (r.getX() + w * 0.2f, r.getY(), r.getX() + w * 0.2f, r.getBottom(), r.getRight() - w * 0.2f, ctr.y); g.fillPath (p); break;
        case I::save:
            p.addRoundedRectangle (r, 2.0f);
            g.strokePath (p, stroke);
            g.fillRect (r.withHeight (h * 0.3f).reduced (w * 0.22f, 0).translated (0, h * 0.05f));
            g.fillEllipse (juce::Rectangle<float> (w * 0.3f, w * 0.3f).withCentre ({ ctr.x, r.getY() + h * 0.68f }));
            break;
        case I::menu:
            for (int i = 0; i < 3; ++i) g.fillRoundedRectangle (r.getX(), r.getY() + h * (0.12f + 0.34f * (float) i), w, h * 0.12f, 1.0f);
            break;
        case I::help:
            g.drawEllipse (r, stroke.getStrokeThickness());
            g.setFont (theme::font (h * 0.8f, 3));
            g.drawText ("?", r, juce::Justification::centred);
            break;
        case I::whatsThis:
            p.startNewSubPath (r.getX() + w * 0.2f, r.getY());
            p.lineTo (r.getX() + w * 0.2f, r.getBottom() - h * 0.15f);
            p.lineTo (r.getX() + w * 0.42f, r.getBottom() - h * 0.35f);
            p.lineTo (r.getX() + w * 0.62f, r.getBottom());
            p.lineTo (r.getX() + w * 0.72f, r.getBottom() - h * 0.08f);
            p.lineTo (r.getX() + w * 0.55f, r.getBottom() - h * 0.42f);
            p.lineTo (r.getX() + w * 0.85f, r.getBottom() - h * 0.42f);
            p.closeSubPath();
            g.fillPath (p);
            break;
        case I::browse:
            for (int i = 0; i < 3; ++i)
            {
                g.fillEllipse (r.getX(), r.getY() + h * (0.08f + 0.34f * (float) i), h * 0.16f, h * 0.16f);
                g.fillRoundedRectangle (r.getX() + w * 0.3f, r.getY() + h * (0.1f + 0.34f * (float) i), w * 0.7f, h * 0.12f, 1.0f);
            }
            break;
        case I::edit:
            p.startNewSubPath (r.getX(), r.getBottom());
            p.lineTo (r.getX() + w * 0.25f, r.getBottom() - h * 0.05f);
            p.lineTo (r.getRight(), r.getY() + h * 0.28f);
            p.lineTo (r.getRight() - w * 0.28f, r.getY());
            p.lineTo (r.getX() + w * 0.05f, r.getBottom() - h * 0.25f);
            p.closeSubPath();
            g.fillPath (p);
            break;
        case I::close:
            p.startNewSubPath (r.getTopLeft()); p.lineTo (r.getBottomRight());
            p.startNewSubPath (r.getTopRight()); p.lineTo (r.getBottomLeft());
            g.strokePath (p, stroke);
            break;
        case I::plus:
            p.startNewSubPath (ctr.x, r.getY()); p.lineTo (ctr.x, r.getBottom());
            p.startNewSubPath (r.getX(), ctr.y); p.lineTo (r.getRight(), ctr.y);
            g.strokePath (p, stroke);
            break;
        case I::gear:
        {
            const float ro = w * 0.5f, ri = w * 0.36f;
            for (int i = 0; i < 16; ++i)
            {
                const float a = (float) i / 16.0f * dsp::kTwoPi;
                const float rr = (i % 2 == 0) ? ro : ri;
                const juce::Point<float> pt (ctr.x + std::cos (a) * rr, ctr.y + std::sin (a) * rr);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            p.addEllipse (juce::Rectangle<float> (w * 0.3f, w * 0.3f).withCentre (ctr));
            p.setUsingNonZeroWinding (false);
            g.fillPath (p);
            break;
        }
        case I::folder:
            p.startNewSubPath (r.getX(), r.getBottom());
            p.lineTo (r.getX(), r.getY() + h * 0.1f);
            p.lineTo (r.getX() + w * 0.38f, r.getY() + h * 0.1f);
            p.lineTo (r.getX() + w * 0.48f, r.getY() + h * 0.25f);
            p.lineTo (r.getRight(), r.getY() + h * 0.25f);
            p.lineTo (r.getRight(), r.getBottom());
            p.closeSubPath();
            g.fillPath (p);
            break;
        case I::star:
        {
            for (int i = 0; i < 10; ++i)
            {
                const float a = -dsp::kPi * 0.5f + (float) i * dsp::kPi / 5.0f;
                const float rr = (i % 2 == 0) ? w * 0.5f : w * 0.2f;
                const juce::Point<float> pt (ctr.x + std::cos (a) * rr, ctr.y + std::sin (a) * rr);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            g.fillPath (p);
            break;
        }
        case I::power:
            p.addCentredArc (ctr.x, ctr.y, w * 0.42f, h * 0.42f, 0.0f, 0.6f, dsp::kTwoPi - 0.6f, true);
            p.startNewSubPath (ctr.x, r.getY() - h * 0.02f);
            p.lineTo (ctr.x, ctr.y);
            g.strokePath (p, juce::PathStrokeType (std::max (1.2f, w * 0.14f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        case I::dice:
            g.drawRoundedRectangle (r, 2.0f, stroke.getStrokeThickness());
            for (auto f : { juce::Point<float> (0.3f, 0.3f), juce::Point<float> (0.7f, 0.7f), juce::Point<float> (0.5f, 0.5f) })
                g.fillEllipse (juce::Rectangle<float> (w * 0.16f, w * 0.16f).withCentre ({ r.getX() + w * f.x, r.getY() + h * f.y }));
            break;
    }
}

void drawSectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Colour c)
{
    g.setFont (theme::label (11.0f));
    g.setColour (c);
    g.drawText (text.toUpperCase(), r, juce::Justification::centredLeft);
}

void layoutRow (juce::Rectangle<int> r, std::initializer_list<juce::Component*> comps, int gap)
{
    const int n = (int) comps.size();
    if (n == 0) return;
    const int w = (r.getWidth() - gap * (n - 1)) / n;
    for (auto* c : comps)
    {
        if (c != nullptr) c->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (gap);
    }
}

} // namespace cureum
