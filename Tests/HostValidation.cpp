// Host-side validation of the built plugin binary.
//
// Loads CUREUM.vst3 (or the AU on macOS) through JUCE's plugin hosting code, exactly as a DAW
// would, and runs the kind of checks pluginval performs: plugin info and bus layouts, audio
// processing at several sample rates and block sizes, parameter behaviour, state save/restore
// into a fresh instance, parameter fuzzing, changing parameters from another thread while
// processing, and opening the editor while audio is running.

#include "TestHelpers.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include <thread>
#include <atomic>

namespace cureumtest
{
namespace
{
    struct Report
    {
        int passes = 0, failures = 0;

        void check (bool ok, const juce::String& what)
        {
            if (ok) { ++passes; return; }
            ++failures;
            std::cout << "  FAIL: " << what << std::endl;
        }
        static void section (const juce::String& name) { std::cout << "-- " << name << std::endl; }
    };

    const int chord[] = { 48, 55, 60, 64, 67 };

    struct Stats
    {
        bool finite = true;
        float peakWhileHeld = 0.0f;
        float peakAtEnd = 0.0f;
    };

    // Plays a chord for the first half of `seconds` and releases it for the second half. The
    // last block may be shorter than `block`, as it is in hosts that split buffers at loop points.
    Stats play (juce::AudioPluginInstance& p, double sampleRate, int block, double seconds,
                const std::function<void (int)>& beforeBlock = {}, bool prepare = true)
    {
        Stats st;
        if (prepare)
        {
            p.setRateAndBufferSizeDetails (sampleRate, block);
            p.prepareToPlay (sampleRate, block);
        }

        const int channels = std::max (1, std::max (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()));
        juce::AudioBuffer<float> buffer (channels, block);
        juce::MidiBuffer midi;
        const int total = std::max (1, (int) (seconds * sampleRate));
        const int release = total / 2;
        const int endWindow = total - std::max (1, (int) (0.1 * sampleRate));

        for (int pos = 0, index = 0; pos < total; pos += block, ++index)
        {
            const int n = std::min (block, total - pos);
            buffer.setSize (channels, n, false, false, true);
            buffer.clear();
            midi.clear();

            if (pos == 0)
                for (int note : chord)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (release >= pos && release < pos + n)
                for (int note : chord)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), release - pos);

            if (beforeBlock) beforeBlock (index);
            p.processBlock (buffer, midi);

            float peak = 0.0f;
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float s = buffer.getSample (c, i);
                    if (! std::isfinite (s)) st.finite = false;
                    else peak = std::max (peak, std::abs (s));
                }
            if (pos < release) st.peakWhileHeld = std::max (st.peakWhileHeld, peak);
            if (pos >= endWindow) st.peakAtEnd = std::max (st.peakAtEnd, peak);
        }

        if (prepare) p.releaseResources();
        return st;
    }

    bool isUserParameter (juce::AudioProcessorParameter* param)
    {
        // Skip the wrapper's bypass parameter and the hidden MIDI-CC parameters the VST3 wrapper
        // adds; they are not part of CUREUM's own parameter set.
        const auto name = param->getName (64);
        return param->isAutomatable() && ! name.startsWithIgnoreCase ("MIDI CC") && name != "Bypass";
    }

    juce::Array<juce::AudioProcessorParameter*> userParameters (juce::AudioPluginInstance& p)
    {
        juce::Array<juce::AudioProcessorParameter*> list;
        for (auto* param : p.getParameters())
            if (isUserParameter (param)) list.add (param);
        return list;
    }

    float tolerance (juce::AudioProcessorParameter* param)
    {
        const int steps = param->getNumSteps();
        if (steps > 1 && steps < juce::AudioProcessor::getDefaultNumParameterSteps())
            return 1.0f / (float) (steps - 1) + 1.0e-4f;
        return 1.0e-3f;
    }

    void pumpMessages (int ms)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    std::unique_ptr<juce::AudioPluginInstance> instantiate (juce::AudioPluginFormatManager& fm,
                                                            const juce::PluginDescription& desc,
                                                            juce::String& error)
    {
        return fm.createPluginInstance (desc, 48000.0, 512, error);
    }
}

