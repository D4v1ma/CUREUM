#include "Pages.h"
#include "../Core/Settings.h"
#include "../Presets/SerumImport.h"

namespace cureum
{

using namespace pid;

//==============================================================================
void ParamBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f, 6.0f);
    g.setColour (theme::track);
    g.fillRoundedRectangle (r, 3.0f);
    const float n = getNorm();
    const float zero = spec().toNormalised (0.0f);
    auto fill = juce::Rectangle<float>::leftTopRightBottom (r.getX() + r.getWidth() * std::min (zero, n), r.getY(),
                                                           r.getX() + r.getWidth() * std::max (zero, n), r.getBottom());
    g.setColour (n >= zero ? theme::mod : theme::warn.withAlpha (0.85f));
    g.fillRoundedRectangle (fill, 3.0f);
    g.setColour (theme::text);
    g.setFont (theme::value (11.0f));
    g.drawText (spec().formatValue (getPlain()), getLocalBounds(), juce::Justification::centred);
}

void ParamBar::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    startNorm = getNorm();
    beginGesture();
}

void ParamBar::mouseDrag (const juce::MouseEvent& e)
{
    const float sens = e.mods.isShiftDown() ? 0.1f : 1.0f;
    setNorm (startNorm + sens * (float) e.getDistanceFromDragStartX() / (float) std::max (40, getWidth()));
    services.showStatus (getHelpTitle());
    repaint();
}

void ParamBar::mouseUp (const juce::MouseEvent&) { endGesture(); }
void ParamBar::mouseDoubleClick (const juce::MouseEvent&) { beginGesture(); setNorm (param->getDefaultValue()); endGesture(); }

//==============================================================================
namespace
{
    std::vector<int> fxParams (FxType t)
    {
        switch (t)
        {
            case FxType::hyper:      return { hyper (Hyper::retrig), hyper (Hyper::mix), hyper (Hyper::rate), hyper (Hyper::detune), hyper (Hyper::unison), hyper (Hyper::dimSize), hyper (Hyper::dimMix) };
            case FxType::distortion: return { dist (Dist::mode), dist (Dist::filterPos), dist (Dist::filterType), dist (Dist::drive), dist (Dist::mix), dist (Dist::freq), dist (Dist::bw) };
            case FxType::flanger:    return { flanger (Flanger::sync), flanger (Flanger::syncRate), flanger (Flanger::mix), flanger (Flanger::rate), flanger (Flanger::depth), flanger (Flanger::feedback), flanger (Flanger::stereo) };
            case FxType::phaser:     return { phaser (Phaser::sync), phaser (Phaser::syncRate), phaser (Phaser::mix), phaser (Phaser::rate), phaser (Phaser::depth), phaser (Phaser::freq), phaser (Phaser::feedback), phaser (Phaser::stereo), phaser (Phaser::stages) };
            case FxType::chorus:     return { chorus (Chorus::sync), chorus (Chorus::syncRate), chorus (Chorus::mix), chorus (Chorus::rate), chorus (Chorus::delay1), chorus (Chorus::delay2), chorus (Chorus::depth), chorus (Chorus::feedback), chorus (Chorus::lpf) };
            case FxType::delay:      return { delay (Delay::mode), delay (Delay::sync), delay (Delay::link), delay (Delay::syncL), delay (Delay::syncR), delay (Delay::mix), delay (Delay::timeL), delay (Delay::timeR), delay (Delay::feedback), delay (Delay::freq), delay (Delay::bw) };
            case FxType::compressor: return { comp (Comp::multiband), comp (Comp::mix), comp (Comp::threshold), comp (Comp::ratio), comp (Comp::attack), comp (Comp::release), comp (Comp::gain), comp (Comp::lowGain), comp (Comp::midGain), comp (Comp::highGain) };
            case FxType::reverb:     return { reverb (Reverb::mode), reverb (Reverb::mix), reverb (Reverb::size), reverb (Reverb::preDelay), reverb (Reverb::lowCut), reverb (Reverb::damp), reverb (Reverb::highCut), reverb (Reverb::width) };
            case FxType::eq:         return { eq (Eq::lowType), eq (Eq::highType), eq (Eq::lowFreq), eq (Eq::lowQ), eq (Eq::lowGain), eq (Eq::highFreq), eq (Eq::highQ), eq (Eq::highGain) };
            case FxType::filter:     return { fxFilter (FxFilter::type), fxFilter (FxFilter::mix), fxFilter (FxFilter::cutoff), fxFilter (FxFilter::reso), fxFilter (FxFilter::drive), fxFilter (FxFilter::var), fxFilter (FxFilter::stereo) };
            case FxType::bode:       return { bode (Bode::mix), bode (Bode::shift), bode (Bode::feedback), bode (Bode::spread) };
            case FxType::utility:    return { util (Util::monoBass), util (Util::invertL), util (Util::invertR), util (Util::gain), util (Util::pan), util (Util::width), util (Util::monoFreq) };
            case FxType::convolve:   return { conv (Conv::ir), conv (Conv::mix), conv (Conv::preDelay), conv (Conv::lowCut), conv (Conv::highCut), conv (Conv::gain) };
            case FxType::plugin1: case FxType::plugin2: case FxType::plugin3: case FxType::plugin4:
                return { plugin ((int) t - (int) FxType::plugin1, Plugin::mix) };
            case FxType::count: break;
        }
        return {};
    }

