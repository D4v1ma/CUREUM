// CUREUM factory presets, built in code so they always match the current engine.
#include "PresetManager.h"
#include "../DSP/WavetableFactory.h"
#include "../Core/ValueMaps.h"

namespace cureum
{

using namespace pid;

namespace
{
    struct Ctx
    {
        PresetBuilder& b;

        void osc (int o, const char* table, float wtPos = 0.0f, float level = 0.75f)
        {
            b.setBool (pid::osc (o, Osc::enable), true);
            b.setWavetable (o, WavetableFactory::get (table));
            b.set (pid::osc (o, Osc::wtPos), wtPos);
            b.set (pid::osc (o, Osc::level), level);
        }
        void unison (int o, int voices, float detune, float blend = 0.75f, float width = 1.0f)
        {
            b.set (pid::osc (o, Osc::unison), (float) voices);
            b.set (pid::osc (o, Osc::detune), detune);
            b.set (pid::osc (o, Osc::blend), blend);
            b.set (pid::osc (o, Osc::width), width);
        }
        void pitch (int o, int octave, int semi = 0, float fine = 0.0f)
        {
            b.set (pid::osc (o, Osc::octave), (float) octave);
            b.set (pid::osc (o, Osc::semi), (float) semi);
            b.set (pid::osc (o, Osc::fine), fine);
        }
        void warp (int o, WarpMode m, float amount, int slot = 1)
        {
            b.setChoice (pid::osc (o, slot == 1 ? Osc::warp1Mode : Osc::warp2Mode), (int) m);
            b.set (pid::osc (o, slot == 1 ? Osc::warp1Amt : Osc::warp2Amt), amount);
        }
        void filter (int f, const char* type, float hz, float reso = 0.1f, float drive = 0.0f, float var = 0.0f)
        {
            b.setBool (pid::filter (f, Filter::enable), true);
            b.setChoice (pid::filter (f, Filter::type), juce::jmax (0, filterTypeNames().indexOf (type)));
            b.set (pid::filter (f, Filter::cutoff), hz);
            b.set (pid::filter (f, Filter::reso), reso);
            b.set (pid::filter (f, Filter::drive), drive);
            b.set (pid::filter (f, Filter::var), var);
        }
        void env (int e, float a, float d, float s, float r, float hold = 0.0f)
        {
            b.set (pid::env (e, Env::attack), a);
            b.set (pid::env (e, Env::decay), d);
            b.set (pid::env (e, Env::sustain), s);
            b.set (pid::env (e, Env::release), r);
            b.set (pid::env (e, Env::hold), hold);
        }
        void lfoSync (int l, int syncIndex, LfoMode mode = LfoMode::trig, const char* shape = nullptr)
        {
            b.setBool (pid::lfo (l, Lfo::sync), true);
            b.setChoice (pid::lfo (l, Lfo::syncRate), syncIndex);
            b.setChoice (pid::lfo (l, Lfo::mode), (int) mode);
            if (shape != nullptr) b.setLfoShape (l, LfoShape::preset (shape));
        }
        void lfoHz (int l, float hz, LfoMode mode = LfoMode::free, const char* shape = nullptr)
        {
            b.setBool (pid::lfo (l, Lfo::sync), false);
            b.set (pid::lfo (l, Lfo::rate), hz);
            b.setChoice (pid::lfo (l, Lfo::mode), (int) mode);
            if (shape != nullptr) b.setLfoShape (l, LfoShape::preset (shape));
        }
        void mod (ModSource s, int dest, float amount, bool bipolar = false, ModSource aux = ModSource::none)
        {
            b.addModulation (s, dest, amount, bipolar, aux);
        }
        void fx (FxType t) { b.setBool (pid::fxEnable (t), true); }
        void reverb (float mix, float size, bool plate = false)
        {
            fx (FxType::reverb);
            b.set (pid::reverb (Reverb::mix), mix);
            b.set (pid::reverb (Reverb::size), size);
            b.setChoice (pid::reverb (Reverb::mode), plate ? 0 : 1);
        }
        void delay (float mix, int syncIndex, float feedback, bool pingPong = true)
        {
            fx (FxType::delay);
            b.set (pid::delay (Delay::mix), mix);
            b.setChoice (pid::delay (Delay::syncL), syncIndex);
            b.setChoice (pid::delay (Delay::syncR), syncIndex);
            b.set (pid::delay (Delay::feedback), feedback);
            b.setChoice (pid::delay (Delay::mode), pingPong ? 1 : 0);
        }
        void macroName (int m, const char* n) { b.setMacroName (m, n); }
    };

