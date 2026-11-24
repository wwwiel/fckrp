#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace fckrp
{
// One-pole smoothing coefficient for a time constant in seconds.
inline float timeCoef (double seconds, double sampleRate)
{
    if (seconds <= 0.0)
        return 0.0f;
    return (float) std::exp (-1.0 / (seconds * sampleRate));
}

inline float gainToDb (float g)
{
    return 20.0f * std::log10 (std::max (g, 1.0e-6f));
}

inline float dbToGain (float db)
{
    return std::pow (10.0f, db * 0.05f);
}
} // namespace fckrp
