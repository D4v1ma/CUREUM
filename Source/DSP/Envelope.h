#pragma once

#include "../Core/DspMath.h"

namespace cureum
{

// AHDSR envelope with curved attack, decay and release segments (Serum-style).
class Envelope
{
public:
    enum class Stage { idle, attack, hold, decay, sustain, release };

    void setSampleRate (double rate) noexcept { sr = (float) rate; }

    void setParameters (float attackS, float holdS, float decayS, float sustainLevel, float releaseS,
                        float attackCurve, float decayCurve, float releaseCurve) noexcept
    {
        aInc = rateFor (attackS);
        hInc = rateFor (holdS);
        dInc = rateFor (decayS);
        rInc = rateFor (releaseS);
        sustain = dsp::clamp01 (sustainLevel);
        aCurve = attackCurve; dCurve = decayCurve; rCurve = releaseCurve;
    }

    void noteOn (bool fromCurrentLevel) noexcept
    {
        startLevel = fromCurrentLevel ? value : 0.0f;
        stage = Stage::attack;
        t = 0.0f;
    }

    void noteOff() noexcept
    {
        if (stage == Stage::idle || stage == Stage::release) return;
        releaseLevel = value;
        stage = Stage::release;
        t = 0.0f;
    }

    void reset() noexcept { stage = Stage::idle; value = 0.0f; t = 0.0f; }

    inline float next() noexcept
    {
        switch (stage)
        {
            case Stage::idle: value = 0.0f; break;
            case Stage::attack:
                t += aInc;
                if (t >= 1.0f) { t = 0.0f; value = 1.0f; stage = Stage::hold; }
                else value = startLevel + (1.0f - startLevel) * dsp::shapeCurve (t, aCurve);
                break;
            case Stage::hold:
                t += hInc;
                value = 1.0f;
                if (t >= 1.0f) { t = 0.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                t += dInc;
                if (t >= 1.0f) { t = 0.0f; value = sustain; stage = Stage::sustain; }
                else value = 1.0f - (1.0f - sustain) * dsp::shapeCurve (t, dCurve);
                break;
            case Stage::sustain:
                value += (sustain - value) * 0.01f;   // follow sustain changes smoothly
                break;
            case Stage::release:
                t += rInc;
                if (t >= 1.0f) { t = 0.0f; value = 0.0f; stage = Stage::idle; }
                else value = releaseLevel * (1.0f - dsp::shapeCurve (t, rCurve));
                break;
        }
        return value;
    }

    float current() const noexcept { return value; }
    bool isActive() const noexcept { return stage != Stage::idle; }
    bool isReleasing() const noexcept { return stage == Stage::release; }
    Stage getStage() const noexcept { return stage; }

private:
    float rateFor (float seconds) const noexcept { return seconds <= 0.00002f ? 1.0f : 1.0f / (seconds * sr); }

    float sr = 44100.0f;
    Stage stage = Stage::idle;
    float value = 0.0f, t = 0.0f, startLevel = 0.0f, releaseLevel = 0.0f;
    float aInc = 1.0f, hInc = 1.0f, dInc = 1.0f, rInc = 1.0f, sustain = 1.0f;
    float aCurve = 0.0f, dCurve = 0.0f, rCurve = 0.0f;
};

} // namespace cureum
