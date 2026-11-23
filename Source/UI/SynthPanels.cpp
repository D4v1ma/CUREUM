#include "SynthPanels.h"
#include "../DSP/WavetableFactory.h"
#include "../Presets/WavetableIO.h"
#include "../Core/Settings.h"

namespace cureum
{

using namespace pid;

//==============================================================================
SectionPanel::SectionPanel (EditorServices& s, const juce::String& t, int enableParam)
    : services (s), proc (s.getProcessor()), title (t)
{
    setName (t);
    if (enableParam >= 0)
    {
        power = std::make_unique<ParamToggle> (s, enableParam, ParamToggle::Style::power);
        addAndMakeVisible (*power);
    }
}

juce::Rectangle<int> SectionPanel::getHeaderArea() const  { return getLocalBounds().removeFromTop (26).reduced (8, 0); }
juce::Rectangle<int> SectionPanel::getContentArea() const { return getLocalBounds().withTrimmedTop (28).reduced (8, 6); }

void SectionPanel::paint (juce::Graphics& g)
{
    theme::drawPanel (g, getLocalBounds().toFloat());
    auto h = getHeaderArea();
    g.setColour (theme::outline.withAlpha (0.6f));
    g.drawHorizontalLine (26, 8.0f, (float) getWidth() - 8.0f);
    if (title.isNotEmpty())
    {
        if (power != nullptr) h.removeFromLeft (22);
        g.setFont (theme::title (12.5f));
        const bool off = power != nullptr && proc.getPlain (power->getParamIndex()) < 0.5f;
        g.setColour (off ? theme::textDim : titleColour);
        g.drawText (title, h, juce::Justification::centredLeft);
    }
}

void SectionPanel::resized()
{
    if (power != nullptr)
        power->setBounds (getHeaderArea().removeFromLeft (18).withSizeKeepingCentre (17, 17));
    layoutContent (getContentArea());
}

//==============================================================================
juce::PopupMenu buildWarpMenu (int current)
{
    using W = WarpMode;
    juce::PopupMenu m;
    auto add = [&] (juce::PopupMenu& menu, W w) { menu.addItem ((int) w + 1, warpModeNames()[(int) w], true, (int) w == current); };
    add (m, W::off);
    juce::PopupMenu sync, bend, shape, remap, cross, noiseSub;
    for (auto w : { W::sync, W::syncWindow }) add (sync, w);
    for (auto w : { W::bendPlus, W::bendMinus, W::bendPlusMinus, W::asymPlus, W::asymMinus, W::asymPlusMinus }) add (bend, w);
    for (auto w : { W::pwm, W::flip, W::mirror, W::quantize, W::squash, W::fold, W::phaseDistort, W::bitCrush }) add (shape, w);
    for (auto w : { W::remap1, W::remap2, W::remap3, W::remap4 }) add (remap, w);
    for (auto w : { W::fmOsc, W::pmOsc, W::amOsc, W::rmOsc, W::fmA, W::fmB, W::fmC, W::pmA, W::pmB, W::pmC, W::pmSelf }) add (cross, w);
    for (auto w : { W::fmNoise, W::amNoise, W::rmNoise, W::fmSub, W::amSub, W::rmSub }) add (noiseSub, w);
    m.addSubMenu ("Sync", sync);
    m.addSubMenu ("Bend / Asym", bend);
    m.addSubMenu ("Shape", shape);
    m.addSubMenu ("Remap", remap);
    m.addSubMenu ("Osc FM / PM / AM / RM", cross);
    m.addSubMenu ("Noise / Sub modulation", noiseSub);
    return m;
}

juce::PopupMenu buildFilterTypeMenu (int current)
{
    juce::PopupMenu m;
    juce::StringArray cats;
    for (auto& t : filterTypes()) cats.addIfNotAlreadyThere (t.category);
    for (auto& c : cats)
    {
        juce::PopupMenu sub;
        bool ticked = false;
        for (int i = 0; i < numFilterTypes(); ++i)
            if (juce::String (filterTypes()[(size_t) i].category) == c)
            {
                sub.addItem (i + 1, filterTypes()[(size_t) i].name, true, i == current);
                ticked = ticked || i == current;
            }
        m.addSubMenu (c, sub, true, nullptr, ticked);
    }
    return m;
}

//==============================================================================
OscPanel::OscPanel (EditorServices& s, int o)
    : SectionPanel (s, "OSC " + oscLetter (o), pid::osc (o, Osc::enable)), osc (o), view (s, o),
      warp1 (s, pid::osc (o, Osc::warp1Mode), [] (juce::PopupMenu& m, int c) { m = buildWarpMenu (c); }),
      warp2 (s, pid::osc (o, Osc::warp2Mode), [] (juce::PopupMenu& m, int c) { m = buildWarpMenu (c); })
{
    addAndMakeVisible (view);
    view.onDoubleClick = [this] { if (onEditWavetable) onEditWavetable (osc); };
    addAndMakeVisible (tableButton);
    tableButton.onClick = [this] { showTableMenu(); };
    for (auto* b : { &prev, &next, &edit, &gear }) addAndMakeVisible (b);
    prev.onClick = [this] { stepTable (-1); };
    next.onClick = [this] { stepTable (1); };
    edit.onClick = [this] { if (onEditWavetable) onEditWavetable (osc); };
    gear.onClick = [this] { showAdvanced(); };
    warp1.setPrefix ("WARP 1");
    warp2.setPrefix ("WARP 2");
    addAndMakeVisible (warp1);
    addAndMakeVisible (warp2);
    for (auto p : { Osc::wtPos, Osc::warp1Amt, Osc::warp2Amt, Osc::level, Osc::pan,
                    Osc::unison, Osc::detune, Osc::blend, Osc::width, Osc::phase,
                    Osc::octave, Osc::semi, Osc::fine, Osc::coarse, Osc::randPhase })
    {
        knobs.push_back (std::make_unique<ModKnob> (s, pid::osc (o, p), ModKnob::Style::normal));
        addAndMakeVisible (*knobs.back());
    }
}

void OscPanel::paint (juce::Graphics& g)
{
    SectionPanel::paint (g);
    if (auto t = proc.getWavetable (osc))
        if (tableButton.getButtonText() != t->getName())
            tableButton.setButtonText (t->getName());
}

void OscPanel::layoutContent (juce::Rectangle<int> r)
{
    auto h = getHeaderArea();
    h.removeFromLeft (70);
    gear.setBounds (h.removeFromRight (22).withSizeKeepingCentre (20, 20));
    edit.setBounds (h.removeFromRight (22).withSizeKeepingCentre (20, 20));
    next.setBounds (h.removeFromRight (18).withSizeKeepingCentre (16, 16));
    prev.setBounds (h.removeFromRight (18).withSizeKeepingCentre (16, 16));
    tableButton.setBounds (h.reduced (2, 3));

    view.setBounds (r.removeFromTop (juce::jmax (80, r.getHeight() - 3 * 56 - 30)));
    r.removeFromTop (5);
    auto wr = r.removeFromTop (22);
    warp1.setBounds (wr.removeFromLeft (wr.getWidth() / 2 - 2));
    wr.removeFromLeft (4);
    warp2.setBounds (wr);
    r.removeFromTop (3);
    const int rowH = r.getHeight() / 3;
    for (int row = 0; row < 3; ++row)
    {
        auto rr = r.removeFromTop (rowH);
        const int w = rr.getWidth() / 5;
        for (int k = 0; k < 5; ++k)
            knobs[(size_t) (row * 5 + k)]->setBounds (rr.removeFromLeft (w).reduced (1, 1));
    }
}

void OscPanel::stepTable (int dir)
{
    auto t = proc.getWavetable (osc);
    const auto& names = WavetableFactory::names();
    int i = t != nullptr ? names.indexOf (t->getName()) : -1;
    i = (i + dir + names.size()) % names.size();
    proc.setWavetable (osc, WavetableFactory::get (names[i]));
    services.showStatus ("Osc " + oscLetter (osc) + " wavetable: " + names[i], true);
    repaint();
}

void OscPanel::showTableMenu()
{
    juce::PopupMenu m, factory, user, serum;
    const auto current = proc.getWavetable (osc);
    for (int i = 0; i < WavetableFactory::names().size(); ++i)
        factory.addItem (i + 1, WavetableFactory::names()[i], true, current != nullptr && current->getName() == WavetableFactory::names()[i]);
    m.addSubMenu ("Factory", factory);

    auto files = std::make_shared<juce::Array<juce::File>>();
    for (const auto& e : juce::RangedDirectoryIterator (Settings::get().getUserWavetableFolder(), true, "*.wav", juce::File::findFiles))
    {
        files->add (e.getFile());
        user.addItem (1000 + files->size() - 1, e.getFile().getFileNameWithoutExtension());
    }
    m.addSubMenu ("User Wavetables", user, user.getNumItems() > 0);

    std::map<juce::String, juce::PopupMenu> serumFolders;
    int count = 0;
    for (auto& root : Settings::get().getSerumFolders())
    {
        juce::File tables = juce::File (root).getChildFile ("Tables");
        if (! tables.isDirectory()) tables = juce::File (root);
        for (const auto& e : juce::RangedDirectoryIterator (tables, true, "*.wav", juce::File::findFiles))
        {
            if (++count > 1500) break;
            files->add (e.getFile());
            serumFolders[e.getFile().getParentDirectory().getFileName()].addItem (1000 + files->size() - 1, e.getFile().getFileNameWithoutExtension());
        }
    }
    for (auto& [folder, menu] : serumFolders) serum.addSubMenu (folder, menu);
    m.addSubMenu ("Serum Tables", serum, ! serumFolders.empty());
    m.addSeparator();
    m.addItem (9001, "Import wavetable / audio...");
    m.addItem (9002, "Export wavetable as WAV...");
    m.addItem (9003, "Edit wavetable...");

    juce::Component::SafePointer<OscPanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&tableButton), [safe, files] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = *safe;
        if (r < 1000) p.proc.setWavetable (p.osc, WavetableFactory::get (WavetableFactory::names()[r - 1]));
        else if (r < 9000)
        {
            juce::String err;
            if (auto t = WavetableIO::loadWavetable ((*files)[r - 1000], err)) p.proc.setWavetable (p.osc, t);
            else p.services.showStatus ("Could not load wavetable: " + err, true);
        }
        else if (r == 9001) p.importTable();
        else if (r == 9002) p.exportTable();
        else if (r == 9003 && p.onEditWavetable) p.onEditWavetable (p.osc);
        p.repaint();
    });
}

