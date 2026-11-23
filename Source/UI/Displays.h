#pragma once

#include "Widgets.h"

namespace cureum
{

// 3D wavetable display. Drag vertically to scan the wavetable position.
class WavetableView : public juce::Component, public HelpTextProvider, public Refreshable
{
public:
    WavetableView (EditorServices& s, int osc);
    void refresh() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    juce::String getHelpTitle() const override { return "Wavetable display (Osc " + oscLetter (osc) + ")"; }
    juce::String getHelpBody() const override  { return "Shows every frame of the wavetable in 3D; the bright line is the current position. Drag up/down to scan. Double-click to open the wavetable editor."; }
    juce::String getHelpTopic() const override { return "oscillators"; }
    std::function<void()> onDoubleClick;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }

private:
    void rebuild();
    EditorServices& services;
    CureumProcessor& proc;
    const int osc;
    const Wavetable* cachedTable = nullptr;
    std::vector<std::vector<float>> frames;   // decimated frames for drawing
    float pos = 0.0f, livePos = -1.0f, dragStartNorm = 0.0f;
    bool enabled = true;
};

// Filter frequency response. Drag horizontally for cutoff, vertically for resonance.
class FilterView : public juce::Component, public HelpTextProvider, private juce::AsyncUpdater, public Refreshable
{
public:
    FilterView (EditorServices& s, int filterIndex);   // -1 = FX rack filter
    void setFilter (int filterIndex) { filter = filterIndex; key = {}; }
    void refresh() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    juce::String getHelpTitle() const override { return "Filter response"; }
    juce::String getHelpBody() const override  { return "The filter's frequency response. Drag left/right to move the cutoff, up/down to change resonance."; }
    juce::String getHelpTopic() const override { return "filters"; }
private:
    void handleAsyncUpdate() override;
    int typeParam() const, cutoffParam() const, resoParam() const, driveParam() const, varParam() const, enableParam() const;
    EditorServices& services;
    CureumProcessor& proc;
    int filter;
    juce::String key;
    std::vector<float> freqs, mags;
    float liveCutoff = -1.0f;
    float startCut = 0.0f, startRes = 0.0f;
};

// Envelope display with draggable stage handles.
class EnvelopeView : public juce::Component, public HelpTextProvider, public Refreshable
{
public:
    EnvelopeView (EditorServices& s);
    void setEnvelope (int e) { env = e; repaint(); }
    void refresh() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    juce::String getHelpTitle() const override { return "Envelope " + juce::String (env + 1); }
    juce::String getHelpBody() const override  { return "Drag the handles to set attack, decay, sustain and release. Drag the small handles in the middle of a segment to bend its curve."; }
    juce::String getHelpTopic() const override { return "envelopes"; }
private:
    struct Geometry { float xA, xH, xD, xS, xR, ySus; juce::Rectangle<float> area; float scale; };
    Geometry geometry() const;
    int handleAt (juce::Point<float> p) const;
    float timeToWidth (float seconds) const;
    float widthToTime (float w) const;
    EditorServices& services;
    CureumProcessor& proc;
    int env = 0, dragHandle = -1, hoverHandle = -1;
    float startA = 0, startB = 0;
    juce::Point<float> dragStart;
    std::array<float, 8> last {};
    float liveValue = -1.0f;
    int liveStage = 0;
};

// Drawable LFO shape editor.
class LfoEditor : public juce::Component, public HelpTextProvider, public Refreshable
{
public:
    LfoEditor (EditorServices& s);
    void setLfo (int l);
    void refresh() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    juce::String getHelpTitle() const override { return "LFO " + juce::String (lfo + 1) + " shape"; }
    juce::String getHelpBody() const override  { return "Drag points to reshape. Double-click to add or delete a point. Drag the small handle between two points to curve the segment. Hold Shift to disable grid snapping."; }
    juce::String getHelpTopic() const override { return "lfos"; }
    int grid = 8;
private:
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float> p) const;
    void commit (bool final);
    EditorServices& services;
    CureumProcessor& proc;
    int lfo = 0;
    std::vector<LfoPoint> points;
    int dragPoint = -1, dragCurve = -1, hoverPoint = -1, hoverCurve = -1;
    float curveStart = 0.5f, dragStartY = 0.0f;
    float phase = -1.0f;
    int lastStructure = -1;
    juce::uint32 lastCommit = 0;
};

class ScopeView : public juce::Component, public Refreshable
{
public:
    explicit ScopeView (CureumProcessor& p) : proc (p) {}
    void refresh() override { repaint(); }
    void paint (juce::Graphics&) override;
private:
    CureumProcessor& proc;
};

} // namespace cureum
