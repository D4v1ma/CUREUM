#include "TestHelpers.h"
#include "DSP/Oversampling.h"
#include "DSP/Filter.h"
#include "DSP/WavetableFactory.h"
#include "Presets/Cbor.h"
#include "Presets/StateFormat.h"
#include "Presets/WavetableIO.h"
#include "Core/ValueMaps.h"

using namespace cureum;
using namespace cureumtest;

namespace
{
    juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> props)
    {
        auto* o = new juce::DynamicObject();
        for (auto& p : props) o->setProperty (p.first, p.second);
        return juce::var (o);
    }
}

//==============================================================================
struct DspTests : public juce::UnitTest
{
    DspTests() : juce::UnitTest ("DSP building blocks", "CUREUM") {}

    void runTest() override
    {
        beginTest ("Half-band decimator passes low frequencies and rejects images");
        {
            auto measure = [] (double freqAtHighRate)
            {
                Downsampler2x d;
                double peak = 0.0;
                for (int i = 0; i < 8192; ++i)
                {
                    const float a = (float) std::sin (2.0 * dsp::kPiD * freqAtHighRate * (2 * i));
                    const float b = (float) std::sin (2.0 * dsp::kPiD * freqAtHighRate * (2 * i + 1));
                    const float y = d.process (a, b);
                    if (i > 1024) peak = std::max (peak, (double) std::abs (y));
                }
                return peak;
            };
            expectWithinAbsoluteError (measure (0.02), 1.0, 0.02);
            expectLessThan (measure (0.45), 0.001);   // > 60 dB down
        }

        beginTest ("Value maps match Serum curves");
        expectWithinAbsoluteError (maps::envTimeFrom01 (0.11f), 0.000515f, 0.00001f);
        expectWithinAbsoluteError (maps::envTimeFrom01 (0.5f), 1.0f, 0.0001f);
        expectWithinAbsoluteError (maps::cutoffFrom01 (0.5f), 424.6f, 1.0f);
        expectWithinAbsoluteError (maps::cutoffTo01 (maps::cutoffFrom01 (0.3f)), 0.3f, 1.0e-4f);
        expectWithinAbsoluteError (maps::bodeTo01 (maps::bodeFrom01 (0.8f)), 0.8f, 1.0e-4f);

        beginTest ("Wavetable mip-maps keep amplitude and choose safe levels");
        {
            auto table = WavetableFactory::get ("Sine");
            expect (table != nullptr);
            const float* lvl = table->levelFrame (3, 0);
            float peak = 0.0f;
            for (int i = 0; i < Wavetable::levelSize (3); ++i) peak = std::max (peak, std::abs (lvl[i]));
            expectWithinAbsoluteError (peak, 1.0f, 0.02f);
            expectEquals (Wavetable::levelForIncrement (1.0f / 2048.0f), 0);
            expectEquals (Wavetable::levelForIncrement (440.0f / 48000.0f), 5);
            // level L must contain no harmonic above Nyquist: 1024 >> L harmonics * inc <= 0.5
            for (float f : { 50.0f, 220.0f, 1000.0f, 5000.0f, 15000.0f })
            {
                const float inc = f / 48000.0f;
                const int l = Wavetable::levelForIncrement (inc);
                expect ((float) (1024 >> l) * inc <= 0.5001f || l == Wavetable::kNumLevels - 1, "aliasing level for " + juce::String (f));
            }
        }

        beginTest ("Every factory wavetable builds");
        for (auto& n : WavetableFactory::names())
        {
            auto t = WavetableFactory::get (n);
            expect (t != nullptr && t->getNumFrames() >= 1, n);
        }

        beginTest ("Every filter type is stable at high resonance");
        {
            dsp::Random rng (7);
            for (int type = 0; type < numFilterTypes(); ++type)
            {
                MultiFilter f;
                f.prepare (96000.0, 192000.0);
                f.setType (type);
                bool finite = true;
                float peak = 0.0f;
                for (int block = 0; block < 300; ++block)
                {
                    f.setParameters (100.0f + 8000.0f * (float) (block % 50) / 50.0f, 0.95f, 0.5f, 0.5f, 0.3f);
                    for (int i = 0; i < 64; ++i)
                    {
                        float l = rng.bipolar() * 0.5f, r = rng.bipolar() * 0.5f;
                        f.process (l, r);
                        finite = finite && std::isfinite (l) && std::isfinite (r);
                        peak = std::max (peak, std::abs (l));
                    }
                }
                expect (finite, juce::String ("non-finite output: ") + filterTypes()[(size_t) type].name);
                expectLessThan (peak, 60.0f, juce::String ("runaway output: ") + filterTypes()[(size_t) type].name);
            }
        }

        beginTest ("CBOR decoder handles maps, arrays, floats and text");
        {
            const juce::uint8 data[] = { 0xa3, 0x61, 0x61, 0x01, 0x61, 0x62, 0x82, 0xf9, 0x3c, 0x00, 0x20,
                                         0x61, 0x63, 0xfb, 0x40, 0x09, 0x21, 0xfb, 0x54, 0x44, 0x2d, 0x18 };
            juce::String err;
            auto v = CborDecoder::decode (data, sizeof (data), err);
            expect (err.isEmpty(), err);
            expectEquals ((int) v["a"], 1);
            expectWithinAbsoluteError ((double) v["b"][0], 1.0, 1.0e-9);
            expectEquals ((int) v["b"][1], -1);
            expectWithinAbsoluteError ((double) v["c"], 3.14159265358979, 1.0e-9);
            const juce::uint8 bad[] = { 0xbf, 0x61 };
            CborDecoder::decode (bad, sizeof (bad), err);
            expect (err.isNotEmpty());
        }
    }
};

