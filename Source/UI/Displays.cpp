#include "Displays.h"
#include "../Core/ValueMaps.h"

namespace cureum
{

using namespace pid;

static void drawDisplayBackground (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::ColourGradient grad (juce::Colour (0xff0c1018), r.getX(), r.getY(), juce::Colour (0xff080a10), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

//==============================================================================
WavetableView::WavetableView (EditorServices& s, int o) : services (s), proc (s.getProcessor()), osc (o)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void WavetableView::rebuild()
{
    frames.clear();
    auto table = proc.getWavetable (osc);
    cachedTable = table.get();
    if (table == nullptr) return;
    const int n = table->getNumFrames();
    const int shown = std::min (n, 40);
    for (int k = 0; k < shown; ++k)
    {
        const int f = shown == 1 ? 0 : juce::roundToInt ((float) k / (float) (shown - 1) * (float) (n - 1));
        const float* raw = table->getFrame (f);
        std::vector<float> pts (128);
        for (int i = 0; i < 128; ++i) pts[(size_t) i] = raw[i * 16];
        frames.push_back (std::move (pts));
    }
}

void WavetableView::refresh()
{
    bool changed = false;
    auto t = proc.getWavetable (osc);
    if (t.get() != cachedTable) { rebuild(); changed = true; }
    const float p = proc.getPlain (pid::osc (osc, Osc::wtPos));
    if (p != pos) { pos = p; changed = true; }
    float live = -1.0f;
    if (const auto* m = proc.getCompiledMatrix(); m != nullptr && m->modulated[(size_t) pid::osc (osc, Osc::wtPos)] && proc.feedback.anyVoice.load())
        live = proc.feedback.modNorm[(size_t) pid::osc (osc, Osc::wtPos)].load();
    if (std::abs (live - livePos) > 0.002f) { livePos = live; changed = true; }
    const bool en = proc.getPlain (pid::osc (osc, Osc::enable)) > 0.5f;
    if (en != enabled) { enabled = en; changed = true; }
    if (changed) repaint();
}

void WavetableView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    auto table = proc.getWavetable (osc);
    if (table == nullptr || frames.empty()) return;

    auto area = r.reduced (10.0f, 12.0f);
    const float depthX = area.getWidth() * 0.22f, depthY = area.getHeight() * 0.36f;
    const float w = area.getWidth() - depthX;
    const float amp = area.getHeight() * 0.24f;
    const float baseY = area.getBottom() - amp - 4.0f;
    const int shown = (int) frames.size();
    const float alphaScale = enabled ? 1.0f : 0.4f;

    auto drawFrame = [&] (const float* pts, int count, int stride, float t, juce::Colour c, float thickness)
    {
        juce::Path p;
        const float x0 = area.getX() + t * depthX, y0 = baseY - t * depthY;
        for (int i = 0; i < count; ++i)
        {
            const float x = x0 + w * (float) i / (float) (count - 1);
            const float y = y0 - pts[i * stride] * amp;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness));
    };

    for (int k = shown - 1; k >= 0; --k)
    {
        const float t = shown == 1 ? 0.0f : (float) k / (float) (shown - 1);
        drawFrame (frames[(size_t) k].data(), 128, 1, t, theme::accentDeep.withAlpha ((0.10f + 0.22f * (1.0f - t)) * alphaScale), 1.0f);
    }

    auto currentFrame = [&] (float position, std::vector<float>& out)
    {
        const int n = table->getNumFrames();
        const float fp = position * (float) (n - 1);
        const int f0 = juce::jlimit (0, n - 1, (int) fp);
        const int f1 = std::min (n - 1, f0 + 1);
        const float fr = table->interpolatesFrames() ? fp - (float) f0 : 0.0f;
        const float* a = table->getFrame (f0);
        const float* b = table->getFrame (f1);
        out.resize (256);
        for (int i = 0; i < 256; ++i) out[(size_t) i] = a[i * 8] + fr * (b[i * 8] - a[i * 8]);
    };
    std::vector<float> cur;
    currentFrame (pos, cur);
    drawFrame (cur.data(), 256, 1, pos, theme::accent.withAlpha (0.25f * alphaScale), 5.0f);
    drawFrame (cur.data(), 256, 1, pos, theme::accent.withAlpha (alphaScale), 1.8f);
    if (livePos >= 0.0f && std::abs (livePos - pos) > 0.003f)
    {
        currentFrame (livePos, cur);
        drawFrame (cur.data(), 256, 1, livePos, theme::mod.withAlpha (0.9f * alphaScale), 1.5f);
    }

    g.setFont (theme::label (10.0f));
    g.setColour (theme::textFaint);
    const int frame = juce::roundToInt ((livePos >= 0.0f ? livePos : pos) * (float) (table->getNumFrames() - 1)) + 1;
    g.drawText (juce::String (frame) + " / " + juce::String (table->getNumFrames()), r.reduced (8.0f, 5.0f).toNearestInt(), juce::Justification::topRight);
}

void WavetableView::mouseDown (const juce::MouseEvent&)
{
    dragStartNorm = proc.getPlain (pid::osc (osc, Osc::wtPos));
    if (auto* p = proc.getParam (pid::osc (osc, Osc::wtPos))) p->beginChangeGesture();
}

void WavetableView::mouseDrag (const juce::MouseEvent& e)
{
    const float v = juce::jlimit (0.0f, 1.0f, dragStartNorm - (float) e.getDistanceFromDragStartY() / (float) std::max (60, getHeight() * 2));
    proc.setPlain (pid::osc (osc, Osc::wtPos), v);
    services.showStatus ("Osc " + oscLetter (osc) + " WT Position  " + juce::String (v * 100.0f, 1) + "%");
}

void WavetableView::mouseUp (const juce::MouseEvent&)
{
    if (auto* p = proc.getParam (pid::osc (osc, Osc::wtPos))) p->endChangeGesture();
}

void WavetableView::mouseEnter (const juce::MouseEvent&)
{
    services.showStatus (getHelpTitle() + "  -  " + getHelpBody());
}

//==============================================================================
FilterView::FilterView (EditorServices& s, int f) : services (s), proc (s.getProcessor()), filter (f)
{
    for (int i = 0; i < 160; ++i)
        freqs.push_back (20.0f * std::pow (1000.0f, (float) i / 159.0f));
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

int FilterView::typeParam() const   { return filter >= 0 ? pid::filter (filter, Filter::type) : fxFilter (FxFilter::type); }
int FilterView::cutoffParam() const { return filter >= 0 ? pid::filter (filter, Filter::cutoff) : fxFilter (FxFilter::cutoff); }
int FilterView::resoParam() const   { return filter >= 0 ? pid::filter (filter, Filter::reso) : fxFilter (FxFilter::reso); }
int FilterView::driveParam() const  { return filter >= 0 ? pid::filter (filter, Filter::drive) : fxFilter (FxFilter::drive); }
int FilterView::varParam() const    { return filter >= 0 ? pid::filter (filter, Filter::var) : fxFilter (FxFilter::var); }
int FilterView::enableParam() const { return filter >= 0 ? pid::filter (filter, Filter::enable) : fxFilter (FxFilter::enable); }

void FilterView::refresh()
{
    juce::String k;
    for (int p : { typeParam(), cutoffParam(), resoParam(), driveParam(), varParam(), enableParam() })
        k << proc.getPlain (p) << ";";
    if (k != key) { key = k; triggerAsyncUpdate(); }

    float live = -1.0f;
    if (const auto* m = proc.getCompiledMatrix(); m != nullptr && m->modulated[(size_t) cutoffParam()]
        && (proc.feedback.anyVoice.load() || filter < 0))
        live = proc.feedback.modNorm[(size_t) cutoffParam()].load();
    if (std::abs (live - liveCutoff) > 0.002f) { liveCutoff = live; repaint(); }
}

void FilterView::handleAsyncUpdate()
{
    MultiFilter::measureResponse ((int) proc.getPlain (typeParam()), proc.getPlain (cutoffParam()), proc.getPlain (resoParam()),
                                  proc.getPlain (driveParam()) * 0.2f, proc.getPlain (varParam()), 48000.0, freqs, mags);
    repaint();
}

void FilterView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    auto area = r.reduced (6.0f, 6.0f);
    const bool on = proc.getPlain (enableParam()) > 0.5f;

    auto xFor = [&] (float hz) { return area.getX() + area.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); };
    auto yFor = [&] (float db) { return area.getY() + area.getHeight() * (1.0f - (juce::jlimit (-36.0f, 24.0f, db) + 36.0f) / 60.0f); };

