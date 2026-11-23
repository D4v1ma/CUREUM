#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace cureum::dsp
{
    constexpr float kPi     = 3.14159265358979323846f;
    constexpr float kTwoPi  = 6.28318530717958647692f;
    constexpr double kPiD   = 3.14159265358979323846;

    inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    // Rational tanh approximation, accurate to ~1e-4 in [-3, 3] and saturating outside.
    inline float fastTanh (float x) noexcept
    {
        if (x > 4.97f) return 1.0f;
        if (x < -4.97f) return -1.0f;
        const float x2 = x * x;
        const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
        const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
        return a / b;
    }

    inline float softClip (float x) noexcept { return fastTanh (x); }

    // Fast sine for x in [0, 1) (one cycle). Parabolic approximation with refinement.
    inline float fastSin01 (float x) noexcept
    {
        float t = x - std::floor (x);
        t = t * 2.0f - 1.0f;                   // -1..1  maps to -pi..pi
        float y = 4.0f * t * (1.0f - std::abs (t));
        y = 0.225f * (y * std::abs (y) - y) + y;
        return -y;                             // sin(2*pi*x) has the opposite sign of the -pi..pi parabola
    }

    inline float semitonesToRatio (float st) noexcept { return std::exp2 (st * (1.0f / 12.0f)); }

    inline float midiToHz (float note, float a4 = 440.0f) noexcept { return a4 * std::exp2 ((note - 69.0f) * (1.0f / 12.0f)); }

    // Bipolar curve used by envelope segments and modulation curves.
    // c in [-1, 1]: 0 = linear, positive bends towards an exponential shape.
    inline float shapeCurve (float t, float c) noexcept
    {
        t = clamp01 (t);
        if (std::abs (c) < 1.0e-4f) return t;
        const float k = c * 8.0f;  // steepness
        return (1.0f - std::exp (-k * t)) / (1.0f - std::exp (-k));
    }

    // Simple xorshift PRNG (real-time safe, deterministic per seed).
    struct Random
    {
        uint32_t state = 0x12345678u;
        explicit Random (uint32_t seed = 0x12345678u) noexcept : state (seed ? seed : 0x9e3779b9u) {}
        uint32_t next() noexcept { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
        float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // [0, 1)
        float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }                         // [-1, 1)
    };

    // One-pole smoothing coefficient for a time constant in seconds.
    inline float onePoleCoeff (float seconds, float sampleRate) noexcept
    {
        if (seconds <= 0.0f) return 0.0f;
        return std::exp (-1.0f / (seconds * sampleRate));
    }

    inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    inline float flushDenormal (float x) noexcept { return std::abs (x) < 1.0e-20f ? 0.0f : x; }

    // Equal-power pan: p in [-1, 1].
    inline void panGains (float p, float& l, float& r) noexcept
    {
        const float a = (std::clamp (p, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
        l = std::cos (a) * 1.41421356f;
        r = std::sin (a) * 1.41421356f;
    }
}
