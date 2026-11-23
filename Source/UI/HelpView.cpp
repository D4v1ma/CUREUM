#include "HelpView.h"
#include "CureumBinaryData.h"

namespace cureum
{

const std::vector<HelpTopic>& HelpLibrary::topics()
{
    static const std::vector<HelpTopic> list = []
    {
        std::vector<std::pair<juce::String, HelpTopic>> found;
        for (int i = 0; i < CureumBinary::namedResourceListSize; ++i)
        {
            const auto* name = CureumBinary::namedResourceList[i];
            const juce::String file = CureumBinary::getNamedResourceOriginalFilename (name);
            if (! file.endsWithIgnoreCase (".md")) continue;
            int size = 0;
            const char* data = CureumBinary::getNamedResource (name, size);
            HelpTopic t;
            t.markdown = juce::String::fromUTF8 (data, size);
            auto base = file.upToLastOccurrenceOf (".", false, false);
            t.id = base.fromFirstOccurrenceOf ("-", false, false);
            if (t.id.isEmpty()) t.id = base;
            const auto firstLine = t.markdown.upToFirstOccurrenceOf ("\n", false, false);
            t.title = firstLine.startsWith ("# ") ? firstLine.substring (2).trim() : t.id;
            found.push_back ({ file, t });
        }
        std::sort (found.begin(), found.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        std::vector<HelpTopic> out;
        for (auto& f : found) out.push_back (f.second);
        return out;
    }();
    return list;
}

const HelpTopic* HelpLibrary::find (const juce::String& id)
{
    for (auto& t : topics())
        if (t.id == id) return &t;
    return nullptr;
}

std::vector<int> HelpLibrary::search (const juce::String& query)
{
    std::vector<std::pair<int, int>> scored;
    const auto words = juce::StringArray::fromTokens (query.toLowerCase(), " ", "");
    for (int i = 0; i < (int) topics().size(); ++i)
    {
        const auto& t = topics()[(size_t) i];
        int score = 0;
        for (auto& w : words)
        {
            if (w.isEmpty()) continue;
            if (t.title.toLowerCase().contains (w)) score += 10;
            int pos = 0, hits = 0;
            const auto body = t.markdown.toLowerCase();
            while ((pos = body.indexOf (pos, w)) >= 0 && hits < 20) { ++hits; pos += w.length(); }
            score += hits;
            if (hits == 0 && ! t.title.toLowerCase().contains (w)) { score = 0; break; }
        }
        if (score > 0 || query.trim().isEmpty()) scored.push_back ({ i, score });
    }
    std::stable_sort (scored.begin(), scored.end(), [] (auto& a, auto& b) { return a.second > b.second; });
    std::vector<int> r;
    for (auto& s : scored) r.push_back (s.first);
    return r;
}

//==============================================================================
namespace
{
    juce::AttributedString inlineMarkdown (const juce::String& text, float size, juce::Colour colour, int weight, const juce::String& highlight)
    {
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        bool bold = false, code = false;
        juce::String run;
        auto flush = [&]
        {
            if (run.isEmpty()) return;
            const auto font = code ? theme::font (size - 0.5f, 1) : theme::font (size, bold ? 3 : weight);
            const auto c = code ? theme::accent : (bold ? theme::text : colour);
            if (highlight.isNotEmpty() && run.containsIgnoreCase (highlight))
            {
                const int p = run.indexOfIgnoreCase (highlight);
                as.append (run.substring (0, p), font, c);
                as.append (run.substring (p, p + highlight.length()), theme::font (size, 3), theme::gold);
                as.append (run.substring (p + highlight.length()), font, c);
            }
            else
                as.append (run, font, c);
            run.clear();
        };
        for (int i = 0; i < text.length(); ++i)
        {
            if (text[i] == '*' && i + 1 < text.length() && text[i + 1] == '*' && ! code) { flush(); bold = ! bold; ++i; continue; }
            if (text[i] == '`') { flush(); code = ! code; continue; }
            run << text[i];
        }
        flush();
        return as;
    }
}

void MarkdownView::setMarkdown (const juce::String& md, const juce::String& hl)
{
    highlight = hl;
    blocks.clear();
    juce::String para;
    auto flushPara = [&]
    {
        if (para.trim().isNotEmpty()) blocks.push_back ({ Block::para, para.trim() });
        para.clear();
    };
    for (auto line : juce::StringArray::fromLines (md))
    {
        const auto t = line.trim();
        if (t.isEmpty()) { flushPara(); continue; }
        if (t.startsWith ("## ")) { flushPara(); blocks.push_back ({ Block::h2, t.substring (3) }); continue; }
        if (t.startsWith ("# "))  { flushPara(); blocks.push_back ({ Block::h1, t.substring (2) }); continue; }
        if (t.startsWith ("- "))  { flushPara(); blocks.push_back ({ Block::bullet, t.substring (2) }); continue; }
        if (t.length() > 2 && juce::CharacterFunctions::isDigit (t[0]) && t.containsChar ('.') && t.indexOfChar ('.') < 3 && t[t.indexOfChar ('.') + 1] == ' ')
        {
            flushPara();
            Block b { Block::numbered, t.fromFirstOccurrenceOf (". ", false, false) };
            b.number = t.getIntValue();
            blocks.push_back (b);
            continue;
        }
        para << (para.isEmpty() ? "" : " ") << t;
    }
    flushPara();
}

int MarkdownView::layoutForWidth (int width)
{
    int y = 6;
    for (auto& b : blocks)
    {
        float size = 14.0f;
        int weight = 0, indent = 0, spaceBefore = 6;
        auto colour = theme::textDim.brighter (0.3f);
        if (b.kind == Block::h1) { size = 24.0f; weight = 3; colour = theme::text; spaceBefore = 4; }
        if (b.kind == Block::h2) { size = 17.0f; weight = 3; colour = theme::accent; spaceBefore = 16; }
        if (b.kind == Block::bullet || b.kind == Block::numbered) { indent = 22; spaceBefore = 3; }
        auto as = inlineMarkdown (b.text, size, colour, weight, highlight);
        b.layout.createLayout (as, (float) (width - indent - 8));
        y += spaceBefore;
        b.y = y;
        b.h = (int) std::ceil (b.layout.getHeight());
        y += b.h + 2;
    }
    return y + 12;
}

int MarkdownView::getYForText (const juce::String& text) const
{
    for (auto& b : blocks)
        if (b.text.containsIgnoreCase (text)) return b.y;
    return 0;
}

void MarkdownView::paint (juce::Graphics& g)
{
    for (auto& b : blocks)
    {
        const int indent = (b.kind == Block::bullet || b.kind == Block::numbered) ? 22 : 0;
        if (b.kind == Block::bullet)
        {
            g.setColour (theme::accent);
            g.fillEllipse (8.0f, (float) b.y + 7.0f, 5.0f, 5.0f);
        }
        if (b.kind == Block::numbered)
        {
            g.setColour (theme::accent);
            g.setFont (theme::font (13.0f, 3));
            g.drawText (juce::String (b.number) + ".", 0, b.y, 20, 18, juce::Justification::centredRight);
        }
        b.layout.draw (g, juce::Rectangle<float> ((float) indent, (float) b.y, (float) getWidth() - (float) indent - 8.0f, (float) b.h + 2.0f));
    }
}

//==============================================================================
HelpOverlay::HelpOverlay (EditorServices& s) : services (s)
{
    search.setTextToShowWhenEmpty ("Search help...", theme::textFaint);
    search.setFont (theme::value (14.0f));
    search.addListener (this);
    addAndMakeVisible (search);
    addAndMakeVisible (close);
    close.onClick = [this] { if (onClose) onClose(); };
    viewport.setViewedComponent (&view, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    visibleTopics = HelpLibrary::search ({});
    setWantsKeyboardFocus (true);
}

void HelpOverlay::show (const juce::String& topicId, const juce::String& focus)
{
    search.clear();
    visibleTopics = HelpLibrary::search ({});
    int idx = 0;
    for (int i = 0; i < (int) HelpLibrary::topics().size(); ++i)
        if (HelpLibrary::topics()[(size_t) i].id == topicId) idx = i;
    focusTitle.clear(); focusBody.clear();
    if (focus.isNotEmpty())
        for (auto& sp : ParamRegistry::get().all())
            if (sp.name == focus) { focusTitle = sp.name; focusBody = sp.help; break; }
    selectTopic (idx, focus);
    grabKeyboardFocus();
}

void HelpOverlay::selectTopic (int index, const juce::String& focus)
{
    const auto& all = HelpLibrary::topics();
    if (all.empty()) return;
    current = juce::jlimit (0, (int) all.size() - 1, index);
    juce::String hl = focus;
    if (hl.isEmpty() && search.getText().trim().isNotEmpty()) hl = search.getText().trim();
    // Highlight the short label part of a parameter name ("Osc A Level" -> "Level").
    if (focusTitle.isNotEmpty()) hl = focusTitle.fromLastOccurrenceOf (" ", false, false);
    view.setMarkdown (all[(size_t) current].markdown, hl);
    resized();
    if (hl.isNotEmpty()) viewport.setViewPosition (0, std::max (0, view.getYForText (hl) - 40));
    else viewport.setViewPosition (0, 0);
    repaint();
}

void HelpOverlay::textEditorTextChanged (juce::TextEditor&)
{
    visibleTopics = HelpLibrary::search (search.getText());
    focusTitle.clear();
    if (! visibleTopics.empty()) selectTopic (visibleTopics.front());
    repaint();
}

juce::Rectangle<int> HelpOverlay::listArea() const
{
    return getLocalBounds().reduced (12).removeFromLeft (230).withTrimmedTop (44);
}

void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (theme::background.withAlpha (0.97f));
    auto r = getLocalBounds().reduced (12);
    theme::drawPanel (g, r.toFloat());
    auto list = listArea();
    g.setFont (theme::label (12.0f));
    for (int i = 0; i < (int) visibleTopics.size(); ++i)
    {
        const int idx = visibleTopics[(size_t) i];
        auto row = juce::Rectangle<int> (list.getX(), list.getY() + i * 30, list.getWidth(), 28);
        if (idx == current)
        {
            g.setColour (theme::accent.withAlpha (0.14f));
            g.fillRoundedRectangle (row.toFloat().reduced (4.0f, 1.0f), 5.0f);
            g.setColour (theme::accent);
            g.fillRoundedRectangle ((float) row.getX() + 4.0f, (float) row.getY() + 6.0f, 3.0f, (float) row.getHeight() - 12.0f, 1.5f);
        }
        g.setColour (idx == current ? theme::text : theme::textDim);
        g.drawText (HelpLibrary::topics()[(size_t) idx].title, row.reduced (14, 0), juce::Justification::centredLeft);
    }
    if (focusTitle.isNotEmpty())
    {
        auto card = r.withTrimmedLeft (250).reduced (16, 0).withTrimmedTop (48).removeFromTop (64);
        g.setColour (theme::gold.withAlpha (0.1f));
        g.fillRoundedRectangle (card.toFloat(), 6.0f);
        g.setColour (theme::gold.withAlpha (0.6f));
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (theme::gold);
        g.setFont (theme::title (14.0f));
        g.drawText (focusTitle, card.reduced (12, 6).removeFromTop (18), juce::Justification::centredLeft);
        g.setColour (theme::text);
        g.setFont (theme::value (13.0f));
        g.drawFittedText (focusBody, card.reduced (12, 6).withTrimmedTop (20), juce::Justification::topLeft, 2);
    }
    g.setColour (theme::textFaint);
    g.setFont (theme::label (10.5f));
    g.drawText ("F1 opens and closes help  -  Right-click any control for its help", r.reduced (16, 8).removeFromBottom (16), juce::Justification::centredLeft);
}

void HelpOverlay::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (44).reduced (12, 8);
    close.setBounds (top.removeFromRight (28));
    search.setBounds (top.removeFromLeft (206));
    auto content = r.withTrimmedLeft (250).reduced (16, 0).withTrimmedBottom (28);
    if (focusTitle.isNotEmpty()) content.removeFromTop (72);
    viewport.setBounds (content);
    const int h = view.layoutForWidth (content.getWidth() - 12);
    view.setSize (content.getWidth() - 12, h);
}

void HelpOverlay::mouseDown (const juce::MouseEvent& e)
{
    auto list = listArea();
    if (! list.contains (e.getPosition())) return;
    const int i = (e.y - list.getY()) / 30;
    if (i >= 0 && i < (int) visibleTopics.size())
    {
        focusTitle.clear();
        selectTopic (visibleTopics[(size_t) i]);
    }
}

bool HelpOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

//==============================================================================
HelpBubble::HelpBubble (EditorServices& s, const juce::String& t, const juce::String& b, const juce::String& tp)
    : services (s), title (t), body (b), topic (tp)
{
    addAndMakeVisible (more);
    more.onClick = [this]
    {
        services.openHelp (topic, title.upToFirstOccurrenceOf ("  ", false, false));
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
    };
    juce::TextLayout tl;
    juce::AttributedString as;
    as.append (body, theme::value (13.0f), theme::text);
    as.setWordWrap (juce::AttributedString::byWord);
    tl.createLayout (as, 300.0f);
    setSize (320, 60 + (int) tl.getHeight());
}

void HelpBubble::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    g.setColour (theme::gold);
    g.setFont (theme::title (14.0f));
    g.drawText (title, r.removeFromTop (20), juce::Justification::centredLeft);
    juce::AttributedString as;
    as.append (body, theme::value (13.0f), theme::text);
    as.setWordWrap (juce::AttributedString::byWord);
    juce::TextLayout tl;
    tl.createLayout (as, (float) r.getWidth());
    tl.draw (g, r.withTrimmedBottom (26).toFloat());
}

void HelpBubble::resized()
{
    more.setBounds (getLocalBounds().reduced (10).removeFromBottom (22).removeFromRight (130));
}

} // namespace cureum
