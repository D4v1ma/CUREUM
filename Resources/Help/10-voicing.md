# Voicing, tuning and quality

## Voicing panel (OSC page)

- **POLY**: maximum number of simultaneous notes (1-32). When the limit is reached the oldest released note is stolen first.
- **MONO**: one note at a time with last-note priority.
- **LEGATO**: in mono mode, overlapping notes glide without restarting the envelopes.
- **PORTA**: glide time. **CURVE** shapes the glide. **ALWAYS** glides even between separate notes; **SCALED** makes the glide time proportional to the interval.

## GLOBAL page

- **TUNE / TRANS**: master tuning in cents and transposition in semitones.
- **BEND + / BEND -**: pitch-bend range up and down.
- **VEL>AMP**: how much velocity controls loudness. Like Serum the default is 0%; route Velocity in the matrix for more control.
- **MPE**: MIDI Polyphonic Expression. Each note gets its own pitch bend (range **MPE BEND**), pressure (MPE Pressure source) and slide (CC 74, MPE Slide source).
- **QUALITY**: internal oversampling of the voice engine. *High (2x)* and *Ultra (4x)* reduce aliasing from hard sync, FM, warps and resonant filters at the cost of CPU.
