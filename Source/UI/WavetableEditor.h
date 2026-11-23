#pragma once

#include "Widgets.h"

namespace cureum
{

class WavetableEditor : public juce::Component
{
public:
    WavetableEditor (EditorServices& s, int osc);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void()> onClose;

private:
    juce::Rectangle<int> stripArea() const, waveArea() const, harmonicsArea() const;
    void drawAt (juce::Point<float> p, bool line);
    void setHarmonicAt (juce::Point<float> p);
    void rebuildHarmonics();
    void applyFormula();
    void apply();
    void showProcessMenu();
    void importFile();
    void exportFile();
    void normaliseAll();

    EditorServices& services;
    CureumProcessor& proc;
    const int osc;
    juce::String name;
    std::vector<std::vector<float>> frames;
    int selected = 0;
    std::vector<float> harmonics;
    enum class Drag { none, wave, harmonics, strip } drag = Drag::none;
    juce::Point<float> lastPoint, downPoint;
    std::vector<float> undoFrame;

    juce::TextEditor formula, frameCount;
    juce::TextButton applyFormulaButton { "Apply formula" }, addFrame { "Add frame" }, deleteFrame { "Delete" },
                     smooth { "Smooth" }, normalise { "Normalise" }, invert { "Invert" }, process { "Process..." },
                     importButton { "Import..." }, exportButton { "Export..." }, applyButton { "Apply" }, cancelButton { "Cancel" };
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace cureum