    g.setColour (theme::outline.withAlpha (0.6f));
    for (float hz : { 100.0f, 1000.0f, 10000.0f }) g.drawVerticalLine ((int) xFor (hz), area.getY(), area.getBottom());
    g.drawHorizontalLine ((int) yFor (0.0f), area.getX(), area.getRight());
    g.setFont (theme::label (9.0f));
    g.setColour (theme::textFaint);
    g.drawText ("100", juce::Rectangle<float> (xFor (100.0f) + 2, area.getBottom() - 11, 30, 10), juce::Justification::left);
    g.drawText ("1k", juce::Rectangle<float> (xFor (1000.0f) + 2, area.getBottom() - 11, 30, 10), juce::Justification::left);
    g.drawText ("10k", juce::Rectangle<float> (xFor (10000.0f) + 2, area.getBottom() - 11, 30, 10), juce::Justification::left);

    if (mags.size() == freqs.size())
    {
        juce::Path curve;
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            const float db = juce::Decibels::gainToDecibels (std::max (1.0e-5f, mags[i]));
            const float x = xFor (freqs[i]), y = yFor (db);
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (area.getRight(), area.getBottom());
        fill.lineTo (area.getX(), area.getBottom());
        fill.closeSubPath();
        const auto c = on ? theme::accent : theme::textFaint;
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.28f), 0, area.getY(), c.withAlpha (0.02f), 0, area.getBottom(), false));
        g.fillPath (fill);
        g.setColour (c);
        g.strokePath (curve, juce::PathStrokeType (1.8f));
    }

    if (liveCutoff >= 0.0f && on)
    {
        const float hz = ParamRegistry::get().spec (cutoffParam()).fromNormalised (liveCutoff);
        g.setColour (theme::mod.withAlpha (0.8f));
        g.drawVerticalLine ((int) xFor (juce::jlimit (20.0f, 20000.0f, hz)), area.getY(), area.getBottom());
    }

    g.setFont (theme::value (10.5f));
    g.setColour (theme::textDim);
    g.drawText (filterTypeNames()[(int) proc.getPlain (typeParam())] + "   " + ParamRegistry::get().spec (cutoffParam()).formatValue (proc.getPlain (cutoffParam())),
                area.reduced (4.0f, 2.0f).toNearestInt(), juce::Justification::topLeft);
}

