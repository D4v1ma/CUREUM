#include "TestHelpers.h"
#include "Presets/StateFormat.h"
#include "Core/Settings.h"

namespace cureumtest
{

RenderResult renderProcessor (CureumProcessor& p, double seconds, std::vector<int> notes, double sampleRate, int block)
{
    RenderResult r;
    p.setRateAndBufferSizeDetails (sampleRate, block);
    p.prepareToPlay (sampleRate, block);
    const int total = (int) (seconds * sampleRate);
    r.audio.setSize (2, total);
    r.audio.clear();
    juce::AudioBuffer<float> buf (2, block);
    double sumSq = 0.0;
    const int noteOffAt = (int) (total * 0.7);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        juce::MidiBuffer midi;
        if (pos == 0)
            for (int note : notes) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (pos <= noteOffAt && pos + n > noteOffAt)
            for (int note : notes) midi.addEvent (juce::MidiMessage::noteOff (1, note), noteOffAt - pos);
        buf.setSize (2, n, false, false, true);
        buf.clear();
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
        {
            r.audio.copyFrom (c, pos, buf, c, 0, n);
            for (int i = 0; i < n; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (v));
                sumSq += (double) v * v;
            }
        }
    }
    r.rms = (float) std::sqrt (sumSq / std::max (1, total * 2));
    return r;
}

//==============================================================================
namespace
{
    struct CborWriter
    {
        juce::MemoryOutputStream out;
        void head (int major, juce::uint64 v)
        {
            if (v < 24) out.writeByte ((char) ((major << 5) | (int) v));
            else if (v < 256) { out.writeByte ((char) ((major << 5) | 24)); out.writeByte ((char) v); }
            else if (v < 65536) { out.writeByte ((char) ((major << 5) | 25)); out.writeByte ((char) (v >> 8)); out.writeByte ((char) v); }
            else { out.writeByte ((char) ((major << 5) | 26)); for (int s = 24; s >= 0; s -= 8) out.writeByte ((char) (v >> s)); }
        }
        void write (const juce::var& v)
        {
            if (v.isString()) { auto s = v.toString().toStdString(); head (3, s.size()); out.write (s.data(), s.size()); }
            else if (v.isBool()) out.writeByte ((bool) v ? (char) 0xf5 : (char) 0xf4);
            else if (v.isInt() || v.isInt64())
            {
                const auto i = (juce::int64) v;
                if (i >= 0) head (0, (juce::uint64) i); else head (1, (juce::uint64) (-1 - i));
            }
            else if (v.isDouble())
            {
                out.writeByte ((char) 0xfb);
                const double d = v;
                juce::uint64 bits;
                std::memcpy (&bits, &d, 8);
                for (int s = 56; s >= 0; s -= 8) out.writeByte ((char) (bits >> s));
            }
            else if (auto* a = v.getArray()) { head (4, (juce::uint64) a->size()); for (auto& e : *a) write (e); }
            else if (auto* o = v.getDynamicObject())
            {
                head (5, (juce::uint64) o->getProperties().size());
                for (auto& p : o->getProperties()) { write (p.name.toString()); write (p.value); }
            }
            else out.writeByte ((char) 0xf6);
        }
    };
}

