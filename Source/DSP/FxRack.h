#pragma once

#include "Effects.h"
#include "SpaceEffects.h"
#include "../Host/PluginSlot.h"
#include <array>

namespace cureum
{

// Which effects are in the rack and in which order (Serum-style reorderable chain).
struct FxOrder
{
    std::vector<FxType> units;

    static FxOrder defaultOrder()
    {
        FxOrder o;
        for (auto t : { FxType::hyper, FxType::distortion, FxType::flanger, FxType::phaser, FxType::chorus,
                        FxType::delay, FxType::compressor, FxType::reverb, FxType::eq, FxType::filter })
            o.units.push_back (t);
        return o;
    }
    bool contains (FxType t) const { return std::find (units.begin(), units.end(), t) != units.end(); }
    juce::String toString() const
    {
        juce::StringArray s;
        for (auto t : units) s.add (fxTypeKey (t));
        return s.joinIntoString (",");
    }
    static FxOrder fromString (const juce::String& str)
    {
        FxOrder o;
        for (auto& k : juce::StringArray::fromTokens (str, ",", ""))
            for (int i = 0; i < kNumFxTypes; ++i)
                if (k.trim() == fxTypeKey ((FxType) i) && ! o.contains ((FxType) i))
                    o.units.push_back ((FxType) i);
        return o;
    }
};

class FxRack
{
public:
    FxRack();
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const FxContext& ctx, const FxOrder& order);
    int getLatencySamples() const;

    FxUnit& unit (FxType t) { return *units[(size_t) t]; }
    PluginSlot& pluginSlot (int i) { return *static_cast<PluginSlot*> (units[(size_t) FxType::plugin1 + (size_t) i].get()); }
    Convolver& convolver() { return *static_cast<Convolver*> (units[(size_t) FxType::convolve].get()); }
    Compressor& compressor() { return *static_cast<Compressor*> (units[(size_t) FxType::compressor].get()); }

private:
    std::array<std::unique_ptr<FxUnit>, (size_t) kNumFxTypes> units;
    std::array<bool, (size_t) kNumFxTypes> wasEnabled {};
};

} // namespace cureum