    juce::String unitTitle (CureumProcessor& p, FxType t)
    {
        if (isPluginSlot (t))
        {
            const auto n = p.getFxRack().pluginSlot ((int) t - (int) FxType::plugin1).getPluginName();
            return n.isNotEmpty() ? n.toUpperCase() : fxTypeNames()[(int) t].toUpperCase() + " (EMPTY)";
        }
        return fxTypeNames()[(int) t].toUpperCase();
    }
}

class FxPage::UnitPanel : public SectionPanel, public Refreshable
{
public:
    UnitPanel (FxPage& o, FxType t) : SectionPanel (o.services, unitTitle (o.proc, t), pid::fxEnable (t)), owner (o), type (t)
    {
        for (int p : fxParams (t))
        {
            const auto& s = ParamRegistry::get().spec (p);
            std::unique_ptr<juce::Component> w;
            if (s.kind == ParamKind::choice)
            {
                ParamMenu::MenuBuilder builder;
                if (p == fxFilter (FxFilter::type)) builder = [] (juce::PopupMenu& m, int c) { m = buildFilterTypeMenu (c); };
                auto menu = std::make_unique<ParamMenu> (o.services, p, builder);
                menu->setPrefix (s.shortName);
                if (p == conv (Conv::ir))
                {
                    menu->onExtraItem = [this] (int) { loadIr(); };
                    menu->setPrefix ("IR");
                }
                w = std::move (menu);
                menus.push_back (w.get());
            }
            else if (s.kind == ParamKind::toggle)
            {
                w = std::make_unique<ParamToggle> (o.services, p, ParamToggle::Style::pill);
                menus.push_back (w.get());
            }
            else
            {
                w = std::make_unique<ModKnob> (o.services, p);
                knobs.push_back (w.get());
            }
            addAndMakeVisible (*w);
            widgets.push_back (std::move (w));
        }
        if (t == FxType::convolve)
        {
            // the IR menu offers "Load IR file..."
            if (auto* m = dynamic_cast<ParamMenu*> (menus.front()))
                juce::ignoreUnused (m);
        }
        if (t == FxType::filter)
        {
            filterView = std::make_unique<FilterView> (o.services, -1);
            addAndMakeVisible (*filterView);
        }
        if (isPluginSlot (t))
        {
            for (auto* b : { &loadButton, &editButton, &removeButton }) addAndMakeVisible (b);
            const int slot = (int) t - (int) FxType::plugin1;
            loadButton.onClick = [this, slot] { owner.showPluginMenu (slot); };
            editButton.onClick = [this, slot] { owner.openPluginEditor (slot); };
            removeButton.onClick = [this, slot] { owner.proc.clearPluginSlot (slot); owner.rebuild(); };
        }
    }

    int preferredHeight (int width) const
    {
        int h = 34 + (menus.empty() && ! isPluginSlot (type) ? 0 : 26);
        const int perRow = std::max (1, (width - 16 - (filterView != nullptr ? 150 : 0)) / 62);
        const int rows = knobs.empty() ? 0 : ((int) knobs.size() + perRow - 1) / perRow;
        return h + rows * 60 + 6;
    }

    void layoutContent (juce::Rectangle<int> r) override
    {
        if (! menus.empty() || isPluginSlot (type))
        {
            auto row = r.removeFromTop (22);
            if (isPluginSlot (type))
            {
                loadButton.setBounds (row.removeFromLeft (90)); row.removeFromLeft (4);
                editButton.setBounds (row.removeFromLeft (70)); row.removeFromLeft (4);
                removeButton.setBounds (row.removeFromLeft (70)); row.removeFromLeft (8);
            }
            const int n = (int) menus.size();
            for (auto* m : menus)
            {
                const int w = dynamic_cast<ParamToggle*> (m) != nullptr ? 64 : std::min (180, (row.getWidth() - 4 * n) / std::max (1, n));
                m->setBounds (row.removeFromLeft (w));
                row.removeFromLeft (4);
            }
            r.removeFromTop (4);
        }
        if (filterView != nullptr)
            filterView->setBounds (r.removeFromLeft (140).withHeight (54));
        const int perRow = std::max (1, r.getWidth() / 62);
        for (size_t i = 0; i < knobs.size(); ++i)
            knobs[i]->setBounds (r.getX() + (int) (i % (size_t) perRow) * 62, r.getY() + (int) (i / (size_t) perRow) * 60, 60, 58);
    }

    void paint (juce::Graphics& g) override
    {
        title = unitTitle (proc, type);
        SectionPanel::paint (g);
        if (type == FxType::compressor)
        {
            g.setFont (theme::value (11.0f));
            g.setColour (theme::textDim);
            g.drawText ("GR " + juce::String (proc.feedback.compGr.load(), 1) + " dB", getHeaderArea(), juce::Justification::centredRight);
        }
    }

    void refresh() override
    {
        if (type == FxType::compressor)
        {
            const float gr = proc.feedback.compGr.load();
            if (std::abs (gr - lastGr) > 0.1f) { lastGr = gr; repaint (getHeaderArea()); }
        }
        if (isPluginSlot (type))
        {
            const bool loaded = proc.getFxRack().pluginSlot ((int) type - (int) FxType::plugin1).isLoaded();
            editButton.setEnabled (loaded);
            removeButton.setEnabled (loaded);
        }
    }