int runValidate (const juce::File& pluginFile)
{
    Report r;
    const auto start = juce::Time::getMillisecondCounterHiRes();

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    //------------------------------------------------------------------------------------------
    Report::section ("Scan " + pluginFile.getFullPathName());
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* format : formats.getFormats())
        if (format->fileMightContainThisPluginType (pluginFile.getFullPathName()))
            format->findAllTypesForFile (found, pluginFile.getFullPathName());

    r.check (found.size() == 1, "expected exactly one plugin in the file, found " + juce::String (found.size()));
    if (found.isEmpty())
    {
        std::cout << "\n==== Validation: " << r.passes << " passed, " << r.failures << " failed ====" << std::endl;
        return 1;
    }
    const auto desc = *found[0];
    std::cout << "   " << desc.pluginFormatName << " | " << desc.name << " | " << desc.manufacturerName
              << " | " << desc.category << " | version " << desc.version << std::endl;
    r.check (desc.name == "CUREUM", "plugin name is CUREUM");
    r.check (desc.isInstrument, "plugin reports itself as an instrument");

    //------------------------------------------------------------------------------------------
    Report::section ("Instantiate");
    juce::String error;
    auto plugin = instantiate (formats, desc, error);
    r.check (plugin != nullptr, "create instance: " + error);
    if (plugin == nullptr)
    {
        std::cout << "\n==== Validation: " << r.passes << " passed, " << r.failures << " failed ====" << std::endl;
        return 1;
    }
    auto& p = *plugin;
    r.check (p.acceptsMidi(), "accepts MIDI");
    r.check (! p.producesMidi(), "does not produce MIDI");
    r.check (p.getTotalNumOutputChannels() == 2, "stereo output (got " + juce::String (p.getTotalNumOutputChannels()) + ")");
    r.check (p.getTotalNumInputChannels() == 0, "no audio inputs (got " + juce::String (p.getTotalNumInputChannels()) + ")");
    r.check (p.getLatencySamples() >= 0, "latency is not negative");
    r.check (! std::isnan (p.getTailLengthSeconds()), "tail length is a number");
    r.check (p.getTailLengthSeconds() >= 0.0, "tail length is not negative");

    {
        auto layout = p.getBusesLayout();
        r.check (p.checkBusesLayoutSupported (layout), "the default bus layout is supported");
    }

    //------------------------------------------------------------------------------------------
    Report::section ("Audio processing (sample rates x block sizes)");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (int block : { 64, 128, 256, 512, 1024 })
        {
            const auto st = play (p, sr, block, 1.2);
            const auto tag = juce::String (sr, 0) + " Hz / " + juce::String (block);
            r.check (st.finite, "finite output at " + tag);
            r.check (st.peakWhileHeld > 0.01f, "audible output while notes are held at " + tag
                                                    + " (peak " + juce::String (st.peakWhileHeld) + ")");
            r.check (st.peakWhileHeld < 4.0f, "output level is sane at " + tag);
            r.check (st.peakAtEnd < 1.0e-3f, "silence after release at " + tag + " (peak " + juce::String (st.peakAtEnd) + ")");
        }

    Report::section ("Unusual block sizes (1, 7, 100, 441, 2048, 4096)");
    for (int block : { 1, 7, 100, 441, 2048, 4096 })
    {
        const auto st = play (p, 48000.0, block, block == 1 ? 0.3 : 1.0);
        r.check (st.finite, "finite output with block size " + juce::String (block));
        r.check (st.peakWhileHeld > 0.01f, "audible output with block size " + juce::String (block));
    }

    Report::section ("Re-prepare without release (host changes settings while active)");
    {
        p.prepareToPlay (44100.0, 256);
        p.prepareToPlay (96000.0, 1024);
        const auto st = play (p, 96000.0, 1024, 0.5, {}, false);
        p.releaseResources();
        r.check (st.finite, "finite output after re-prepare");
    }

    //------------------------------------------------------------------------------------------
    Report::section ("Parameters");
    auto params = userParameters (p);
    std::cout << "   " << params.size() << " automatable parameters (" << p.getParameters().size() << " including wrapper parameters)" << std::endl;
    r.check (params.size() == (int) pid::total, "exposes all " + juce::String ((int) pid::total) + " CUREUM parameters");
    {
        int badDefault = 0, badName = 0, badText = 0, badRoundTrip = 0;
        juce::StringArray names;
        for (auto* param : params)
        {
            const auto name = param->getName (128);
            if (name.isEmpty() || names.contains (name)) ++badName;
            names.add (name);

            const float def = param->getDefaultValue();
            if (! (def >= 0.0f && def <= 1.0f)) ++badDefault;

            for (float v : { 0.0f, 0.37f, 1.0f })
            {
                param->setValueNotifyingHost (v);
                const float got = param->getValue();
                if (! (got >= 0.0f && got <= 1.0f) || std::abs (got - v) > tolerance (param)) ++badRoundTrip;
                if (param->getText (v, 32).isEmpty()) ++badText;
            }
            param->setValueNotifyingHost (def);
        }
        r.check (badName == 0, juce::String (badName) + " parameters with empty or duplicate names");
        r.check (badDefault == 0, juce::String (badDefault) + " parameters with defaults outside 0..1");
        r.check (badText == 0, juce::String (badText) + " parameters without value text");
        r.check (badRoundTrip == 0, juce::String (badRoundTrip) + " parameter values that did not read back");
    }

    //------------------------------------------------------------------------------------------
    Report::section ("State save and restore into a new instance");
    {
        std::mt19937 rng (1234);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        for (auto* param : params) param->setValueNotifyingHost (uni (rng));
        play (p, 48000.0, 512, 0.1);

        juce::MemoryBlock state;
        p.getStateInformation (state);
        r.check (state.getSize() > 0, "state is not empty (" + juce::String ((int) state.getSize()) + " bytes)");

        std::vector<float> expected;
        for (auto* param : params) expected.push_back (param->getValue());

        auto second = instantiate (formats, desc, error);
        r.check (second != nullptr, "create second instance: " + error);
        if (second != nullptr)
        {
            second->setStateInformation (state.getData(), (int) state.getSize());
            pumpMessages (200);
            auto params2 = userParameters (*second);
            r.check (params2.size() == params.size(), "second instance has the same parameters");
            int mismatches = 0;
            juce::String firstMismatch;
            for (int i = 0; i < std::min (params.size(), params2.size()); ++i)
                if (std::abs (params2[i]->getValue() - expected[(size_t) i]) > tolerance (params2[i]))
                {
                    if (mismatches++ == 0)
                        firstMismatch = params2[i]->getName (64) + " expected " + juce::String (expected[(size_t) i]) + " got " + juce::String (params2[i]->getValue());
                }
            r.check (mismatches == 0, juce::String (mismatches) + " parameters differ after restoring state" + (mismatches > 0 ? " (first: " + firstMismatch + ")" : juce::String()));

            juce::MemoryBlock state2;
            second->getStateInformation (state2);
            r.check (state2.getSize() > 0, "restored instance saves state again");

            const auto st = play (*second, 48000.0, 512, 0.5);
            r.check (st.finite, "restored random state produces finite audio");
        }

        for (auto* param : params) param->setValueNotifyingHost (param->getDefaultValue());
    }

    //------------------------------------------------------------------------------------------
    Report::section ("Parameter fuzzing");
    {
        std::mt19937 rng (99);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        int nonFinite = 0;
        for (int round = 0; round < 12; ++round)
        {
            for (auto* param : params) param->setValueNotifyingHost (uni (rng));
            const auto st = play (p, 48000.0, 256, 0.4);
            if (! st.finite) ++nonFinite;
        }
        r.check (nonFinite == 0, juce::String (nonFinite) + " of 12 random patches produced NaN/Inf");
        for (auto* param : params) param->setValueNotifyingHost (param->getDefaultValue());
    }

    Report::section ("Automation every block");
    {
        std::mt19937 rng (7);
        std::uniform_int_distribution<int> pick (0, params.size() - 1);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        const auto st = play (p, 48000.0, 64, 1.0, [&] (int)
        {
            for (int k = 0; k < 8; ++k) params[pick (rng)]->setValueNotifyingHost (uni (rng));
        });
        r.check (st.finite, "finite output while automating parameters every block");
        for (auto* param : params) param->setValueNotifyingHost (param->getDefaultValue());
    }

    //------------------------------------------------------------------------------------------
    Report::section ("Parameter changes from another thread while processing");
    {
        std::atomic<bool> stop { false };
        std::thread changer ([&]
        {
            std::mt19937 rng (5);
            std::uniform_int_distribution<int> pick (0, params.size() - 1);
            std::uniform_real_distribution<float> uni (0.0f, 1.0f);
            while (! stop.load())
            {
                params[pick (rng)]->setValue (uni (rng));
                std::this_thread::yield();
            }
        });
        const auto st = play (p, 48000.0, 256, 2.0);
        stop = true;
        changer.join();
        r.check (st.finite, "finite output while another thread changes parameters");
        for (auto* param : params) param->setValueNotifyingHost (param->getDefaultValue());
    }

    //------------------------------------------------------------------------------------------
    Report::section ("Editor");
    {
        r.check (p.hasEditor(), "has an editor");
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
        r.check (editor != nullptr, "editor can be created");
        if (editor != nullptr)
        {
            std::cout << "   editor size " << editor->getWidth() << " x " << editor->getHeight() << std::endl;
            r.check (editor->getWidth() >= 600 && editor->getHeight() >= 400, "editor has a usable size");

            editor->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
            editor->setVisible (true);
            pumpMessages (500);

            // Keep the editor open while audio runs on another thread (the UI polls meters,
            // scopes and modulation feedback from the audio thread).
            p.setRateAndBufferSizeDetails (48000.0, 512);
            p.prepareToPlay (48000.0, 512);
            std::atomic<bool> stop { false };
            std::atomic<bool> finite { true };
            std::thread audio ([&]
            {
                while (! stop.load())
                {
                    const auto st = play (p, 48000.0, 512, 0.25, {}, false);
                    if (! st.finite) finite = false;
                }
            });
            pumpMessages (1500);
            stop = true;
            audio.join();
            p.releaseResources();
            r.check (finite.load(), "finite output while the editor is open");

            // Delete the editor while it is still on screen, as hosts do: the hosted view must be
            // detached before its parent window goes away.
            editor.reset();
            pumpMessages (100);
            r.check (true, "editor closed");

            // Reopen once: hosts create and destroy the editor many times per session.
            std::unique_ptr<juce::AudioProcessorEditor> again (p.createEditorAndMakeActive());
            r.check (again != nullptr, "editor can be reopened");
            if (again != nullptr)
            {
                again->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
                again->setVisible (true);
                pumpMessages (300);
                again.reset();
            }
        }
    }

    //------------------------------------------------------------------------------------------
    Report::section ("Shutdown");
    plugin.reset();
    pumpMessages (100);
    r.check (true, "instance deleted");

    const auto seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    std::cout << "\n==== Validation: " << r.passes << " passed, " << r.failures << " failed ("
              << juce::String (seconds, 1) << " s) ====" << std::endl;
    return r.failures == 0 ? 0 : 1;
}

} // namespace cureumtest
