#include "StateFormat.h"
#include "WavetableIO.h"
#include "../DSP/WavetableFactory.h"

namespace cureum
{

juce::ValueTree StateFormat::wavetableToTree (const Wavetable& t, int osc)
{
    juce::ValueTree tree ("Wavetable");
    tree.setProperty ("osc", osc, nullptr);
    tree.setProperty ("name", t.getName(), nullptr);
    tree.setProperty ("source", t.sourcePath, nullptr);
    tree.setProperty ("factory", t.isFactory, nullptr);
    tree.setProperty ("frames", t.getNumFrames(), nullptr);
    tree.setProperty ("interp", t.interpolatesFrames(), nullptr);
    if (! t.isFactory)
    {
        const auto& raw = t.getRawData();
        juce::MemoryBlock mb (raw.data(), raw.size() * sizeof (float));
        tree.setProperty ("data", mb.toBase64Encoding(), nullptr);
    }
    return tree;
}

WavetablePtr StateFormat::wavetableFromTree (const juce::ValueTree& tree)
{
    const auto name = tree.getProperty ("name").toString();
    if ((bool) tree.getProperty ("factory", false))
        if (auto t = WavetableFactory::get (name))
            return t;

    const auto data = tree.getProperty ("data").toString();
    if (data.isNotEmpty())
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (data) && mb.getSize() >= sizeof (float) * Wavetable::kFrameSize)
        {
            std::vector<float> frames (mb.getSize() / sizeof (float));
            std::memcpy (frames.data(), mb.getData(), frames.size() * sizeof (float));
            Wavetable::Options o;
            o.interpolateFrames = tree.getProperty ("interp", true);
            auto t = std::const_pointer_cast<Wavetable> (Wavetable::create (name, std::move (frames), o));
            t->sourcePath = tree.getProperty ("source").toString();
            return t;
        }
    }

    const juce::String source = tree.getProperty ("source").toString();
    if (juce::File::isAbsolutePath (source) && juce::File (source).existsAsFile())
    {
        juce::String err;
        if (auto t = WavetableIO::loadWavetable (juce::File (source), err))
            return t;
    }
    return WavetableFactory::get (WavetableFactory::closestMatch (name));
}

bool StateFormat::writePresetFile (const juce::ValueTree& state, const juce::File& file)
{
    auto xml = state.createXml();
    if (xml == nullptr)
        return false;
    juce::MemoryOutputStream raw;
    xml->writeTo (raw, juce::XmlElement::TextFormat().singleLine());
    juce::MemoryOutputStream compressed;
    {
        juce::GZIPCompressorOutputStream gz (compressed, 6, juce::GZIPCompressorOutputStream::windowBitsGZIP);
        gz.write (raw.getData(), raw.getDataSize());
        gz.flush();
    }
    file.getParentDirectory().createDirectory();
    return file.replaceWithData (compressed.getData(), compressed.getDataSize());
}

juce::ValueTree StateFormat::readPresetData (const void* data, size_t size)
{
    auto* bytes = static_cast<const juce::uint8*> (data);
    juce::String text;
    if (size > 2 && bytes[0] == 0x1f && bytes[1] == 0x8b)
    {
        juce::MemoryInputStream in (data, size, false);
        juce::GZIPDecompressorInputStream gz (&in, false, juce::GZIPDecompressorInputStream::gzipFormat);
        text = gz.readEntireStreamAsString();
    }
    else
        text = juce::String::fromUTF8 (static_cast<const char*> (data), (int) size);

    if (auto xml = juce::parseXML (text))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasType ("CUREUM"))
            return tree;
    }
    return {};
}

juce::ValueTree StateFormat::readPresetFile (const juce::File& file)
{
    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb))
        return {};
    return readPresetData (mb.getData(), mb.getSize());
}

//==============================================================================
PresetBuilder::PresetBuilder()
{
    const auto& reg = ParamRegistry::get();
    values.resize ((size_t) pid::total);
    for (auto& s : reg.all())
        values[(size_t) s.index] = s.defaultValue;
    for (auto& t : tables)
        t = WavetableFactory::getDefault();
    fxOrder.units.clear();
    for (int m = 0; m < kNumMacros; ++m)
        macroNames[(size_t) m] = "Macro " + juce::String (m + 1);
}