juce::MemoryBlock makeSerum2Preset (const juce::var& data, const juce::String& name)
{
    CborWriter w;
    w.write (data);
    const auto cbor = w.out.getMemoryBlock();

    juce::MemoryOutputStream out;
    out.write ("XferJson", 8); out.writeByte (0);
    const auto meta = juce::String ("{\"fileType\":\"SerumPreset\",\"presetName\":\"") + name + "\",\"presetAuthor\":\"Test\",\"product\":\"Serum2\",\"tags\":[\"Bass\"]}";
    out.writeInt ((int) meta.getNumBytesAsUTF8()); out.writeInt (0);
    out.write (meta.toRawUTF8(), meta.getNumBytesAsUTF8());
    out.writeInt ((int) cbor.getSize()); out.writeInt (2);
    // zstd frame containing a single raw (uncompressed) block
    const juce::uint8 magic[] = { 0x28, 0xb5, 0x2f, 0xfd };
    out.write (magic, 4);
    out.writeByte ((char) 0xa0);                          // single segment, 4-byte content size
    out.writeInt ((int) cbor.getSize());
    const juce::uint32 blockHeader = 1u | ((juce::uint32) cbor.getSize() << 3);
    out.writeByte ((char) (blockHeader & 0xff)); out.writeByte ((char) ((blockHeader >> 8) & 0xff)); out.writeByte ((char) ((blockHeader >> 16) & 0xff));
    out.write (cbor.getData(), cbor.getSize());
    return out.getMemoryBlock();
}

juce::MemoryBlock makeSerum1Fxp (const std::vector<float>& params, const juce::String& name, std::vector<std::array<int, 3>> matrix)
{
    juce::MemoryBlock blob (0x5100, true);
    auto* b = static_cast<char*> (blob.getData());
    for (size_t i = 0; i < params.size() && i < 299; ++i)
        std::memcpy (b + 0x3460 + i * 4, &params[i], 4);
    for (size_t s = 0; s < matrix.size() && s < 16; ++s)
    {
        auto* rec = b + s * 40;
        const float amount = (float) matrix[s][2] / 100.0f, out = 1.0f;
        std::memcpy (rec + 4, &amount, 4);
        std::memcpy (rec + 8, &out, 4);
        const juce::uint16 src = (juce::uint16) matrix[s][0], aux = 0, dst = (juce::uint16) matrix[s][1];
        std::memcpy (rec + 0x14, &src, 2); std::memcpy (rec + 0x16, &aux, 2);
        std::memcpy (rec + 0x1a, &dst, 2);
    }
    std::strncpy (b + 0x4972, name.toRawUTF8(), 40);
    std::strncpy (b + 0x3c08, "/Analog/Basic Shapes.wav", 500);

    juce::MemoryOutputStream z;
    {
        juce::GZIPCompressorOutputStream gz (z, 6);   // zlib format
        gz.write (blob.getData(), blob.getSize());
        gz.flush();
    }
    const auto zdata = z.getMemoryBlock();

    juce::MemoryOutputStream out;
    const juce::uint32 chunkSize = (juce::uint32) zdata.getSize() + 4;
    out.write ("CcnK", 4); out.writeIntBigEndian ((int) (52 + chunkSize));
    out.write ("FPCh", 4); out.writeIntBigEndian (1); out.write ("XfsX", 4); out.writeIntBigEndian (1); out.writeIntBigEndian (1);
    char prg[28] {}; std::strncpy (prg, name.toRawUTF8(), 27); out.write (prg, 28);
    out.writeIntBigEndian ((int) chunkSize);
    out.write (zdata.getData(), zdata.getSize());
    out.writeInt ((int) zdata.getSize());     // trailer: compressed size of the parameter blob (LE)
    return out.getMemoryBlock();
}

