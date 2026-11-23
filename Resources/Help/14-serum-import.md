# Opening Serum presets and banks

CUREUM reads presets from Xfer Records' Serum (version 1) and Serum 2 so you can use your existing sounds.

## What can be opened

- **Serum 2 presets** (`.SerumPreset`)
- **Serum 1 presets** (`.fxp`)
- **Serum 1 banks** (`.fxb`)
- **Preset packs**: `.zip` archives, `.SerumPack` packs and whole folders

Open a single preset with **Menu > Open preset file...**, by double-clicking it in the browser (Serum folders appear there automatically) or by dragging the file onto CUREUM. Use **Menu > Import bank / pack / folder...** to convert a complete library: every preset is converted to CUREUM format and saved under `User/Imported/<bank name>`.

## Serum folders and wavetables

Serum presets usually reference wavetables and noise samples by file name (for example `Analog/Basic Shapes.wav`). If you own Serum, add your Serum preset folders on the GLOBAL page (CUREUM tries to find them automatically). CUREUM then loads the original wavetables and noises. Otherwise it substitutes the closest CUREUM factory table (for example *Basic Shapes* for Serum's Basic Shapes, which uses the same frame order) and shows a note. Wavetables that are embedded in Serum 1 presets are always imported.

## What is converted

- Oscillators A and B (Serum 1) or A, B and C (Serum 2): level, pan, pitch, unison, detune, blend, width, wavetable position, warp modes and amounts, phase.
- Sub and noise oscillators, both filters with type, cutoff, resonance, drive, var, mix and stereo, and the routing.
- Envelopes (times and curves), LFO rates, modes and (Serum 2) drawn shapes, chaos LFOs, macros (values and names).
- The modulation matrix: sources, destinations, amounts, bipolar mode, output and aux sources.
- Effects: type, order and the main parameters of each unit, master volume, mono/legato, glide and pitch bend.

## Limitations

CUREUM is an independent synthesizer, so imported presets are a close starting point rather than an exact copy. The preset formats are not publicly documented; CUREUM's readers are based on community research and on comparing real preset files. In particular:

- Serum 2's sample, granular, spectral and multisample oscillator modes, arpeggiator and clip sequencer are not imported.
- Serum 1 LFO shapes are not imported (CUREUM uses its default triangle).
- Effects, filters and warp modes are CUREUM's own designs and differ in tone.
- Some rarely used modulation sources may map differently; the import notes in the status bar list anything that could not be converted.
