#include "FxRack.h"

namespace cureum
{

FxRack::FxRack()
{
    units[(size_t) FxType::hyper]      = std::make_unique<HyperDimension>();
    units[(size_t) FxType::distortion] = std::make_unique<Distortion>();
    units[(size_t) FxType::flanger]    = std::make_unique<FlangerFx>();
    units[(size_t) FxType::phaser]     = std::make_unique<PhaserFx>();
    units[(size_t) FxType::chorus]     = std::make_unique<ChorusFx>();
    units[(size_t) FxType::delay]      = std::make_unique<StereoDelay>();
    units[(size_t) FxType::compressor] = std::make_unique<Compressor>();
    units[(size_t) FxType::reverb]     = std::make_unique<ReverbFx>();
    units[(size_t) FxType::eq]         = std::make_unique<Equalizer>();
    units[(size_t) FxType::filter]     = std::make_unique<FxFilterUnit>();
    units[(size_t) FxType::bode]       = std::make_unique<BodeShifter>();
    units[(size_t) FxType::utility]    = std::make_unique<Utility>();
    units[(size_t) FxType::convolve]   = std::make_unique<Convolver>();
    for (int i = 0; i < 4; ++i)
        units[(size_t) FxType::plugin1 + (size_t) i] = std::make_unique<PluginSlot> (i);
}

void FxRack::prepare (double sampleRate, int maxBlock)
{
    for (auto& u : units)
        u->prepare (sampleRate, maxBlock);
}

void FxRack::reset()
{
    for (auto& u : units)
        u->reset();
}

void FxRack::process (float* l, float* r, int n, const FxContext& ctx, const FxOrder& order)
{
    for (auto t : order.units)
    {
        const auto i = (size_t) t;
        const bool enabled = ctx.on (pid::fxEnable (t));
        if (enabled && ! wasEnabled[i])
            units[i]->reset();   // start clean, no stale tails
        wasEnabled[i] = enabled;
        if (enabled)
            units[i]->process (l, r, n, ctx);
    }
}

int FxRack::getLatencySamples() const
{
    int total = 0;
    for (auto& u : units)
        total += u->getLatencySamples();
    return total;
}

} // namespace cureum