    void loadIr()
    {
        owner.chooser = std::make_unique<juce::FileChooser> ("Load impulse response", Settings::get().getUserFolder(), "*.wav;*.aif;*.aiff;*.flac");
        juce::Component::SafePointer<UnitPanel> safe (this);
        owner.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            safe->proc.setConvolverIr (ConvIr::custom, fc.getResult());
            safe->services.showStatus ("Impulse response: " + fc.getResult().getFileName(), true);
        });
    }

    FxPage& owner;
    const FxType type;
    std::vector<std::unique_ptr<juce::Component>> widgets;
    std::vector<juce::Component*> menus, knobs;
    std::unique_ptr<FilterView> filterView;
    juce::TextButton loadButton { "Load plugin..." }, editButton { "Edit" }, removeButton { "Remove" };
    float lastGr = 0.0f;
};

//==============================================================================
class FxPage::RackList : public juce::Component
{
public:
    explicit RackList (FxPage& o) : owner (o) {}

    void rebuild()
    {
        toggles.clear();
        order = owner.proc.getFxOrder();
        for (auto t : order.units)
        {
            auto tg = std::make_unique<ParamToggle> (owner.services, pid::fxEnable (t), ParamToggle::Style::power);
            addAndMakeVisible (*tg);
            toggles.push_back (std::move (tg));
        }
        resized();
        repaint();
    }

    void resized() override
    {
        for (size_t i = 0; i < toggles.size(); ++i)
            toggles[i]->setBounds (8, (int) i * rowH + 6, 16, 16);
    }

    void paint (juce::Graphics& g) override
    {
        for (size_t i = 0; i < order.units.size(); ++i)
        {
            auto r = juce::Rectangle<int> (0, (int) i * rowH, getWidth(), rowH - 2);
            const bool on = owner.proc.getPlain (pid::fxEnable (order.units[i])) > 0.5f;
            g.setColour ((int) i == hoverRow ? theme::panelRaised.brighter (0.06f) : theme::panelRaised);
            g.fillRoundedRectangle (r.toFloat(), 5.0f);
            g.setColour (on ? theme::text : theme::textDim);
            g.setFont (theme::label (11.5f));
            g.drawText (unitTitle (owner.proc, order.units[i]), r.withTrimmedLeft (32).withTrimmedRight (40), juce::Justification::centredLeft);
            // grip + remove
            g.setColour (theme::textFaint);
            for (int k = 0; k < 3; ++k) g.fillRect (r.getRight() - 34, r.getCentreY() - 4 + k * 3, 10, 1);
            drawIcon (g, IconButton::Icon::close, juce::Rectangle<float> ((float) r.getRight() - 16.0f, (float) r.getCentreY() - 4.0f, 8.0f, 8.0f),
                      (int) i == hoverRow ? theme::warn : theme::textFaint);
        }
        if (dragRow >= 0 && dropRow >= 0)
        {
            g.setColour (theme::accent);
            g.fillRect (0, dropRow * rowH - 2, getWidth(), 2);
        }
    }

    int rowAt (int y) const { return juce::jlimit (0, (int) order.units.size() - 1, y / rowH); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int r = e.y < (int) order.units.size() * rowH ? rowAt (e.y) : -1;
        if (r != hoverRow) { hoverRow = r; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hoverRow = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y >= (int) order.units.size() * rowH) return;
        const int r = rowAt (e.y);
        if (e.x > getWidth() - 22)
        {
            const auto t = order.units[(size_t) r];
            owner.proc.setPlain (pid::fxEnable (t), 0.0f);
            if (isPluginSlot (t)) owner.proc.clearPluginSlot ((int) t - (int) FxType::plugin1);
            order.units.erase (order.units.begin() + r);
            owner.proc.setFxOrder (order);
            owner.rebuild();
            return;
        }
        dragRow = r;
        owner.services.showStatus ("Drag to reorder the effects. The signal flows from top to bottom.");
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragRow < 0) return;
        dropRow = juce::jlimit (0, (int) order.units.size(), (e.y + rowH / 2) / rowH);
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragRow >= 0 && dropRow >= 0 && dropRow != dragRow && dropRow != dragRow + 1)
        {
            const auto t = order.units[(size_t) dragRow];
            order.units.erase (order.units.begin() + dragRow);
            const int target = dropRow > dragRow ? dropRow - 1 : dropRow;
            order.units.insert (order.units.begin() + target, t);
            owner.proc.setFxOrder (order);
            owner.rebuild();
        }
        else if (dragRow >= 0 && e.getDistanceFromDragStart() < 4)
            owner.viewport.setViewPosition (0, owner.units.size() > (size_t) dragRow ? owner.units[(size_t) dragRow]->getY() : 0);
        dragRow = dropRow = -1;
        repaint();
    }

    FxPage& owner;
    FxOrder order;
    std::vector<std::unique_ptr<ParamToggle>> toggles;
    static constexpr int rowH = 30;
    int hoverRow = -1, dragRow = -1, dropRow = -1;
};

