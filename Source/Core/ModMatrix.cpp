#include "ModMatrix.h"
#include "DspMath.h"
#include <juce_data_structures/juce_data_structures.h>

namespace cureum
{

std::shared_ptr<const CompiledMatrix> CompiledMatrix::compile (const ModSlots& slots)
{
    auto m = std::make_shared<CompiledMatrix>();
    const auto& reg = ParamRegistry::get();

    for (int i = 0; i < kNumModSlots; ++i)
    {
        const auto& s = slots[(size_t) i];
        if (! s.isActive() || s.dest >= pid::total)
            continue;
        const auto& spec = reg.spec (s.dest);
        if (! spec.isModulatable())
            continue;

        Route r;
        r.slot = i;
        r.source = (int) s.source;
        r.aux = (int) s.aux;
        r.dest = s.dest;
        r.bipolar = s.bipolar;
        r.curve = s.curve;
        r.output = s.output;

        m->sourceUsed[(size_t) r.source] = true;
        if (r.aux != 0) m->sourceUsed[(size_t) r.aux] = true;

        m->modulated[(size_t) s.dest] = true;
        if (spec.modScope == ModScope::voice)
        {
            m->voiceRoutes.push_back (r);
            if (std::find (m->voiceDests.begin(), m->voiceDests.end(), r.dest) == m->voiceDests.end())
                m->voiceDests.push_back (r.dest);
        }
        else
        {
            m->globalRoutes.push_back (r);
            if (std::find (m->globalDests.begin(), m->globalDests.end(), r.dest) == m->globalDests.end())
                m->globalDests.push_back (r.dest);
        }
    }
    return m;
}

juce::ValueTree modSlotsToTree (const ModSlots& slots)
{
    juce::ValueTree tree ("ModMatrix");
    const auto& reg = ParamRegistry::get();
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const auto& s = slots[(size_t) i];
        if (s.source == ModSource::none && s.dest < 0)
            continue;
        juce::ValueTree t ("Slot");
        t.setProperty ("index", i, nullptr);
        t.setProperty ("source", modSourceKeys()[(int) s.source], nullptr);
        t.setProperty ("aux", modSourceKeys()[(int) s.aux], nullptr);
        t.setProperty ("dest", s.dest >= 0 ? reg.spec (s.dest).id : juce::String(), nullptr);
        t.setProperty ("bipolar", s.bipolar, nullptr);
        t.setProperty ("curve", s.curve, nullptr);
        t.setProperty ("output", s.output, nullptr);
        t.setProperty ("enabled", s.enabled, nullptr);
        tree.appendChild (t, nullptr);
    }
    return tree;
}

void modSlotsFromTree (const juce::ValueTree& tree, ModSlots& slots)
{
    slots = {};
    const auto& reg = ParamRegistry::get();
    for (const auto& t : tree)
    {
        const int i = t.getProperty ("index", -1);
        if (i < 0 || i >= kNumModSlots)
            continue;
        auto& s = slots[(size_t) i];
        s.source = modSourceFromKey (t.getProperty ("source").toString());
        s.aux = modSourceFromKey (t.getProperty ("aux").toString());
        s.dest = reg.indexOf (t.getProperty ("dest").toString());
        s.bipolar = t.getProperty ("bipolar", false);
        s.curve = (float) (double) t.getProperty ("curve", 0.0);
        s.output = (float) (double) t.getProperty ("output", 1.0);
        s.enabled = t.getProperty ("enabled", true);
    }
}

} // namespace cureum
