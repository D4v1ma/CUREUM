# Wavetable editor

Open it with the pencil button of an oscillator or by double-clicking its 3D display. Edits are made on a copy; press **Apply** to send the result to the oscillator, or **Cancel** to discard it.

## Frames

The strip at the top shows every frame. Click a frame to edit it. **Add** duplicates the selected frame, **Delete** removes it. A table can hold 1 to 256 frames of 2048 samples.

## Drawing

Draw directly on the large waveform with the mouse. Hold Shift to draw straight lines. **Smooth** softens the selected frame, **Normalise** scales the table to full level, **Invert** flips its polarity and **Remove DC** centres it.

## Harmonics

The bar view under the waveform shows the first 64 harmonics of the selected frame. Drag bars up or down to rebuild the frame additively (phases are kept at zero).

## Formulas

Type an expression and press **Apply formula** to generate every frame. Variables:

- `x` - position in the cycle from -1 to 1
- `w` - position in the cycle from 0 to 1
- `y` - frame position from 0 (first) to 1 (last)
- `n`, `N` - frame index and number of frames
- `pi`, `e`

Functions: `sin cos tan asin acos atan sinh cosh tanh abs sqrt exp log log10 pow min max floor ceil round fract sign clamp mix lerp step saw square tri rand noise`. Operators: `+ - * / ^ %` and comparisons `< > <= >= == !=` (they return 1 or 0).

Examples:

- `sin(pi*x)` - a sine
- `x` - a saw
- `sin(pi*x*(1+y*7))` - a sweep of increasing harmonics over the table
- `tanh(sin(pi*x)*(1+y*10))` - a sine that becomes a square
- `square(w + y*0.5) * sin(pi*x)` - a moving gate

Set the number of frames in the **Frames** box before generating.

## Process menu

- **Crossfade first to last**: fills the table by morphing from the first to the last frame.
- **Spectral morph**: morphs harmonics instead of samples, which avoids phase cancellation.
- **Reverse frame order**, **Mirror table**, **Normalise each frame**.

## Import / export

**Import** loads any WAV/AIFF/FLAC (Serum tables are detected automatically). **Export** writes a 32-bit float WAV with a Serum-compatible `clm` marker.
