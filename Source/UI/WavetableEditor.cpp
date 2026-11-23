#include "WavetableEditor.h"
#include "../Core/Formula.h"
#include "../Core/Settings.h"
#include "../Presets/WavetableIO.h"

namespace cureum
{

constexpr int kSize = Wavetable::kFrameSize;
constexpr int kHarmonics = 64;

WavetableEditor::WavetableEditor (EditorServices& s, int o) : services (s), proc (s.getProcessor()), osc (o)
{
    if (auto t = proc.getWavetable (o))
    {
        name = t->getName();
        for (int f = 0; f < t->getNumFrames(); ++f)
            frames.emplace_back (t->getFrame (f), t->getFrame (f) + kSize);
    }
    if (frames.empty()) frames.emplace_back (kSize, 0.0f);
    rebuildHarmonics();

    formula.setText ("sin(pi*x*(1+y*7))", false);
    formula.setFont (theme::value (14.0f));
    frameCount.setText ("64", false);
    frameCount.setInputRestrictions (3, "0123456789");
    frameCount.setFont (theme::value (14.0f));
    frameCount.setJustification (juce::Justification::centred);
    for (auto* c : std::initializer_list<juce::Component*> { &formula, &frameCount, &applyFormulaButton, &addFrame, &deleteFrame, &smooth, &normalise,
                                                              &invert, &process, &importButton, &exportButton, &applyButton, &cancelButton })
        addAndMakeVisible (c);
    applyButton.setColour (juce::TextButton::buttonColourId, theme::accentDeep);

    applyFormulaButton.onClick = [this] { applyFormula(); };
    formula.onReturnKey = [this] { applyFormula(); };
    addFrame.onClick = [this]
    {
        if (frames.size() >= 256) return;
        frames.insert (frames.begin() + selected + 1, frames[(size_t) selected]);
        ++selected; rebuildHarmonics(); repaint();
    };
    deleteFrame.onClick = [this]
    {
        if (frames.size() <= 1) return;
        frames.erase (frames.begin() + selected);
        selected = std::min (selected, (int) frames.size() - 1); rebuildHarmonics(); repaint();
    };
    smooth.onClick = [this]
    {
        auto& f = frames[(size_t) selected];
        std::vector<float> c (f);
        for (int i = 0; i < kSize; ++i)
            f[(size_t) i] = 0.25f * c[(size_t) ((i - 1 + kSize) % kSize)] + 0.5f * c[(size_t) i] + 0.25f * c[(size_t) ((i + 1) % kSize)];
        rebuildHarmonics(); repaint();
    };
    normalise.onClick = [this] { normaliseAll(); };
    invert.onClick = [this] { for (auto& f : frames) for (auto& v : f) v = -v; rebuildHarmonics(); repaint(); };
    process.onClick = [this] { showProcessMenu(); };
    importButton.onClick = [this] { importFile(); };
    exportButton.onClick = [this] { exportFile(); };
    applyButton.onClick = [this] { apply(); if (onClose) onClose(); };
    cancelButton.onClick = [this] { if (onClose) onClose(); };
    setWantsKeyboardFocus (true);
}

juce::Rectangle<int> WavetableEditor::stripArea() const     { return getLocalBounds().reduced (24).withTrimmedTop (40).removeFromTop (64); }
juce::Rectangle<int> WavetableEditor::waveArea() const      { auto r = getLocalBounds().reduced (24).withTrimmedTop (114).withTrimmedRight (250); return r.removeFromTop (r.getHeight() * 64 / 100); }
juce::Rectangle<int> WavetableEditor::harmonicsArea() const { auto r = getLocalBounds().reduced (24).withTrimmedTop (114).withTrimmedRight (250); r.removeFromTop (r.getHeight() * 64 / 100 + 10); return r; }

void WavetableEditor::rebuildHarmonics()
{
    harmonics = Wavetable::harmonicMagnitudes (frames[(size_t) selected].data(), kHarmonics);
}

void WavetableEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background.withAlpha (0.98f));
    auto r = getLocalBounds().reduced (12);
    theme::drawPanel (g, r.toFloat());
    drawSectionTitle (g, "WAVETABLE EDITOR  -  OSC " + oscLetter (osc) + "  -  " + name + "  (" + juce::String (frames.size()) + " frames)",
                      r.reduced (12, 0).removeFromTop (40), theme::text);

    // frame strip
    auto strip = stripArea();
    g.setColour (juce::Colour (0xff080a10));
    g.fillRoundedRectangle (strip.toFloat(), 5.0f);
    const int n = (int) frames.size();
    const float cw = (float) strip.getWidth() / (float) std::max (1, n);
    for (int f = 0; f < n; ++f)
    {
        auto cell = juce::Rectangle<float> ((float) strip.getX() + cw * (float) f, (float) strip.getY(), cw, (float) strip.getHeight()).reduced (cw > 6 ? 1.0f : 0.0f, 4.0f);
        if (f == selected) { g.setColour (theme::accent.withAlpha (0.18f)); g.fillRect (cell); }
        juce::Path p;
        const auto& fr = frames[(size_t) f];
        const int pts = std::max (8, std::min (64, (int) cell.getWidth()));
        for (int i = 0; i < pts; ++i)
        {
            const float x = cell.getX() + cell.getWidth() * (float) i / (float) (pts - 1);
            const float y = cell.getCentreY() - fr[(size_t) (i * (kSize - 1) / (pts - 1))] * cell.getHeight() * 0.42f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (f == selected ? theme::accent : theme::accentDeep.withAlpha (0.7f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    // waveform
    auto wa = waveArea().toFloat();
    g.setColour (juce::Colour (0xff080a10));
    g.fillRoundedRectangle (wa, 6.0f);
    g.setColour (theme::outline);
    g.drawHorizontalLine ((int) wa.getCentreY(), wa.getX(), wa.getRight());
    for (int i = 1; i < 4; ++i) g.drawVerticalLine ((int) (wa.getX() + wa.getWidth() * (float) i / 4.0f), wa.getY(), wa.getBottom());
    juce::Path wave;
    const auto& cur = frames[(size_t) selected];
    for (int i = 0; i < 512; ++i)
    {
        const float x = wa.getX() + wa.getWidth() * (float) i / 511.0f;
        const float y = wa.getCentreY() - juce::jlimit (-1.2f, 1.2f, cur[(size_t) (i * 4)]) * wa.getHeight() * 0.45f;
        if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
    }
    g.setColour (theme::accent.withAlpha (0.2f));
    g.strokePath (wave, juce::PathStrokeType (5.0f));
    g.setColour (theme::accent);
    g.strokePath (wave, juce::PathStrokeType (1.8f));
    g.setFont (theme::label (10.0f));
    g.setColour (theme::textFaint);
    g.drawText ("FRAME " + juce::String (selected + 1) + "  -  draw with the mouse (Shift = line)", wa.reduced (8.0f, 6.0f).toNearestInt(), juce::Justification::topLeft);

    // harmonics
    auto ha = harmonicsArea().toFloat();
    g.setColour (juce::Colour (0xff080a10));
    g.fillRoundedRectangle (ha, 6.0f);
    const float bw = ha.getWidth() / (float) kHarmonics;
    float maxMag = 1.0e-6f;
    for (auto m : harmonics) maxMag = std::max (maxMag, m);
    for (int h = 0; h < kHarmonics; ++h)
    {
        const float m = juce::jlimit (0.0f, 1.0f, harmonics[(size_t) h] / std::max (maxMag, 0.25f));
        auto bar = juce::Rectangle<float> (ha.getX() + bw * (float) h + 1.0f, ha.getBottom() - m * (ha.getHeight() - 16.0f) - 2.0f, bw - 2.0f, m * (ha.getHeight() - 16.0f));
        g.setColour (theme::mod.withAlpha (0.85f));
        g.fillRect (bar);
    }
    g.setColour (theme::textFaint);
    g.drawText ("HARMONICS 1-64  -  drag to edit", ha.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::topLeft);

    // side panel labels
    auto side = getLocalBounds().reduced (24).withTrimmedTop (114).removeFromRight (234);
    g.setColour (theme::textDim);
    g.setFont (theme::label (10.5f));
    g.drawText ("FORMULA", side.removeFromTop (16), juce::Justification::centredLeft);
    side.removeFromTop (30);
    g.drawText ("FRAMES TO GENERATE", side.removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (theme::textFaint);
    g.setFont (theme::value (11.0f));
    g.drawFittedText ("Variables: x (-1..1), w (0..1), y (frame 0..1), n, N.  Functions: sin cos tanh abs pow min max saw square tri noise rand mix clamp...  See Help > Wavetable editor.",
                      side.withTrimmedTop (70).removeFromTop (80), juce::Justification::topLeft, 6);
}

void WavetableEditor::resized()
{
    auto side = getLocalBounds().reduced (24).withTrimmedTop (114).removeFromRight (234);
    side.removeFromTop (16);
    formula.setBounds (side.removeFromTop (26));
    side.removeFromTop (20);
    auto fc = side.removeFromTop (26);
    frameCount.setBounds (fc.removeFromLeft (70));
    fc.removeFromLeft (6);
    applyFormulaButton.setBounds (fc);
    side.removeFromTop (100);
    for (auto* b : { &addFrame, &deleteFrame, &smooth, &normalise, &invert, &process, &importButton, &exportButton })
    {
        b->setBounds (side.removeFromTop (26));
        side.removeFromTop (5);
    }
    auto bottom = side.removeFromBottom (30);
    cancelButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 3));
    bottom.removeFromLeft (6);
    applyButton.setBounds (bottom);
}

void WavetableEditor::drawAt (juce::Point<float> p, bool line)
{
    const auto wa = waveArea().toFloat();
    auto toIndex = [&] (float x) { return juce::jlimit (0, kSize - 1, (int) ((x - wa.getX()) / wa.getWidth() * (float) kSize)); };
    auto toValue = [&] (float y) { return juce::jlimit (-1.0f, 1.0f, (wa.getCentreY() - y) / (wa.getHeight() * 0.45f)); };
    auto& f = frames[(size_t) selected];
    const auto from = line ? downPoint : lastPoint;
    if (line) std::copy (undoFrame.begin(), undoFrame.end(), f.begin());
    const int i0 = toIndex (from.x), i1 = toIndex (p.x);
    const float v0 = toValue (from.y), v1 = toValue (p.y);
    const int a = std::min (i0, i1), b = std::max (i0, i1);
    for (int i = a; i <= b; ++i)
    {
        const float t = b == a ? 1.0f : (float) (i - i0) / (float) (i1 - i0);
        f[(size_t) i] = v0 + (v1 - v0) * juce::jlimit (0.0f, 1.0f, t);
    }
    lastPoint = p;
}

void WavetableEditor::setHarmonicAt (juce::Point<float> p)
{
    const auto ha = harmonicsArea().toFloat();
    const int h = juce::jlimit (0, kHarmonics - 1, (int) ((p.x - ha.getX()) / ha.getWidth() * (float) kHarmonics));
    float maxMag = 0.25f;
    for (auto m : harmonics) maxMag = std::max (maxMag, m);
    harmonics[(size_t) h] = juce::jlimit (0.0f, 1.0f, (ha.getBottom() - p.y) / (ha.getHeight() - 16.0f)) * maxMag;
}

void WavetableEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    downPoint = lastPoint = p;
    if (stripArea().contains (e.getPosition()))
    {
        drag = Drag::strip;
        selected = juce::jlimit (0, (int) frames.size() - 1, (int) ((p.x - (float) stripArea().getX()) / (float) stripArea().getWidth() * (float) frames.size()));
        rebuildHarmonics();
    }
    else if (waveArea().contains (e.getPosition()))
    {
        drag = Drag::wave;
        undoFrame = frames[(size_t) selected];
        drawAt (p, e.mods.isShiftDown());
    }
    else if (harmonicsArea().contains (e.getPosition()))
    {
        drag = Drag::harmonics;
        setHarmonicAt (p);
    }
    repaint();
}

void WavetableEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::wave) drawAt (e.position, e.mods.isShiftDown());
    else if (drag == Drag::harmonics) setHarmonicAt (e.position);
    else if (drag == Drag::strip)
    {
        selected = juce::jlimit (0, (int) frames.size() - 1, (int) ((e.position.x - (float) stripArea().getX()) / (float) stripArea().getWidth() * (float) frames.size()));
        rebuildHarmonics();
    }
    repaint();
}