FxPage::FxPage (EditorServices& s) : services (s), proc (s.getProcessor()), scope (s.getProcessor())
{
    rack = std::make_unique<RackList> (*this);
    addAndMakeVisible (scope);
    addAndMakeVisible (*rack);
    addAndMakeVisible (addButton);
    addButton.onClick = [this] { showAddMenu(); };
    viewport.setViewedComponent (&stack, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    rebuild();
}

FxPage::~FxPage()
{
    pluginWindows.clear();
}

void FxPage::rebuild()
{
    const auto order = proc.getFxOrder();
    lastOrder = order.toString();
    lastStructure = proc.structureVersion.load();
    rack->rebuild();
    units.clear();
    for (auto t : order.units)
    {
        units.push_back (std::make_unique<UnitPanel> (*this, t));
        stack.addAndMakeVisible (*units.back());
    }
    resized();
}

void FxPage::paint (juce::Graphics& g)
{
    auto left = getLocalBounds().reduced (8).removeFromLeft (250);
    theme::drawPanel (g, left.toFloat());
    drawSectionTitle (g, "FX RACK", left.reduced (10, 0).removeFromTop (26).translated (0, 104));
    if (units.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::value (14.0f));
        g.drawText ("The FX rack is empty. Click + ADD EFFECT to insert an effect or a VST3/AU plugin.",
                    getLocalBounds().withTrimmedLeft (270), juce::Justification::centred);
    }
}

void FxPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto left = r.removeFromLeft (250).reduced (8);
    scope.setBounds (left.removeFromTop (92));
    left.removeFromTop (30);
    addButton.setBounds (left.removeFromBottom (28));
    left.removeFromBottom (6);
    rack->setBounds (left);
    r.removeFromLeft (8);
    viewport.setBounds (r);
    const int w = r.getWidth() - 10;
    int y = 0;
    for (auto& u : units)
    {
        const int h = u->preferredHeight (w);
        u->setBounds (0, y, w, h);
        y += h + 8;
    }
    stack.setSize (w, std::max (y, 10));
}

void FxPage::refresh()
{
    if (proc.structureVersion.load() != lastStructure)
    {
        lastStructure = proc.structureVersion.load();
        if (proc.getFxOrder().toString() != lastOrder) rebuild();
        else { rack->repaint(); for (auto& u : units) u->repaint(); }
    }
    rack->repaint();
}

void FxPage::showAddMenu()
{
    juce::PopupMenu m;
    const auto order = proc.getFxOrder();
    for (int i = 0; i < (int) FxType::plugin1; ++i)
        m.addItem (i + 1, fxTypeNames()[i], ! order.contains ((FxType) i));
    m.addSeparator();
    juce::PopupMenu plugins;
    const auto list = proc.getPluginHost().getEffectPlugins();
    int freeSlot = -1;
    for (int sIdx = 0; sIdx < 4; ++sIdx)
        if (! order.contains ((FxType) ((int) FxType::plugin1 + sIdx))) { freeSlot = sIdx; break; }
    for (int i = 0; i < list.size(); ++i)
        plugins.addItem (1000 + i, list[i].manufacturerName + " - " + list[i].name + "  (" + list[i].pluginFormatName + ")", freeSlot >= 0);
    if (list.isEmpty()) plugins.addItem (999, "No plugins scanned yet", false);
    plugins.addSeparator();
    plugins.addItem (900, "Browse for a plugin file...", freeSlot >= 0);
    plugins.addItem (901, "Scan for VST3/AU plugins...");
    m.addSubMenu ("VST3 / AU Plugin", plugins);

    juce::Component::SafePointer<FxPage> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton), [safe, list, freeSlot] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& page = *safe;
        if (r <= (int) FxType::plugin1)
        {
            auto o = page.proc.getFxOrder();
            const auto t = (FxType) (r - 1);
            o.units.push_back (t);
            page.proc.setPlain (pid::fxEnable (t), 1.0f);
            page.proc.setFxOrder (o);
            page.rebuild();
        }
        else if (r >= 1000 && freeSlot >= 0)
        {
            page.proc.loadPluginIntoSlot (freeSlot, list[r - 1000]);
            page.services.showStatus ("Loading " + list[r - 1000].name + "...", true);
        }
        else if (r == 900 && freeSlot >= 0)
        {
            page.chooser = std::make_unique<juce::FileChooser> ("Choose a VST3 plugin", juce::File(), "*.vst3;*.component");
            page.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
                [safe, freeSlot] (const juce::FileChooser& fc)
                {
                    if (safe == nullptr || fc.getResult() == juce::File()) return;
                    juce::String err;
                    const int n = safe->proc.getPluginHost().addPluginFile (fc.getResult(), err);
                    if (n == 0) { safe->services.showStatus (err, true); return; }
                    auto all = safe->proc.getPluginHost().getEffectPlugins();
                    for (auto& d : all)
                        if (d.fileOrIdentifier == fc.getResult().getFullPathName())
                        { safe->proc.loadPluginIntoSlot (freeSlot, d); break; }
                });
        }
        else if (r == 901)
        {
            page.proc.getPluginHost().startScan (Settings::get().getSerumFolders().isEmpty() ? juce::StringArray() : juce::StringArray());
            page.services.showStatus ("Scanning for plugins... (progress is shown on the GLOBAL page)", true);
        }
    });
}

