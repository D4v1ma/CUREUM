#pragma once

#include "StateFormat.h"

namespace cureum
{

// Result of converting a foreign preset into CUREUM's format.
struct ImportedPreset
{
    juce::ValueTree state;          // CUREUM state tree (see PresetBuilder::build)
    juce::String name;
    juce::String sourceFormat;      // "Serum 2", "Serum 1 (FXP)", ...
    juce::StringArray warnings;     // things that could not be converted exactly
    juce::String error;
    bool ok() const { return state.isValid() && error.isEmpty(); }
};

// Readers for Xfer Serum files, written from publicly documented reverse-engineering work and
// verified against real preset files:
//  * Serum 2 ".SerumPreset": "XferJson\0" + uint32 json length + 4 reserved bytes + JSON metadata,
//    then uint32 CBOR length + uint32 (2) + a zstd frame containing CBOR.
//  * Serum 1 ".fxp": VST2 'CcnK'/'FPCh' program for plug-in id 'XfsX'. The opaque chunk is a series
//    of zlib streams (parameter blob, embedded wavetables) followed by a uint32 trailer.
//  * Serum 1 ".fxb" banks (FxBk / FBCh), ".zip" archives, ".SerumPack" packs and folders.
namespace SerumImport
{
    enum class Kind { unknown, serum2, serum1Fxp, serum1Fxb, cureum, archive };

    Kind detect (const void* data, size_t size);
    bool isImportableExtension (const juce::String& extensionLowercase);

    ImportedPreset importSerum2 (const void* data, size_t size, const juce::String& fileName);
    ImportedPreset importFxp (const void* data, size_t size, const juce::String& fileName);
    std::vector<ImportedPreset> importFxb (const void* data, size_t size, const juce::String& fileName);

    // Single preset file of any supported type (.SerumPreset, .fxp, .cureum).
    ImportedPreset importFile (const juce::File& file);

    // Any container: .fxb, .zip, .SerumPack, folder (recursive). Wavetables and noises found in
    // archives are extracted next to the converted presets so references resolve.
    std::vector<ImportedPreset> importBank (const juce::File& fileOrFolder, const juce::File& extractFolder,
                                            std::function<bool (float)> progress = {});

    // Looks for a Serum wavetable / noise file (relative path as stored in presets) in the folders
    // configured under Settings > Serum folders.
    juce::File findSerumResource (const juce::String& relativePath, bool isNoise);
    void rescanSerumFolders();

    // Exposed for tests
    int filterTypeFromSerumName (const juce::String& name);
    WarpMode warpFromSerumName (const juce::String& name);
    ModSource serum1Source (int id);
    ModSource serum2Source (int id, int aux);
    int serum1ParamToCureum (int serumParam, int& oscOrModule);
}

} // namespace cureum
