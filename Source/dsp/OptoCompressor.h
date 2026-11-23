#pragma once

#include "Common.h"

namespace fckrp
{
// Optical-style leveling compressor.
//
// The character of a classic opto (light cell) compressor comes from:
//  - a very soft knee, so compression creeps in gradually,
//  - a two-stage release: part of the gain reduction recovers quickly,
//    the rest recovers slowly,
//  - "memory": the longer and harder it has been compressing, the slower
//    it lets go, which keeps vocals even without pumping,
//  - a slightly slower attack on big jumps, which lets consonants through.
// This is an original model of that behaviour, not a copy of any product.
class OptoCompressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        grFast = grSlow = memory = 0.0f;
        makeup.reset (sampleRate, 0.05);
    }

    void setParameters (float thresholdDb, float ratioIn, float attackMs, float releaseSec, float makeupDb)
    {
        threshold = thresholdDb;
        ratio = std::max (1.0f, ratioIn);
        attack = attackMs * 0.001f;
        release = releaseSec;
        makeup.setTargetValue (dbToGain (makeupDb));
    }

    // Returns the gain reduction in dB at the end of the block (for metering).
    float process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        const float detCoef = timeCoef (0.002, fs);       // light-cell response smoothing
        const float memCoef = timeCoef (1.5, fs);         // how quickly the cell "remembers"
        const float fastRel = timeCoef (std::min (0.06f, release), fs);
        const float knee = 10.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = std::max (peak, std::abs (buffer.getSample (ch, i)));

            // Smoothed RMS-like detector (opto cells respond to average level).
            const float sq = peak * peak;
            det = detCoef * (det - sq) + sq;
            const float levelDb = 10.0f * std::log10 (std::max (det, 1.0e-12f)) + 3.0f;

            // Soft-knee static curve.
            const float over = levelDb - threshold;
            float target = 0.0f;
            const float slope = 1.0f - 1.0f / ratio;
            if (over >= knee * 0.5f)
                target = slope * over;
            else if (over > -knee * 0.5f)
                target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

            // Memory charges with sustained compression and slows the release.
            const float memTarget = juce::jlimit (0.0f, 1.0f, grSlow / 12.0f);
            memory = memCoef * (memory - memTarget) + memTarget;

            // Bigger jumps attack a little slower, like a light cell warming up.
            const float jump = std::max (0.0f, target - grSlow);
            const float att = timeCoef (attack * (1.0f + jump * 0.05f), fs);
            const float slowRel = timeCoef (release * (1.0f + 2.0f * memory), fs);

            grFast = (target > grFast ? att : fastRel) * (grFast - target) + target;
            grSlow = (target > grSlow ? att : slowRel) * (grSlow - target) + target;

            const float gr = 0.4f * grFast + 0.6f * grSlow;
            const float g = dbToGain (-gr) * makeup.getNextValue();
            lastGr = gr;

            for (int ch = 0; ch < numCh; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
        }
        return lastGr;
    }

private:
    double fs = 44100.0;
    float threshold = -20.0f, ratio = 3.0f, attack = 0.01f, release = 0.5f;
    float det = 0.0f, grFast = 0.0f, grSlow = 0.0f, memory = 0.0f, lastGr = 0.0f;
    juce::SmoothedValue<float> makeup { 1.0f };
};
} // namespace fckrp
