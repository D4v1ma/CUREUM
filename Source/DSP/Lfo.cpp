#include "Lfo.h"

namespace cureum
{

//==============================================================================
LfoShape::LfoShape() : LfoShape ({ { 0.0f, 0.0f, 0.5f }, { 0.5f, 1.0f, 0.5f }, { 1.0f, 0.0f, 0.5f } }, "Triangle") {}

LfoShape::LfoShape (std::vector<LfoPoint> p, juce::String n) : pts (std::move (p)), name (std::move (n))
{
    if (pts.size() < 2)
        pts = { { 0.0f, 0.0f, 0.5f }, { 1.0f, 0.0f, 0.5f } };
    std::sort (pts.begin(), pts.end(), [] (const LfoPoint& a, const LfoPoint& b) { return a.x < b.x; });
    for (auto& pt : pts)
    {
        pt.x = dsp::clamp01 (pt.x);
        pt.y = dsp::clamp01 (pt.y);
        pt.curve = dsp::clamp01 (pt.curve);
    }
    pts.front().x = 0.0f;
    pts.back().x = 1.0f;
    compile();
}

float LfoShape::segmentCurve (float t, float curve) noexcept
{
    // curve 0.5 = linear; < 0.5 = slow start (convex), > 0.5 = fast start (concave)
    return dsp::shapeCurve (t, (curve - 0.5f) * 2.0f);
}

float LfoShape::evaluate (float x) const noexcept
{
    x = dsp::clamp01 (x);
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const auto& a = pts[i];
        const auto& b = pts[i + 1];
        if (x <= b.x || i + 2 == pts.size())
        {
            const float w = b.x - a.x;
            if (w <= 1.0e-6f) return b.y;
            const float t = dsp::clamp01 ((x - a.x) / w);
            return a.y + (b.y - a.y) * segmentCurve (t, a.curve);
        }
    }
    return pts.back().y;
}

void LfoShape::compile()
{
    table.resize ((size_t) kTableSize + 1);
    for (int i = 0; i < kTableSize; ++i)
        table[(size_t) i] = evaluate ((float) i / (float) kTableSize);
    table[(size_t) kTableSize] = pts.back().y;
}

std::vector<juce::String> LfoShape::presetNames()
{
    return { "Triangle", "Sine", "Saw Up", "Saw Down", "Square", "Pulse 25%", "Ramp Exp", "Ramp Log",
             "S-Curve", "Steps 4", "Steps 8", "Bounce", "Gate 1/2", "Trance Gate", "Sidechain", "Ramp Up Hold" };
}

LfoShape LfoShape::preset (const juce::String& n)
{
    using P = LfoPoint;
    if (n == "Sine")       return LfoShape ({ { 0, 0.5f, 0.7f }, { 0.25f, 1, 0.3f }, { 0.5f, 0.5f, 0.7f }, { 0.75f, 0, 0.3f }, { 1, 0.5f, 0.5f } }, n);
    if (n == "Saw Up")     return LfoShape ({ { 0, 0, 0.5f }, { 1, 1, 0.5f } }, n);
    if (n == "Saw Down")   return LfoShape ({ { 0, 1, 0.5f }, { 1, 0, 0.5f } }, n);
    if (n == "Square")     return LfoShape ({ { 0, 1, 0.5f }, { 0.4999f, 1, 0.5f }, { 0.5f, 0, 0.5f }, { 1, 0, 0.5f } }, n);
    if (n == "Pulse 25%")  return LfoShape ({ { 0, 1, 0.5f }, { 0.2499f, 1, 0.5f }, { 0.25f, 0, 0.5f }, { 1, 0, 0.5f } }, n);
    if (n == "Ramp Exp")   return LfoShape ({ { 0, 0, 0.15f }, { 1, 1, 0.5f } }, n);
    if (n == "Ramp Log")   return LfoShape ({ { 0, 0, 0.85f }, { 1, 1, 0.5f } }, n);
    if (n == "S-Curve")    return LfoShape ({ { 0, 0, 0.2f }, { 0.5f, 0.5f, 0.8f }, { 1, 1, 0.5f } }, n);
    if (n == "Steps 4" || n == "Steps 8")
    {
        const int steps = n == "Steps 4" ? 4 : 8;
        std::vector<P> p;
        for (int i = 0; i < steps; ++i)
        {
            const float y = (float) i / (float) (steps - 1);
            p.push_back ({ (float) i / (float) steps, y, 0.5f });
            p.push_back ({ (float) (i + 1) / (float) steps - 0.0001f, y, 0.5f });
        }
        p.back().x = 1.0f;
        return LfoShape (p, n);
    }
    if (n == "Bounce")       return LfoShape ({ { 0, 1, 0.2f }, { 0.4f, 0, 0.8f }, { 0.6f, 0.45f, 0.2f }, { 0.8f, 0, 0.8f }, { 0.9f, 0.15f, 0.2f }, { 1, 0, 0.5f } }, n);
    if (n == "Gate 1/2")     return LfoShape ({ { 0, 1, 0.5f }, { 0.48f, 1, 0.5f }, { 0.5f, 0, 0.5f }, { 0.98f, 0, 0.5f }, { 1, 1, 0.5f } }, n);
    if (n == "Trance Gate")  return LfoShape ({ { 0, 1, 0.5f }, { 0.2f, 1, 0.5f }, { 0.21f, 0, 0.5f }, { 0.25f, 0, 0.5f }, { 0.26f, 1, 0.5f }, { 0.45f, 1, 0.5f },
                                                { 0.46f, 0, 0.5f }, { 0.5f, 0, 0.5f }, { 0.51f, 1, 0.5f }, { 0.95f, 1, 0.5f }, { 0.96f, 0, 0.5f }, { 1, 0, 0.5f } }, n);
    if (n == "Sidechain")    return LfoShape ({ { 0, 0, 0.85f }, { 0.35f, 1, 0.5f }, { 1, 1, 0.5f } }, n);
    if (n == "Ramp Up Hold") return LfoShape ({ { 0, 0, 0.5f }, { 0.5f, 1, 0.5f }, { 1, 1, 0.5f } }, n);
    return LfoShape();
}

