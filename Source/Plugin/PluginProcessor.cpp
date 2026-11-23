#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Core/ValueMaps.h"
#include "../Presets/WavetableIO.h"
#include "../Presets/StateFormat.h"

namespace cureum
{

using namespace pid;

//==============================================================================
CureumProcessor::CureumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Params", ParamRegistry::get().createLayout())
{
    params.attach (apvts);
    globalMod.assign ((size_t) pid::total, 0.0f);
    initResources();
    setPlain (pid::osc (0, Osc::wtPos), 1.0f / 6.0f);   // init patch: saw
    startTimerHz (20);
}

CureumProcessor::~CureumProcessor()
{
    stopTimer();
}

void CureumProcessor::initResources()
{
    for (int o = 0; o < kNumOscillators; ++o)
        setWavetable (o, WavetableFactory::getDefault());
    for (int l = 0; l < kNumLfos; ++l)
        setLfoShape (l, LfoShape());
    modSlots = {};
    publishMatrix();
    fxOrder.publish (std::make_shared<FxOrder> (FxOrder::defaultOrder()));
    for (int m = 0; m < kNumMacros; ++m)
        macroNames[(size_t) m] = "Macro " + juce::String (m + 1);
    customNoise.publish (nullptr);
    customNoiseFile = juce::File();
    bumpStructure();
}

void CureumProcessor::resetToInit()
{
    for (int i = 0; i < pid::total; ++i)
        if (auto* p = getParam (i))
            p->setValueNotifyingHost (p->getDefaultValue());
    setPlain (pid::osc (0, Osc::wtPos), 1.0f / 6.0f);
    initResources();
    for (int s = 0; s < 4; ++s)
        fxRack.pluginSlot (s).clear();
    presetInfo = {};
    ++presetVersion;
}

//==============================================================================
void CureumProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = std::max (16, samplesPerBlock);
    engine.prepare (sampleRate, currentBlockSize);
    fxRack.prepare (sampleRate, currentBlockSize);
    fxBuffer.setSize (2, currentBlockSize);
    masterGain.init = false;
    keyboardState.reset();
}

void CureumProcessor::releaseResources()
{
    engine.allSoundOff();
}

bool CureumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CureumProcessor::renderRange (juce::AudioBuffer<float>& buffer, int start, int num, VoiceContext& ctx)
{
    engine.render (buffer.getWritePointer (0) + start, buffer.getWritePointer (1) + start, num, ctx);
}

void CureumProcessor::handleMidi (const juce::MidiMessage& m, VoiceContext& ctx)
{
    const bool mpe = params.on (global (Global::mpeEnable));
    const int ch = m.getChannel();

    if (m.isNoteOn())
    {
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity(), ch, ctx);
        noteStartedThisBlock = true;
        if (mpe && ch >= 1 && ch <= 16)
            engine.mpeBend (ch, mpeChannelBend[(size_t) ch]);
    }
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber(), m.getFloatVelocity(), ch, ctx);
    else if (m.isAllSoundOff())
        engine.allSoundOff();
    else if (m.isAllNotesOff())
        engine.allNotesOff();
    else if (m.isSustainPedalOn())
        engine.setSustain (true);
    else if (m.isSustainPedalOff())
        engine.setSustain (false);
    else if (m.isPitchWheel())
    {
        const float v = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        if (mpe && ch != 1)
        {
            const float semis = v * (float) params.plain (global (Global::mpeBendRange));
            mpeChannelBend[(size_t) juce::jlimit (0, 16, ch)] = semis;
            engine.mpeBend (ch, semis);
        }
        else
            pitchBendValue = v;
    }
    else if (m.isChannelPressure())
    {
        const float v = (float) m.getChannelPressureValue() / 127.0f;
        if (mpe && ch != 1) engine.mpePressure (ch, v);
        else aftertouch = v;
    }
    else if (m.isAftertouch())
        engine.polyAftertouch (m.getNoteNumber(), ch, (float) m.getAfterTouchValue() / 127.0f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float v = (float) m.getControllerValue() / 127.0f;
        if (cc == 1) modWheel = v;
        else if (cc == 74 && mpe) engine.mpeSlide (ch, v);
    }

    ctx.globalSources[(int) ModSource::modWheel] = modWheel;
    ctx.globalSources[(int) ModSource::pitchBend] = pitchBendValue;
    ctx.globalSources[(int) ModSource::aftertouch] = aftertouch;
    ctx.pitchBendSemis = pitchBendValue >= 0.0f ? pitchBendValue * params.plain (global (Global::bendUp))
                                                : pitchBendValue * params.plain (global (Global::bendDown));
}

void CureumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;
    if (n > fxBuffer.getNumSamples())
        fxBuffer.setSize (2, n, false, false, true);   // host exceeded the announced block size

    params.update();

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying();
        }
    currentBpm = bpm;

    const int q = params.choice (global (Global::quality));
    engine.setOversampling (q == 0 ? 1 : (q == 1 ? 2 : 4));

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    VoiceContext ctx;
    ctx.params = &params;
    ctx.matrix = compiledMatrix.get();
    for (int o = 0; o < kNumOscillators; ++o) ctx.tables[o] = wavetableSlots[(size_t) o].get();
    for (int l = 0; l < kNumLfos; ++l) ctx.lfoShapes[l] = lfoShapeSlots[(size_t) l].get();
    ctx.customNoise = customNoise.get();
    ctx.globalSources[(int) ModSource::modWheel] = modWheel;
    ctx.globalSources[(int) ModSource::pitchBend] = pitchBendValue;
    ctx.globalSources[(int) ModSource::aftertouch] = aftertouch;
    ctx.globalSources[(int) ModSource::constant] = 1.0f;
    for (int m = 0; m < kNumMacros; ++m)
        ctx.globalSources[(int) macroSource (m)] = params.plain (macro (m));
    ctx.bpm = bpm;
    ctx.pitchBendSemis = pitchBendValue >= 0.0f ? pitchBendValue * params.plain (global (Global::bendUp))
                                                : pitchBendValue * params.plain (global (Global::bendDown));

    // Free-running LFOs share one phase for all voices; synced ones follow the song position.
    const double renderRate = currentSampleRate * engine.getOversampling();
    double lfoRate[kNumLfos] {};
    bool hostSynced[kNumLfos] {};
    for (int l = 0; l < kNumLfos; ++l)
    {
        if (params.on (lfo (l, Lfo::sync)))
        {
            const double beats = 4.0 * syncRateWholeNotes (params.choice (lfo (l, Lfo::syncRate))) * syncFeelMultiplier (params.choice (lfo (l, Lfo::syncFeel)));
            lfoRate[l] = bpm / 60.0 / beats;
            if (playing) { freeLfoPhase[(size_t) l] = ppq / beats; hostSynced[l] = true; }
        }
        else
            lfoRate[l] = params.plain (lfo (l, Lfo::rate));
        ctx.freeLfoPhase[l] = freeLfoPhase[(size_t) l];
        ctx.freeLfoCyclesPerSample[l] = lfoRate[l] / renderRate;
    }

    auto& out = fxBuffer;
    out.clear (0, 0, n);
    out.clear (1, 0, n);
    noteStartedThisBlock = false;
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { renderRange (out, pos, at - pos, ctx); pos = at; }
        handleMidi (meta.getMessage(), ctx);
    }
    if (pos < n) renderRange (out, pos, n - pos, ctx);
    midi.clear();

    for (int l = 0; l < kNumLfos; ++l)
        if (! hostSynced[l])
        {
            freeLfoPhase[(size_t) l] += lfoRate[l] * n / currentSampleRate;
            if (freeLfoPhase[(size_t) l] > 1.0e6) freeLfoPhase[(size_t) l] -= std::floor (freeLfoPhase[(size_t) l]);
        }

    // Effects
    engine.computeGlobalModulation (ctx, globalMod.data());
    FxContext fctx;
    fctx.params = &params;
    fctx.globalMod = globalMod.data();
    fctx.matrix = ctx.matrix;
    fctx.sampleRate = currentSampleRate;
    fctx.bpm = bpm;
    fctx.noteStarted = noteStartedThisBlock;
    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (1);
    if (auto* order = fxOrder.get())
        fxRack.process (L, R, n, fctx, *order);

    // Master volume, safety
    masterGain.set (maps::levelToGain (fctx.value (global (Global::master))), n);
    float peakL = 0.0f, peakR = 0.0f;
    int sp = feedback.scopePos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.next();
        float a = L[i] * g, b = R[i] * g;
        if (! std::isfinite (a)) a = 0.0f;
        if (! std::isfinite (b)) b = 0.0f;
        a = juce::jlimit (-4.0f, 4.0f, a);
        b = juce::jlimit (-4.0f, 4.0f, b);
        L[i] = a; R[i] = b;
        peakL = std::max (peakL, std::abs (a));
        peakR = std::max (peakR, std::abs (b));
        feedback.scope[(size_t) sp] = 0.5f * (a + b);
        sp = (sp + 1) % Feedback::scopeSize;
    }
    feedback.scopePos.store (sp, std::memory_order_relaxed);

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, out, 0, 0, n);
        buffer.copyFrom (1, 0, out, 1, 0, n);
        for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, out.getReadPointer (0), n, 0.5f);
        buffer.addFrom (0, 0, out.getReadPointer (1), n, 0.5f);
    }

    feedback.peakL = std::max (peakL, feedback.peakL.load() * 0.85f);
    feedback.peakR = std::max (peakR, feedback.peakR.load() * 0.85f);
    feedback.compGr = fxRack.compressor().getGainReductionDb();
    updateFeedback (ctx);

    const double elapsed = juce::Time::getMillisecondCounterHiRes() - t0;
    const double budget = 1000.0 * n / currentSampleRate;
    feedback.cpuLoad = (float) (0.9 * feedback.cpuLoad.load() + 0.1 * (elapsed / budget));
}