void FilterView::mouseDown (const juce::MouseEvent&)
{
    startCut = proc.getParam (cutoffParam())->getValue();
    startRes = proc.getParam (resoParam())->getValue();
    proc.getParam (cutoffParam())->beginChangeGesture();
    proc.getParam (resoParam())->beginChangeGesture();
}

void FilterView::mouseDrag (const juce::MouseEvent& e)
{
    const float dx = (float) e.getDistanceFromDragStartX() / (float) getWidth();
    const float dy = (float) e.getDistanceFromDragStartY() / (float) getHeight();
    proc.getParam (cutoffParam())->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startCut + dx));
    proc.getParam (resoParam())->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startRes - dy));
    const auto& cs = ParamRegistry::get().spec (cutoffParam());
    services.showStatus ("Cutoff " + cs.formatValue (proc.getPlain (cutoffParam())) + "   Resonance " + juce::String (proc.getPlain (resoParam()) * 100.0f, 0) + "%");
}

void FilterView::mouseUp (const juce::MouseEvent&)
{
    proc.getParam (cutoffParam())->endChangeGesture();
    proc.getParam (resoParam())->endChangeGesture();
}

void FilterView::mouseEnter (const juce::MouseEvent&) { services.showStatus (getHelpTitle() + "  -  " + getHelpBody()); }

