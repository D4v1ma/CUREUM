#pragma once

#include <cmath>
#include <algorithm>

// Knob-position <-> physical-value curves. They intentionally match the curves used by Serum
// (verified against real Serum 1 and Serum 2 preset files) so that imported presets sound right:
//  - Envelope / glide times: seconds = 32 * x^5   (x = 0.11 -> 0.515 ms, 0.215 -> 14.7 ms, 0.5 -> 1 s)
//  - Filter cutoff: 8.18 Hz (MIDI note 0) to 22.05 kHz, exponential (x = 0.5 -> ~425 Hz)
namespace cureum::maps
{
    constexpr float kEnvMaxSeconds = 32.0f;
    constexpr float kCutoffMinHz   = 8.1757989f;
    constexpr float kCutoffMaxHz   = 22050.0f;
    constexpr float kLfoMinHz      = 0.01f;
    constexpr float kLfoMaxHz      = 100.0f;
    constexpr float kBodeMaxHz     = 5000.0f;

    inline float envTimeFrom01 (float x) { x = std::clamp (x, 0.0f, 1.0f); return kEnvMaxSeconds * x * x * x * x * x; }
    inline float envTimeTo01 (float s)   { return std::pow (std::clamp (s / kEnvMaxSeconds, 0.0f, 1.0f), 0.2f); }

    inline float cutoffFrom01 (float x)  { return kCutoffMinHz * std::pow (kCutoffMaxHz / kCutoffMinHz, std::clamp (x, 0.0f, 1.0f)); }
    inline float cutoffTo01 (float hz)
    {
        hz = std::clamp (hz, kCutoffMinHz, kCutoffMaxHz);
        return std::log (hz / kCutoffMinHz) / std::log (kCutoffMaxHz / kCutoffMinHz);
    }

    inline float lfoHzFrom01 (float x)   { return kLfoMinHz * std::pow (kLfoMaxHz / kLfoMinHz, std::clamp (x, 0.0f, 1.0f)); }
    inline float lfoHzTo01 (float hz)
    {
        hz = std::clamp (hz, kLfoMinHz, kLfoMaxHz);
        return std::log (hz / kLfoMinHz) / std::log (kLfoMaxHz / kLfoMinHz);
    }

    // Bipolar, finer resolution around 0 Hz.
    inline float bodeFrom01 (float x)
    {
        const float t = std::clamp (x, 0.0f, 1.0f) * 2.0f - 1.0f;
        return kBodeMaxHz * t * std::abs (t) * std::abs (t);
    }
    inline float bodeTo01 (float hz)
    {
        const float r = std::clamp (hz / kBodeMaxHz, -1.0f, 1.0f);
        const float t = std::copysign (std::cbrt (std::abs (r)), r);
        return 0.5f * (t + 1.0f);
    }

    // Amplitude taper used by level knobs (knob^2), like Serum's volume controls.
    inline float levelToGain (float knob) { return knob * knob; }
}