void CureumProcessor::updateFeedback (const VoiceContext& ctx)
{
    const auto* v = engine.newestVoice();
    feedback.activeVoices = engine.numActiveVoices();
    feedback.anyVoice = v != nullptr;
    if (v != nullptr)
    {
        for (int l = 0; l < kNumLfos; ++l)
        {
            feedback.lfoPhase[(size_t) l] = v->lfoPhase (l);
            feedback.lfoValue[(size_t) l] = v->sources()[(int) lfoSource (l)];
        }
        for (int e = 0; e < kNumEnvelopes; ++e)
        {
            feedback.envValue[(size_t) e] = v->getEnvelope (e).current();
            feedback.envStage[(size_t) e] = (int) v->getEnvelope (e).getStage();
        }
    }
    if (ctx.matrix != nullptr)
    {
        for (int d : ctx.matrix->voiceDests)
            feedback.modNorm[(size_t) d] = dsp::clamp01 (params.norm (d) + (v != nullptr ? v->modOffset (d) : 0.0f));
        for (int d : ctx.matrix->globalDests)
            feedback.modNorm[(size_t) d] = dsp::clamp01 (params.norm (d) + globalMod[(size_t) d]);
    }
}

void CureumProcessor::timerCallback()
{
    for (auto& w : wavetableSlots) w.prune();
    for (auto& s : lfoShapeSlots) s.prune();
    compiledMatrix.prune();
    fxOrder.prune();
    customNoise.prune();

    const int ir = params.choice (conv (Conv::ir));
    if (ir != lastConvIr)
    {
        lastConvIr = ir;
        if ((ConvIr) ir != ConvIr::custom)
            fxRack.convolver().loadBuiltIn ((ConvIr) ir);
        else if (fxRack.convolver().getCustomFile().existsAsFile())
            fxRack.convolver().loadFile (fxRack.convolver().getCustomFile());
    }

    const int latency = fxRack.getLatencySamples();
    if (latency != lastLatency)
    {
        lastLatency = latency;
        setLatencySamples (latency);
    }
}

//==============================================================================
float CureumProcessor::getPlain (int index) const
{
    return apvts.getRawParameterValue (ParamRegistry::get().spec (index).id)->load();
}

juce::RangedAudioParameter* CureumProcessor::getParam (int index) const
{
    return apvts.getParameter (ParamRegistry::get().spec (index).id);
}