void OscPanel::importTable()
{
    chooser = std::make_unique<juce::FileChooser> ("Import wavetable or audio", Settings::get().getUserWavetableFolder(), "*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<OscPanel> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        juce::String err;
        if (auto t = WavetableIO::loadWavetable (fc.getResult(), err))
        {
            safe->proc.setWavetable (safe->osc, t);
            safe->services.showStatus ("Imported " + t->getName() + " (" + juce::String (t->getNumFrames()) + " frames)", true);
        }
        else
            safe->services.showStatus ("Import failed: " + err, true);
        safe->repaint();
    });
}

void OscPanel::exportTable()
{
    auto t = proc.getWavetable (osc);
    if (t == nullptr) return;
    chooser = std::make_unique<juce::FileChooser> ("Export wavetable", Settings::get().getUserWavetableFolder().getChildFile (t->getName() + ".wav"), "*.wav");
    juce::Component::SafePointer<OscPanel> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [safe, t] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        const bool ok = WavetableIO::saveWav (fc.getResult().withFileExtension ("wav"), *t);
        safe->services.showStatus (ok ? "Exported " + fc.getResult().getFileName() + " (Serum compatible)" : "Export failed", true);
    });
}

void OscPanel::showAdvanced()
{
    struct Advanced : public juce::Component
    {
        Advanced (EditorServices& s, int o)
            : mode (s, pid::osc (o, Osc::uniMode)), stack (s, pid::osc (o, Osc::uniStack)),
              uniWarp (s, pid::osc (o, Osc::uniWarp)), uniWt (s, pid::osc (o, Osc::uniWtPos)),
              key (s, pid::osc (o, Osc::keyTrack), ParamToggle::Style::pill, "KEY TRACK")
        {
            mode.setPrefix ("MODE"); stack.setPrefix ("STACK");
            for (auto* c : std::initializer_list<juce::Component*> { &mode, &stack, &uniWarp, &uniWt, &key }) addAndMakeVisible (c);
            setSize (230, 150);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            mode.setBounds (r.removeFromTop (22)); r.removeFromTop (4);
            stack.setBounds (r.removeFromTop (22)); r.removeFromTop (4);
            auto row = r.removeFromTop (58);
            uniWarp.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (6, 0));
            uniWt.setBounds (row.reduced (6, 0));
            key.setBounds (r.removeFromTop (20));
        }
        void paint (juce::Graphics& g) override { g.fillAll (theme::panelRaised); }
        ParamMenu mode, stack;
        ModKnob uniWarp, uniWt;
        ParamToggle key;
    };
    auto* top = getTopLevelComponent();
    juce::CallOutBox::launchAsynchronously (std::make_unique<Advanced> (services, osc), top->getLocalArea (&gear, gear.getLocalBounds()), top);
}