//==============================================================================
int runImport (const juce::File& file)
{
    auto r = SerumImport::importFile (file);
    if (! r.ok())
    {
        std::cout << "ERROR: " << r.error << std::endl;
        return 1;
    }
    std::cout << "Format: " << r.sourceFormat << "\nName:   " << r.name << std::endl;
    for (auto& w : r.warnings) std::cout << "  warning: " << w << std::endl;

    CureumProcessor p;
    p.loadStateTree (r.state, false);
    const auto& reg = ParamRegistry::get();
    std::cout << "Changed parameters:" << std::endl;
    for (auto& s : reg.all())
    {
        const float v = p.getPlain (s.index);
        if (std::abs (v - s.defaultValue) > 1.0e-4f && ! s.id.startsWith ("mod"))
            std::cout << "  " << s.id << " = " << s.formatValue (v) << std::endl;
    }
    for (int o = 0; o < kNumOscillators; ++o)
        if (auto t = p.getWavetable (o))
            std::cout << "  osc" << oscLetter (o) << " table: " << t->getName() << " (" << t->getNumFrames() << " frames)" << std::endl;
    std::cout << "Mod matrix:" << std::endl;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const auto& s = p.getModSlots()[(size_t) i];
        if (s.source == ModSource::none) continue;
        std::cout << "  " << modSourceNames()[(int) s.source] << " -> " << reg.spec (s.dest).name << "  "
                  << juce::String (p.getPlain (pid::mod (i)) * 100.0f, 1) << "%" << (s.bipolar ? " (bipolar)" : "")
                  << (s.aux != ModSource::none ? "  aux " + modSourceNames()[(int) s.aux] : juce::String()) << std::endl;
    }
    std::cout << "FX order: " << p.getFxOrder().toString() << std::endl;
    auto rr = renderProcessor (p, 2.0, { 48, 60, 67 });
    std::cout << "Render: peak " << rr.peak << " rms " << rr.rms << (rr.finite ? "" : " NON-FINITE") << std::endl;
    return 0;
}

int runRender (const juce::String& preset, const juce::File& outFile)
{
    CureumProcessor p;
    if (preset.startsWith ("factory:"))
        p.loadStateTree (PresetManager::createFactoryPreset (preset.fromFirstOccurrenceOf (":", false, false).getIntValue()), false);
    else if (preset != "init")
    {
        auto r = SerumImport::importFile (juce::File (preset));
        if (! r.ok()) { std::cout << "ERROR: " << r.error << std::endl; return 1; }
        p.loadStateTree (r.state, false);
    }
    auto rr = renderProcessor (p, 3.0, { 48, 55, 60, 64 });
    juce::WavAudioFormat wav;
    outFile.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream> (outFile);
    std::unique_ptr<juce::OutputStream> os (stream.release());
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (2).withBitsPerSample (24));
    if (writer == nullptr) { std::cout << "Could not create WAV writer" << std::endl; return 1; }
    writer->writeFromAudioSampleBuffer (rr.audio, 0, rr.audio.getNumSamples());
    std::cout << "Wrote " << outFile.getFullPathName() << "  peak " << rr.peak << " rms " << rr.rms << std::endl;
    return rr.finite ? 0 : 1;
}

int runScreenshot (const juce::File& out, const juce::String& tab, float scale, const juce::String& preset)
{
    CureumProcessor p;
    if (preset.startsWith ("factory:"))
        p.loadStateTree (PresetManager::createFactoryPreset (preset.fromFirstOccurrenceOf (":", false, false).getIntValue()), false);
    else if (preset.isNotEmpty() && preset != "init")
    {
        auto r = SerumImport::importFile (juce::File::getCurrentWorkingDirectory().getChildFile (preset));
        if (! r.ok()) { std::cout << "ERROR: " << r.error << std::endl; return 1; }
        p.loadStateTree (r.state, false);
    }
    p.setRateAndBufferSizeDetails (48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
    if (editor == nullptr) return 1;
    if (tab.isNotEmpty())
        editor->getProperties().set ("screenshotTab", tab);
    editor->setVisible (true);

    // Play a chord for a moment so meters, scopes and modulation rings show a live state.
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 120; ++block)
    {
        midi.clear();
        if (block == 0 && preset.isNotEmpty())
            for (int note : { 48, 55, 60, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
        if (block % 6 == 0)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }
    for (int i = 0; i < 10; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    juce::PNGImageFormat png;
    out.deleteFile();
    juce::FileOutputStream fos (out);
    if (! png.writeImageToStream (image, fos)) return 1;
    std::cout << "Wrote " << out.getFullPathName() << " (" << image.getWidth() << "x" << image.getHeight() << ")" << std::endl;
    return 0;
}

} // namespace cureumtest
