# LFOs

Ten LFOs can modulate anything. Select one with its tab; drag the tab onto a knob to use it.

## Shape editor

- Drag points to move them; they snap to the grid (hold **Shift** for free movement).
- **Double-click** empty space to add a point, double-click a point to delete it.
- Drag the small open handle in the middle of a segment to curve it.
- Right-click for preset shapes (sine, saw, square, steps, trance gate, sidechain...), flipping and grid size.

## Controls

- **BPM**: tempo sync on/off. With sync on, **RATE** chooses a musical length (8 bars to 1/128) and **FEEL** makes it dotted or triplet. With sync off, RATE is in Hz (0.01-100 Hz).
- **TYPE**: *Shape* uses the drawn curve; *S&H* steps between random values; *Smooth Rnd* glides between random values; *Lorenz* and *Rossler* are chaotic attractors that never repeat.
- **MODE**: *Trig* restarts the LFO on each note; *Env* plays the shape once (use it as a drawable envelope); *Free* runs continuously and all voices share the same phase. Synced free LFOs follow the song position of your DAW.
- **RISE**: fades the LFO in after the note starts. **DELAY**: waits before starting. **SMOOTH**: rounds off sharp edges. **PHASE**: start position in the cycle.

LFO outputs range from 0 to 1. Enable **Bipolar** on a routing (right-click the knob or use the matrix) to make it swing around the knob's current value.
