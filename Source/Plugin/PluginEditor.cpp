#include "PluginEditor.h"
#include "../Core/Settings.h"
#include "../Presets/WavetableIO.h"

namespace cureum
{

namespace
{
    void refreshRecursive (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (! child->isVisible()) continue;
            if (auto* r = dynamic_cast<Refreshable*> (child)) r->refresh();
            refreshRecursive (*child);
        }
    }

    juce::String topicForTitle (const juce::String& t)
    {
        const auto u = t.toUpperCase();
        if (u.startsWith ("OSC")) return "oscillators";
        if (u.startsWith ("SUB") || u.startsWith ("NOISE")) return "sub-noise";
        if (u.contains ("FILTER")) return "filters";
        if (u.contains ("MACRO")) return "macros";
        if (u.contains ("VOICING")) return "voicing";
        if (u.contains ("ENV")) return "envelopes";
        if (u.contains ("LFO")) return "lfos";
        return "effects";
    }
}

//==============================================================================
class CureumEditor::Header : public juce::Component, public Refreshable
{
public:
    explicit Header (CureumEditor& e) : editor (e), master (e, pid::global (pid::Global::master), ModKnob::Style::small, "MASTER")
    {
        for (auto* b : { &prev, &next, &save, &menu, &help, &whatsThis }) addAndMakeVisible (b);
        prev.onClick = [this] { juce::String m; editor.presets.step (-1, m); editor.showStatus (m, true); };
        next.onClick = [this] { juce::String m; editor.presets.step (1, m); editor.showStatus (m, true); };
        save.onClick = [this] { editor.savePresetDialog(); };
        menu.onClick = [this] { editor.showMainMenu(); };
        help.onClick = [this] { editor.openHelp ("overview"); };
        whatsThis.onClick = [this] { editor.toggleHelpMode(); };
        const char* names[] = { "OSC", "FX", "MATRIX", "GLOBAL" };
        for (int i = 0; i < 4; ++i)
        {
            tabs[i].setButtonText (names[i]);
            tabs[i].setClickingTogglesState (false);
            tabs[i].onClick = [this, i] { editor.selectPage (i); };
            addAndMakeVisible (tabs[i]);
        }
        addAndMakeVisible (master);
    }

    juce::Rectangle<int> presetArea() const { return { 170, 9, 330, 34 }; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (theme::header.brighter (0.05f), 0, 0, theme::header, 0, r.getBottom(), false));
        g.fillRect (r);
        g.setColour (theme::outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, r.getRight());

        // logo
        juce::GlyphArrangement ga;
        ga.addLineOfText (theme::font (25.0f, 3).withExtraKerningFactor (0.12f), "CUREUM", 16.0f, 35.0f);
        juce::Path logo;
        ga.createPath (logo);
        g.setGradientFill (juce::ColourGradient (theme::accent, 16.0f, 0.0f, theme::mod, 150.0f, 0.0f, false));
        g.fillPath (logo);
        g.setColour (theme::textFaint);
        g.setFont (theme::label (8.5f));
        g.drawText ("WAVETABLE SYNTH", 17, 37, 140, 10, juce::Justification::left);

        // preset box
        auto pa = presetArea().toFloat();
        g.setColour (juce::Colour (0xff070a0f));
        g.fillRoundedRectangle (pa, 6.0f);
        g.setColour (hoverPreset ? theme::outlineHi : theme::outline);
        g.drawRoundedRectangle (pa.reduced (0.5f), 6.0f, 1.0f);
        auto text = presetArea().reduced (28, 2);
        g.setColour (theme::text);
        g.setFont (theme::font (15.0f, 2));
        g.drawFittedText (editor.proc.presetInfo.name, text.removeFromTop (19), juce::Justification::centred, 1);
        g.setColour (theme::textFaint);
        g.setFont (theme::value (10.5f));
        auto sub = editor.proc.presetInfo.author.isNotEmpty() ? "by " + editor.proc.presetInfo.author : juce::String ("click to browse presets");
        if (editor.proc.presetInfo.category.isNotEmpty()) sub = editor.proc.presetInfo.category + "  -  " + sub;
        g.drawFittedText (sub, text, juce::Justification::centred, 1);