void FxPage::showPluginMenu (int slot)
{
    juce::PopupMenu m;
    const auto list = proc.getPluginHost().getEffectPlugins();
    for (int i = 0; i < list.size(); ++i)
        m.addItem (1000 + i, list[i].manufacturerName + " - " + list[i].name);
    if (list.isEmpty()) m.addItem (999, "No plugins found - scan first", false);
    m.addSeparator();
    m.addItem (901, "Scan for VST3/AU plugins...");
    juce::Component::SafePointer<FxPage> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, list, slot] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= 1000) safe->proc.loadPluginIntoSlot (slot, list[r - 1000]);
        else if (r == 901) safe->proc.getPluginHost().startScan();
    });
}

void FxPage::openPluginEditor (int slot)
{
    auto* instance = proc.getFxRack().pluginSlot (slot).getInstance();
    if (instance == nullptr) return;
    for (auto& w : pluginWindows)
        if (w != nullptr && w->getName() == instance->getName()) { w->toFront (true); return; }

    struct PluginWindow : public juce::DocumentWindow
    {
        PluginWindow (juce::AudioPluginInstance& p, std::function<void (PluginWindow*)> closer)
            : DocumentWindow (p.getName(), theme::panelRaised, DocumentWindow::closeButton), onCloseFn (std::move (closer))
        {
            setUsingNativeTitleBar (true);
            if (auto* ed = p.hasEditor() ? p.createEditorAndMakeActive() : nullptr)
                setContentOwned (ed, true);
            else
                setContentOwned (new juce::GenericAudioProcessorEditor (p), true);
            setResizable (true, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { if (onCloseFn) onCloseFn (this); }
        std::function<void (PluginWindow*)> onCloseFn;
    };
    juce::Component::SafePointer<FxPage> safe (this);
    pluginWindows.push_back (std::make_unique<PluginWindow> (*instance, [safe] (PluginWindow* w)
    {
        if (safe == nullptr) return;
        juce::MessageManager::callAsync ([safe, w]
        {
            if (safe == nullptr) return;
            auto& v = safe->pluginWindows;
            v.erase (std::remove_if (v.begin(), v.end(), [w] (auto& p) { return p.get() == w; }), v.end());
        });
    }));
}

//==============================================================================
class MatrixPage::Row : public juce::Component
{
public:
    Row (MatrixPage& o, int i) : owner (o), index (i), amount (o.services, pid::mod (i))
    {
        for (auto* b : { &src, &dst, &aux, &type, &enable, &remove }) { addAndMakeVisible (b); b->setColour (juce::TextButton::buttonColourId, theme::knobBody); }
        addAndMakeVisible (amount);
        enable.setClickingTogglesState (false);
        src.onClick = [this] { showSourceMenu (false); };
        aux.onClick = [this] { showSourceMenu (true); };
        dst.onClick = [this] { showDestMenu(); };
        type.onClick = [this] { auto s = slot(); s.bipolar = ! s.bipolar; owner.proc.setModSlot (index, s); };
        enable.onClick = [this] { auto s = slot(); s.enabled = ! s.enabled; owner.proc.setModSlot (index, s); };
        remove.onClick = [this] { owner.proc.removeModSlot (index); };
        refreshRow();
    }

    ModSlot slot() const { return owner.proc.getModSlots()[(size_t) index]; }

    void refreshRow()
    {
        const auto s = slot();
        src.setButtonText (s.source == ModSource::none ? juce::String ("-") : modSourceNames()[(int) s.source]);
        aux.setButtonText (s.aux == ModSource::none ? juce::String ("-") : modSourceNames()[(int) s.aux]);
        dst.setButtonText (s.dest < 0 ? juce::String ("-") : ParamRegistry::get().spec (s.dest).name);
        type.setButtonText (s.bipolar || isNaturallyBipolar (s.source) ? "BIPOLAR" : "UNIPOLAR");
        enable.setButtonText (s.enabled ? "ON" : "OFF");
        enable.setToggleState (s.enabled && s.source != ModSource::none, juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto s = slot();
        g.setColour (index % 2 == 0 ? theme::panel : theme::panelRaised);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (s.source != ModSource::none ? theme::sourceColour ((int) s.source) : theme::textFaint);
        g.setFont (theme::label (11.0f));
        g.drawText (juce::String (index + 1), getLocalBounds().removeFromLeft (30), juce::Justification::centred);
        auto r = getLocalBounds();
        r.removeFromLeft (30 + 150 + 6 + 170 + 6 + 240 + 6 + 80 + 6 + 130 + 6);
        g.setColour (theme::textDim);
        g.setFont (theme::value (11.0f));
        g.drawText ("Curve " + juce::String (juce::roundToInt (s.curve * 100.0f)) + "   Out " + juce::String (juce::roundToInt (s.output * 100.0f)) + "%",
                    r.removeFromLeft (130), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 3);
        r.removeFromLeft (30);
        src.setBounds (r.removeFromLeft (150)); r.removeFromLeft (6);
        amount.setBounds (r.removeFromLeft (170)); r.removeFromLeft (6);
        dst.setBounds (r.removeFromLeft (240)); r.removeFromLeft (6);
        type.setBounds (r.removeFromLeft (80)); r.removeFromLeft (6);
        aux.setBounds (r.removeFromLeft (130)); r.removeFromLeft (6);
        r.removeFromLeft (130);
        remove.setBounds (r.removeFromRight (26));
        r.removeFromRight (6);
        enable.setBounds (r.removeFromRight (44));
    }

    // Drag vertically on the curve/out text to edit them.
    juce::Rectangle<int> curveArea() const
    {
        return { 30 + 150 + 6 + 170 + 6 + 240 + 6 + 80 + 6 + 130 + 6, 0, 130, getHeight() };
    }
    void mouseDown (const juce::MouseEvent& e) override { startCurve = slot().curve; startOut = slot().output; editingOut = e.x > curveArea().getCentreX(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! curveArea().contains (e.getMouseDownPosition())) return;
        auto s = slot();
        const float d = (float) -e.getDistanceFromDragStartY() / 100.0f;
        if (editingOut) s.output = juce::jlimit (0.0f, 1.0f, startOut + d);
        else s.curve = juce::jlimit (-1.0f, 1.0f, startCurve + d);
        owner.proc.setModSlot (index, s);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! curveArea().contains (e.getPosition())) return;
        auto s = slot();
        if (e.x > curveArea().getCentreX()) s.output = 1.0f; else s.curve = 0.0f;
        owner.proc.setModSlot (index, s);
    }

    void showSourceMenu (bool isAux)
    {
        juce::PopupMenu m, envs, lfos, macros, midi, other;
        const auto current = isAux ? slot().aux : slot().source;
        auto add = [&] (juce::PopupMenu& menu, ModSource s, const juce::String& label = {})
        { menu.addItem ((int) s + 1, label.isNotEmpty() ? label : modSourceNames()[(int) s], true, s == current); };
        add (m, ModSource::none);
        for (int e = 0; e < kNumEnvelopes; ++e) add (envs, envSource (e));
        for (int l = 0; l < kNumLfos; ++l) add (lfos, lfoSource (l));
        for (int k = 0; k < kNumMacros; ++k) add (macros, macroSource (k), "Macro " + juce::String (k + 1) + "  (" + owner.proc.getMacroName (k) + ")");
        for (auto s : { ModSource::velocity, ModSource::note, ModSource::modWheel, ModSource::pitchBend, ModSource::aftertouch,
                        ModSource::polyAftertouch, ModSource::releaseVelocity, ModSource::gate }) add (midi, s);
        for (auto s : { ModSource::random1, ModSource::random2, ModSource::constant, ModSource::mpeSlide, ModSource::mpePressure }) add (other, s);
        m.addSubMenu ("Envelopes", envs);
        m.addSubMenu ("LFOs", lfos);
        m.addSubMenu ("Macros", macros);
        m.addSubMenu ("Note / MIDI", midi);
        m.addSubMenu ("Random / MPE / Constant", other);
        juce::Component::SafePointer<Row> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (isAux ? &aux : &src), [safe, isAux] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto s = safe->slot();
            (isAux ? s.aux : s.source) = (ModSource) (r - 1);
            safe->owner.proc.setModSlot (safe->index, s);
        });
    }

    void showDestMenu()
    {
        juce::PopupMenu m;
        const int current = slot().dest;
        m.addItem (100000, "None", true, current < 0);
        for (auto& [group, list] : ParamRegistry::get().modDestinationGroups())
        {
            juce::PopupMenu sub;
            bool ticked = false;
            for (int p : list)
            {
                sub.addItem (p + 1, ParamRegistry::get().spec (p).name, true, p == current);
                ticked = ticked || p == current;
            }
            m.addSubMenu (group, sub, true, nullptr, ticked);
        }
        juce::Component::SafePointer<Row> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&dst), [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto s = safe->slot();
            s.dest = r == 100000 ? -1 : r - 1;
            safe->owner.proc.setModSlot (safe->index, s);
        });
    }

    MatrixPage& owner;
    const int index;
    juce::TextButton src, dst, aux, type, enable, remove { "x" };
    ParamBar amount;
    float startCurve = 0.0f, startOut = 1.0f;
    bool editingOut = false;
};