//==============================================================================
struct ImportTests : public juce::UnitTest
{
    ImportTests() : juce::UnitTest ("Serum importers", "CUREUM") {}

    void runTest() override
    {
        using namespace pid;

        beginTest ("Synthetic Serum 2 preset");
        {
            auto data = obj ({
                { "Oscillator0", obj ({ { "plainParams", "default" },
                                        { "WTOsc0", obj ({ { "plainParams", obj ({ { "kParamTablePos", 128.5 }, { "kParamWarp", 0.4 }, { "kParamWarpMenu", "kSync" } }) },
                                                           { "relativePathToWT", "/Analog/Basic Shapes.wav" } }) } }) },
                { "Oscillator1", obj ({ { "plainParams", obj ({ { "kParamEnable", 1.0 }, { "kParamOctave", 2.0 }, { "kParamUnison", 4.0 }, { "kParamVolume", 0.25 } }) } }) },
                { "VoiceFilter0", obj ({ { "plainParams", obj ({ { "kParamEnable", 1.0 }, { "kParamType", "LBH12" }, { "kParamFreq", 0.25 }, { "kParamReso", 40.0 } }) } }) },
                { "Env1", obj ({ { "plainParams", obj ({ { "kParamAttack", 0.25 }, { "kParamDecay", 0.5 }, { "kParamSustain", 0.3 } }) } }) },
                { "LFO2", obj ({ { "plainParams", obj ({ { "kParamMode", "Free" }, { "kParamType", "Lorenz" } }) } }) },
                { "Macro0", obj ({ { "name", "Cutoff" }, { "plainParams", obj ({ { "kParamValue", 50.0 } }) } }) },
                { "ModSlot0", obj ({ { "source", juce::Array<juce::var> { 7, 0 } }, { "destModuleTypeString", "VoiceFilter" }, { "destModuleID", 0 },
                                     { "destModuleParamName", "kParamFreq" }, { "plainParams", obj ({ { "kParamAmount", -30.0 }, { "kParamBipolar", 1.0 } }) } }) },
                { "FXRack0", obj ({ { "FX", juce::Array<juce::var> {
                                        obj ({ { "type", 6 }, { "FXReverb", obj ({ { "plainParams", obj ({ { "kParamWet", 40.0 }, { "kParamSize", 70.0 } }) } }) } }),
                                        obj ({ { "type", 0 }, { "FXDistortion", obj ({ { "plainParams", obj ({ { "kParamDrive", 60.0 }, { "kParamMode", "kDiode2" } }) } }) } }) } } }) },
                { "Global0", obj ({ { "plainParams", obj ({ { "kParamMonoToggle", 1.0 }, { "kParamPolyCount", 8.0 } }) } }) },
            });
            auto file = makeSerum2Preset (data, "Synthetic Bass");
            auto r = SerumImport::importSerum2 (file.getData(), file.getSize(), "Synthetic.SerumPreset");
            expect (r.ok(), r.error);
            expectEquals (r.name, juce::String ("Synthetic Bass"));

            CureumProcessor p;
            p.loadStateTree (r.state, false);
            expectWithinAbsoluteError (p.getPlain (osc (0, Osc::wtPos)), (128.5f - 1.0f) / 255.0f, 0.002f);
            expectEquals ((int) p.getPlain (osc (0, Osc::warp1Mode)), (int) WarpMode::sync);
            expect (p.getPlain (osc (1, Osc::enable)) > 0.5f);
            expectEquals ((int) p.getPlain (osc (1, Osc::octave)), 2);
            expectEquals ((int) p.getPlain (osc (1, Osc::unison)), 4);
            expectWithinAbsoluteError (p.getPlain (osc (1, Osc::level)), 0.5f, 0.01f);
            expectEquals (filterTypeNames()[(int) p.getPlain (filter (0, Filter::type))], juce::String ("L/B/H 12"));
            expectWithinAbsoluteError (p.getPlain (filter (0, Filter::cutoff)), maps::cutoffFrom01 (0.25f), 0.5f);
            expectWithinAbsoluteError (p.getPlain (filter (0, Filter::reso)), 0.4f, 0.001f);
            expectWithinAbsoluteError (p.getPlain (env (1, Env::attack)), 0.25f, 0.001f);
            expectWithinAbsoluteError (p.getPlain (env (1, Env::sustain)), 0.3f, 0.001f);
            expectEquals ((int) p.getPlain (lfo (2, Lfo::type)), (int) LfoType::lorenz);
            expectEquals ((int) p.getPlain (lfo (2, Lfo::mode)), (int) LfoMode::free);
            expectWithinAbsoluteError (p.getPlain (macro (0)), 0.5f, 0.001f);
            expectEquals (p.getMacroName (0), juce::String ("Cutoff"));
            expect (p.getPlain (reverb (Reverb::enable)) > 0.5f);
            expectWithinAbsoluteError (p.getPlain (reverb (Reverb::mix)), 0.4f, 0.001f);
            expectEquals ((int) p.getPlain (dist (Dist::mode)), (int) DistMode::diode2);
            expect (p.getPlain (global (Global::mono)) > 0.5f);
            expectEquals ((int) p.getFxOrder().units.front(), (int) FxType::reverb);
            const auto& slot = p.getModSlots()[0];
            expect (slot.source == ModSource::lfo2);
            expectEquals (slot.dest, filter (0, Filter::cutoff));
            expect (slot.bipolar);
            expectWithinAbsoluteError (p.getPlain (pid::mod (0)), -0.3f, 0.001f);
            auto rr = renderProcessor (p, 1.0, { 36 });
            expect (rr.finite && rr.rms > 0.001f && rr.peak < 4.0f);
        }

        beginTest ("Synthetic Serum 1 FXP");
        {
            std::vector<float> P (299, 0.0f);
            P[0] = 0.6f; P[1] = 0.8f; P[2] = 0.5f; P[3] = 0.625f; P[4] = 0.5f; P[5] = 0.5f; P[6] = 3.0f / 15.0f; P[7] = 0.4f; P[8] = 0.75f;
            P[10] = 0.5f; P[11] = 0.5f; P[12] = 1.0f; P[13] = 0.5f; P[14] = 0.75f; P[16] = 0.5f; P[17] = 0.5f; P[18] = 0.5f; P[23] = 0.5f;
            P[35] = 0.2f; P[37] = 0.5f; P[38] = 0.5f; P[39] = 0.3f;
            P[44] = 3.0f / 95.0f; P[45] = 0.4f; P[46] = 0.6f; P[49] = 1.0f; P[50] = 0.5f;
            P[61] = 0.5f; P[166] = 0.5417f; P[167] = 0.4583f; P[168] = 6.0f / 23.0f;
            P[212] = 1.0f; P[216] = 1.0f; P[40] = 1.0f; P[160] = 1.0f; P[81] = 0.4f;
            auto file = makeSerum1Fxp (P, "Synthetic S1", { { 3, 45, 60 }, { 1, 11, 25 } });
            auto r = SerumImport::importFxp (file.getData(), file.getSize(), "Synthetic.fxp");
            expect (r.ok(), r.error);
            expectEquals (r.name, juce::String ("Synthetic S1"));
            CureumProcessor p;
            p.loadStateTree (r.state, false);
            expectWithinAbsoluteError (p.getPlain (global (Global::master)), 0.6f, 0.001f);
            expectWithinAbsoluteError (p.getPlain (osc (0, Osc::level)), 0.8f, 0.001f);
            expectEquals ((int) p.getPlain (osc (0, Osc::octave)), 1);
            expectEquals ((int) p.getPlain (osc (0, Osc::unison)), 4);
            expectEquals ((int) p.getPlain (osc (0, Osc::warp1Mode)), 6);
            expectWithinAbsoluteError (p.getPlain (osc (0, Osc::wtPos)), 0.5f, 0.001f);
            expectEquals (filterTypeNames()[(int) p.getPlain (filter (0, Filter::type))], juce::String ("MG Low 24"));
            expect (p.getPlain (filter (0, Filter::enable)) > 0.5f);
            expectWithinAbsoluteError (p.getPlain (env (0, Env::attack)), maps::envTimeFrom01 (0.2f), 0.0001f);
            expectEquals ((int) p.getPlain (global (Global::bendUp)), 2);
            expect (p.getPlain (reverb (Reverb::enable)) > 0.5f);
            expectEquals (p.getWavetable (0)->getName(), juce::String ("Basic Shapes"));
            expect (p.getModSlots()[0].source == ModSource::env2);
            expectEquals (p.getModSlots()[0].dest, filter (0, Filter::cutoff));
            expectWithinAbsoluteError (p.getPlain (pid::mod (0)), 0.6f, 0.001f);
            expect (p.getModSlots()[1].source == ModSource::modWheel);
            expectEquals (p.getModSlots()[1].dest, osc (0, Osc::wtPos));
        }

        beginTest ("Serum filter names");
        expectEquals (filterTypeNames()[SerumImport::filterTypeFromSerumName ("kMGLow24")], juce::String ("MG Low 24"));
        expectEquals (filterTypeNames()[SerumImport::filterTypeFromSerumName ("LBH12")], juce::String ("L/B/H 12"));
        expectEquals (SerumImport::filterTypeFromSerumName ("kNone"), -1);

        beginTest ("Wavetable WAV round trip with clm chunk");
        {
            auto t = WavetableFactory::get ("PWM");
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cureum_test_wt.wav");
            expect (WavetableIO::saveWav (f, *t));
            juce::MemoryBlock mb;
            f.loadFileAsData (mb);
            expectEquals (WavetableIO::findClmFrameSize (mb.getData(), mb.getSize()), 2048);
            juce::String err;
            auto back = WavetableIO::loadWavetable (f, err);
            expect (back != nullptr, err);
            expectEquals (back->getNumFrames(), t->getNumFrames());
            f.deleteFile();
        }

        // Optional: real Serum files (not redistributed with the repository).
        const auto fixtures = juce::SystemStats::getEnvironmentVariable ("CUREUM_FIXTURES", {});
        if (fixtures.isNotEmpty())
        {
            beginTest ("Real Serum presets in " + fixtures);
            for (const auto& e : juce::RangedDirectoryIterator (juce::File (fixtures), true, "*.fxp;*.SerumPreset", juce::File::findFiles))
            {
                auto r = SerumImport::importFile (e.getFile());
                expect (r.ok(), e.getFile().getFileName() + ": " + r.error);
                if (! r.ok()) continue;
                CureumProcessor p;
                p.loadStateTree (r.state, false);
                auto rr = renderProcessor (p, 1.0, { 48, 60 });
                expect (rr.finite, "non-finite render: " + e.getFile().getFileName());
                expectLessThan (rr.peak, 4.0f, "too loud: " + e.getFile().getFileName());
                logMessage (e.getFile().getFileName() + " -> " + r.name + "  rms " + juce::String (rr.rms, 4) + "  warnings " + juce::String (r.warnings.size()));
            }
        }
    }
};