void WavetableEditor::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::harmonics)
    {
        Wavetable::framesFromHarmonics (harmonics.data(), nullptr, kHarmonics, frames[(size_t) selected].data());
        rebuildHarmonics();
    }
    else if (drag == Drag::wave)
        rebuildHarmonics();
    drag = Drag::none;
    repaint();
}

void WavetableEditor::applyFormula()
{
    Formula f;
    juce::String err;
    if (! f.compile (formula.getText(), err))
    {
        services.showStatus ("Formula error: " + err, true);
        return;
    }
    const int count = juce::jlimit (1, 256, frameCount.getText().getIntValue());
    const auto data = f.render (count);
    frames.clear();
    for (int i = 0; i < count; ++i)
        frames.emplace_back (data.begin() + i * kSize, data.begin() + (i + 1) * kSize);
    selected = 0;
    name = "Formula";
    normaliseAll();
    services.showStatus ("Generated " + juce::String (count) + " frames from the formula.", true);
}

void WavetableEditor::normaliseAll()
{
    float peak = 0.0f;
    for (auto& f : frames) for (auto v : f) peak = std::max (peak, std::abs (v));
    if (peak > 1.0e-6f) for (auto& f : frames) for (auto& v : f) v /= peak;
    rebuildHarmonics();
    repaint();
}