//==============================================================================
EnvelopeView::EnvelopeView (EditorServices& s) : services (s), proc (s.getProcessor()) {}

float EnvelopeView::timeToWidth (float t) const
{
    const float full = (float) getWidth() * 0.27f;
    return full * std::log1p (std::max (0.0f, t) * 30.0f) / std::log1p (32.0f * 30.0f) + 2.0f;
}

float EnvelopeView::widthToTime (float w) const
{
    const float full = (float) getWidth() * 0.27f;
    const float x = juce::jlimit (0.0f, 1.0f, (w - 2.0f) / full);
    return std::expm1 (x * std::log1p (32.0f * 30.0f)) / 30.0f;
}

EnvelopeView::Geometry EnvelopeView::geometry() const
{
    Geometry g;
    g.area = getLocalBounds().toFloat().reduced (8.0f, 10.0f);
    const float a = proc.getPlain (pid::env (env, Env::attack)), h = proc.getPlain (pid::env (env, Env::hold));
    const float d = proc.getPlain (pid::env (env, Env::decay)), r = proc.getPlain (pid::env (env, Env::release));
    const float s = proc.getPlain (pid::env (env, Env::sustain));
    g.xA = g.area.getX() + timeToWidth (a);
    g.xH = g.xA + timeToWidth (h) * 0.5f;
    g.xD = g.xH + timeToWidth (d);
    g.xS = g.xD + g.area.getWidth() * 0.1f;
    g.xR = g.xS + timeToWidth (r);
    g.ySus = g.area.getBottom() - s * g.area.getHeight();
    g.scale = 1.0f;
    return g;
}

int EnvelopeView::handleAt (juce::Point<float> p) const
{
    const auto g = geometry();
    const float top = g.area.getY(), bottom = g.area.getBottom();
    const juce::Point<float> handles[] = {
        { g.xA, top }, { g.xH, top }, { g.xD, g.ySus }, { g.xR, bottom },
        { (g.area.getX() + g.xA) * 0.5f, (top + bottom) * 0.5f }, { (g.xH + g.xD) * 0.5f, (top + g.ySus) * 0.5f }, { (g.xS + g.xR) * 0.5f, (g.ySus + bottom) * 0.5f } };
    for (int i = 0; i < 7; ++i)
        if (p.getDistanceFrom (handles[i]) < 9.0f) return i;
    return -1;
}

void EnvelopeView::refresh()
{
    std::array<float, 8> now {};
    int k = 0;
    for (auto p : { Env::attack, Env::hold, Env::decay, Env::sustain, Env::release, Env::aCurve, Env::dCurve, Env::rCurve })
        now[(size_t) k++] = proc.getPlain (pid::env (env, p));
    const float lv = proc.feedback.anyVoice.load() ? proc.feedback.envValue[(size_t) env].load() : -1.0f;
    const int st = proc.feedback.envStage[(size_t) env].load();
    if (now != last || std::abs (lv - liveValue) > 0.003f || st != liveStage)
    {
        last = now; liveValue = lv; liveStage = st;
        repaint();
    }
}