        // meters and stats
        auto meter = juce::Rectangle<float> ((float) getWidth() - 140.0f, 12.0f, 10.0f, 28.0f);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto bar = meter.withWidth (4.0f).translated ((float) ch * 5.0f, 0.0f);
            g.setColour (theme::track);
            g.fillRoundedRectangle (bar, 1.5f);
            const float pk = ch == 0 ? editor.proc.feedback.peakL.load() : editor.proc.feedback.peakR.load();
            const float db = juce::Decibels::gainToDecibels (pk, -60.0f);
            const float h = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f) * bar.getHeight();
            g.setColour (pk > 1.0f ? theme::warn : (pk > 0.7f ? theme::gold : theme::good));
            g.fillRoundedRectangle (bar.withTrimmedTop (bar.getHeight() - h), 1.5f);
        }
        g.setFont (theme::value (11.0f));
        g.setColour (theme::textDim);
        g.drawText ("VOICES " + juce::String (editor.proc.feedback.activeVoices.load()), getWidth() - 122, 10, 110, 16, juce::Justification::left);
        g.drawText ("CPU " + juce::String (juce::roundToInt (editor.proc.feedback.cpuLoad.load() * 100.0f)) + "%", getWidth() - 122, 26, 110, 16, juce::Justification::left);
    }

    void resized() override
    {
        auto pa = presetArea();
        prev.setBounds (pa.getX() + 4, pa.getY() + 7, 20, 20);
        next.setBounds (pa.getRight() - 24, pa.getY() + 7, 20, 20);
        save.setBounds (pa.getRight() + 8, 12, 28, 28);
        menu.setBounds (pa.getRight() + 40, 12, 28, 28);
        int x = pa.getRight() + 90;
        for (auto& t : tabs) { t.setBounds (x, 13, 82, 26); x += 86; }
        help.setBounds (x + 12, 12, 28, 28);
        whatsThis.setBounds (x + 44, 12, 28, 28);
        master.setBounds (getWidth() - 200, 2, 52, 48);
    }

    void refresh() override
    {
        const auto key = editor.proc.presetInfo.name + editor.proc.presetInfo.author
                       + juce::String (editor.proc.feedback.activeVoices.load()) + juce::String (juce::roundToInt (editor.proc.feedback.cpuLoad.load() * 100.0f))
                       + juce::String (juce::roundToInt (editor.proc.feedback.peakL.load() * 40.0f)) + juce::String (juce::roundToInt (editor.proc.feedback.peakR.load() * 40.0f));
        if (key != lastKey) { lastKey = key; repaint(); }
        master.refresh();
        whatsThis.active = editor.helpMode;
    }

    void setPage (int p)
    {
        for (int i = 0; i < 4; ++i) tabs[i].setToggleState (i == p, juce::dontSendNotification);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool h = presetArea().contains (e.getPosition());
        if (h != hoverPreset) { hoverPreset = h; repaint(); }
        if (h) editor.showStatus ("Preset: " + editor.proc.presetInfo.name + "  -  click to open the preset browser; use the arrows for previous / next.");
    }
    void mouseExit (const juce::MouseEvent&) override { hoverPreset = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (presetArea().reduced (26, 0).contains (e.getPosition())) editor.showBrowser();
    }

    CureumEditor& editor;
    IconButton prev { IconButton::Icon::left, "Previous preset", "Loads the previous preset of the current list." };
    IconButton next { IconButton::Icon::right, "Next preset", "Loads the next preset of the current list." };
    IconButton save { IconButton::Icon::save, "Save preset", "Saves the current sound as a CUREUM preset (Ctrl/Cmd+S)." };
    IconButton menu { IconButton::Icon::menu, "Menu", "Init, open and import presets or banks, zoom and more." };
    IconButton help { IconButton::Icon::help, "Help", "Opens the searchable help browser (F1)." };
    IconButton whatsThis { IconButton::Icon::whatsThis, "What's this?", "Toggles help mode: click any control to see what it does." };
    juce::TextButton tabs[4];
    ModKnob master;
    juce::String lastKey;
    bool hoverPreset = false;
};