void WavetableEditor::showProcessMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Crossfade first to last frame (64 frames)");
    m.addItem (2, "Spectral morph first to last frame (64 frames)");
    m.addItem (3, "Reverse frame order");
    m.addItem (4, "Mirror table (forward + backward)");
    m.addItem (5, "Normalise each frame");
    m.addItem (6, "Remove DC");
    juce::Component::SafePointer<WavetableEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&process), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& ed = *safe;
        auto& fr = ed.frames;
        if ((r == 1 || r == 2) && fr.size() >= 1)
        {
            const auto a = fr.front(), b = fr.back();
            fr.clear();
            const auto ma = Wavetable::harmonicMagnitudes (a.data(), 512), mb = Wavetable::harmonicMagnitudes (b.data(), 512);
            for (int k = 0; k < 64; ++k)
            {
                const float t = (float) k / 63.0f;
                std::vector<float> f (kSize);
                if (r == 1) for (int i = 0; i < kSize; ++i) f[(size_t) i] = a[(size_t) i] + (b[(size_t) i] - a[(size_t) i]) * t;
                else
                {
                    std::vector<float> mags (512);
                    for (int h = 0; h < 512; ++h) mags[(size_t) h] = ma[(size_t) h] + (mb[(size_t) h] - ma[(size_t) h]) * t;
                    Wavetable::framesFromHarmonics (mags.data(), nullptr, 512, f.data());
                }
                fr.push_back (std::move (f));
            }
        }
        else if (r == 3) std::reverse (fr.begin(), fr.end());
        else if (r == 4) { auto copy = fr; for (auto it = copy.rbegin(); it != copy.rend() && fr.size() < 256; ++it) fr.push_back (*it); }
        else if (r == 5) for (auto& f : fr) { float p = 0; for (auto v : f) p = std::max (p, std::abs (v)); if (p > 1e-6f) for (auto& v : f) v /= p; }
        else if (r == 6) for (auto& f : fr) { double s = 0; for (auto v : f) s += v; const auto dc = (float) (s / kSize); for (auto& v : f) v -= dc; }
        ed.selected = juce::jlimit (0, (int) fr.size() - 1, ed.selected);
        ed.rebuildHarmonics();
        ed.repaint();
    });
}

