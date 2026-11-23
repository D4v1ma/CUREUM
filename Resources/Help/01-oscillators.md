# Wavetable oscillators

OSC A, OSC B and OSC C are identical wavetable oscillators. A wavetable is a series of single-cycle waveforms (frames). The oscillator plays one frame at a time; **WT POS** chooses the frame and smoothly morphs between neighbours. Every frame is stored as a band-limited mip-map, so high notes stay clean.

## Header

- **Power**: enables the oscillator. A disabled oscillator still runs silently if another oscillator uses it as an FM/PM/AM/RM source.
- **Wavetable name**: opens the wavetable menu: Factory tables, your User Wavetables folder, your Serum tables (if you added a Serum folder on the GLOBAL page), import, export and the editor.
- **Arrows**: previous / next factory table.
- **Pencil**: opens the wavetable editor. Double-clicking the 3D display does the same.
- **Gear**: unison mode, unison stack, warp and WT-position spread, and key tracking.

## 3D display

Shows all frames; the bright line is the current frame and the purple line is the modulated frame of the newest voice. Drag up and down on the display to scan the wavetable.

## Controls

- **WT POS**: wavetable position (frame). The most important modulation target for movement.
- **WARP 1 / WARP 2**: amount of the two warp modes selected in the menus above the knobs.
- **LEVEL / PAN**: output level and stereo position.
- **UNISON**: 1-16 stacked copies of the oscillator. **DETUNE** spreads their pitch, **BLEND** balances the centre voice against the outer voices (75% = all equal), **WIDTH** spreads them in stereo.
- **PHASE / RAND**: start phase of the waveform and how much it is randomised per note.
- **OCT / SEM / FIN / CRS**: octave, semitone, fine (cents) and continuous coarse pitch (great for pitch envelopes).

## Warp modes

Two warp stages run one after another. Choose a mode in the WARP menus, then set the amount with the WARP knobs.

- **Sync / Sync (Window)**: hard sync to a faster virtual oscillator. The window version fades each cycle for a smoother, vocal tone.
- **Bend +, Bend -, Bend +/-**: bend the phase toward the edges or the centre of the cycle.
- **Asym +, Asym -, Asym +/-**: asymmetric phase bend; moves the waveform's energy to the start or end of the cycle.
- **PWM**: squeezes the waveform into the first part of the cycle and leaves silence after it (pulse-width style).
- **Flip**: inverts the part of the cycle after the warp point.
- **Mirror**: plays the cycle forward then backward.
- **Remap 1-4**: phase remapping curves (S-curve, sine, stairs, wobble).
- **Quantize**: steps the phase for lo-fi, stepped tones.
- **Squash, Fold, Bit Crush**: soft-clip, wavefold or bit-reduce the waveform.
- **Phase Distort**: Casio CZ-style phase distortion.
- **FM / PM / AM / RM (Osc)**: frequency, phase, amplitude or ring modulation by the partner oscillator (A is modulated by B; B and C by A). **FM (A/B/C)** and **PM (A/B/C)** choose the modulator explicitly, **PM (Self)** is feedback.
- **FM / AM / RM (Noise)** and **(Sub)**: modulation by the noise or sub oscillator.

## Unison options (gear button)

- **Mode**: Linear (even spread), Super (clustered like classic super-saws), Exp, Inverse or Random detune distribution.
- **Stack**: adds octaves or fifths to the unison voices for chords from a single note.
- **Uni Warp / Uni WT**: gives each unison voice a different warp amount or wavetable position, for very wide sounds.
- **Key Track**: when off the oscillator always plays C3, which is useful for drones and effects.

## Importing your own wavetables

Choose **Import wavetable / audio...** in the wavetable menu. WAV files with Serum's `clm` marker are read with their frame size. Other files are imported as a single cycle (short files) or sliced into 2048-sample frames (long files). Use **Export wavetable as WAV** to save a table that Serum and other synths can read.
