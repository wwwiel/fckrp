#pragma once

#include "Common.h"

namespace fckrp
{
// Send-style vocal delay: the dry vocal passes unchanged and the echoes are
// added on top. Repeats are band-limited (low cut + high cut) so they stay
// behind the vocal instead of fighting it.
class VocalDelay
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        size = (int) (sampleRate * 2.1) + 4;
        lines.assign ((size_t) numChannels, std::vector<float> ((size_t) size, 0.0f));
        filt.assign ((size_t) numChannels, { 0.0f, 0.0f });
        writePos = 0;
        timeSamples.reset (sampleRate, 0.2);
        mix.reset (sampleRate, 0.05);
        feedback.reset (sampleRate, 0.05);
        lpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 5000.0f / (float) sampleRate);
        hpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 200.0f / (float) sampleRate);
    }

    void reset()
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& f : filt) f = { 0.0f, 0.0f };
    }

    void setParameters (float timeMs, float feedbackAmount, float mixAmount)
    {
        timeSamples.setTargetValue (juce::jlimit (1.0f, (float) size - 3.0f, timeMs * 0.001f * (float) fs));
        feedback.setTargetValue (juce::jlimit (0.0f, 0.9f, feedbackAmount));
        mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mixAmount));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = std::min (buffer.getNumChannels(), (int) lines.size());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float d = timeSamples.getNextValue();
            const float fb = feedback.getNextValue();
            const float m = mix.getNextValue();

            float readPos = (float) writePos - d;
            if (readPos < 0.0f) readPos += (float) size;
            const int i0 = (int) readPos;
            const int i1 = (i0 + 1) % size;
            const float frac = readPos - (float) i0;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& line = lines[(size_t) ch];
                const float wet = line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);

                // Band-limit the repeats.
                auto& f = filt[(size_t) ch];
                f[0] += lpCoef * (wet - f[0]);          // low-pass state
                f[1] += hpCoef * (f[0] - f[1]);         // low-frequency state
                const float shaped = f[0] - f[1];

                const float dry = buffer.getSample (ch, i);
                line[(size_t) writePos] = dry + fb * shaped;
                buffer.setSample (ch, i, dry + m * shaped);
            }
            writePos = (writePos + 1) % size;
        }
    }

private:
    double fs = 44100.0;
    int size = 1, writePos = 0;
    std::vector<std::vector<float>> lines;
    std::vector<std::array<float, 2>> filt;
    juce::SmoothedValue<float> timeSamples { 1.0f }, mix { 0.0f }, feedback { 0.0f };
    float lpCoef = 0.5f, hpCoef = 0.01f;
};
} // namespace fckrp
