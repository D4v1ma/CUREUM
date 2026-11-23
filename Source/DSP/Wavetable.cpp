#include "Wavetable.h"
#include <juce_dsp/juce_dsp.h>

namespace cureum
{

namespace
{
    // FFT objects are immutable after construction and safe to use from several threads.
    const juce::dsp::FFT& fftForOrder (int order)
    {
        static const std::array<std::unique_ptr<juce::dsp::FFT>, 12> ffts = []
        {
            std::array<std::unique_ptr<juce::dsp::FFT>, 12> a;
            for (int i = 4; i < 12; ++i)
                a[(size_t) i] = std::make_unique<juce::dsp::FFT> (i);
            return a;
        }();
        return *ffts[(size_t) juce::jlimit (4, 11, order)];
    }

    int orderForSize (int size)
    {
        int order = 0;
        while ((1 << order) < size) ++order;
        return order;
    }
}

std::shared_ptr<const Wavetable> Wavetable::create (const juce::String& tableName, std::vector<float> frames, Options options)
{
    auto table = std::shared_ptr<Wavetable> (new Wavetable());
    table->name = tableName;

    const int n = juce::jlimit (1, kMaxFrames, (int) (frames.size() / kFrameSize));
    frames.resize ((size_t) n * kFrameSize, 0.0f);
    table->numFrames = n;
    table->interpolate = options.interpolateFrames;
    table->raw = std::move (frames);
    table->build (options);
    return table;
}

void Wavetable::build (const Options& options)
{
    // Sanitise, remove DC and optionally normalise the raw frames.
    for (auto& s : raw)
        if (! std::isfinite (s)) s = 0.0f;

    if (options.removeDC)
    {
        for (int f = 0; f < numFrames; ++f)
        {
            float* fr = raw.data() + (size_t) f * kFrameSize;
            double sum = 0.0;
            for (int i = 0; i < kFrameSize; ++i) sum += fr[i];
            const auto dc = (float) (sum / kFrameSize);
            for (int i = 0; i < kFrameSize; ++i) fr[i] -= dc;
        }
    }

    if (options.normalisePerFrame)
    {
        for (int f = 0; f < numFrames; ++f)
        {
            float* fr = raw.data() + (size_t) f * kFrameSize;
            float peak = 0.0f;
            for (int i = 0; i < kFrameSize; ++i) peak = std::max (peak, std::abs (fr[i]));
            if (peak > 1.0e-6f)
                for (int i = 0; i < kFrameSize; ++i) fr[i] /= peak;
        }
    }
    else if (options.normalise)
    {
        float peak = 0.0f;
        for (auto s : raw) peak = std::max (peak, std::abs (s));
        if (peak > 1.0e-6f)
            for (auto& s : raw) s /= peak;
    }

    // Allocate mip storage: each level frame gets one guard sample for interpolation.
    size_t total = 0;
    for (int l = 0; l < kNumLevels; ++l)
    {
        levelOffsets[(size_t) l] = total;
        total += (size_t) numFrames * (size_t) (kLevelSizes[(size_t) l] + 1);
    }
    mips.assign (total, 0.0f);

    const auto& forward = fftForOrder (11);
    std::vector<float> spectrum ((size_t) kFrameSize * 2);
    std::vector<float> work ((size_t) kFrameSize * 2);

    for (int f = 0; f < numFrames; ++f)
    {
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        std::copy_n (raw.data() + (size_t) f * kFrameSize, kFrameSize, spectrum.data());
        forward.performRealOnlyForwardTransform (spectrum.data(), true);
        spectrum[0] = spectrum[1] = 0.0f; // DC

        for (int l = 0; l < kNumLevels; ++l)
        {
            const int size = kLevelSizes[(size_t) l];
            const int maxHarm = std::min (1023, 1024 >> l);
            const float scale = (float) size / (float) kFrameSize;

            std::fill (work.begin(), work.end(), 0.0f);
            for (int k = 1; k <= maxHarm && k < size / 2; ++k)
            {
                work[(size_t) k * 2]     = spectrum[(size_t) k * 2] * scale;
                work[(size_t) k * 2 + 1] = spectrum[(size_t) k * 2 + 1] * scale;
            }
            fftForOrder (orderForSize (size)).performRealOnlyInverseTransform (work.data());

            float* dst = mips.data() + levelOffsets[(size_t) l] + (size_t) f * (size_t) (size + 1);
            std::copy_n (work.data(), size, dst);
            dst[size] = dst[0];
        }
    }
}

std::vector<float> Wavetable::harmonicMagnitudes (const float* frame, int numHarmonics)
{
    std::vector<float> spectrum ((size_t) kFrameSize * 2, 0.0f);
    std::copy_n (frame, kFrameSize, spectrum.data());
    fftForOrder (11).performRealOnlyForwardTransform (spectrum.data(), true);

    std::vector<float> mags ((size_t) numHarmonics, 0.0f);
    for (int k = 1; k <= numHarmonics && k < kFrameSize / 2; ++k)
    {
        const float re = spectrum[(size_t) k * 2], im = spectrum[(size_t) k * 2 + 1];
        mags[(size_t) k - 1] = std::sqrt (re * re + im * im) * (2.0f / (float) kFrameSize);
    }
    return mags;
}

void Wavetable::framesFromHarmonics (const float* magnitudes, const float* phases, int numHarmonics, float* outFrame)
{
    std::vector<float> work ((size_t) kFrameSize * 2, 0.0f);
    for (int k = 1; k <= numHarmonics && k < kFrameSize / 2; ++k)
    {
        const float m = magnitudes[k - 1] * (float) kFrameSize * 0.5f;
        const float ph = phases != nullptr ? phases[k - 1] : 0.0f;
        // Sine phase convention: amplitude m of sin(2*pi*k*x + ph)
        work[(size_t) k * 2]     = m * std::sin (ph);
        work[(size_t) k * 2 + 1] = -m * std::cos (ph);
    }
    fftForOrder (11).performRealOnlyInverseTransform (work.data());
    std::copy_n (work.data(), kFrameSize, outFrame);
}

} // namespace cureum
