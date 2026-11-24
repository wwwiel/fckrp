#pragma once

#include "Common.h"

namespace fckrp
{
// "Punch": drives the vocal into a fast look-ahead peak limiter, the way
// rap vocals are made dense and in-your-face. Peaks are held at a ceiling
// of -1 dBFS while everything below comes up, so the voice gets louder and
// thicker without audible clipping.
//
// It always runs (with drive 1 and nothing to limit when off), so the
// plugin's latency never changes.
class PunchLimiter
{
public:
    static constexpr double lookaheadSeconds = 0.0015;

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        look = std::max (1, (int) std::round (lookaheadSeconds * sampleRate));
        delay.assign ((size_t) numChannels, std::vector<float> ((size_t) look, 0.0f));
        req.assign ((size_t) look, 1.0f);
        pos = 0;
        gain = 1.0f;
        attCoef = 1.0f - std::exp (-4.0f / (float) look);   // settles within the look-ahead
        relCoef = timeCoef (0.08, sampleRate);
        drive.reset (sampleRate, 0.05);
    }

    int getLatencySamples() const { return look; }

    // amount 0..1 -> 0..12 dB of drive into the limiter.
    void setAmount (float amount, bool enabled)
    {
        drive.setTargetValue (enabled ? dbToGain (12.0f * juce::jlimit (0.0f, 1.0f, amount)) : 1.0f);
        active = enabled;
    }

    // Returns the deepest gain reduction in dB during the block.
    float process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = std::min (buffer.getNumChannels(), (int) delay.size());
        const int n = buffer.getNumSamples();
        const float ceiling = dbToGain (-1.0f);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float d = drive.getNextValue();

            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = std::max (peak, std::abs (buffer.getSample (ch, i) * d));

            req[(size_t) pos] = (active && peak > ceiling) ? ceiling / peak : 1.0f;
            float target = 1.0f;
            for (float r : req)
                target = std::min (target, r);

            gain = target < gain ? gain + (target - gain) * attCoef
                                 : target + (gain - target) * relCoef;
            maxGr = std::max (maxGr, -gainToDb (gain));

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& line = delay[(size_t) ch];
                const float in = buffer.getSample (ch, i) * d;
                float out = line[(size_t) pos] * gain;
                line[(size_t) pos] = in;
                if (active)
                    out = juce::jlimit (-ceiling, ceiling, out);   // catch the last bit of overshoot
                buffer.setSample (ch, i, out);
            }
            pos = (pos + 1) % look;
        }
        return maxGr;
    }

private:
    double fs = 44100.0;
    int look = 1, pos = 0;
    std::vector<std::vector<float>> delay;
    std::vector<float> req;
    float gain = 1.0f, attCoef = 0.5f, relCoef = 0.99f;
    bool active = false;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> drive { 1.0f };
};
} // namespace fckrp
