#pragma once

#include "../DSP/Wavetable.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace cureum
{

// Reading and writing wavetable WAV files, including Serum's convention: a 'clm ' RIFF chunk whose
// text starts with "<!>2048" announces the frame size (Serum stores 2048-sample frames).
namespace WavetableIO
{
    enum class ImportMode { automatic, singleCycle, fixedFrames, slice256 };

    struct Result
    {
        std::vector<float> frames;   // numFrames * 2048
        int numFrames = 0;
        int sourceFrameSize = 0;
        bool hadClmChunk = false;
        juce::String error;
        bool ok() const { return numFrames > 0 && error.isEmpty(); }
    };

    Result loadFile (const juce::File& file, ImportMode mode = ImportMode::automatic);
    Result loadFromMemory (const void* data, size_t size, ImportMode mode = ImportMode::automatic);

    // Convenience: build a Wavetable directly.
    WavetablePtr loadWavetable (const juce::File& file, juce::String& error, ImportMode mode = ImportMode::automatic);

    bool saveWav (const juce::File& file, const Wavetable& table);

    // Returns the frame size announced by a 'clm ' chunk, or 0.
    int findClmFrameSize (const void* data, size_t size);

    // Resamples one cycle of arbitrary length to 2048 samples (cubic interpolation).
    void resampleCycle (const float* in, int inLength, float* out2048);
}

} // namespace cureum