void EnvelopeView::paint (juce::Graphics& gr)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (gr, r);
    const auto g = geometry();
    const float top = g.area.getY(), bottom = g.area.getBottom(), h = g.area.getHeight();
    const float ac = proc.getPlain (pid::env (env, Env::aCurve)), dc = proc.getPlain (pid::env (env, Env::dCurve)), rc = proc.getPlain (pid::env (env, Env::rCurve));
    const float s = proc.getPlain (pid::env (env, Env::sustain));

    juce::Path p;
    p.startNewSubPath (g.area.getX(), bottom);
    for (int i = 1; i <= 24; ++i)
    {
        const float t = (float) i / 24.0f;
        p.lineTo (g.area.getX() + (g.xA - g.area.getX()) * t, bottom - h * dsp::shapeCurve (t, ac));
    }
    p.lineTo (g.xH, top);
    for (int i = 1; i <= 24; ++i)
    {
        const float t = (float) i / 24.0f;
        p.lineTo (g.xH + (g.xD - g.xH) * t, top + (h * (1.0f - s)) * dsp::shapeCurve (t, dc));
    }
    p.lineTo (g.xS, g.ySus);
    for (int i = 1; i <= 24; ++i)
    {
        const float t = (float) i / 24.0f;
        p.lineTo (g.xS + (g.xR - g.xS) * t, g.ySus + (bottom - g.ySus) * dsp::shapeCurve (t, rc));
    }
    juce::Path fill (p);
    fill.lineTo (g.xR, bottom);
    fill.closeSubPath();
    const auto c = theme::sourceColour ((int) envSource (env));
    gr.setGradientFill (juce::ColourGradient (c.withAlpha (0.25f), 0, top, c.withAlpha (0.02f), 0, bottom, false));
    gr.fillPath (fill);
    gr.setColour (c);
    gr.strokePath (p, juce::PathStrokeType (1.8f));

    gr.setColour (theme::outline);
    gr.drawVerticalLine ((int) g.xS, top, bottom);

    const juce::Point<float> handles[] = {
        { g.xA, top }, { g.xH, top }, { g.xD, g.ySus }, { g.xR, bottom },
        { (g.area.getX() + g.xA) * 0.5f, bottom - h * dsp::shapeCurve (0.5f, ac) },
        { (g.xH + g.xD) * 0.5f, top + h * (1.0f - s) * dsp::shapeCurve (0.5f, dc) },
        { (g.xS + g.xR) * 0.5f, g.ySus + (bottom - g.ySus) * dsp::shapeCurve (0.5f, rc) } };
    for (int i = 0; i < 7; ++i)
    {
        const float sz = i < 4 ? 7.0f : 5.0f;
        gr.setColour (i == hoverHandle || i == dragHandle ? juce::Colours::white : (i < 4 ? c : c.withAlpha (0.6f)));
        if (i < 4) gr.fillEllipse (juce::Rectangle<float> (sz, sz).withCentre (handles[i]));
        else gr.drawEllipse (juce::Rectangle<float> (sz, sz).withCentre (handles[i]), 1.2f);
    }

    if (liveValue >= 0.0f && liveStage != 0)
    {
        float x = g.xS;
        const auto stage = (Envelope::Stage) liveStage;
        if (stage == Envelope::Stage::attack) x = g.area.getX() + (g.xA - g.area.getX()) * liveValue;
        else if (stage == Envelope::Stage::hold) x = (g.xA + g.xH) * 0.5f;
        else if (stage == Envelope::Stage::decay) x = g.xH + (g.xD - g.xH) * (s < 0.999f ? juce::jlimit (0.0f, 1.0f, (1.0f - liveValue) / (1.0f - s)) : 0.5f);
        else if (stage == Envelope::Stage::sustain) x = (g.xD + g.xS) * 0.5f;
        else if (stage == Envelope::Stage::release) x = g.xS + (g.xR - g.xS) * (s > 0.001f ? juce::jlimit (0.0f, 1.0f, 1.0f - liveValue / std::max (0.001f, s)) : 0.5f);
        gr.setColour (juce::Colours::white);
        gr.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x, bottom - liveValue * h }));
    }

    gr.setFont (theme::label (9.5f));
    gr.setColour (theme::textFaint);
    gr.drawText ("ENV " + juce::String (env + 1) + (env == 0 ? "  (AMP)" : ""), r.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::topRight);
}