//==============================================================================
SubPanel::SubPanel (EditorServices& s)
    : SectionPanel (s, "SUB", sub (Sub::enable)), shape (s, sub (Sub::shape)), route (s, pid::route (Route::sub)),
      octave (s, sub (Sub::octave)), semi (s, sub (Sub::coarse)), level (s, sub (Sub::level)), pan (s, sub (Sub::pan))
{
    route.setPrefix ("TO");
    for (auto* c : std::initializer_list<juce::Component*> { &shape, &route, &octave, &semi, &level, &pan }) addAndMakeVisible (c);
}

void SubPanel::layoutContent (juce::Rectangle<int> r)
{
    shape.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    route.setBounds (r.removeFromBottom (22));
    r.removeFromBottom (4);
    auto top = r.removeFromTop (r.getHeight() / 2);
    layoutRow (top, { &octave, &semi }, 2);
    layoutRow (r, { &level, &pan }, 2);
}

NoisePanel::NoisePanel (EditorServices& s)
    : SectionPanel (s, "NOISE", noise (Noise::enable)),
      type (s, noise (Noise::type), [this] (juce::PopupMenu& m, int c)
      {
          for (int i = 0; i < noiseTypeNames().size(); ++i)
          {
              if (i == (int) NoiseType::custom)
              {
                  m.addSeparator();
                  const auto n = proc.getCustomNoiseName();
                  m.addItem (i + 1, n.isNotEmpty() ? "Custom: " + n : juce::String ("Custom Sample"), n.isNotEmpty(), i == c);
                  m.addItem (5000, "Load sample...");
              }
              else
                  m.addItem (i + 1, noiseTypeNames()[i], true, i == c);
          }
      }),
      route (s, pid::route (Route::noise)),
      pitch (s, noise (Noise::pitch)), fine (s, noise (Noise::fine)), level (s, noise (Noise::level)), pan (s, noise (Noise::pan)),
      key (s, noise (Noise::keyTrack), ParamToggle::Style::pill, "KEY"), oneShot (s, noise (Noise::oneShot), ParamToggle::Style::pill, "1-SHOT"),
      stereo (s, noise (Noise::stereo), ParamToggle::Style::pill, "ST")
{
    route.setPrefix ("TO");
    for (auto* c : std::initializer_list<juce::Component*> { &type, &route, &pitch, &fine, &level, &pan, &key, &oneShot, &stereo }) addAndMakeVisible (c);
    type.onExtraItem = [this] (int) { loadSample(); };
}

