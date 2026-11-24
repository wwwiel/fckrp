#pragma once

#include "Common.h"

namespace fckrp
{
// Split-band de-esser: the signal is split at the chosen frequency with a
// Linkwitz-Riley crossover (low + high sum back to a flat response) and only
// the high band is turned down when "s" sounds go above the threshold.
class DeEsser
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        fs = sampleRate;
        crossover.prepare ({ sampleRate, (juce::uint32) maxBlock, (juce::uint32) numChannels });
        crossover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        env = 0.0f;
        gain = 1.0f;
    }

    void setParameters (float freqHz, float thresholdDb)
    {
        crossover.setCutoffFrequency (std::min (freqHz, (float) fs * 0.45f));
        threshold = thresholdDb;
    }

    // Returns the largest gain reduction in dB during this block (for metering).
    float process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = std::min (buffer.getNumChannels(), 2);
        const int numSamples = buffer.getNumSamples();
        const float att = timeCoef (0.0005, fs);
        const float rel = timeCoef (0.060, fs);
        const float gAtt = timeCoef (0.001, fs);
        const float gRel = timeCoef (0.050, fs);
        float maxGr = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float lows[2] {}, highs[2] {};
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                crossover.processSample (ch, buffer.getSample (ch, i), lows[ch], highs[ch]);
                peak = std::max (peak, std::abs (highs[ch]));
            }

            env = (peak > env ? att : rel) * (env - peak) + peak;
            const float over = gainToDb (env) - threshold;
            const float grDb = over > 0.0f ? std::min (over * 0.75f, maxRangeDb) : 0.0f;
            const float target = dbToGain (-grDb);
            gain = (target < gain ? gAtt : gRel) * (gain - target) + target;
            maxGr = std::max (maxGr, -gainToDb (gain));

            for (int ch = 0; ch < numCh; ++ch)
                buffer.setSample (ch, i, lows[ch] + highs[ch] * gain);
        }
        return maxGr;
    }

private:
    static constexpr float maxRangeDb = 12.0f;
    double fs = 44100.0;
    juce::dsp::LinkwitzRileyFilter<float> crossover;
    float threshold = -30.0f;
    float env = 0.0f, gain = 1.0f;
};
} // namespace fckrp