void EnvelopeView::mouseMove (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h != hoverHandle) { hoverHandle = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EnvelopeView::mouseDown (const juce::MouseEvent& e)
{
    dragHandle = handleAt (e.position);
    dragStart = e.position;
    const auto g = geometry();
    switch (dragHandle)
    {
        case 0: startA = g.xA - g.area.getX(); break;
        case 1: startA = (g.xH - g.xA) * 2.0f; break;
        case 2: startA = g.xD - g.xH; startB = proc.getPlain (pid::env (env, Env::sustain)); break;
        case 3: startA = g.xR - g.xS; break;
        case 4: startA = proc.getPlain (pid::env (env, Env::aCurve)); break;
        case 5: startA = proc.getPlain (pid::env (env, Env::dCurve)); break;
        case 6: startA = proc.getPlain (pid::env (env, Env::rCurve)); break;
        default: break;
    }
}

void EnvelopeView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragHandle < 0) return;
    const auto g = geometry();
    const float dx = e.position.x - dragStart.x, dy = e.position.y - dragStart.y;
    auto set = [this] (Env p, float v) { proc.setPlain (pid::env (env, p), v); };
    switch (dragHandle)
    {
        case 0: set (Env::attack, widthToTime (startA + dx)); break;
        case 1: set (Env::hold, widthToTime (startA + dx * 2.0f)); break;
        case 2: set (Env::decay, widthToTime (startA + dx)); set (Env::sustain, juce::jlimit (0.0f, 1.0f, startB - dy / g.area.getHeight())); break;
        case 3: set (Env::release, widthToTime (startA + dx)); break;
        case 4: set (Env::aCurve, juce::jlimit (-1.0f, 1.0f, startA - dy / 60.0f)); break;
        case 5: set (Env::dCurve, juce::jlimit (-1.0f, 1.0f, startA + dy / 60.0f)); break;
        case 6: set (Env::rCurve, juce::jlimit (-1.0f, 1.0f, startA + dy / 60.0f)); break;
        default: break;
    }
    const auto& reg = ParamRegistry::get();
    juce::String msg;
    for (auto p : { Env::attack, Env::hold, Env::decay, Env::sustain, Env::release })
        msg << reg.spec (pid::env (env, p)).shortName << " " << reg.spec (pid::env (env, p)).formatValue (proc.getPlain (pid::env (env, p))) << "   ";
    services.showStatus (msg);
    repaint();
}

void EnvelopeView::mouseUp (const juce::MouseEvent&) { dragHandle = -1; repaint(); }
void EnvelopeView::mouseEnter (const juce::MouseEvent&) { services.showStatus (getHelpTitle() + "  -  " + getHelpBody()); }

//==============================================================================
LfoEditor::LfoEditor (EditorServices& s) : services (s), proc (s.getProcessor())
{
    setLfo (0);
}

void LfoEditor::setLfo (int l)
{
    lfo = l;
    points = proc.getLfoShape (l).points();
    lastStructure = proc.structureVersion.load();
    repaint();
}

void LfoEditor::refresh()
{
    if (proc.structureVersion.load() != lastStructure && dragPoint < 0 && dragCurve < 0)
    {
        lastStructure = proc.structureVersion.load();
        points = proc.getLfoShape (lfo).points();
        repaint();
    }
    const float ph = proc.feedback.anyVoice.load() || proc.getPlain (pid::lfo (lfo, Lfo::mode)) == (float) (int) LfoMode::free
                       ? proc.feedback.lfoPhase[(size_t) lfo].load() : -1.0f;
    if (std::abs (ph - phase) > 0.001f) { phase = ph; repaint(); }
}

juce::Rectangle<float> LfoEditor::plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 10.0f); }

juce::Point<float> LfoEditor::toScreen (float x, float y) const
{
    const auto a = plotArea();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}

juce::Point<float> LfoEditor::fromScreen (juce::Point<float> p) const
{
    const auto a = plotArea();
    return { juce::jlimit (0.0f, 1.0f, (p.x - a.getX()) / a.getWidth()), juce::jlimit (0.0f, 1.0f, (a.getBottom() - p.y) / a.getHeight()) };
}

void LfoEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    const auto a = plotArea();
    g.setColour (theme::outline.withAlpha (0.5f));
    for (int i = 1; i < grid; ++i) g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / (float) grid), a.getY(), a.getBottom());
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.0f), a.getX(), a.getRight());

    const auto type = (LfoType) (int) proc.getPlain (pid::lfo (lfo, Lfo::type));
    const auto c = theme::mod;
    if (type != LfoType::shape)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::value (13.0f));
        g.drawText (lfoTypeNames()[(int) type] + " generator (shape not used)", a.toNearestInt(), juce::Justification::centred);
    }
    else
    {
        juce::Path p;
        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            const auto& pa = points[i];
            const auto& pb = points[i + 1];
            for (int k = 0; k <= 20; ++k)
            {
                const float t = (float) k / 20.0f;
                const float x = pa.x + (pb.x - pa.x) * t;
                const float y = pa.y + (pb.y - pa.y) * LfoShape::segmentCurve (t, pa.curve);
                const auto s = toScreen (x, y);
                if (i == 0 && k == 0) p.startNewSubPath (s); else p.lineTo (s);
            }
        }
        juce::Path fill (p);
        fill.lineTo (toScreen (1.0f, 0.0f));
        fill.lineTo (toScreen (0.0f, 0.0f));
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.25f), 0, a.getY(), c.withAlpha (0.02f), 0, a.getBottom(), false));
        g.fillPath (fill);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.8f));

        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto s = toScreen (points[i].x, points[i].y);
            g.setColour ((int) i == hoverPoint || (int) i == dragPoint ? juce::Colours::white : c.brighter (0.3f));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (s));
            if (i + 1 < points.size())
            {
                const auto& pa = points[i];
                const auto& pb = points[i + 1];
                const auto m = toScreen ((pa.x + pb.x) * 0.5f, pa.y + (pb.y - pa.y) * LfoShape::segmentCurve (0.5f, pa.curve));
                g.setColour ((int) i == hoverCurve || (int) i == dragCurve ? juce::Colours::white : c.withAlpha (0.6f));
                g.drawEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (m), 1.2f);
            }
        }
    }

    if (phase >= 0.0f)
    {
        const float x = phase - std::floor (phase);
        const auto top = toScreen (x, 1.0f), bot = toScreen (x, 0.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (top.x, top.y, bot.x, bot.y, 1.0f);
        const float v = proc.feedback.lfoValue[(size_t) lfo].load();
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (toScreen (x, v)));
    }
}

void LfoEditor::mouseMove (const juce::MouseEvent& e)
{
    int hp = -1, hc = -1;
    for (size_t i = 0; i < points.size(); ++i)
    {
        if (e.position.getDistanceFrom (toScreen (points[i].x, points[i].y)) < 8.0f) { hp = (int) i; break; }
        if (i + 1 < points.size())
        {
            const auto& pa = points[i];
            const auto& pb = points[i + 1];
            if (e.position.getDistanceFrom (toScreen ((pa.x + pb.x) * 0.5f, pa.y + (pb.y - pa.y) * LfoShape::segmentCurve (0.5f, pa.curve))) < 7.0f)
                hc = (int) i;
        }
    }
    if (hp != hoverPoint || hc != hoverCurve) { hoverPoint = hp; hoverCurve = hc; repaint(); }
}

void LfoEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m, presets;
        int id = 1;
        for (auto& n : LfoShape::presetNames()) presets.addItem (id++, n);
        m.addSubMenu ("Load shape", presets);
        m.addSeparator();
        m.addItem (100, "Flip horizontally");
        m.addItem (101, "Flip vertically");
        m.addItem (102, "Reset (triangle)");
        juce::PopupMenu gridMenu;
        for (int gsz : { 2, 4, 8, 16, 32 }) gridMenu.addItem (200 + gsz, "Grid " + juce::String (gsz), true, grid == gsz);
        m.addSubMenu ("Grid", gridMenu);
        m.addItem (300, "Help: LFO editor");
        juce::Component::SafePointer<LfoEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe;
            const auto names = LfoShape::presetNames();
            if (r >= 1 && r <= (int) names.size()) ed.points = LfoShape::preset (names[(size_t) r - 1]).points();
            else if (r == 100) { for (auto& p : ed.points) p.x = 1.0f - p.x; std::reverse (ed.points.begin(), ed.points.end());
                                  for (size_t i = 0; i + 1 < ed.points.size(); ++i) ed.points[i].curve = 1.0f - ed.points[i + 1].curve; }
            else if (r == 101) { for (auto& p : ed.points) { p.y = 1.0f - p.y; p.curve = 1.0f - p.curve; } }
            else if (r == 102) ed.points = LfoShape().points();
            else if (r > 200 && r < 300) { ed.grid = r - 200; ed.repaint(); return; }
            else if (r == 300) { ed.services.openHelp ("lfos"); return; }
            ed.commit (true);
        });
        return;
    }
    dragPoint = hoverPoint;
    dragCurve = dragPoint < 0 ? hoverCurve : -1;
    if (dragCurve >= 0) { curveStart = points[(size_t) dragCurve].curve; dragStartY = e.position.y; }
}

void LfoEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragPoint >= 0)
    {
        auto p = fromScreen (e.position);
        if (! e.mods.isShiftDown())
        {
            p.x = std::round (p.x * (float) grid) / (float) grid;
            p.y = std::round (p.y * 8.0f) / 8.0f;
        }
        auto& pt = points[(size_t) dragPoint];
        const bool first = dragPoint == 0, last = dragPoint == (int) points.size() - 1;
        if (first) pt.x = 0.0f;
        else if (last) pt.x = 1.0f;
        else pt.x = juce::jlimit (points[(size_t) dragPoint - 1].x, points[(size_t) dragPoint + 1].x, p.x);
        pt.y = p.y;
        commit (false);
        repaint();
    }
    else if (dragCurve >= 0)
    {
        const auto& pa = points[(size_t) dragCurve];
        const auto& pb = points[(size_t) dragCurve + 1];
        const float dir = pb.y >= pa.y ? 1.0f : -1.0f;
        points[(size_t) dragCurve].curve = juce::jlimit (0.02f, 0.98f, curveStart + dir * (dragStartY - e.position.y) / 120.0f);
        commit (false);
        repaint();
    }
}

void LfoEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragPoint >= 0 || dragCurve >= 0) commit (true);
    dragPoint = dragCurve = -1;
}

void LfoEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (hoverPoint > 0 && hoverPoint < (int) points.size() - 1)
        points.erase (points.begin() + hoverPoint);
    else if (hoverPoint < 0)
    {
        auto p = fromScreen (e.position);
        if (! e.mods.isShiftDown()) p.x = std::round (p.x * (float) grid) / (float) grid;
        if (p.x <= 0.0f || p.x >= 1.0f) return;
        auto it = std::find_if (points.begin(), points.end(), [&] (const LfoPoint& q) { return q.x > p.x; });
        points.insert (it, { p.x, p.y, 0.5f });
    }
    hoverPoint = -1;
    commit (true);
    repaint();
}

void LfoEditor::commit (bool final)
{
    const auto now = juce::Time::getMillisecondCounter();
    if (! final && now - lastCommit < 40) return;
    lastCommit = now;
    proc.setLfoShape (lfo, LfoShape (points, "Custom"));
    lastStructure = proc.structureVersion.load();
}

void LfoEditor::mouseEnter (const juce::MouseEvent&) { services.showStatus (getHelpTitle() + "  -  " + getHelpBody()); }

//==============================================================================
void ScopeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    const auto& buf = proc.feedback.scope;
    const int size = CureumProcessor::Feedback::scopeSize;
    const int pos = proc.feedback.scopePos.load();
    // find a rising zero crossing to stabilise the display
    int start = (pos - 1024 + size) % size;
    for (int i = 0; i < 512; ++i)
    {
        const int a = (start + i) % size, b = (start + i + 1) % size;
        if (buf[(size_t) a] <= 0.0f && buf[(size_t) b] > 0.0f) { start = b; break; }
    }
    juce::Path p;
    const int n = 512;
    for (int i = 0; i < n; ++i)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, buf[(size_t) ((start + i) % size)]);
        const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
        const float y = r.getCentreY() - v * r.getHeight() * 0.42f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (theme::accent.withAlpha (0.2f));
    g.strokePath (p, juce::PathStrokeType (4.0f));
    g.setColour (theme::accent);
    g.strokePath (p, juce::PathStrokeType (1.4f));
}

} // namespace cureum
