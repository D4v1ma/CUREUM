#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <vector>
#include <array>

namespace cureum
{

// An immutable, band-limited wavetable. Frames are 2048 samples (the Serum convention) and are
// stored as mip-maps: level L keeps at most 1024 >> L harmonics, so a lookup never aliases when
// the right level is chosen for the playback frequency. Instances are shared between the audio
// and message threads through std::shared_ptr<const Wavetable>.
class Wavetable
{
public:
    static constexpr int kFrameSize = 2048;
    static constexpr int kMaxFrames = 256;
    static constexpr int kNumLevels = 11;

    struct Options
    {
        bool removeDC = true;
        bool normalise = false;   // scale so the loudest frame peaks at 1.0
        bool normalisePerFrame = false;   // scale every frame to peak at 1.0
        bool interpolateFrames = true;
    };

    // frames: numFrames * kFrameSize samples. Frames of other sizes must be resampled first.
    static std::shared_ptr<const Wavetable> create (const juce::String& name, std::vector<float> frames, Options options);
    static std::shared_ptr<const Wavetable> create (const juce::String& name, std::vector<float> frames)
    {
        return create (name, std::move (frames), Options {});
    }

    const juce::String& getName() const noexcept          { return name; }
    int getNumFrames() const noexcept                      { return numFrames; }
    bool interpolatesFrames() const noexcept               { return interpolate; }
    const float* getFrame (int f) const noexcept           { return raw.data() + (size_t) juce::jlimit (0, numFrames - 1, f) * kFrameSize; }
    const std::vector<float>& getRawData() const noexcept  { return raw; }

    // Where the table came from: a factory table name, a file path, or empty for user tables.
    juce::String sourcePath;
    bool isFactory = false;

    // ---- real-time access
    static int levelSize (int level) noexcept               { return kLevelSizes[(size_t) level]; }
    const float* levelFrame (int level, int frame) const noexcept
    {
        return mips.data() + levelOffsets[(size_t) level] + (size_t) frame * (size_t) (kLevelSizes[(size_t) level] + 1);
    }

    // Pick the mip level for a phase increment in cycles per sample (no harmonics above Nyquist).
    static int levelForIncrement (float inc) noexcept
    {
        const float x = std::abs (inc) * 2048.0f;
        if (x <= 1.0f) return 0;
        int e;
        const float m = std::frexp (x, &e);                // x = m * 2^e, m in [0.5, 1)
        const int lvl = (m == 0.5f) ? e - 1 : e;           // ceil(log2(x))
        return juce::jlimit (0, kNumLevels - 1, lvl);
    }

    // Linear-interpolated lookup inside one level frame; phase in [0, 1).
    static inline float lookup (const float* table, int size, float phase) noexcept
    {
        const float p = phase * (float) size;
        int i = (int) p;
        const float f = p - (float) i;
        i &= (size - 1);
        return table[i] + f * (table[i + 1] - table[i]);
    }

    // Average spectral magnitude of a frame (used by the UI's harmonic editor).
    static std::vector<float> harmonicMagnitudes (const float* frame, int numHarmonics);

    // Builds frames from harmonic magnitudes/phases (additive synthesis via inverse FFT).
    static void framesFromHarmonics (const float* magnitudes, const float* phases, int numHarmonics, float* outFrame);

private:
    Wavetable() = default;
    void build (const Options& options);

    static constexpr std::array<int, kNumLevels> kLevelSizes { 2048, 2048, 1024, 512, 256, 128, 64, 64, 64, 64, 64 };

    juce::String name;
    int numFrames = 0;
    bool interpolate = true;
    std::vector<float> raw;
    std::vector<float> mips;
    std::array<size_t, kNumLevels> levelOffsets {};
};

using WavetablePtr = std::shared_ptr<const Wavetable>;

} // namespace cureum
