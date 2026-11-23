#pragma once

#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

namespace fckrp
{
struct Preset
{
    juce::String name;
    // Parameter ID -> value in the parameter's own units (dB, Hz, ms, 0..1).
    // Parameters not listed fall back to their default.
    std::vector<std::pair<juce::String, float>> values;
};

const std::vector<Preset>& getFactoryPresets();
} // namespace fckrp