void NoisePanel::layoutContent (juce::Rectangle<int> r)
{
    type.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    auto bottom = r.removeFromBottom (22);
    route.setBounds (bottom.removeFromRight (bottom.getWidth() / 2));
    bottom.removeFromRight (4);
    layoutRow (bottom, { &key, &oneShot, &stereo }, 3);
    r.removeFromBottom (4);
    auto top = r.removeFromTop (r.getHeight() / 2);
    layoutRow (top, { &pitch, &fine }, 2);
    layoutRow (r, { &level, &pan }, 2);
}

void NoisePanel::loadSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load noise sample", Settings::get().getUserFolder(), "*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<NoisePanel> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        juce::String err;
        if (safe->proc.loadCustomNoise (fc.getResult(), err)) safe->services.showStatus ("Noise sample: " + fc.getResult().getFileName(), true);
        else safe->services.showStatus (err, true);
    });
}

//==============================================================================
FilterPanel::FilterPanel (EditorServices& s)
    : SectionPanel (s, {}), view (s, 0)
{
    addAndMakeVisible (view);
    for (int f = 0; f < kNumFilters; ++f)
    {
        filterPower[f] = std::make_unique<ParamToggle> (s, filter (f, Filter::enable), ParamToggle::Style::power);
        addAndMakeVisible (*filterPower[f]);
        type[f] = std::make_unique<ParamMenu> (s, filter (f, Filter::type), [] (juce::PopupMenu& m, int c) { m = buildFilterTypeMenu (c); });
        type[f]->setPrefix ("TYPE");
        addChildComponent (*type[f]);
        for (auto p : { Filter::cutoff, Filter::reso, Filter::drive, Filter::var, Filter::mix, Filter::stereo, Filter::level, Filter::keyTrack })
        {
            knobs[f].push_back (std::make_unique<ModKnob> (s, filter (f, p)));
            addChildComponent (*knobs[f].back());
        }
    }
    const char* srcLabels[] = { "A", "B", "C", "N", "S" };
    for (int i = 0; i < 5; ++i)
    {
        routes[i] = std::make_unique<ParamMenu> (s, pid::route ((Route) i), [] (juce::PopupMenu& m, int c)
        {
            const char* names[] = { "Filter 1", "Filter 2", "Filter 1+2", "Direct" };
            for (int k = 0; k < 4; ++k) m.addItem (k + 1, names[k], true, k == c);
        });
        routes[i]->setPrefix (srcLabels[i]);
        routes[i]->setCompact (true);
        routes[i]->displayText = [] (int c) { const char* shortNames[] = { "F1", "F2", "1+2", "DIR" }; return juce::String (shortNames[juce::jlimit (0, 3, c)]); };
        addAndMakeVisible (*routes[i]);
    }
    config = std::make_unique<ParamMenu> (s, pid::route (Route::filterConfig));
    addAndMakeVisible (*config);
    selectFilter (0);
}

