# Welcome to CUREUM

CUREUM is a wavetable synthesizer with a Serum-style workflow: three wavetable oscillators, a sub and a noise oscillator, two multimode filters, four envelopes, ten LFOs, eight macros, a 64-slot modulation matrix and a reorderable effects rack that can also host your own VST3/AU effect plugins. It opens Serum 1 and Serum 2 presets and banks.

## Quick start

1. Play a note: use your MIDI keyboard or the on-screen keyboard at the bottom of the OSC page.
2. Pick a preset: click the preset name in the header to open the browser, or use the arrows next to it.
3. Shape the sound: change the wavetable of OSC A (click its name), scan through it with **WT POS**, add **UNISON** voices and **DETUNE**.
4. Turn on a filter (power button on the FILTER panel) and sweep **CUTOFF**.
5. Modulate: drag the **LFO 1** tab (or any envelope, macro or source chip) onto a knob. A coloured ring appears around the knob; drag that ring to set the depth.
6. Add effects on the **FX** page and save your sound with the disk icon.

## Interface tour

- **Header**: preset browser and navigation, save, the main menu, page tabs (OSC, FX, MATRIX, GLOBAL), help buttons, master volume, output meter and voice/CPU display.
- **OSC page**: the three wavetable oscillators, the filter section, the sub and noise oscillators, the modulation section (envelopes and LFOs), macros, voicing and the keyboard.
- **FX page**: the effects rack. Effects run from top to bottom; drag rows to reorder them.
- **MATRIX page**: every modulation routing in a table, with amount, curve, output scaling and an auxiliary (multiplier) source.
- **GLOBAL page**: tuning, pitch bend, velocity, MPE, oversampling quality, library folders (including your Serum folders), plugin scanning and interface zoom.
- **Status bar** (bottom): describes whatever is under the mouse. This is the fastest way to learn what a control does.

## Getting help

- Hover over anything and read the status bar.
- Right-click a knob and choose **Help** to open the matching help page.
- Click the **?** button in the header (or press F1) for this help browser. It has a search box.
- Click the **arrow** (What's this?) button, then click any control to see a short explanation in a bubble. Click the button again to leave help mode.

## Signal flow

Oscillators A, B, C, the noise and the sub oscillator are mixed into the voice filters according to the routing row of the FILTER panel. Envelope 1 always controls the amplitude of each voice. The voices are summed, then pass through the effects rack and the master volume.
