# Effects

The FX page holds the effects rack. Effects process the sum of all voices from top to bottom. Drag a row in the rack list to move an effect, click the power button to bypass it, click **x** to remove it and **+ ADD EFFECT** to insert one. Click a row to jump to its controls.

- **Hyper/Dimension**: Hyper stacks detuned, modulated copies of the sound (RATE, DETUNE, UNISON, RETRIG). Dimension widens the stereo image (SIZE, DIM).
- **Distortion**: 18 modes (Tube, Soft/Hard Clip, Diode 1/2, Linear/Sine Fold, Zero-Square, Downsample, Asymmetric, Rectify, X-Shaper, Sine Shaper, Stomp Box, Bit Crush, Fuzz, Overdrive) with an optional pre or post filter (FREQ, Q).
- **Flanger / Phaser / Chorus**: classic modulation effects. RATE can be tempo-synced with BPM; STEREO offsets the left and right modulation.
- **Delay**: Normal, Ping-Pong or Tap -> Delay. Times can be free (ms) or synced; LINK uses the left time for both sides. FREQ and WIDTH set a band-pass filter in the feedback path.
- **Compressor**: threshold, ratio, attack, release and make-up gain. **MULTI** switches to a three-band upward/downward compressor (OTT-style) with per-band output (LOW, MID, HIGH). The header shows the gain reduction.
- **Reverb**: Plate or Hall with size, pre-delay, low cut, damping, high cut and width.
- **EQ**: two bands, each a shelf, a bell or a pass filter.
- **Filter**: one more instance of the voice filter, applied to the whole mix.
- **Bode Shifter**: frequency shifting by a fixed number of Hz (inharmonic, metallic or barber-pole sounds with FEEDBACK).
- **Utility**: gain, balance, stereo width, mono bass below a crossover, polarity inversion.
- **Convolve**: convolution with a built-in impulse response (rooms, hall, plate, spring, cathedral, metallic, gated) or any WAV you load with **Load IR file...**.
- **Plugin Slots 1-4**: host your own VST3 (or AU on macOS) effects. See *Plugin hosting*.

Most effect parameters are modulation destinations: drag an LFO onto the reverb mix or a macro onto the delay feedback.