juce::Rectangle<int> FilterPanel::tabArea (int f) const
{
    const auto h = getHeaderArea();
    const int w = h.getWidth() / 2;
    return { h.getX() + f * w, h.getY(), w, h.getHeight() };
}

void FilterPanel::selectFilter (int f)
{
    current = f;
    view.setFilter (f);
    for (int k = 0; k < kNumFilters; ++k)
    {
        type[k]->setVisible (k == f);
        for (auto& kn : knobs[k]) kn->setVisible (k == f);
    }
    refreshLabels();
    repaint();
}

void FilterPanel::refreshLabels()
{
    for (int f = 0; f < kNumFilters; ++f)
    {
        const int t = (int) proc.getPlain (filter (f, Filter::type));
        if (t != lastTypes[f])
        {
            lastTypes[f] = t;
            knobs[f][3]->setLabel (juce::String (filterTypes()[(size_t) juce::jlimit (0, numFilterTypes() - 1, t)].varLabel).toUpperCase());
        }
    }
}

void FilterPanel::paint (juce::Graphics& g)
{
    refreshLabels();
    theme::drawPanel (g, getLocalBounds().toFloat());
    g.setColour (theme::outline.withAlpha (0.6f));
    g.drawHorizontalLine (26, 8.0f, (float) getWidth() - 8.0f);
    for (int f = 0; f < kNumFilters; ++f)
    {
        auto t = tabArea (f);
        if (f == current)
        {
            g.setColour (theme::accent.withAlpha (0.12f));
            g.fillRoundedRectangle (t.toFloat().reduced (0.0f, 3.0f), 4.0f);
        }
        g.setFont (theme::title (12.5f));
        g.setColour (f == current ? theme::text : theme::textDim);
        g.drawText ("FILTER " + juce::String (f + 1), t.withTrimmedLeft (24), juce::Justification::centredLeft);
    }
}

void FilterPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int f = 0; f < kNumFilters; ++f)
        if (tabArea (f).contains (e.getPosition())) selectFilter (f);
}

void FilterPanel::layoutContent (juce::Rectangle<int> r)
{
    for (int f = 0; f < kNumFilters; ++f)
        filterPower[f]->setBounds (tabArea (f).removeFromLeft (20).withSizeKeepingCentre (17, 17));
    for (int f = 0; f < kNumFilters; ++f) type[f]->setBounds (r.getX(), r.getY(), r.getWidth(), 22);
    r.removeFromTop (26);
    auto bottom = r.removeFromBottom (22);
    r.removeFromBottom (4);
    config->setBounds (bottom.removeFromRight (70));
    bottom.removeFromRight (4);
    const int rw = bottom.getWidth() / 5;
    for (int i = 0; i < 5; ++i) routes[i]->setBounds (bottom.removeFromLeft (rw).reduced (1, 0));
    const int knobRows = 2, rowH = 56;
    view.setBounds (r.removeFromTop (std::max (60, r.getHeight() - knobRows * rowH - 4)));
    r.removeFromTop (4);
    for (int f = 0; f < kNumFilters; ++f)
    {
        auto rr = r;
        for (int row = 0; row < 2; ++row)
        {
            auto line = rr.removeFromTop (rowH);
            const int w = line.getWidth() / 4;
            for (int k = 0; k < 4; ++k) knobs[f][(size_t) (row * 4 + k)]->setBounds (line.removeFromLeft (w).reduced (1));
        }
    }
}

