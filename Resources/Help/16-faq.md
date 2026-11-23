# FAQ and troubleshooting

**I hear nothing.** Check that OSC A is switched on, that its LEVEL and the MASTER volume are up and that Env 1 has a non-zero sustain or a long enough decay. If a filter is enabled, open its cutoff.

**Imported Serum presets sound different.** Add your Serum folders on the GLOBAL page so the original wavetables and noise samples are found, then reload the preset. See *Opening Serum presets* for what is converted.

**My plugin doesn't appear in the list.** Run **Scan for plugins** on the GLOBAL page, or add the plugin file directly. Only effects (not instruments) are listed, and only VST3 and AU formats are supported.

**CPU usage is high.** Reduce unison voices, polyphony or the QUALITY setting, and disable unused oscillators and effects. Hosted plugins and the convolution reverb also add CPU load.

**The window is too small or large.** Change the zoom on the GLOBAL page or drag the bottom-right corner of the window.

**Where are my files?** Presets: `Documents/CUREUM/Presets`. Wavetables: `Documents/CUREUM/Wavetables`. Settings and the plugin list are stored in your user application-data folder under `CUREUM`.