//==============================================================================
class CureumEditor::OscPage : public juce::Component
{
public:
    explicit OscPage (CureumEditor& e) : filters (e), sub (e), noise (e), mod (e), macros (e), voicing (e),
        keyboard (e.proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        for (int o = 0; o < kNumOscillators; ++o)
        {
            osc[o] = std::make_unique<OscPanel> (e, o);
            osc[o]->onEditWavetable = [&e] (int which) { e.showWavetableEditor (which); };
            addAndMakeVisible (*osc[o]);
        }
        for (auto* c : std::initializer_list<juce::Component*> { &filters, &sub, &noise, &mod, &macros, &voicing, &keyboard })
            addAndMakeVisible (c);
        keyboard.setAvailableRange (24, 108);
        keyboard.setLowestVisibleKey (36);
        keyboard.setKeyWidth (17.0f);
        keyboard.setScrollButtonsVisible (true);
        keyboard.setWantsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override
    {
        theme::drawPanel (g, keyboardFrame.toFloat());
    }

    void resized() override
    {
        const int m = 8;
        int x = m;
        for (int o = 0; o < kNumOscillators; ++o) { osc[o]->setBounds (x, m, 282, 352); x += 290; }
        filters.setBounds (x, m, getWidth() - x - m, 352);
        const int y2 = m + 352 + 8, h2 = 226;
        sub.setBounds (m, y2, 128, h2);
        noise.setBounds (144, y2, 196, h2);
        mod.setBounds (348, y2, 556, h2);
        macros.setBounds (912, y2, getWidth() - 912 - m, h2);
        const int y3 = y2 + h2 + 8, h3 = getHeight() - y3 - m;
        voicing.setBounds (m, y3, 330, h3);
        keyboardFrame = juce::Rectangle<int> (346, y3, getWidth() - 346 - m, h3);
        keyboard.setBounds (keyboardFrame.reduced (8, 8));
    }

    std::unique_ptr<OscPanel> osc[kNumOscillators];
    FilterPanel filters;
    SubPanel sub;
    NoisePanel noise;
    ModulationPanel mod;
    MacroPanel macros;
    VoicingPanel voicing;
    juce::MidiKeyboardComponent keyboard;
    juce::Rectangle<int> keyboardFrame;
};

//==============================================================================
class CureumEditor::HelpModeOverlay : public juce::Component
{
public:
    explicit HelpModeOverlay (CureumEditor& e) : editor (e) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::gold.withAlpha (0.8f));
        g.drawRect (getLocalBounds(), 2);
        auto banner = getLocalBounds().removeFromTop (22).withSizeKeepingCentre (520, 22).translated (0, 54);
        g.setColour (theme::gold.withAlpha (0.92f));
        g.fillRoundedRectangle (banner.toFloat(), 5.0f);
        g.setColour (theme::background);
        g.setFont (theme::label (11.5f));
        g.drawText ("HELP MODE - click any control to learn what it does (Esc to exit)", banner, juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override;
    CureumEditor& editor;
};

//==============================================================================
class CureumEditor::MainView : public juce::Component
{
public:
    explicit MainView (CureumEditor& e)
        : header (e), oscPage (e), fxPage (e), matrixPage (e), globalPage (e), browser (e, e.presets), help (e), helpOverlay (e)
    {
        addAndMakeVisible (header);
        addAndMakeVisible (oscPage);
        addChildComponent (fxPage);
        addChildComponent (matrixPage);
        addChildComponent (globalPage);
        addChildComponent (browser);
        addChildComponent (help);
        addChildComponent (helpOverlay);
        browser.onClose = [this] { browser.setVisible (false); };
        help.onClose = [this] { help.setVisible (false); };
        globalPage.onZoom = [&e] (float s) { e.setScale (s); };
    }

    juce::Rectangle<int> pageArea() const { return getLocalBounds().withTrimmedTop (52).withTrimmedBottom (24); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::background);
        auto status = getLocalBounds().removeFromBottom (24);
        g.setColour (theme::header);
        g.fillRect (status);
        g.setColour (theme::outline);
        g.drawHorizontalLine (status.getY(), 0.0f, (float) getWidth());
        const bool message = juce::Time::getMillisecondCounter() - messageTime < 6000;
        g.setFont (theme::value (12.5f));
        g.setColour (message ? theme::accent : theme::textDim);
        g.drawFittedText (statusText, status.reduced (12, 0).withTrimmedRight (160), juce::Justification::centredLeft, 1);
        g.setColour (theme::textFaint);
        g.setFont (theme::label (10.0f));
        g.drawText ("CUREUM " CUREUM_VERSION_STRING, status.reduced (12, 0), juce::Justification::centredRight);
    }

