#pragma once

#include <juce_core/juce_core.h>

namespace cureum
{

// Minimal, defensive CBOR (RFC 8949) decoder producing juce::var trees: maps become
// DynamicObjects (keys converted to strings), arrays become var arrays, byte strings become
// MemoryBlocks. Used for the payload of Serum 2 presets.
class CborDecoder
{
public:
    // Returns a void var and sets `error` if the data is malformed.
    static juce::var decode (const void* data, size_t size, juce::String& error);

private:
    CborDecoder (const juce::uint8* d, size_t n) : p (d), end (d + n) {}
    juce::var item (int depth);
    bool readUint (int info, juce::uint64& out);
    bool need (size_t n) const { return (size_t) (end - p) >= n; }

    const juce::uint8* p;
    const juce::uint8* end;
    juce::String err;
    static constexpr int kMaxDepth = 64;
};

} // namespace cureum