MatrixPage::MatrixPage (EditorServices& s) : services (s), proc (s.getProcessor())
{
    for (int i = 0; i < kNumModSlots; ++i)
    {
        rowComps.push_back (std::make_unique<Row> (*this, i));
        rows.addAndMakeVisible (*rowComps.back());
    }
    viewport.setViewedComponent (&rows, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    addAndMakeVisible (activeOnly);
    addAndMakeVisible (clearAll);
    activeOnly.onClick = [this] { layoutRows(); };
    clearAll.onClick = [this] { for (int i = 0; i < kNumModSlots; ++i) if (proc.getModSlots()[(size_t) i].source != ModSource::none) proc.removeModSlot (i); };
}

MatrixPage::~MatrixPage() = default;

void MatrixPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    theme::drawPanel (g, r.toFloat());
    auto head = r.reduced (10, 0).removeFromTop (34).withTrimmedTop (6);
    drawSectionTitle (g, "MODULATION MATRIX", head.removeFromLeft (200), theme::text);
    auto cols = r.reduced (10, 0).withTrimmedTop (40).removeFromTop (18);
    cols.removeFromLeft (30);
    g.setFont (theme::label (10.0f));
    g.setColour (theme::textFaint);
    for (auto [label, w] : std::initializer_list<std::pair<const char*, int>> { { "SOURCE", 156 }, { "AMOUNT", 176 }, { "DESTINATION", 246 }, { "TYPE", 86 }, { "AUX (MULTIPLY)", 136 }, { "CURVE / OUTPUT (drag)", 136 } })
        g.drawText (label, cols.removeFromLeft (w), juce::Justification::centredLeft);
}