void CureumProcessor::setPlain (int index, float plainValue)
{
    if (auto* p = getParam (index))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

WavetablePtr CureumProcessor::getWavetable (int osc) const
{
    const juce::ScopedLock sl (stateLock);
    return wavetables[(size_t) juce::jlimit (0, kNumOscillators - 1, osc)];
}

void CureumProcessor::setWavetable (int osc, WavetablePtr table)
{
    if (table == nullptr) table = WavetableFactory::getDefault();
    {
        const juce::ScopedLock sl (stateLock);
        wavetables[(size_t) osc] = table;
    }
    wavetableSlots[(size_t) osc].publish (table);
    bumpStructure();
}

const LfoShape& CureumProcessor::getLfoShape (int l) const
{
    return lfoShapes[(size_t) juce::jlimit (0, kNumLfos - 1, l)];
}

void CureumProcessor::setLfoShape (int l, const LfoShape& shape)
{
    lfoShapes[(size_t) l] = shape;
    lfoShapeSlots[(size_t) l].publish (std::make_shared<LfoShape> (shape));
    bumpStructure();
}

void CureumProcessor::publishMatrix()
{
    compiledMatrix.publish (CompiledMatrix::compile (modSlots));
}

void CureumProcessor::setModSlot (int index, const ModSlot& slot)
{
    if (index < 0 || index >= kNumModSlots) return;
    modSlots[(size_t) index] = slot;
    publishMatrix();
    bumpStructure();
}

void CureumProcessor::setModSlots (const ModSlots& slots)
{
    modSlots = slots;
    publishMatrix();
    bumpStructure();
}

int CureumProcessor::findFreeModSlot() const
{
    for (int i = 0; i < kNumModSlots; ++i)
        if (modSlots[(size_t) i].source == ModSource::none && modSlots[(size_t) i].dest < 0)
            return i;
    return -1;
}

int CureumProcessor::addModulation (ModSource source, int destParam, float amount, bool bipolar)
{
    if (source == ModSource::none || destParam < 0 || ! ParamRegistry::get().spec (destParam).isModulatable())
        return -1;
    for (int i = 0; i < kNumModSlots; ++i)
        if (modSlots[(size_t) i].source == source && modSlots[(size_t) i].dest == destParam)
            return i;   // already routed
    const int slot = findFreeModSlot();
    if (slot < 0) return -1;
    ModSlot s;
    s.source = source;
    s.dest = destParam;
    s.bipolar = bipolar || isNaturallyBipolar (source);
    setPlain (pid::mod (slot), amount);
    setModSlot (slot, s);
    return slot;
}

void CureumProcessor::removeModSlot (int index)
{
    if (index < 0 || index >= kNumModSlots) return;
    setPlain (pid::mod (index), 0.0f);
    setModSlot (index, ModSlot {});
}

std::vector<int> CureumProcessor::slotsForDestination (int destParam) const
{
    std::vector<int> r;
    for (int i = 0; i < kNumModSlots; ++i)
        if (modSlots[(size_t) i].dest == destParam && modSlots[(size_t) i].source != ModSource::none)
            r.push_back (i);
    return r;
}

FxOrder CureumProcessor::getFxOrder() const
{
    if (auto o = fxOrder.getShared()) return *o;
    return FxOrder::defaultOrder();
}

void CureumProcessor::setFxOrder (const FxOrder& order)
{
    fxOrder.publish (std::make_shared<FxOrder> (order));
    bumpStructure();
}

void CureumProcessor::setMacroName (int m, const juce::String& name)
{
    macroNames[(size_t) juce::jlimit (0, kNumMacros - 1, m)] = name;
    bumpStructure();
}

void CureumProcessor::loadPluginIntoSlot (int slot, const juce::PluginDescription& desc, const juce::MemoryBlock* stateToRestore)
{
    juce::MemoryBlock state;
    if (stateToRestore != nullptr) state = *stateToRestore;
    juce::WeakReference<CureumProcessor> weak (this);
    pluginHost->createInstance (desc, currentSampleRate, currentBlockSize,
        [weak, slot, state] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
        {
            if (weak == nullptr) return;
            if (instance == nullptr)
            {
                DBG ("Plugin load failed: " + error);
                return;
            }
            if (state.getSize() > 0)
                instance->setStateInformation (state.getData(), (int) state.getSize());
            weak->fxRack.pluginSlot (slot).setInstance (std::move (instance));
            auto order = weak->getFxOrder();
            const auto type = (FxType) ((int) FxType::plugin1 + slot);
            if (! order.contains (type)) { order.units.push_back (type); weak->setFxOrder (order); }
            weak->setPlain (pid::plugin (slot, pid::Plugin::enable), 1.0f);
            weak->bumpStructure();
        });
}

void CureumProcessor::clearPluginSlot (int slot)
{
    fxRack.pluginSlot (slot).clear();
    bumpStructure();
}

void CureumProcessor::setConvolverIr (ConvIr type, const juce::File& customFile)
{
    if (type == ConvIr::custom && customFile.existsAsFile())
        fxRack.convolver().loadFile (customFile);
    setPlain (conv (Conv::ir), (float) (int) type);
    lastConvIr = -1;
    bumpStructure();
}

bool CureumProcessor::loadCustomNoise (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Could not read " + file.getFileName(); return false; }
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
    if (len < 16) { error = "The file is too short."; return false; }
    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);
    auto s = std::make_shared<NoiseSample>();
    s->name = file.getFileNameWithoutExtension();
    s->sampleRate = reader->sampleRate;
    s->left.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    const int rc = buf.getNumChannels() > 1 ? 1 : 0;
    s->right.assign (buf.getReadPointer (rc), buf.getReadPointer (rc) + len);
    customNoise.publish (s);
    customNoiseFile = file;
    setPlain (noise (Noise::type), (float) (int) NoiseType::custom);
    bumpStructure();
    return true;
}