//==============================================================================
class ModulationPanel::SourceTab : public juce::Component
{
public:
    SourceTab (ModulationPanel& o, int idx) : owner (o), index (idx)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    ModSource source() const { return index < kNumEnvelopes ? envSource (index) : lfoSource (index - kNumEnvelopes); }
    juce::String label() const { return index < kNumEnvelopes ? "ENV " + juce::String (index + 1) : "LFO " + juce::String (index - kNumEnvelopes + 1); }
    void paint (juce::Graphics& g) override
    {
        const bool sel = owner.selected == index;
        const auto c = theme::sourceColour ((int) source());
        auto r = getLocalBounds().toFloat().reduced (1.0f, 3.0f);
        g.setColour (sel ? c.withAlpha (0.22f) : (hover ? theme::knobBody : juce::Colours::transparentBlack));
        g.fillRoundedRectangle (r, 4.0f);
        if (sel) { g.setColour (c); g.fillRoundedRectangle (r.removeFromBottom (2.0f), 1.0f); }
        const bool used = owner.proc.getCompiledMatrix() != nullptr && owner.proc.getCompiledMatrix()->sourceUsed[(size_t) source()];
        g.setColour (sel ? theme::text : (used ? c : theme::textDim));
        g.setFont (theme::label (10.5f));
        g.drawText (label(), getLocalBounds(), juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override
    {
        hover = true; repaint();
        owner.services.showStatus (label() + "  -  Click to edit. Drag onto any knob to modulate it.");
    }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (e.getDistanceFromDragStart() < 4) owner.select (index); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 5) return;
        auto& dc = owner.services.getDragContainer();
        if (dc.isDragAndDropActive()) return;
        juce::Image img (juce::Image::ARGB, 70, 22, true);
        {
            juce::Graphics g (img);
            g.setColour (theme::sourceColour ((int) source()).withAlpha (0.9f));
            g.fillRoundedRectangle (0.0f, 0.0f, 70.0f, 22.0f, 5.0f);
            g.setColour (theme::background);
            g.setFont (theme::label (12.0f));
            g.drawText (label(), 0, 0, 70, 22, juce::Justification::centred);
        }
        dc.startDragging ("modsrc:" + juce::String ((int) source()), this, juce::ScaledImage (img), true);
    }
    ModulationPanel& owner;
    const int index;
    bool hover = false;
};

ModulationPanel::ModulationPanel (EditorServices& s) : SectionPanel (s, {}), envView (s), lfoEditor (s)
{
    for (int i = 0; i < kNumEnvelopes + kNumLfos; ++i)
    {
        tabs.push_back (std::make_unique<SourceTab> (*this, i));
        addAndMakeVisible (*tabs.back());
    }
    addAndMakeVisible (envView);
    addChildComponent (lfoEditor);
    for (int e = 0; e < kNumEnvelopes; ++e)
        for (auto p : { Env::attack, Env::hold, Env::decay, Env::sustain, Env::release, Env::aCurve, Env::dCurve, Env::rCurve })
        {
            envKnobs[e].push_back (std::make_unique<ModKnob> (s, env (e, p), p >= Env::aCurve ? ModKnob::Style::small : ModKnob::Style::normal));
            addChildComponent (*envKnobs[e].back());
        }
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& w = lfoW[l];
        w.rate = std::make_unique<ModKnob> (s, lfo (l, Lfo::rate));
        w.syncRate = std::make_unique<ModKnob> (s, lfo (l, Lfo::syncRate));
        w.rise = std::make_unique<ModKnob> (s, lfo (l, Lfo::rise));
        w.delay = std::make_unique<ModKnob> (s, lfo (l, Lfo::delay));
        w.smooth = std::make_unique<ModKnob> (s, lfo (l, Lfo::smooth));
        w.phase = std::make_unique<ModKnob> (s, lfo (l, Lfo::phase));
        w.sync = std::make_unique<ParamToggle> (s, lfo (l, Lfo::sync), ParamToggle::Style::pill, "BPM");
        w.type = std::make_unique<ParamMenu> (s, lfo (l, Lfo::type));
        w.mode = std::make_unique<ParamMenu> (s, lfo (l, Lfo::mode));
        w.feel = std::make_unique<ParamMenu> (s, lfo (l, Lfo::syncFeel));
        w.type->setPrefix ("TYPE"); w.mode->setPrefix ("MODE"); w.feel->setPrefix ("FEEL");
        for (juce::Component* c : { (juce::Component*) w.rate.get(), (juce::Component*) w.syncRate.get(), (juce::Component*) w.rise.get(),
                                    (juce::Component*) w.delay.get(), (juce::Component*) w.smooth.get(), (juce::Component*) w.phase.get(),
                                    (juce::Component*) w.sync.get(), (juce::Component*) w.type.get(), (juce::Component*) w.mode.get(), (juce::Component*) w.feel.get() })
            addChildComponent (c);
    }
    select (0);
}