    void resized() override
    {
        header.setBounds (getLocalBounds().removeFromTop (52));
        const auto pa = pageArea();
        for (auto* p : std::initializer_list<juce::Component*> { &oscPage, &fxPage, &matrixPage, &globalPage, &browser, &help })
            p->setBounds (pa);
        if (wtEditor != nullptr) wtEditor->setBounds (pa);
        helpOverlay.setBounds (getLocalBounds());
    }

    Header header;
    OscPage oscPage;
    FxPage fxPage;
    MatrixPage matrixPage;
    GlobalPage globalPage;
    PresetBrowser browser;
    HelpOverlay help;
    std::unique_ptr<WavetableEditor> wtEditor;
    HelpModeOverlay helpOverlay;
    juce::String statusText { "Welcome to CUREUM. Hover over any control for help, press F1 for the manual." };
    juce::uint32 messageTime = 0;
    int page = 0;
};

void CureumEditor::HelpModeOverlay::mouseDown (const juce::MouseEvent& e)
{
    auto& v = *editor.view;
    setInterceptsMouseClicks (false, false);
    auto* target = v.getComponentAt (e.getEventRelativeTo (&v).getPosition());
    setInterceptsMouseClicks (true, true);

    // help mode buttons stay usable
    if (target == &v.header.whatsThis) { editor.toggleHelpMode(); return; }

    juce::String title, body, topic = "overview";
    for (auto* c = target; c != nullptr && c != &v; c = c->getParentComponent())
    {
        if (auto* h = dynamic_cast<HelpTextProvider*> (c))
        {
            title = h->getHelpTitle(); body = h->getHelpBody(); topic = h->getHelpTopic();
            break;
        }
        if (auto* p = dynamic_cast<SectionPanel*> (c))
        {
            juce::ignoreUnused (p);
            topic = topicForTitle (c->getName());
        }
    }
    if (title.isEmpty())
    {
        title = "CUREUM";
        body = "This area has no specific help. Click a knob, button, display or menu - or press F1 to browse the manual.";
    }
    auto bubble = std::make_unique<HelpBubble> (editor, title, body, topic);
    auto* top = editor.getTopLevelComponent();
    const auto area = top->getLocalArea (&v, juce::Rectangle<int> (1, 1).withPosition (e.getEventRelativeTo (&v).getPosition()));
    juce::CallOutBox::launchAsynchronously (std::move (bubble), area.expanded (8), top);
}

//==============================================================================
CureumEditor::CureumEditor (CureumProcessor& p) : AudioProcessorEditor (p), proc (p), presets (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    view = std::make_unique<MainView> (*this);
    addAndMakeVisible (*view);
    view->setBounds (0, 0, kBaseWidth, kBaseHeight);
    selectPage (0);

    setResizable (true, true);
    setResizeLimits (kBaseWidth / 2, kBaseHeight / 2, kBaseWidth * 2, kBaseHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);
    scale = juce::jlimit (0.5f, 2.0f, Settings::get().getUiScale());
    setSize ((int) (kBaseWidth * scale), (int) (kBaseHeight * scale));
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

CureumEditor::~CureumEditor()
{
    stopTimer();
    view.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void CureumEditor::paint (juce::Graphics& g) { g.fillAll (theme::background); }

void CureumEditor::resized()
{
    scale = (float) getWidth() / (float) kBaseWidth;
    view->setTransform (juce::AffineTransform::scale (scale));
}

void CureumEditor::setScale (float s)
{
    Settings::get().setUiScale (s);
    setSize ((int) (kBaseWidth * s), (int) (kBaseHeight * s));
}

void CureumEditor::selectPage (int index)
{
    auto& v = *view;
    v.page = juce::jlimit (0, 3, index);
    closeOverlays();
    v.oscPage.setVisible (v.page == 0);
    v.fxPage.setVisible (v.page == 1);
    v.matrixPage.setVisible (v.page == 2);
    v.globalPage.setVisible (v.page == 3);
    v.header.setPage (v.page);
}

void CureumEditor::closeOverlays()
{
    auto& v = *view;
    v.browser.setVisible (false);
    v.help.setVisible (false);
    if (v.wtEditor != nullptr)
    {
        v.removeChildComponent (v.wtEditor.get());
        v.wtEditor.reset();
    }
}

void CureumEditor::showStatus (const juce::String& text, bool isMessage)
{
    auto& v = *view;
    if (! isMessage && juce::Time::getMillisecondCounter() - v.messageTime < 2500)
        return;   // keep important messages on screen for a moment
    v.statusText = text;
    if (isMessage) v.messageTime = juce::Time::getMillisecondCounter();
    v.repaint (v.getLocalBounds().removeFromBottom (24));
}

void CureumEditor::openHelp (const juce::String& topicId, const juce::String& focusText)
{
    auto& v = *view;
    if (helpMode) toggleHelpMode();
    v.browser.setVisible (false);
    v.help.setVisible (true);
    v.help.toFront (true);
    v.help.show (topicId, focusText);
}

void CureumEditor::showBrowser()
{
    auto& v = *view;
    v.help.setVisible (false);
    v.browser.setVisible (true);
    v.browser.toFront (true);
    v.browser.open();
}

void CureumEditor::showWavetableEditor (int osc)
{
    auto& v = *view;
    closeOverlays();
    v.wtEditor = std::make_unique<WavetableEditor> (*this, osc);
    v.wtEditor->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<CureumEditor> (this)] { if (safe != nullptr) safe->closeOverlays(); }); };
    v.addAndMakeVisible (*v.wtEditor);
    v.wtEditor->setBounds (v.pageArea());
    v.wtEditor->grabKeyboardFocus();
    v.helpOverlay.toFront (false);
}

