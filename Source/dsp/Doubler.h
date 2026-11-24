#pragma once

#include "Common.h"

namespace fckrp
{
// Stereo doubler: two extra "takes" of the vocal made from short delays
// (about 14 and 21 ms) whose delay time drifts slowly. The drift bends the
// pitch by a few cents, so the copies sound like separate performances.
//
// Mono-safe: the two takes are added only as a side signal (left gets
// takeL - takeR, right gets the opposite). When the mix is summed to mono
// the side cancels completely, so mono playback sounds exactly like the
// vocal without the doubler: no comb filtering, no level jump.
class Doubler
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        size = (int) (sampleRate * 0.06) + 4;
        line.assign ((size_t) size, 0.0f);
        writePos = 0;
        phaseL = 0.0;
        phaseR = 0.37;
        amount.reset (sampleRate, 0.05);
        depth.reset (sampleRate, 0.05);
    }

    void reset()
    {
        std::fill (line.begin(), line.end(), 0.0f);
    }

    // amountIn 0..1: how loud the doubled takes are.
    // detuneIn 0..1: how far the takes drift in pitch and timing.
    void setParameters (float amountIn, float detuneIn)
    {
        amount.setTargetValue (juce::jlimit (0.0f, 1.0f, amountIn));
        depth.setTargetValue (juce::jlimit (0.0f, 1.0f, detuneIn));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2)
            return; // a doubler has no meaning on a mono track

        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        const double twoPi = juce::MathConstants<double>::twoPi;
        const double incL = 0.43 / fs, incR = 0.31 / fs;   // slow, unrelated drift rates

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float mid = 0.5f * (left[i] + right[i]);
            line[(size_t) writePos] = mid;

            const float a = amount.getNextValue();
            const float d = depth.getNextValue();

            // Up to ~2.5 ms of drift: roughly +-10 cents at full detune.
            const float modL = (float) std::sin (twoPi * phaseL) * d * 0.0025f;
            const float modR = (float) std::sin (twoPi * phaseR) * d * 0.0025f;
            phaseL += incL; if (phaseL >= 1.0) phaseL -= 1.0;
            phaseR += incR; if (phaseR >= 1.0) phaseR -= 1.0;

            const float takeL = read ((0.014f + modL) * (float) fs);
            const float takeR = read ((0.021f + modR) * (float) fs);

            const float side = 0.5f * (takeL - takeR) * a;
            left[i] += side;
            right[i] -= side;

            writePos = (writePos + 1) % size;
        }
    }

private:
    float read (float delaySamples) const
    {
        float pos = (float) writePos - delaySamples;
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos % size;
        const int i1 = (i0 + 1) % size;
        const float frac = pos - std::floor (pos);
        return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
    }

    double fs = 44100.0;
    int size = 1, writePos = 0;
    std::vector<float> line;
    double phaseL = 0.0, phaseR = 0.0;
    juce::SmoothedValue<float> amount { 0.0f }, depth { 0.5f };
};

// Mid/side stereo width. 0 = mono, 1 = unchanged, 2 = twice as wide.
// The mid (what you hear in mono) is never touched, so it is mono-safe.
class StereoWidth
{
public:
    void prepare (double sampleRate) { width.reset (sampleRate, 0.05); }
    void setWidth (float w) { width.setTargetValue (juce::jlimit (0.0f, 2.0f, w)); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2)
            return;
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float w = width.getNextValue();
            const float m = 0.5f * (l[i] + r[i]);
            const float s = 0.5f * (l[i] - r[i]) * w;
            l[i] = m + s;
            r[i] = m - s;
        }
    }

private:
    juce::SmoothedValue<float> width { 1.0f };
};
} // namespace fckrp