ModulationPanel::~ModulationPanel() = default;

void ModulationPanel::select (int tab)
{
    selected = juce::jlimit (0, kNumEnvelopes + kNumLfos - 1, tab);
    const bool isEnv = selected < kNumEnvelopes;
    envView.setVisible (isEnv);
    lfoEditor.setVisible (! isEnv);
    if (isEnv) envView.setEnvelope (selected);
    else lfoEditor.setLfo (selected - kNumEnvelopes);
    for (int e = 0; e < kNumEnvelopes; ++e)
        for (auto& k : envKnobs[e]) k->setVisible (isEnv && e == selected);
    for (int l = 0; l < kNumLfos; ++l)
    {
        const bool vis = ! isEnv && l == selected - kNumEnvelopes;
        auto& w = lfoW[l];
        const bool sync = proc.getPlain (lfo (l, Lfo::sync)) > 0.5f;
        lastSync[l] = sync;
        w.rate->setVisible (vis && ! sync);
        w.syncRate->setVisible (vis && sync);
        for (juce::Component* c : { (juce::Component*) w.rise.get(), (juce::Component*) w.delay.get(), (juce::Component*) w.smooth.get(),
                                    (juce::Component*) w.phase.get(), (juce::Component*) w.sync.get(), (juce::Component*) w.type.get(),
                                    (juce::Component*) w.mode.get(), (juce::Component*) w.feel.get() })
            c->setVisible (vis);
    }
    for (auto& t : tabs) t->repaint();
    resized();
}

void ModulationPanel::refresh()
{
    if (selected >= kNumEnvelopes)
    {
        const int l = selected - kNumEnvelopes;
        const bool sync = proc.getPlain (lfo (l, Lfo::sync)) > 0.5f;
        if (sync != lastSync[l]) select (selected);
    }
}

void ModulationPanel::paint (juce::Graphics& g)
{
    theme::drawPanel (g, getLocalBounds().toFloat());
    g.setColour (theme::outline.withAlpha (0.6f));
    g.drawHorizontalLine (26, 8.0f, (float) getWidth() - 8.0f);
}

void ModulationPanel::layoutContent (juce::Rectangle<int> r)
{
    auto h = getHeaderArea();
    const int tw = h.getWidth() / (int) tabs.size();
    for (auto& t : tabs) t->setBounds (h.removeFromLeft (tw));

    auto display = r.removeFromLeft (r.getWidth() * 52 / 100);
    envView.setBounds (display);
    lfoEditor.setBounds (display);
    r.removeFromLeft (8);

    if (selected < kNumEnvelopes)
    {
        auto& k = envKnobs[selected];
        auto block = r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 128));
        auto top = block.removeFromTop (66);
        layoutRow (top, { k[0].get(), k[1].get(), k[2].get(), k[3].get(), k[4].get() }, 0);
        block.removeFromTop (6);
        auto bottom = block.withSizeKeepingCentre (r.getWidth() * 3 / 5, std::min (block.getHeight(), 54));
        layoutRow (bottom, { k[5].get(), k[6].get(), k[7].get() }, 0);
    }
    else
    {
        auto& w = lfoW[selected - kNumEnvelopes];
        auto menus = r.removeFromTop (22);
        layoutRow (menus, { w.sync.get(), w.type.get(), w.mode.get() }, 4);
        w.sync->setBounds (w.sync->getBounds().withWidth (40));
        r.removeFromTop (4);
        auto feelRow = r.removeFromBottom (22);
        w.feel->setBounds (feelRow.removeFromLeft (feelRow.getWidth() / 2));
        auto knobRow = r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 66));
        const int kw = knobRow.getWidth() / 5;
        auto first = knobRow.removeFromLeft (kw);
        w.rate->setBounds (first);
        w.syncRate->setBounds (first);
        layoutRow (knobRow, { w.rise.get(), w.delay.get(), w.smooth.get(), w.phase.get() }, 0);
    }
}

