#include "PluginSlot.h"

namespace cureum
{

PluginSlot::~PluginSlot()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    if (instance != nullptr)
        instance->releaseResources();
    instance.reset();
}

void PluginSlot::prepareInstance (juce::AudioPluginInstance& p)
{
    // Prefer a stereo in / stereo out layout; fall back to whatever the plugin offers.
    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add (juce::AudioChannelSet::stereo());
    stereo.outputBuses.add (juce::AudioChannelSet::stereo());
    if (p.checkBusesLayoutSupported (stereo))
        p.setBusesLayout (stereo);
    else
        p.enableAllBuses();

    p.setRateAndBufferSizeDetails (sr, maxBlock);
    p.setNonRealtime (false);
    p.prepareToPlay (sr, maxBlock);
}

void PluginSlot::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = std::max (16, block);
    const juce::SpinLock::ScopedLockType sl (lock);
    if (instance != nullptr)
    {
        instance->releaseResources();
        prepareInstance (*instance);
        const int ch = std::max ({ 2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels() });
        buffer.setSize (ch, maxBlock);
    }
    midi.ensureSize (256);
    mix.init = false;
}

void PluginSlot::reset()
{
    const juce::SpinLock::ScopedTryLockType tl (lock);
    if (tl.isLocked() && instance != nullptr)
        instance->reset();
}

void PluginSlot::process (float* l, float* r, int n, const FxContext& ctx)
{
    const juce::SpinLock::ScopedTryLockType tl (lock);
    if (! tl.isLocked() || instance == nullptr || n > buffer.getNumSamples())
        return;

    const int chans = buffer.getNumChannels();
    const int ins = instance->getTotalNumInputChannels();
    const int outs = instance->getTotalNumOutputChannels();
    for (int c = 0; c < chans; ++c)
        buffer.clear (c, 0, n);
    if (ins >= 1) juce::FloatVectorOperations::copy (buffer.getWritePointer (0), l, n);
    if (ins >= 2) juce::FloatVectorOperations::copy (buffer.getWritePointer (1), r, n);
    else if (ins == 1) juce::FloatVectorOperations::add (buffer.getWritePointer (0), r, n);

    juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), chans, n);
    midi.clear();
    instance->processBlock (view, midi);

    mix.set (ctx.value (pid::plugin (index, pid::Plugin::mix)), n);
    const float* outL = view.getReadPointer (0);
    const float* outR = outs >= 2 ? view.getReadPointer (1) : outL;
    for (int i = 0; i < n; ++i)
    {
        const float m = mix.next();
        const float a = std::isfinite (outL[i]) ? outL[i] : 0.0f;
        const float b = std::isfinite (outR[i]) ? outR[i] : 0.0f;
        l[i] += (a - l[i]) * m;
        r[i] += (b - r[i]) * m;
    }
}

int PluginSlot::getLatencySamples() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return instance != nullptr ? instance->getLatencySamples() : 0;
}

void PluginSlot::setInstance (std::unique_ptr<juce::AudioPluginInstance> newInstance)
{
    if (newInstance != nullptr)
    {
        prepareInstance (*newInstance);
        const int ch = std::max ({ 2, newInstance->getTotalNumInputChannels(), newInstance->getTotalNumOutputChannels() });
        juce::AudioBuffer<float> newBuffer (ch, maxBlock);

        std::unique_ptr<juce::AudioPluginInstance> old;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            old = std::move (instance);
            instance = std::move (newInstance);
            buffer = std::move (newBuffer);
        }
        if (old != nullptr) old->releaseResources();
    }
    else
    {
        clear();
        return;
    }
    if (onChange) onChange();
}

void PluginSlot::clear()
{
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (instance);
    }
    if (old != nullptr) old->releaseResources();
    old.reset();
    if (onChange) onChange();
}

bool PluginSlot::isLoaded() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return instance != nullptr;
}

juce::String PluginSlot::getPluginName() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return instance != nullptr ? instance->getName() : juce::String();
}

juce::PluginDescription PluginSlot::getDescription() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return instance != nullptr ? instance->getPluginDescription() : juce::PluginDescription();
}

juce::ValueTree PluginSlot::saveState() const
{
    juce::ValueTree tree ("PluginSlot");
    tree.setProperty ("index", index, nullptr);
    juce::AudioPluginInstance* p = nullptr;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        p = instance.get();
        if (p == nullptr)
            return tree;
        if (auto xml = p->getPluginDescription().createXml())
            tree.setProperty ("description", xml->toString (juce::XmlElement::TextFormat().singleLine()), nullptr);
    }
    juce::MemoryBlock state;
    p->getStateInformation (state);
    tree.setProperty ("state", state.toBase64Encoding(), nullptr);
    return tree;
}

bool PluginSlot::readState (const juce::ValueTree& tree, juce::PluginDescription& desc, juce::MemoryBlock& state)
{
    const auto xmlText = tree.getProperty ("description").toString();
    if (xmlText.isEmpty())
        return false;
    auto xml = juce::parseXML (xmlText);
    if (xml == nullptr || ! desc.loadFromXml (*xml))
        return false;
    state.reset();
    state.fromBase64Encoding (tree.getProperty ("state").toString());
    return true;
}

} // namespace cureum