juce::String CureumProcessor::getCustomNoiseName() const
{
    if (auto s = customNoise.getShared()) return s->name;
    return {};
}

//==============================================================================
juce::ValueTree CureumProcessor::createStateTree (bool includePlugins) const
{
    auto& self = const_cast<CureumProcessor&> (*this);
    juce::ValueTree root ("CUREUM");
    root.setProperty ("format", 1, nullptr);
    root.setProperty ("version", CUREUM_VERSION_STRING, nullptr);
    root.appendChild (self.apvts.copyState(), nullptr);

    juce::ValueTree meta ("Meta");
    meta.setProperty ("name", presetInfo.name, nullptr);
    meta.setProperty ("author", presetInfo.author, nullptr);
    meta.setProperty ("description", presetInfo.description, nullptr);
    meta.setProperty ("category", presetInfo.category, nullptr);
    meta.setProperty ("tags", presetInfo.tags.joinIntoString (","), nullptr);
    root.appendChild (meta, nullptr);

    juce::ValueTree tables ("Wavetables");
    for (int o = 0; o < kNumOscillators; ++o)
        if (auto t = getWavetable (o))
            tables.appendChild (StateFormat::wavetableToTree (*t, o), nullptr);
    root.appendChild (tables, nullptr);

    juce::ValueTree shapes ("LfoShapes");
    for (int l = 0; l < kNumLfos; ++l)
    {
        juce::ValueTree s ("Shape");
        s.setProperty ("lfo", l, nullptr);
        s.setProperty ("json", juce::JSON::toString (lfoShapes[(size_t) l].toVar(), true), nullptr);
        shapes.appendChild (s, nullptr);
    }
    root.appendChild (shapes, nullptr);

    root.appendChild (modSlotsToTree (modSlots), nullptr);

    juce::ValueTree fx ("Fx");
    fx.setProperty ("order", getFxOrder().toString(), nullptr);
    if (self.fxRack.convolver().getCustomFile().existsAsFile())
        fx.setProperty ("convFile", self.fxRack.convolver().getCustomFile().getFullPathName(), nullptr);
    root.appendChild (fx, nullptr);

    juce::ValueTree macros ("Macros");
    for (int m = 0; m < kNumMacros; ++m)
        macros.setProperty ("m" + juce::String (m + 1), macroNames[(size_t) m], nullptr);
    root.appendChild (macros, nullptr);

    if (customNoiseFile.existsAsFile())
    {
        juce::ValueTree n ("Noise");
        n.setProperty ("file", customNoiseFile.getFullPathName(), nullptr);
        root.appendChild (n, nullptr);
    }

    if (includePlugins)
    {
        juce::ValueTree plugins ("Plugins");
        for (int s = 0; s < 4; ++s)
            if (self.fxRack.pluginSlot (s).isLoaded())
                plugins.appendChild (self.fxRack.pluginSlot (s).saveState(), nullptr);
        root.appendChild (plugins, nullptr);
    }
    return root;
}