    struct Def { const char* name; const char* category; std::function<void (Ctx&)> build; };

    const std::vector<Def>& defs()
    {
        static const std::vector<Def> d {
            //------------------------------------------------------------------ Bass
            { "Reese Machine", "Bass", [] (Ctx& c)
              {
                  c.osc (0, "Reese", 0.3f); c.unison (0, 4, 0.32f);
                  c.osc (1, "Saw Bright", 0.6f, 0.6f); c.pitch (1, -1);
                  c.filter (0, "MG Low 24", 900.0f, 0.25f, 0.2f);
                  c.b.setBool (global (Global::mono), true);
                  c.lfoHz (0, 0.18f, LfoMode::free, "Sine");
                  c.mod (ModSource::lfo1, osc (0, Osc::wtPos), 0.35f);
                  c.mod (ModSource::macro1, filter (0, Filter::cutoff), 0.45f); c.macroName (0, "Brightness");
                  c.fx (FxType::distortion); c.b.set (dist (Dist::drive), 0.3f); c.b.set (dist (Dist::mix), 0.6f);
                  c.fx (FxType::compressor);
              } },
            { "Wobble Monster", "Bass", [] (Ctx& c)
              {
                  c.osc (0, "Growl", 0.2f); c.unison (0, 2, 0.2f);
                  c.osc (1, "Square Fold", 0.4f, 0.55f); c.pitch (1, -1);
                  c.b.setBool (sub (Sub::enable), true); c.b.set (sub (Sub::octave), -1.0f); c.b.set (sub (Sub::level), 0.6f);
                  c.filter (0, "MG Low 24", 300.0f, 0.45f, 0.3f);
                  c.b.setBool (global (Global::mono), true); c.b.setBool (global (Global::legato), true); c.b.set (global (Global::portaTime), 0.04f);
                  c.lfoSync (0, 7, LfoMode::trig, "Triangle");
                  c.mod (ModSource::lfo1, filter (0, Filter::cutoff), 0.55f);
                  c.mod (ModSource::lfo1, osc (0, Osc::wtPos), 0.5f);
                  c.mod (ModSource::macro1, lfo (0, Lfo::syncRate), 0.3f); c.macroName (0, "Wobble Rate");
                  c.fx (FxType::distortion); c.b.setChoice (dist (Dist::mode), (int) DistMode::softClip); c.b.set (dist (Dist::drive), 0.3f);
                  c.fx (FxType::compressor); c.b.setBool (comp (Comp::multiband), true); c.b.set (comp (Comp::mix), 0.4f);
                  c.b.set (global (Global::master), 0.6f);
              } },
            { "Sub Punch", "Bass", [] (Ctx& c)
              {
                  c.osc (0, "Sine", 0.0f, 0.55f);
                  c.b.setBool (sub (Sub::enable), true); c.b.set (sub (Sub::level), 0.7f);
                  c.env (0, 0.001f, 0.35f, 0.65f, 0.08f);
                  c.env (1, 0.0f, 0.06f, 0.0f, 0.05f);
                  c.mod (ModSource::env2, osc (0, Osc::coarse), 0.18f);
                  c.b.setBool (noise (Noise::enable), true); c.b.setChoice (noise (Noise::type), (int) NoiseType::click); c.b.set (noise (Noise::level), 0.35f);
                  c.fx (FxType::compressor);
              } },
            { "Acid Squelch", "Bass", [] (Ctx& c)
              {
                  c.osc (0, "Saw", 0.0f, 0.7f);
                  c.filter (0, "Diode LP", 280.0f, 0.82f, 0.25f, 0.4f);
                  c.env (1, 0.0f, 0.22f, 0.0f, 0.1f);
                  c.mod (ModSource::env2, filter (0, Filter::cutoff), 0.45f);
                  c.mod (ModSource::velocity, filter (0, Filter::cutoff), 0.15f);
                  c.b.setBool (global (Global::mono), true); c.b.setBool (global (Global::legato), true); c.b.set (global (Global::portaTime), 0.07f);
                  c.fx (FxType::distortion); c.b.setChoice (dist (Dist::mode), (int) DistMode::overdrive); c.b.set (dist (Dist::drive), 0.35f);
                  c.delay (0.18f, 6, 0.35f);
              } },
            { "FM Growler", "Bass", [] (Ctx& c)
              {
                  c.osc (0, "Sine", 0.0f, 0.75f); c.warp (0, WarpMode::fmOsc, 0.45f);
                  c.b.setWavetable (1, WavetableFactory::get ("Sine")); c.pitch (1, 1);
                  c.filter (0, "MG Low 12", 2500.0f, 0.2f);
                  c.lfoSync (0, 6, LfoMode::trig, "Saw Down");
                  c.mod (ModSource::lfo1, osc (0, Osc::warp1Amt), 0.4f);
                  c.mod (ModSource::macro1, osc (0, Osc::warp1Amt), 0.5f); c.macroName (0, "FM Amount");
                  c.b.setBool (global (Global::mono), true);
                  c.fx (FxType::distortion); c.b.set (dist (Dist::drive), 0.25f);
              } },
            //------------------------------------------------------------------ Lead
            { "Supersaw Anthem", "Lead", [] (Ctx& c)
              {
                  c.osc (0, "Saw", 0.0f, 0.7f); c.unison (0, 9, 0.55f); c.b.setChoice (osc (0, Osc::uniMode), (int) UnisonMode::super);
                  c.osc (1, "Saw Bright", 1.0f, 0.5f); c.unison (1, 7, 0.45f); c.pitch (1, 1);
                  c.filter (0, "Low 12", 9000.0f, 0.15f);
                  c.env (0, 0.005f, 1.0f, 0.85f, 0.35f);
                  c.fx (FxType::hyper); c.b.set (hyper (Hyper::mix), 0.35f);
                  c.reverb (0.25f, 0.55f); c.delay (0.18f, 6, 0.35f);
                  c.mod (ModSource::macro1, filter (0, Filter::cutoff), -0.45f); c.macroName (0, "Darken");
              } },
            { "Sync Screamer", "Lead", [] (Ctx& c)
              {
                  c.osc (0, "Saw Bright", 0.8f, 0.7f); c.warp (0, WarpMode::syncWindow, 0.35f); c.unison (0, 3, 0.25f);
                  c.env (1, 0.0f, 0.6f, 0.2f, 0.3f);
                  c.mod (ModSource::env2, osc (0, Osc::warp1Amt), 0.5f);
                  c.mod (ModSource::modWheel, osc (0, Osc::warp1Amt), 0.4f);
                  c.b.setBool (global (Global::mono), true); c.b.set (global (Global::portaTime), 0.05f);
                  c.fx (FxType::distortion); c.b.set (dist (Dist::drive), 0.3f);
                  c.delay (0.2f, 7, 0.4f);
              } },
            { "Glide Lead", "Lead", [] (Ctx& c)
              {
                  c.osc (0, "Basic Shapes", 0.5f, 0.65f);
                  c.osc (1, "Saw", 0.0f, 0.55f); c.pitch (1, 0, 0, 7.0f);
                  c.filter (0, "MG Low 24", 3500.0f, 0.3f);
                  c.b.setBool (global (Global::mono), true); c.b.setBool (global (Global::legato), true); c.b.set (global (Global::portaTime), 0.12f);
                  c.lfoHz (1, 5.5f, LfoMode::trig, "Sine"); c.b.set (lfo (1, Lfo::delay), 0.35f); c.b.set (lfo (1, Lfo::rise), 0.5f);
                  c.mod (ModSource::lfo2, osc (0, Osc::coarse), 0.006f, true);
                  c.mod (ModSource::lfo2, osc (1, Osc::coarse), 0.006f, true);
                  c.reverb (0.2f, 0.4f, true);
              } },
            { "FM Bell Lead", "Lead", [] (Ctx& c)
              {
                  c.osc (0, "FM Bells", 0.3f, 0.7f); c.unison (0, 2, 0.15f);
                  c.env (1, 0.0f, 0.9f, 0.1f, 0.4f);
                  c.mod (ModSource::env2, osc (0, Osc::wtPos), 0.5f);
                  c.env (0, 0.003f, 1.5f, 0.5f, 0.6f);
                  c.reverb (0.3f, 0.6f, true); c.delay (0.15f, 6, 0.3f);
              } },
            //------------------------------------------------------------------ Pad
            { "Warm Analog Pad", "Pad", [] (Ctx& c)
              {
                  c.osc (0, "Saw Bright", 0.4f, 0.6f); c.unison (0, 5, 0.4f);
                  c.osc (1, "Saw Bright", 0.6f, 0.5f); c.unison (1, 5, 0.45f); c.pitch (1, 0, 0, -9.0f);
                  c.filter (0, "MG Low 12", 1800.0f, 0.2f);
                  c.env (0, 0.8f, 2.0f, 0.8f, 2.2f);
                  c.lfoHz (0, 0.12f, LfoMode::free, "Sine");
                  c.mod (ModSource::lfo1, filter (0, Filter::cutoff), 0.12f, true);
                  c.fx (FxType::chorus); c.reverb (0.45f, 0.75f);
              } },
            { "Choir of Glass", "Pad", [] (Ctx& c)
              {
                  c.osc (0, "Vocal Choir", 0.2f, 0.65f); c.unison (0, 4, 0.25f);
                  c.osc (1, "Formant Vowels", 0.1f, 0.45f); c.pitch (1, 1);
                  c.env (0, 1.2f, 2.0f, 0.9f, 3.0f);
                  c.lfoHz (0, 0.07f, LfoMode::free, "Triangle");
                  c.mod (ModSource::lfo1, osc (0, Osc::wtPos), 0.6f);
                  c.mod (ModSource::lfo1, osc (1, Osc::wtPos), 0.8f);
                  c.fx (FxType::hyper); c.b.set (hyper (Hyper::dimMix), 0.6f);
                  c.reverb (0.5f, 0.85f, true);
              } },
            { "Evolving Spectra", "Pad", [] (Ctx& c)
              {
                  c.osc (0, "Spectral Comb", 0.0f, 0.6f); c.unison (0, 3, 0.3f);
                  c.osc (1, "Harmonic Sweep", 0.3f, 0.45f); c.pitch (1, -1);
                  c.env (0, 1.5f, 3.0f, 0.9f, 3.5f);
                  c.lfoHz (0, 0.05f, LfoMode::free, "Sine");
                  c.lfoHz (1, 0.09f, LfoMode::free, "Triangle");
                  c.mod (ModSource::lfo1, osc (0, Osc::wtPos), 0.9f);
                  c.mod (ModSource::lfo2, osc (1, Osc::wtPos), 0.5f);
                  c.reverb (0.5f, 0.9f); c.delay (0.2f, 5, 0.45f);
              } },
            { "Dark Drone", "Pad", [] (Ctx& c)
              {
                  c.osc (0, "Hollow", 0.4f, 0.6f); c.unison (0, 6, 0.3f);
                  c.b.setBool (noise (Noise::enable), true); c.b.setChoice (noise (Noise::type), (int) NoiseType::wind); c.b.set (noise (Noise::level), 0.4f);
                  c.filter (0, "Formant-II", 425.0f, 0.4f, 0.0f, 0.3f);
                  c.env (0, 2.0f, 3.0f, 1.0f, 4.0f);
                  c.lfoHz (0, 0.03f, LfoMode::free, "Sine");
                  c.mod (ModSource::lfo1, filter (0, Filter::var), 0.8f);
                  c.fx (FxType::convolve); c.b.setChoice (conv (Conv::ir), (int) ConvIr::cathedral); c.b.set (conv (Conv::mix), 0.45f);
              } },
            //------------------------------------------------------------------ Pluck / Keys
            { "Crystal Pluck", "Pluck", [] (Ctx& c)
              {
                  c.osc (0, "Saw Bright", 0.9f, 0.7f); c.unison (0, 3, 0.2f);
                  c.filter (0, "Low 24", 400.0f, 0.3f);
                  c.env (0, 0.001f, 0.45f, 0.0f, 0.3f);
                  c.env (1, 0.0f, 0.25f, 0.0f, 0.2f);
                  c.mod (ModSource::env2, filter (0, Filter::cutoff), 0.55f);
                  c.mod (ModSource::velocity, filter (0, Filter::cutoff), 0.2f);
                  c.delay (0.25f, 6, 0.4f); c.reverb (0.25f, 0.5f, true);
              } },
            { "Digital Keys", "Keys", [] (Ctx& c)
              {
                  c.osc (0, "FM Sweep", 0.2f, 0.65f);
                  c.osc (1, "Sine", 0.0f, 0.4f); c.pitch (1, 1);
                  c.env (0, 0.002f, 1.2f, 0.3f, 0.5f);
                  c.env (1, 0.0f, 0.6f, 0.1f, 0.4f);
                  c.mod (ModSource::env2, osc (0, Osc::wtPos), 0.4f);
                  c.mod (ModSource::velocity, osc (0, Osc::wtPos), 0.2f);
                  c.fx (FxType::chorus); c.reverb (0.2f, 0.45f, true);
              } },
            { "Marimba Hit", "Keys", [] (Ctx& c)
              {
                  c.osc (0, "Sine", 0.0f, 0.7f);
                  c.osc (1, "Triangle", 0.0f, 0.35f); c.pitch (1, 2);
                  c.env (0, 0.001f, 0.35f, 0.0f, 0.25f);
                  c.b.setBool (noise (Noise::enable), true); c.b.setChoice (noise (Noise::type), (int) NoiseType::click); c.b.set (noise (Noise::level), 0.3f);
                  c.reverb (0.2f, 0.35f);
              } },
            { "Bell Tower", "Keys", [] (Ctx& c)
              {
                  c.osc (0, "FM Bells", 0.6f, 0.65f); c.unison (0, 2, 0.1f);
                  c.osc (1, "Sine Stack", 0.5f, 0.4f); c.pitch (1, 1);
                  c.env (0, 0.002f, 3.5f, 0.0f, 3.0f);
                  c.reverb (0.45f, 0.8f, true);
              } },
            { "House Organ", "Keys", [] (Ctx& c)
              {
                  c.osc (0, "Organ", 0.4f, 0.65f);
                  c.env (0, 0.002f, 0.2f, 1.0f, 0.08f);
                  c.fx (FxType::phaser); c.b.set (phaser (Phaser::mix), 0.4f); c.b.set (phaser (Phaser::rate), 5.5f);
                  c.fx (FxType::chorus); c.reverb (0.18f, 0.35f);
              } },
            //------------------------------------------------------------------ FX
            { "Noise Riser", "FX", [] (Ctx& c)
              {
                  c.b.setBool (osc (0, Osc::enable), false);
                  c.b.setBool (noise (Noise::enable), true); c.b.setChoice (noise (Noise::type), (int) NoiseType::white); c.b.set (noise (Noise::level), 0.6f);
                  c.filter (0, "Band 12", 300.0f, 0.5f);
                  c.env (0, 0.01f, 1.0f, 1.0f, 1.5f);
                  c.lfoSync (0, 1, LfoMode::env, "Ramp Exp");
                  c.mod (ModSource::lfo1, filter (0, Filter::cutoff), 0.6f);
                  c.mod (ModSource::lfo1, noise (Noise::pitch), 0.3f);
                  c.reverb (0.4f, 0.8f);
              } },
            { "Laser Zap", "FX", [] (Ctx& c)
              {
                  c.osc (0, "Saw", 0.0f, 0.7f); c.pitch (0, 1);
                  c.env (0, 0.0f, 0.3f, 0.0f, 0.1f);
                  c.env (1, 0.0f, 0.25f, 0.0f, 0.2f);
                  c.mod (ModSource::env2, osc (0, Osc::coarse), 0.4f);
                  c.fx (FxType::distortion); c.b.set (dist (Dist::drive), 0.2f);
                  c.delay (0.3f, 7, 0.5f);
              } },
            { "Sidechain Pump Pad", "Pad", [] (Ctx& c)
              {
                  c.osc (0, "Saw Bright", 0.5f, 0.6f); c.unison (0, 7, 0.45f);
                  c.filter (0, "MG Low 12", 2500.0f, 0.15f);
                  c.env (0, 0.3f, 1.0f, 1.0f, 1.0f);
                  c.lfoSync (0, 5, LfoMode::free, "Sidechain");
                  c.mod (ModSource::lfo1, global (Global::master), 0.6f, false);
                  c.reverb (0.3f, 0.6f);
              } },
            { "Arp Pluck Saw", "Pluck", [] (Ctx& c)
              {
                  c.osc (0, "Saw", 0.0f, 0.65f); c.unison (0, 2, 0.15f);
                  c.filter (0, "MG Low 24", 700.0f, 0.35f);
                  c.env (0, 0.001f, 0.25f, 0.0f, 0.15f);
                  c.env (1, 0.0f, 0.15f, 0.0f, 0.1f);
                  c.mod (ModSource::env2, filter (0, Filter::cutoff), 0.45f);
                  c.delay (0.3f, 7, 0.45f);
              } },
            { "Vowel Bass", "Bass", [] (Ctx& c)
              {
                  c.osc (0, "Formant Vowels", 0.0f, 0.75f); c.unison (0, 2, 0.12f);
                  c.b.setBool (sub (Sub::enable), true); c.b.set (sub (Sub::octave), -1.0f);
                  c.filter (0, "Formant-II", 425.0f, 0.3f);
                  c.lfoSync (0, 5, LfoMode::trig, "Triangle");
                  c.mod (ModSource::lfo1, osc (0, Osc::wtPos), 1.0f);
                  c.mod (ModSource::lfo1, filter (0, Filter::var), 0.6f);
                  c.b.setBool (global (Global::mono), true);
                  c.fx (FxType::distortion); c.b.set (dist (Dist::drive), 0.2f);
              } },
            { "Chaos Texture", "FX", [] (Ctx& c)
              {
                  c.osc (0, "Noise Cycles", 0.0f, 0.8f); c.unison (0, 4, 0.3f);
                  c.filter (0, "MG Low 12", 3000.0f, 0.3f);
                  c.env (0, 0.5f, 2.0f, 0.8f, 2.0f);
                  c.b.setChoice (lfo (8, Lfo::type), (int) LfoType::lorenz); c.b.setBool (lfo (8, Lfo::sync), false); c.b.set (lfo (8, Lfo::rate), 0.6f);
                  c.b.setChoice (lfo (9, Lfo::type), (int) LfoType::rossler); c.b.setBool (lfo (9, Lfo::sync), false); c.b.set (lfo (9, Lfo::rate), 0.4f);
                  c.mod (ModSource::lfo9, osc (0, Osc::wtPos), 1.0f);
                  c.mod (ModSource::lfo10, filter (0, Filter::cutoff), 0.2f, true);
                  c.reverb (0.45f, 0.75f);
              } },
        };
        return d;
    }
}

int PresetManager::getNumFactoryPresets()              { return (int) defs().size(); }
juce::String PresetManager::getFactoryName (int i)     { return defs()[(size_t) juce::jlimit (0, (int) defs().size() - 1, i)].name; }
juce::String PresetManager::getFactoryCategory (int i) { return defs()[(size_t) juce::jlimit (0, (int) defs().size() - 1, i)].category; }

juce::ValueTree PresetManager::createFactoryPreset (int index)
{
    const auto& d = defs()[(size_t) juce::jlimit (0, (int) defs().size() - 1, index)];
    PresetBuilder b;
    b.name = d.name;
    b.author = "CUREUM";
    b.category = d.category;
    b.tags.add (d.category);
    b.setFxOrder (FxOrder::defaultOrder());
    b.setWavetable (0, WavetableFactory::getDefault());
    b.set (osc (0, Osc::wtPos), 1.0f / 6.0f);
    Ctx c { b };
    d.build (c);
    return b.build();
}

} // namespace cureum
