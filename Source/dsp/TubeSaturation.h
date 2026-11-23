#pragma once

#include "Common.h"

namespace fckrp
{
// Asymmetric tube-style saturation with 2x oversampling to keep aliasing down.
// The bias makes the curve slightly lopsided, which adds warm even harmonics.
class TubeSaturation
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampling->initProcessing ((size_t) maxBlock);
        drive.reset (sampleRate * 2.0, 0.05);
        dcState.assign ((size_t) numChannels, { 0.0f, 0.0f });
        dcCoef = 1.0f - (float) (2.0 * juce::MathConstants<double>::pi * 10.0 / (sampleRate * 2.0));
    }

    float getLatencySamples() const { return oversampling ? oversampling->getLatencyInSamples() : 0.0f; }

    void reset()
    {
        if (oversampling) oversampling->reset();
        for (auto& s : dcState) s = { 0.0f, 0.0f };
    }

    // amount: 0 = clean, 1 = heavy drive.
    void setAmount (float amount)
    {
        drive.setTargetValue (juce::jlimit (0.0f, 1.0f, amount));
    }

    void process (juce::AudioBuffer<float>& buffer, bool enabled)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampling->processSamplesUp (block);

        const int numCh = (int) up.getNumChannels();
        const int n = (int) up.getNumSamples();
        constexpr float bias = 0.05f;
        const float biasOut = std::tanh (bias);

        for (int i = 0; i < n; ++i)
        {
            const float amt = drive.getNextValue();
            if (! enabled || amt <= 0.0001f)
                continue;

            const float g = dbToGain (amt * 12.0f);
            const float comp = 1.0f / std::pow (g, 0.8f);

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = up.getSample (ch, i);
                float y = (std::tanh (g * x + bias) - biasOut) * comp;

                // DC blocker for the offset created by the asymmetric curve.
                auto& s = dcState[(size_t) ch];
                const float out = y - s[0] + dcCoef * s[1];
                s[0] = y;
                s[1] = out;
                up.setSample (ch, i, out);
            }
        }

        oversampling->processSamplesDown (block);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::SmoothedValue<float> drive { 0.0f };
    std::vector<std::array<float, 2>> dcState;
    float dcCoef = 0.999f;
};
} // namespace fckrp