void CureumEditor::toggleHelpMode()
{
    helpMode = ! helpMode;
    auto& v = *view;
    v.helpOverlay.setVisible (helpMode);
    if (helpMode) v.helpOverlay.toFront (false);
    showStatus (helpMode ? "Help mode: click any control to learn what it does. Press Esc or the arrow button to leave help mode." : "Help mode off.", true);
    v.header.whatsThis.active = helpMode;
    v.header.whatsThis.repaint();
}

void CureumEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Presets are saved in " + Settings::get().getUserPresetFolder().getFullPathName(),
                                     juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", proc.presetInfo.name == "Init" ? juce::String ("My Sound") : proc.presetInfo.name, "Name:");
    w->addTextEditor ("folder", {}, "Sub-folder (optional):");
    w->addTextEditor ("author", proc.presetInfo.author, "Author:");
    w->addTextEditor ("tags", proc.presetInfo.tags.joinIntoString (", "), "Tags:");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<CureumEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1) return;
        safe->proc.presetInfo.author = w->getTextEditorContents ("author");
        safe->proc.presetInfo.tags = juce::StringArray::fromTokens (w->getTextEditorContents ("tags"), ",", "");
        safe->proc.presetInfo.tags.trim();
        safe->proc.presetInfo.tags.removeEmptyStrings();
        juce::String msg;
        safe->presets.saveCurrent (w->getTextEditorContents ("name"), w->getTextEditorContents ("folder"), msg);
        safe->showStatus (msg, true);
    }), true);
}

void CureumEditor::showMainMenu()
{
    juce::PopupMenu m, zoom;
    m.addItem (1, "Init preset");
    m.addItem (2, "Open preset file...");
    m.addItem (3, "Import bank / pack (.fxb, .zip, .SerumPack)...");
    m.addItem (4, "Import folder of presets...");
    m.addItem (5, "Save preset...", true, false);
    m.addItem (6, "Preset browser");
    m.addSeparator();
    const float scales[] = { 0.75f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f };
    for (int i = 0; i < 8; ++i) zoom.addItem (100 + i, juce::String (juce::roundToInt (scales[i] * 100.0f)) + "%", true, std::abs (scale - scales[i]) < 0.02f);
    m.addSubMenu ("Zoom", zoom);
    m.addItem (7, "Rescan Serum folders");
    m.addSeparator();
    m.addItem (8, "Help");
    m.addItem (9, "What's this? (help mode)", true, helpMode);
    juce::Component::SafePointer<CureumEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&view->header.menu), [safe, scales] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& ed = *safe;
        switch (r)
        {
            case 1: ed.presets.initPreset(); ed.showStatus ("Init preset loaded.", true); break;
            case 2: ed.showBrowser(); ed.view->browser.importFile(); break;
            case 3: ed.showBrowser(); ed.view->browser.importBank (false); break;
            case 4: ed.showBrowser(); ed.view->browser.importBank (true); break;
            case 5: ed.savePresetDialog(); break;
            case 6: ed.showBrowser(); break;
            case 7: SerumImport::rescanSerumFolders(); ed.presets.rescan(); ed.showStatus ("Serum folders rescanned.", true); break;
            case 8: ed.openHelp ("overview"); break;
            case 9: ed.toggleHelpMode(); break;
            default:
                if (r >= 100 && r < 108) ed.setScale (scales[r - 100]);
                break;
        }
    });
}