void PresetBuilder::set (int index, float plainValue)
{
    if (index < 0 || index >= pid::total || ! std::isfinite (plainValue))
        return;
    const auto& s = ParamRegistry::get().spec (index);
    values[(size_t) index] = s.range.snapToLegalValue (juce::jlimit (s.range.start, s.range.end, plainValue));
}

void PresetBuilder::setNormalised (int index, float norm)
{
    if (index < 0 || index >= pid::total || ! std::isfinite (norm))
        return;
    values[(size_t) index] = ParamRegistry::get().spec (index).fromNormalised (norm);
}

int PresetBuilder::addModulation (ModSource src, int dest, float amount, bool bipolar, ModSource aux, float curve, float output)
{
    if (src == ModSource::none || dest < 0 || dest >= pid::total)
        return -1;
    if (! ParamRegistry::get().spec (dest).isModulatable())
        return -1;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto& s = slots[(size_t) i];
        if (s.source != ModSource::none)
            continue;
        s.source = src;
        s.aux = aux;
        s.dest = dest;
        s.bipolar = bipolar || isNaturallyBipolar (src);
        s.curve = juce::jlimit (-1.0f, 1.0f, curve);
        s.output = juce::jlimit (0.0f, 1.0f, output);
        values[(size_t) pid::mod (i)] = juce::jlimit (-1.0f, 1.0f, amount);
        return i;
    }
    warnings.addIfNotAlreadyThere ("Some modulation routings were dropped (all 64 slots are used).");
    return -1;
}

juce::ValueTree PresetBuilder::build() const
{
    const auto& reg = ParamRegistry::get();
    juce::ValueTree root ("CUREUM");
    root.setProperty ("format", 1, nullptr);
    root.setProperty ("version", CUREUM_VERSION_STRING, nullptr);

    juce::ValueTree params ("Params");
    for (auto& s : reg.all())
    {
        juce::ValueTree p ("PARAM");
        p.setProperty ("id", s.id, nullptr);
        p.setProperty ("value", values[(size_t) s.index], nullptr);
        params.appendChild (p, nullptr);
    }
    root.appendChild (params, nullptr);

    juce::ValueTree meta ("Meta");
    meta.setProperty ("name", name, nullptr);
    meta.setProperty ("author", author, nullptr);
    meta.setProperty ("description", description, nullptr);
    meta.setProperty ("category", category, nullptr);
    meta.setProperty ("tags", tags.joinIntoString (","), nullptr);
    root.appendChild (meta, nullptr);

    juce::ValueTree wt ("Wavetables");
    for (int o = 0; o < kNumOscillators; ++o)
        if (tables[(size_t) o] != nullptr)
            wt.appendChild (StateFormat::wavetableToTree (*tables[(size_t) o], o), nullptr);
    root.appendChild (wt, nullptr);

    juce::ValueTree sh ("LfoShapes");
    for (int l = 0; l < kNumLfos; ++l)
    {
        juce::ValueTree s ("Shape");
        s.setProperty ("lfo", l, nullptr);
        s.setProperty ("json", juce::JSON::toString (shapes[(size_t) l].toVar(), true), nullptr);
        sh.appendChild (s, nullptr);
    }
    root.appendChild (sh, nullptr);
    root.appendChild (modSlotsToTree (slots), nullptr);

    juce::ValueTree fx ("Fx");
    fx.setProperty ("order", (fxOrder.units.empty() ? FxOrder::defaultOrder() : fxOrder).toString(), nullptr);
    root.appendChild (fx, nullptr);

    juce::ValueTree macros ("Macros");
    for (int m = 0; m < kNumMacros; ++m)
        macros.setProperty ("m" + juce::String (m + 1), macroNames[(size_t) m], nullptr);
    root.appendChild (macros, nullptr);

    if (noiseFile.existsAsFile())
    {
        juce::ValueTree n ("Noise");
        n.setProperty ("file", noiseFile.getFullPathName(), nullptr);
        root.appendChild (n, nullptr);
    }
    return root;
}

} // namespace cureum
