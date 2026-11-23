#pragma once

#include "Common.h"

namespace fckrp
{
// Send-style hall: the dry vocal stays at full level and the reverb is added.
// The reverb input is low-cut so the tail does not get muddy.
class VocalReverb
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        reverb.setSampleRate (sampleRate);
        reverb.reset();
        send.setSize (2, maxBlock);
        hpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 250.0f / (float) sampleRate);
        hpState[0] = hpState[1] = 0.0f;
    }

    void reset() { reverb.reset(); hpState[0] = hpState[1] = 0.0f; }

    void setParameters (float size, float damping, float mix)
    {
        juce::Reverb::Parameters p;
        p.roomSize = juce::jlimit (0.0f, 1.0f, size);
        p.damping = juce::jlimit (0.0f, 1.0f, damping);
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        reverb.setParameters (p);
        wet = juce::jlimit (0.0f, 1.0f, mix) * 0.35f;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = std::min (buffer.getNumChannels(), 2);
        const int n = buffer.getNumSamples();
        if (numCh == 0 || n > send.getNumSamples())
            return;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto* in = buffer.getReadPointer (ch);
            auto* out = send.getWritePointer (ch);
            float& s = hpState[ch];
            for (int i = 0; i < n; ++i)
            {
                s += hpCoef * (in[i] - s);
                out[i] = in[i] - s;
            }
        }

        if (numCh == 2)
            reverb.processStereo (send.getWritePointer (0), send.getWritePointer (1), n);
        else
            reverb.processMono (send.getWritePointer (0), n);

        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFromWithRamp (ch, 0, send.getReadPointer (ch), n, lastWet, wet);
        lastWet = wet;
    }

private:
    juce::Reverb reverb;
    juce::AudioBuffer<float> send;
    float hpCoef = 0.03f, hpState[2] {};
    float wet = 0.0f, lastWet = 0.0f;
};
} // namespace fckrp