void MatrixPage::resized()
{
    auto r = getLocalBounds().reduced (8).reduced (10, 0);
    auto head = r.removeFromTop (34).withTrimmedTop (6);
    clearAll.setBounds (head.removeFromRight (90).reduced (0, 2));
    head.removeFromRight (10);
    activeOnly.setBounds (head.removeFromRight (180));
    r.removeFromTop (24);
    viewport.setBounds (r.withTrimmedBottom (8));
    layoutRows();
}

void MatrixPage::layoutRows()
{
    const int w = viewport.getWidth() - 10;
    int y = 0;
    bool shownEmpty = false;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const bool used = proc.getModSlots()[(size_t) i].source != ModSource::none || proc.getModSlots()[(size_t) i].dest >= 0;
        bool visible = ! activeOnly.getToggleState() || used;
        if (activeOnly.getToggleState() && ! used && ! shownEmpty) { visible = true; shownEmpty = true; }
        rowComps[(size_t) i]->setVisible (visible);
        if (visible) { rowComps[(size_t) i]->setBounds (0, y, w, 30); y += 32; }
    }
    rows.setSize (w, y);
}

void MatrixPage::refresh()
{
    if (proc.structureVersion.load() != lastStructure)
    {
        lastStructure = proc.structureVersion.load();
        for (auto& r : rowComps) r->refreshRow();
        if (activeOnly.getToggleState()) layoutRows();
    }
}

//==============================================================================
GlobalPage::GlobalPage (EditorServices& s) : services (s), proc (s.getProcessor())
{
    for (auto p : { Global::tune, Global::transpose, Global::bendUp, Global::bendDown, Global::velToAmp, Global::mpeBendRange, Global::polyphony })
    {
        widgets.push_back (std::make_unique<ModKnob> (s, global (p)));
        tuneRow.push_back (widgets.back().get());
    }
    auto q = std::make_unique<ParamMenu> (s, global (Global::quality));
    q->setPrefix ("QUALITY");
    tuneRow.push_back (q.get());
    widgets.push_back (std::move (q));
    widgets.push_back (std::make_unique<ParamToggle> (s, global (Global::mpeEnable), ParamToggle::Style::pill, "MPE"));
    tuneRow.push_back (widgets.back().get());
    for (auto& w : widgets) addAndMakeVisible (*w);

    for (auto* l : { &presetFolderLabel, &serumFoldersLabel, &pluginStatus, &aboutText })
    {
        l->setFont (theme::value (12.5f));
        l->setColour (juce::Label::textColourId, theme::textDim);
        l->setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (*l);
    }
    for (auto* b : { &changePresetFolder, &openPresetFolder, &addSerumFolder, &removeSerumFolder, &detectSerum, &scanPlugins, &addPluginFile })
        addAndMakeVisible (b);

    changePresetFolder.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose the preset folder", Settings::get().getUserPresetFolder());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().isDirectory()) { Settings::get().setString ("presetFolder", fc.getResult().getFullPathName()); refreshFolders(); }
        });
    };
    openPresetFolder.onClick = [] { Settings::get().getUserPresetFolder().startAsProcess(); };
    addSerumFolder.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Add a Serum presets / tables folder", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            if (! fc.getResult().isDirectory()) return;
            auto f = Settings::get().getSerumFolders();
            f.addIfNotAlreadyThere (fc.getResult().getFullPathName());
            Settings::get().setSerumFolders (f);
            SerumImport::rescanSerumFolders();
            refreshFolders();
        });
    };
    removeSerumFolder.onClick = [this]
    {
        auto f = Settings::get().getSerumFolders();
        if (! f.isEmpty()) f.remove (f.size() - 1);
        Settings::get().setSerumFolders (f);
        SerumImport::rescanSerumFolders();
        refreshFolders();
    };
    detectSerum.onClick = [this]
    {
        auto f = Settings::get().getSerumFolders();
        for (auto& d : Settings::detectSerumFolders()) f.addIfNotAlreadyThere (d);
        Settings::get().setSerumFolders (f);
        SerumImport::rescanSerumFolders();
        refreshFolders();
        services.showStatus (f.isEmpty() ? "No Serum folders were found automatically." : "Serum folders updated.", true);
    };
    scanPlugins.onClick = [this] { proc.getPluginHost().startScan(); };
    addPluginFile.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Add a VST3/AU plugin", juce::File(), "*.vst3;*.component");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File()) return;
            juce::String err;
            const int n = proc.getPluginHost().addPluginFile (fc.getResult(), err);
            services.showStatus (n > 0 ? "Added " + juce::String (n) + " plugin(s)." : err, true);
        });
    };

    zoom.addItemList ({ "75%", "90%", "100%", "110%", "125%", "150%", "175%", "200%" }, 1);
    const float scales[] = { 0.75f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f };
    const float current = Settings::get().getUiScale();
    for (int i = 0; i < 8; ++i) if (std::abs (scales[i] - current) < 0.01f) zoom.setSelectedId (i + 1, juce::dontSendNotification);
    zoom.onChange = [this, scales]
    {
        const float s = scales[juce::jlimit (0, 7, zoom.getSelectedId() - 1)];
        if (onZoom) onZoom (s);
    };
    addAndMakeVisible (zoom);

    aboutText.setText ("CUREUM " CUREUM_VERSION_STRING "  -  wavetable synthesizer.\n"
                       "Reads Serum 1 (.fxp/.fxb) and Serum 2 (.SerumPreset/.SerumPack) presets for interoperability. "
                       "Serum is a trademark of Xfer Records; CUREUM is an independent project and is not affiliated with Xfer Records.\n"
                       "Built with JUCE. Fonts: Barlow (SIL Open Font License). Zstandard (BSD).", juce::dontSendNotification);
    refreshFolders();
}

