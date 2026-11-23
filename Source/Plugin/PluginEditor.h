#pragma once
#include "PluginProcessor.h"

namespace cureum
{
class CureumEditor : public juce::AudioProcessorEditor
{
public:
    explicit CureumEditor (CureumProcessor& p) : AudioProcessorEditor (p), generic (p) { addAndMakeVisible (generic); setSize (900, 600); }
    void resized() override { generic.setBounds (getLocalBounds()); }
private:
    juce::GenericAudioProcessorEditor generic;
};
}