//==============================================================================
MacroPanel::MacroPanel (EditorServices& s) : SectionPanel (s, "MACROS")
{
    for (int m = 0; m < kNumMacros; ++m)
    {
        knobs.push_back (std::make_unique<ModKnob> (s, macro (m), ModKnob::Style::normal, " "));
        addAndMakeVisible (*knobs.back());
        auto l = std::make_unique<juce::Label>();
        l->setFont (theme::label (10.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, theme::textDim);
        l->setEditable (false, true, false);
        l->onTextChange = [this, m] { proc.setMacroName (m, names[(size_t) m]->getText()); };
        l->setTooltip ("Double-click to rename");
        addAndMakeVisible (*l);
        names.push_back (std::move (l));
        auto h = std::make_unique<ModSourceHandle> (s, macroSource (m), juce::String (m + 1));
        h->compact = true;
        addAndMakeVisible (*h);
        handles.push_back (std::move (h));
    }
    for (auto src : { ModSource::velocity, ModSource::note, ModSource::modWheel, ModSource::aftertouch,
                      ModSource::pitchBend, ModSource::random1, ModSource::random2, ModSource::gate })
    {
        extraSources.push_back (std::make_unique<ModSourceHandle> (s, src));
        addAndMakeVisible (*extraSources.back());
    }
    refreshNames();
}

void MacroPanel::refreshNames()
{
    for (int m = 0; m < kNumMacros; ++m)
        if (! names[(size_t) m]->isBeingEdited() && names[(size_t) m]->getText() != proc.getMacroName (m))
            names[(size_t) m]->setText (proc.getMacroName (m), juce::dontSendNotification);
}

void MacroPanel::layoutContent (juce::Rectangle<int> r)
{
    auto chips = r.removeFromBottom (46);
    const int cw = chips.getWidth() / 4;
    for (int i = 0; i < (int) extraSources.size(); ++i)
        extraSources[(size_t) i]->setBounds (chips.getX() + (i % 4) * cw + 1, chips.getY() + (i / 4) * 23 + 1, cw - 2, 21);
    r.removeFromBottom (4);
    const int cellW = r.getWidth() / 4, cellH = r.getHeight() / 2;
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto cell = juce::Rectangle<int> (r.getX() + (m % 4) * cellW, r.getY() + (m / 4) * cellH, cellW, cellH).reduced (2);
        names[(size_t) m]->setBounds (cell.removeFromBottom (14));
        handles[(size_t) m]->setBounds (cell.getX(), cell.getY(), 18, 16);
        knobs[(size_t) m]->setBounds (cell);
    }
}

//==============================================================================
VoicingPanel::VoicingPanel (EditorServices& s)
    : SectionPanel (s, "VOICING"), poly (s, global (Global::polyphony)), porta (s, global (Global::portaTime)), curve (s, global (Global::portaCurve)),
      mono (s, global (Global::mono), ParamToggle::Style::pill), legato (s, global (Global::legato), ParamToggle::Style::pill),
      always (s, global (Global::portaAlways), ParamToggle::Style::pill), scaled (s, global (Global::portaScaled), ParamToggle::Style::pill)
{
    for (auto* c : std::initializer_list<juce::Component*> { &poly, &porta, &curve, &mono, &legato, &always, &scaled }) addAndMakeVisible (c);
}

void VoicingPanel::layoutContent (juce::Rectangle<int> r)
{
    auto toggles = r.removeFromRight (r.getWidth() * 42 / 100);
    auto t1 = toggles.removeFromTop (toggles.getHeight() / 2).reduced (0, 3);
    layoutRow (t1, { &mono, &legato }, 4);
    layoutRow (toggles.reduced (0, 3), { &always, &scaled }, 4);
    layoutRow (r, { &poly, &porta, &curve }, 2);
}

} // namespace cureum
