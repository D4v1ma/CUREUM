# Modulation

Almost every continuous control in CUREUM can be modulated.

## Drag and drop

1. Pick a source: an ENV/LFO tab, a macro handle (the small arrow chip next to each macro) or a chip such as VELOCITY, NOTE, MOD WHEEL, AFTERTOUCH, PITCH BEND, RANDOM or GATE.
2. Drag it onto a knob. The knob is highlighted when it can accept the source.
3. A coloured ring appears around the knob. It shows the modulation range: orange for envelopes, purple for LFOs, green for macros, pink for other sources.
4. To change the depth, drag the ring (the outer edge of the knob) or hold **Alt** and drag anywhere on the knob.

While notes play, a small dot on the knob shows the live modulated value.

Right-click a modulated knob to select which routing the ring edits, toggle **Bipolar**, invert the depth, remove it or jump to the matrix.

## Sources

- **Env 1-4**, **LFO 1-10**, **Macro 1-8**
- **Velocity**, **Release Vel**, **Note** (key position), **Gate** (1 while held)
- **Mod Wheel**, **Pitch Bend**, **Aftertouch**, **Poly AT**
- **Random 1** (0..1) and **Random 2** (-1..1): a new value for every note
- **MPE Slide** and **MPE Pressure** (with MPE enabled)
- **Constant**: always 1, useful to offset a control from the matrix

Effect parameters can be modulated too. Because effects process the mix of all voices, per-voice sources are taken from the most recently played note.
