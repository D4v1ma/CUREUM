#pragma once

#include "Wavetable.h"

namespace cureum
{

// Built-in wavetables, synthesised on demand (no sample data ships with the plugin).
class WavetableFactory
{
public:
    static const juce::StringArray& names();
    static WavetablePtr get (const juce::String& name);           // nullptr if unknown
    static WavetablePtr getDefault()                              { return get ("Basic Shapes"); }

    // Finds the closest factory table for a foreign table name (used when a Serum preset references
    // a wavetable file that is not installed).
    static juce::String closestMatch (const juce::String& foreignName);
};

} // namespace cureum
