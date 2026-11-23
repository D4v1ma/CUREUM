#pragma once

#include "Widgets.h"

namespace cureum
{

struct HelpTopic
{
    juce::String id, title, markdown;
};

class HelpLibrary
{
public:
    static const std::vector<HelpTopic>& topics();
    static const HelpTopic* find (const juce::String& id);
    static std::vector<int> search (const juce::String& query);   // indices sorted by relevance
};

// Renders a small Markdown subset: #/## headings, paragraphs, "-" and "1." lists, **bold** and `code`.
class MarkdownView : public juce::Component
{
public:
    void setMarkdown (const juce::String& md, const juce::String& highlight = {});
    int layoutForWidth (int width);
    int getYForText (const juce::String& text) const;
    void paint (juce::Graphics&) override;
private:
    struct Block { enum Kind { h1, h2, para, bullet, numbered } kind; juce::String text; int number = 0; juce::TextLayout layout; int y = 0, h = 0; };
    std::vector<Block> blocks;
    juce::String highlight;
};

class HelpOverlay : public juce::Component, private juce::TextEditor::Listener
{
public:
    explicit HelpOverlay (EditorServices& s);
    void show (const juce::String& topicId, const juce::String& focus = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void()> onClose;
private:
    void textEditorTextChanged (juce::TextEditor&) override;
    void selectTopic (int index, const juce::String& focus = {});
    juce::Rectangle<int> listArea() const;
    EditorServices& services;
    juce::TextEditor search;
    IconButton close { IconButton::Icon::close, "Close help", "Closes the help browser (Esc)." };
    juce::Viewport viewport;
    MarkdownView view;
    std::vector<int> visibleTopics;
    int current = 0;
    juce::String focusTitle, focusBody;
};

// Bubble shown in "What's this?" mode.
class HelpBubble : public juce::Component
{
public:
    HelpBubble (EditorServices& s, const juce::String& title, const juce::String& body, const juce::String& topic);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    EditorServices& services;
    juce::String title, body, topic;
    juce::TextButton more { "Open help page" };
};

} // namespace cureum