juce::var LfoShape::toVar() const
{
    juce::Array<juce::var> xs, ys, cs;
    for (auto& p : pts) { xs.add (p.x); ys.add (p.y); cs.add (p.curve); }
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name);
    o->setProperty ("x", xs);
    o->setProperty ("y", ys);
    o->setProperty ("c", cs);
    return juce::var (o);
}

LfoShape LfoShape::fromVar (const juce::var& v)
{
    auto* xs = v["x"].getArray();
    auto* ys = v["y"].getArray();
    auto* cs = v["c"].getArray();
    if (xs == nullptr || ys == nullptr || xs->size() < 2 || xs->size() != ys->size())
        return LfoShape();
    std::vector<LfoPoint> p;
    for (int i = 0; i < xs->size(); ++i)
        p.push_back ({ (float) (double) (*xs)[i], (float) (double) (*ys)[i],
                       cs != nullptr && i < cs->size() ? (float) (double) (*cs)[i] : 0.5f });
    return LfoShape (std::move (p), v["name"].toString());
}

//==============================================================================
void LfoGen::noteOn (const Settings& s, uint32_t seed) noexcept
{
    rng = dsp::Random (seed);
    phase = s.phaseOffset;
    lastCycle = std::floor (phase);
    elapsed = 0.0f;
    envDone = false;
    shPrev = rng.uniform();
    shNext = rng.uniform();
    cx = 0.1f + rng.bipolar() * 0.05f; cy = 0.0f; cz = 0.0f;
    if (s.type == LfoType::rossler) { cx = 1.0f; cy = 1.0f; cz = 0.0f; }
    smoothed = -1.0f;
}

float LfoGen::sample (const Settings& s, const LfoShape* shape, double ph) noexcept
{
    switch (s.type)
    {
        case LfoType::shape:
            return shape != nullptr ? shape->lookup ((float) (ph - std::floor (ph))) : 0.0f;
        case LfoType::sampleHold:
            return shPrev;
        case LfoType::smoothRandom:
        {
            const float t = (float) (ph - std::floor (ph));
            const float w = 0.5f - 0.5f * std::cos (dsp::kPi * t);
            return shPrev + (shNext - shPrev) * w;
        }
        case LfoType::lorenz:  return dsp::clamp01 ((cx + 20.0f) / 40.0f);
        case LfoType::rossler: return dsp::clamp01 ((cx + 11.0f) / 23.0f);
        case LfoType::count:   break;
    }
    return 0.0f;
}

float LfoGen::advance (const Settings& s, const LfoShape* shape, double cycles, float seconds, float rateHz, double globalPhase) noexcept
{
    elapsed += seconds;

    if (s.mode == LfoMode::free && globalPhase >= 0.0)
        phase = globalPhase + s.phaseOffset;
    else if (! envDone)
        phase += cycles;

    if (s.mode == LfoMode::env && phase - s.phaseOffset >= 1.0)
    {
        envDone = true;
        phase = s.phaseOffset + 0.99999;
    }

    // New random targets on every cycle boundary.
    const double cyc = std::floor (phase);
    if (cyc != lastCycle)
    {
        lastCycle = cyc;
        shPrev = s.type == LfoType::smoothRandom ? shNext : rng.uniform();
        shNext = rng.uniform();
    }

    // Chaotic attractors are integrated in fixed sub-steps for stability.
    if (s.type == LfoType::lorenz || s.type == LfoType::rossler)
    {
        float dtTotal = juce::jlimit (0.0f, 2.0f, rateHz * seconds * (s.type == LfoType::lorenz ? 0.5f : 3.0f));
        while (dtTotal > 0.0f)
        {
            const float dt = std::min (dtTotal, 0.005f);
            dtTotal -= dt;
            if (s.type == LfoType::lorenz)
            {
                const float dx = 10.0f * (cy - cx), dy = cx * (28.0f - cz) - cy, dz = cx * cy - (8.0f / 3.0f) * cz;
                cx += dx * dt; cy += dy * dt; cz += dz * dt;
            }
            else
            {
                const float dx = -cy - cz, dy = cx + 0.2f * cy, dz = 0.2f + cz * (cx - 5.7f);
                cx += dx * dt; cy += dy * dt; cz += dz * dt;
            }
        }
        if (! std::isfinite (cx)) { cx = 0.1f; cy = 0.0f; cz = 0.0f; }
    }

    float v = sample (s, shape, phase);

    // Delay and rise (fade-in)
    if (elapsed < s.delay) v = 0.0f;
    else if (s.rise > 0.0f) v *= dsp::clamp01 ((elapsed - s.delay) / s.rise);

    // Smoothing (relative to the cycle length)
    if (smoothed < 0.0f) smoothed = v;
    if (s.smooth > 0.0f)
    {
        const float tau = s.smooth * s.smooth * 0.5f / std::max (0.05f, rateHz);
        const float a = std::exp (-seconds / std::max (1.0e-4f, tau));
        smoothed = v + a * (smoothed - v);
    }
    else
        smoothed = v;

    out = smoothed;
    return out;
}

} // namespace cureum
