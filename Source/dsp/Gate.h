#pragma once

#include "Common.h"

namespace fckrp
{
// Gentle vocal gate: turns noise between phrases down by a fixed range
// instead of cutting it to silence, with hysteresis and hold so it does
// not chatter on word endings.
class Gate
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        env = 0.0f;
        gain = 1.0f;
        holdCounter = 0;
        isOpen = true;
    }

    void setThreshold (float db)
    {
        openLevel = dbToGain (db);
        closeLevel = dbToGain (db - 6.0f);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        const float envAtt = timeCoef (0.0005, fs);
        const float envRel = timeCoef (0.040, fs);
        const float openCoef = timeCoef (0.002, fs);
        const float closeCoef = timeCoef (0.080, fs);
        const int holdSamples = (int) (0.050 * fs);
        const float floorGain = dbToGain (-24.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = std::max (peak, std::abs (buffer.getSample (ch, i)));

            const float c = peak > env ? envAtt : envRel;
            env = c * (env - peak) + peak;

            if (env > openLevel)
            {
                isOpen = true;
                holdCounter = holdSamples;
            }
            else if (env < closeLevel)
            {
                if (holdCounter > 0)
                    --holdCounter;
                else
                    isOpen = false;
            }

            const float target = isOpen ? 1.0f : floorGain;
            const float g = target > gain ? openCoef : closeCoef;
            gain = g * (gain - target) + target;

            for (int ch = 0; ch < numCh; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * gain);
        }
    }

private:
    double fs = 44100.0;
    float openLevel = 0.001f, closeLevel = 0.0005f;
    float env = 0.0f, gain = 1.0f;
    int holdCounter = 0;
    bool isOpen = true;
};
} // namespace fckrp