void CureumProcessor::loadStateTree (const juce::ValueTree& tree, bool restorePlugins)
{
    if (! tree.isValid())
        return;
    const auto& reg = ParamRegistry::get();

    // Parameters: start from defaults so missing values never leak from the previous preset.
    {
        const auto in = tree.getChildWithName ("Params");
        juce::ValueTree full ("Params");
        for (auto& s : reg.all())
        {
            float v = s.defaultValue;
            const auto child = in.getChildWithProperty ("id", s.id);
            if (child.isValid())
                v = (float) (double) child.getProperty ("value", (double) s.defaultValue);
            juce::ValueTree p ("PARAM");
            p.setProperty ("id", s.id, nullptr);
            p.setProperty ("value", v, nullptr);
            full.appendChild (p, nullptr);
        }
        apvts.replaceState (full);
    }

    const auto meta = tree.getChildWithName ("Meta");
    presetInfo.name = meta.getProperty ("name", "Untitled").toString();
    presetInfo.author = meta.getProperty ("author").toString();
    presetInfo.description = meta.getProperty ("description").toString();
    presetInfo.category = meta.getProperty ("category").toString();
    presetInfo.tags = juce::StringArray::fromTokens (meta.getProperty ("tags").toString(), ",", "");
    presetInfo.tags.removeEmptyStrings();

    for (int o = 0; o < kNumOscillators; ++o)
        setWavetable (o, WavetableFactory::getDefault());
    for (const auto& t : tree.getChildWithName ("Wavetables"))
    {
        const int o = t.getProperty ("osc", -1);
        if (o >= 0 && o < kNumOscillators)
            setWavetable (o, StateFormat::wavetableFromTree (t));
    }

    for (int l = 0; l < kNumLfos; ++l)
        lfoShapes[(size_t) l] = LfoShape();
    for (const auto& s : tree.getChildWithName ("LfoShapes"))
    {
        const int l = s.getProperty ("lfo", -1);
        if (l >= 0 && l < kNumLfos)
            lfoShapes[(size_t) l] = LfoShape::fromVar (juce::JSON::parse (s.getProperty ("json").toString()));
    }
    for (int l = 0; l < kNumLfos; ++l)
        lfoShapeSlots[(size_t) l].publish (std::make_shared<LfoShape> (lfoShapes[(size_t) l]));

    ModSlots slots;
    modSlotsFromTree (tree.getChildWithName ("ModMatrix"), slots);
    modSlots = slots;
    publishMatrix();

    const auto fx = tree.getChildWithName ("Fx");
    auto order = fx.isValid() ? FxOrder::fromString (fx.getProperty ("order").toString()) : FxOrder::defaultOrder();
    fxOrder.publish (std::make_shared<FxOrder> (order));
    const juce::File convFile (fx.getProperty ("convFile").toString());
    if (juce::File::isAbsolutePath (convFile.getFullPathName()) && convFile.existsAsFile())
        fxRack.convolver().loadFile (convFile);
    lastConvIr = -1;

    const auto macros = tree.getChildWithName ("Macros");
    for (int m = 0; m < kNumMacros; ++m)
        macroNames[(size_t) m] = macros.getProperty ("m" + juce::String (m + 1), "Macro " + juce::String (m + 1)).toString();

    customNoise.publish (nullptr);
    customNoiseFile = juce::File();
    const juce::File noiseFile (tree.getChildWithName ("Noise").getProperty ("file").toString());
    if (juce::File::isAbsolutePath (noiseFile.getFullPathName()) && noiseFile.existsAsFile())
    {
        juce::String err;
        loadCustomNoise (noiseFile, err);
    }

    if (restorePlugins)
        restorePluginSlots (tree.getChildWithName ("Plugins"));

    bumpStructure();
    ++presetVersion;
}

void CureumProcessor::restorePluginSlots (const juce::ValueTree& plugins)
{
    auto restore = [weak = juce::WeakReference<CureumProcessor> (this), plugins]
    {
        if (weak == nullptr) return;
        for (int s = 0; s < 4; ++s)
            weak->fxRack.pluginSlot (s).clear();
        for (const auto& p : plugins)
        {
            const int idx = p.getProperty ("index", -1);
            juce::PluginDescription desc;
            juce::MemoryBlock state;
            if (idx >= 0 && idx < 4 && PluginSlot::readState (p, desc, state))
                weak->loadPluginIntoSlot (idx, desc, &state);
        }
    };
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        restore();
    else
        juce::MessageManager::callAsync (restore);
}

void CureumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree (true).createXml())
        copyXmlToBinary (*xml, destData);
}

void CureumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        loadStateTree (juce::ValueTree::fromXml (*xml), true);
}

juce::AudioProcessorEditor* CureumProcessor::createEditor()
{
    return new CureumEditor (*this);
}

} // namespace cureum

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cureum::CureumProcessor();
}