bool CureumEditor::keyPressed (const juce::KeyPress& k)
{
    auto& v = *view;
    if (k == juce::KeyPress::F1Key)
    {
        if (v.help.isVisible()) v.help.setVisible (false);
        else openHelp ("overview");
        return true;
    }
    if (k == juce::KeyPress::escapeKey)
    {
        if (helpMode) { toggleHelpMode(); return true; }
        if (v.browser.isVisible() || v.help.isVisible() || v.wtEditor != nullptr) { closeOverlays(); return true; }
    }
    if (k.getModifiers().isCommandDown() && k.getKeyCode() == 'S') { savePresetDialog(); return true; }
    return false;
}

bool CureumEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (SerumImport::isImportableExtension (ext) || ext == ".wav" || ext == ".aif" || ext == ".aiff" || juce::File (f).isDirectory())
            return true;
    }
    return false;
}

void CureumEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (files.isEmpty()) return;
    const juce::File f (files[0]);
    const auto ext = f.getFileExtension().toLowerCase();
    if (ext == ".wav" || ext == ".aif" || ext == ".aiff")
    {
        // Dropped onto an oscillator? Otherwise Osc A.
        int osc = 0;
        const auto p = view->getLocalPoint (this, juce::Point<int> (x, y));
        for (int o = 0; o < kNumOscillators; ++o)
            if (view->oscPage.isVisible() && view->oscPage.osc[o]->getBounds().translated (0, 52).contains (p)) osc = o;
        juce::String err;
        if (auto t = WavetableIO::loadWavetable (f, err))
        {
            proc.setWavetable (osc, t);
            showStatus ("Loaded wavetable " + t->getName() + " into Osc " + oscLetter (osc), true);
        }
        else showStatus ("Could not load " + f.getFileName() + ": " + err, true);
        return;
    }
    loadFile (f);
}

void CureumEditor::loadFile (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    juce::String msg;
    if (f.isDirectory() || ext == ".fxb" || ext == ".zip" || ext == ".serumpack")
    {
        presets.importBank (f, msg);
        showStatus (msg, true);
        showBrowser();
        return;
    }
    presets.loadFile (f, msg);
    if (! presets.lastWarnings.isEmpty()) msg << "  -  " << presets.lastWarnings.size() << " import note(s): " << presets.lastWarnings[0];
    showStatus (msg, true);
}

void CureumEditor::timerCallback()
{
    auto& v = *view;
    if (getProperties().contains ("screenshotTab"))
    {
        const auto tab = getProperties()["screenshotTab"].toString();
        getProperties().remove ("screenshotTab");
        if (tab == "fx") selectPage (1);
        else if (tab == "matrix") selectPage (2);
        else if (tab == "global") selectPage (3);
        else if (tab == "browser") showBrowser();
        else if (tab == "help") openHelp ("modulation", "");
        else if (tab == "wteditor") showWavetableEditor (0);
        else if (tab == "lfo") v.oscPage.mod.select (4);
    }

    v.header.refresh();
    if (v.oscPage.isVisible()) v.oscPage.macros.refreshNames();
    for (auto* page : std::initializer_list<juce::Component*> { &v.oscPage, &v.fxPage, &v.matrixPage, &v.globalPage })
        if (page->isVisible())
        {
            if (auto* r = dynamic_cast<Refreshable*> (page)) r->refresh();
            refreshRecursive (*page);
        }
    if (proc.presetVersion.load() != lastPresetVersion)
    {
        lastPresetVersion = proc.presetVersion.load();
        v.header.repaint();
    }
    if (juce::Time::getMillisecondCounter() - v.messageTime > 6000 && juce::Time::getMillisecondCounter() - v.messageTime < 6100)
        v.repaint (v.getLocalBounds().removeFromBottom (24));
}

} // namespace cureum