void WavetableEditor::importFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Import wavetable or audio", Settings::get().getUserWavetableFolder(), "*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<WavetableEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        auto r = WavetableIO::loadFile (fc.getResult());
        if (! r.ok()) { safe->services.showStatus ("Import failed: " + r.error, true); return; }
        safe->frames.clear();
        for (int i = 0; i < r.numFrames; ++i)
            safe->frames.emplace_back (r.frames.begin() + i * kSize, r.frames.begin() + (i + 1) * kSize);
        safe->name = fc.getResult().getFileNameWithoutExtension();
        safe->selected = 0;
        safe->normaliseAll();
    });
}

void WavetableEditor::exportFile()
{
    std::vector<float> flat;
    for (auto& f : frames) flat.insert (flat.end(), f.begin(), f.end());
    auto table = Wavetable::create (name, flat, Wavetable::Options { false, false, true });
    chooser = std::make_unique<juce::FileChooser> ("Export wavetable", Settings::get().getUserWavetableFolder().getChildFile (name + ".wav"), "*.wav");
    juce::Component::SafePointer<WavetableEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [safe, table] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        safe->services.showStatus (WavetableIO::saveWav (fc.getResult().withFileExtension ("wav"), *table) ? "Wavetable exported." : "Export failed.", true);
    });
}

void WavetableEditor::apply()
{
    std::vector<float> flat;
    for (auto& f : frames) flat.insert (flat.end(), f.begin(), f.end());
    Wavetable::Options o;
    o.normalise = false;
    proc.setWavetable (osc, Wavetable::create (name.isNotEmpty() ? name : juce::String ("Edited"), std::move (flat), o));
    services.showStatus ("Wavetable applied to Osc " + oscLetter (osc) + ".", true);
}

bool WavetableEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

} // namespace cureum