void GlobalPage::refreshFolders()
{
    presetFolderLabel.setText ("User presets: " + Settings::get().getUserPresetFolder().getFullPathName(), juce::dontSendNotification);
    const auto f = Settings::get().getSerumFolders();
    serumFoldersLabel.setText (f.isEmpty() ? juce::String ("No Serum folders configured. Add your Serum Presets / Serum 2 Presets folder so CUREUM can browse "
                                                            "your Serum library and load the original wavetables and noise samples.")
                                           : "Serum folders:\n" + f.joinIntoString ("\n"), juce::dontSendNotification);
}

void GlobalPage::refresh()
{
    auto& host = proc.getPluginHost();
    juce::String t = juce::String (host.getEffectPlugins().size()) + " effect plugins known.";
    if (host.isScanning())
        t = "Scanning " + juce::String (juce::roundToInt (host.getScanProgress() * 100.0f)) + "%  " + host.getCurrentlyScanning();
    if (pluginStatus.getText() != t) pluginStatus.setText (t, juce::dontSendNotification);
}

void GlobalPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    const int h1 = 130, h2 = 210, h3 = 110;
    auto s1 = r.removeFromTop (h1); r.removeFromTop (8);
    auto s2 = r.removeFromTop (h2); r.removeFromTop (8);
    auto s3row = r.removeFromTop (h3); r.removeFromTop (8);
    auto s3 = s3row.removeFromLeft (s3row.getWidth() / 2 - 4); s3row.removeFromLeft (8);
    for (auto sec : { s1, s2, s3, s3row, r }) theme::drawPanel (g, sec.toFloat());
    drawSectionTitle (g, "TUNING, VOICING AND QUALITY", s1.reduced (12, 0).removeFromTop (28), theme::text);
    drawSectionTitle (g, "LIBRARY FOLDERS", s2.reduced (12, 0).removeFromTop (28), theme::text);
    drawSectionTitle (g, "PLUGIN HOSTING (VST3 / AU EFFECTS)", s3.reduced (12, 0).removeFromTop (28), theme::text);
    drawSectionTitle (g, "INTERFACE", s3row.reduced (12, 0).removeFromTop (28), theme::text);
    drawSectionTitle (g, "ABOUT", r.reduced (12, 0).removeFromTop (28), theme::text);
}

void GlobalPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto s1 = r.removeFromTop (130).reduced (12, 0).withTrimmedTop (30).withTrimmedBottom (8); r.removeFromTop (8);
    auto s2 = r.removeFromTop (210).reduced (12, 0).withTrimmedTop (30).withTrimmedBottom (8); r.removeFromTop (8);
    auto s3row = r.removeFromTop (110); r.removeFromTop (8);
    auto s3 = s3row.removeFromLeft (s3row.getWidth() / 2 - 4).reduced (12, 0).withTrimmedTop (30).withTrimmedBottom (8);
    s3row.removeFromLeft (8);
    auto s4 = s3row.reduced (12, 0).withTrimmedTop (30).withTrimmedBottom (8);
    auto s5 = r.reduced (12, 0).withTrimmedTop (30).withTrimmedBottom (8);

    auto knobs = s1;
    for (auto* c : tuneRow)
    {
        if (dynamic_cast<ModKnob*> (c) != nullptr) c->setBounds (knobs.removeFromLeft (70).withSizeKeepingCentre (62, 66));
        else { knobs.removeFromLeft (10); c->setBounds (knobs.removeFromLeft (c == tuneRow.back() ? 60 : 170).withSizeKeepingCentre (c == tuneRow.back() ? 60 : 170, 24)); }
    }

    auto row = s2.removeFromTop (24);
    openPresetFolder.setBounds (row.removeFromRight (70));
    row.removeFromRight (6);
    changePresetFolder.setBounds (row.removeFromRight (90));
    presetFolderLabel.setBounds (row);
    s2.removeFromTop (8);
    auto btns = s2.removeFromRight (120);
    addSerumFolder.setBounds (btns.removeFromTop (26)); btns.removeFromTop (6);
    removeSerumFolder.setBounds (btns.removeFromTop (26)); btns.removeFromTop (6);
    detectSerum.setBounds (btns.removeFromTop (26));
    serumFoldersLabel.setBounds (s2.withTrimmedRight (10));

    auto pb = s3.removeFromTop (26);
    scanPlugins.setBounds (pb.removeFromLeft (150)); pb.removeFromLeft (6);
    addPluginFile.setBounds (pb.removeFromLeft (150));
    pluginStatus.setBounds (s3.withTrimmedTop (6));

    zoom.setBounds (s4.removeFromTop (26).removeFromLeft (120));
    aboutText.setBounds (s5);
}

} // namespace cureum