//==============================================================================
struct EngineTests : public juce::UnitTest
{
    EngineTests() : juce::UnitTest ("Engine and presets", "CUREUM") {}

    void runTest() override
    {
        using namespace pid;

        beginTest ("Init patch makes sound and stops");
        {
            CureumProcessor p;
            auto r = renderProcessor (p, 1.5, { 60 });
            expect (r.finite);
            expectGreaterThan (r.rms, 0.01f);
            expectLessThan (r.peak, 2.0f);
            // tail after release must decay to silence
            float tail = 0.0f;
            for (int i = r.audio.getNumSamples() - 2000; i < r.audio.getNumSamples(); ++i) tail = std::max (tail, std::abs (r.audio.getSample (0, i)));
            expectLessThan (tail, 0.001f);
        }

        beginTest ("All warp modes render finite audio");
        for (int w = 0; w < (int) WarpMode::count; ++w)
        {
            CureumProcessor p;
            p.setPlain (osc (0, Osc::warp1Mode), (float) w);
            p.setPlain (osc (0, Osc::warp1Amt), 0.7f);
            p.setPlain (osc (1, Osc::enable), 1.0f);
            p.setPlain (osc (0, Osc::unison), 3.0f);
            auto r = renderProcessor (p, 0.3, { 57 });
            expect (r.finite && r.peak < 4.0f, "warp " + warpModeNames()[w]);
        }

        beginTest ("All effects render finite audio");
        for (int t = 0; t < kNumFxTypes; ++t)
        {
            CureumProcessor p;
            p.setPlain (pid::fxEnable ((FxType) t), 1.0f);
            auto order = p.getFxOrder();
            if (! order.contains ((FxType) t)) order.units.push_back ((FxType) t);
            p.setFxOrder (order);
            auto r = renderProcessor (p, 0.6, { 60, 64 });
            expect (r.finite && r.peak < 4.0f, "fx " + fxTypeNames()[t]);
        }

        beginTest ("Oversampling modes");
        for (int q = 0; q < 3; ++q)
        {
            CureumProcessor p;
            p.setPlain (global (Global::quality), (float) q);
            auto r = renderProcessor (p, 0.5, { 72 });
            expect (r.finite && r.rms > 0.01f, "quality " + juce::String (q));
        }

        beginTest ("Factory presets render cleanly");
        for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
        {
            CureumProcessor p;
            p.loadStateTree (PresetManager::createFactoryPreset (i), false);
            auto r = renderProcessor (p, 1.5, { 48, 55, 60 });
            expect (r.finite, "non-finite: " + PresetManager::getFactoryName (i));
            expectLessThan (r.peak, 2.5f, "too loud: " + PresetManager::getFactoryName (i));
            expectGreaterThan (r.rms, 0.002f, "silent: " + PresetManager::getFactoryName (i));
            logMessage (PresetManager::getFactoryName (i) + ": peak " + juce::String (r.peak, 3) + " rms " + juce::String (r.rms, 4));
        }

        beginTest ("State round trip");
        {
            CureumProcessor a;
            a.loadStateTree (PresetManager::createFactoryPreset (1), false);
            a.setMacroName (2, "Space");
            auto tree = a.createStateTree();
            CureumProcessor b;
            b.loadStateTree (tree, false);
            for (int i = 0; i < pid::total; ++i)
                expectWithinAbsoluteError (b.getPlain (i), a.getPlain (i), 1.0e-5f);
            for (int s = 0; s < kNumModSlots; ++s)
                expect (a.getModSlots()[(size_t) s] == b.getModSlots()[(size_t) s]);
            expectEquals (b.getMacroName (2), juce::String ("Space"));
            expectEquals (b.getFxOrder().toString(), a.getFxOrder().toString());
            expectEquals (b.getWavetable (0)->getName(), a.getWavetable (0)->getName());

            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cureum_roundtrip.cureum");
            expect (StateFormat::writePresetFile (tree, f));
            auto back = StateFormat::readPresetFile (f);
            expect (back.isEquivalentTo (tree));
            f.deleteFile();
        }

        beginTest ("Mono legato and polyphony limit");
        {
            CureumProcessor p;
            p.setPlain (global (Global::polyphony), 2.0f);
            p.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            for (int n : { 60, 62, 64, 65 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            p.processBlock (buf, midi);
            for (int i = 0; i < 20; ++i) { juce::MidiBuffer m; p.processBlock (buf, m); }
            expectLessOrEqual (p.feedback.activeVoices.load(), 2);
        }
    }
};

static DspTests dspTests;
static ImportTests importTests;
static EngineTests engineTests;
