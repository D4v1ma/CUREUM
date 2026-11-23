# Plugin hosting (VST3 / AU effects)

CUREUM can load up to four third-party effect plugins into its FX rack, so they become part of the preset.

1. Go to the **GLOBAL** page and click **Scan for plugins**. CUREUM scans the standard VST3 folders (and Audio Units on macOS). The result is remembered for all CUREUM instances. You can also add a single plugin with **Add plugin file...**.
2. On the **FX** page click **+ ADD EFFECT > VST3 / AU Plugin** and pick a plugin. It is inserted at the end of the rack (move it like any other effect).
3. Use **Edit** to open the plugin's own window, **Load plugin...** to swap it, and **Remove** to clear the slot. **MIX** blends the plugin's output with its input.

The hosted plugin's settings are saved inside CUREUM's preset and your DAW project. If a preset refers to a plugin that is not installed, the slot stays empty.

Notes:

- Only effect plugins are listed. VST2 plugins are not supported (the VST2 SDK is no longer available); use the VST3 version of your plugins.
- Plugins run inside CUREUM's process, so an unstable plugin can affect CUREUM and your DAW.
- The latency of hosted plugins is reported to your DAW for automatic compensation.
