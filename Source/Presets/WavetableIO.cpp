#include "WavetableIO.h"

namespace cureum::WavetableIO
{

int findClmFrameSize (const void* data, size_t size)
{
    auto* p = static_cast<const juce::uint8*> (data);
    if (size < 12 || std::memcmp (p, "RIFF", 4) != 0 || std::memcmp (p + 8, "WAVE", 4) != 0)
        return 0;
    size_t pos = 12;
    while (pos + 8 <= size)
    {
        const juce::uint32 chunkSize = juce::ByteOrder::littleEndianInt (p + pos + 4);
        if (std::memcmp (p + pos, "clm ", 4) == 0)
        {
            const auto len = (size_t) std::min<juce::uint32> (chunkSize, (juce::uint32) std::min<size_t> (64, size - pos - 8));
            const juce::String text (juce::CharPointer_UTF8 (reinterpret_cast<const char*> (p + pos + 8)), len);
            const int marker = text.indexOf ("<!>");
            if (marker >= 0)
            {
                const int frame = text.substring (marker + 3).getIntValue();
                if (frame >= 16 && frame <= 65536)
                    return frame;
            }
            return 2048;
        }
        pos += 8 + chunkSize + (chunkSize & 1);
    }
    return 0;
}

void resampleCycle (const float* in, int len, float* out)
{
    if (len <= 0) { std::fill (out, out + Wavetable::kFrameSize, 0.0f); return; }
    if (len == Wavetable::kFrameSize) { std::copy (in, in + len, out); return; }
    for (int i = 0; i < Wavetable::kFrameSize; ++i)
    {
        const double pos = (double) i * (double) len / (double) Wavetable::kFrameSize;
        const int i1 = (int) pos;
        const float t = (float) (pos - i1);
        auto at = [&] (int k) { k %= len; if (k < 0) k += len; return in[k]; };
        const float y0 = at (i1 - 1), y1 = at (i1), y2 = at (i1 + 1), y3 = at (i1 + 2);
        // Catmull-Rom
        out[i] = 0.5f * ((2.0f * y1) + (-y0 + y2) * t + (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * t * t + (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t * t * t);
    }
}

Result loadFromMemory (const void* data, size_t size, ImportMode mode)
{
    Result result;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto stream = std::make_unique<juce::MemoryInputStream> (data, size, false);
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::move (stream)));
    if (reader == nullptr)
    {
        result.error = "Unsupported or damaged audio file.";
        return result;
    }
    const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) Wavetable::kFrameSize * 1024);
    if (length < 8)
    {
        result.error = "The file is too short to be a wavetable.";
        return result;
    }
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
    reader->read (&buffer, 0, length, 0, true, true);
    // mono mix
    std::vector<float> mono ((size_t) length, 0.0f);
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        juce::FloatVectorOperations::add (mono.data(), buffer.getReadPointer (c), length);
    if (buffer.getNumChannels() > 1)
        juce::FloatVectorOperations::multiply (mono.data(), 1.0f / (float) buffer.getNumChannels(), length);

    int frameSize = findClmFrameSize (data, size);
    result.hadClmChunk = frameSize > 0;

    if (mode == ImportMode::singleCycle || (mode == ImportMode::automatic && frameSize == 0 && length <= 8192 && length % Wavetable::kFrameSize != 0))
        frameSize = length;
    else if (mode == ImportMode::slice256)
        frameSize = std::max (1, length / 256);
    else if (frameSize == 0)
    {
        if (length % Wavetable::kFrameSize == 0) frameSize = Wavetable::kFrameSize;
        else if (length <= 8192) frameSize = length;
        else frameSize = Wavetable::kFrameSize;   // treat as consecutive 2048-sample slices
    }

    const int numFrames = juce::jlimit (1, Wavetable::kMaxFrames, length / std::max (1, frameSize));
    result.sourceFrameSize = frameSize;
    result.numFrames = numFrames;
    result.frames.resize ((size_t) numFrames * Wavetable::kFrameSize);
    // When there are more source frames than allowed, pick them evenly.
    const int totalSourceFrames = std::max (1, length / std::max (1, frameSize));
    for (int f = 0; f < numFrames; ++f)
    {
        const int src = numFrames == 1 ? 0 : (int) ((juce::int64) f * (totalSourceFrames - 1) / std::max (1, numFrames - 1));
        resampleCycle (mono.data() + (size_t) src * (size_t) frameSize, std::min (frameSize, length - src * frameSize),
                       result.frames.data() + (size_t) f * Wavetable::kFrameSize);
    }
    return result;
}

Result loadFile (const juce::File& file, ImportMode mode)
{
    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb))
    {
        Result r;
        r.error = "Could not read " + file.getFullPathName();
        return r;
    }
    return loadFromMemory (mb.getData(), mb.getSize(), mode);
}

WavetablePtr loadWavetable (const juce::File& file, juce::String& error, ImportMode mode)
{
    auto r = loadFile (file, mode);
    if (! r.ok())
    {
        error = r.error;
        return nullptr;
    }
    Wavetable::Options o;
    o.normalise = true;
    auto table = std::const_pointer_cast<Wavetable> (Wavetable::create (file.getFileNameWithoutExtension(), std::move (r.frames), o));
    table->sourcePath = file.getFullPathName();
    return table;
}

bool saveWav (const juce::File& file, const Wavetable& table)
{
    const auto& raw = table.getRawData();
    juce::MemoryOutputStream out;
    const juce::String clmText = "<!>2048 00000000 wavetable CUREUM";
    const int clmLen = (int) clmText.getNumBytesAsUTF8() + 1;
    const int clmPadded = clmLen + (clmLen & 1);
    const auto dataBytes = (juce::uint32) (raw.size() * sizeof (float));
    const juce::uint32 riffSize = 4 + (8 + 16) + (8 + (juce::uint32) clmPadded) + (8 + dataBytes);

    out.write ("RIFF", 4); out.writeInt ((int) riffSize); out.write ("WAVE", 4);
    out.write ("fmt ", 4); out.writeInt (16);
    out.writeShort (3);            // IEEE float
    out.writeShort (1);            // mono
    out.writeInt (44100);
    out.writeInt (44100 * 4);
    out.writeShort (4);
    out.writeShort (32);
    out.write ("clm ", 4); out.writeInt (clmLen);
    out.write (clmText.toRawUTF8(), (size_t) clmLen - 1); out.writeByte (0);
    if (clmLen & 1) out.writeByte (0);
    out.write ("data", 4); out.writeInt ((int) dataBytes);
    for (float s : raw) out.writeFloat (s);

    return file.replaceWithData (out.getData(), out.getDataSize());
}

} // namespace cureum::WavetableIO
