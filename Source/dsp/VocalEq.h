#pragma once

#include "Common.h"

namespace fckrp
{
// Four-band vocal EQ: low cut, "mud" bell around 300 Hz,
// presence bell around 3.5 kHz and an air shelf at 10 kHz.
class VocalEq
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        channels.clear();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels.emplace_back();
            for (auto& f : c.bands)
                f.coefficients = new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        }
        lastLowCut = lastMud = lastPresence = lastAir = -1000.0f;
    }

    void setParameters (float lowCutHz, float mudDb, float presenceDb, float airDb)
    {
        using AC = juce::dsp::IIR::ArrayCoefficients<float>;
        const float nyq = (float) fs * 0.45f;

        if (! juce::exactlyEqual (lowCutHz, lastLowCut))
        {
            const auto c = AC::makeHighPass (fs, std::min (lowCutHz, nyq), 0.707f);
            for (auto& ch : channels) *ch.bands[0].coefficients = c;
            lastLowCut = lowCutHz;
        }
        if (! juce::exactlyEqual (mudDb, lastMud))
        {
            const auto c = AC::makePeakFilter (fs, 300.0f, 1.0f, dbToGain (mudDb));
            for (auto& ch : channels) *ch.bands[1].coefficients = c;
            lastMud = mudDb;
        }
        if (! juce::exactlyEqual (presenceDb, lastPresence))
        {
            const auto c = AC::makePeakFilter (fs, std::min (3500.0f, nyq), 0.8f, dbToGain (presenceDb));
            for (auto& ch : channels) *ch.bands[2].coefficients = c;
            lastPresence = presenceDb;
        }
        if (! juce::exactlyEqual (airDb, lastAir))
        {
            const auto c = AC::makeHighShelf (fs, std::min (10000.0f, nyq), 0.707f, dbToGain (airDb));
            for (auto& ch : channels) *ch.bands[3].coefficients = c;
            lastAir = airDb;
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = std::min (buffer.getNumChannels(), (int) channels.size());
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            auto& c = channels[(size_t) ch];
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float x = d[i];
                for (auto& f : c.bands)
                    x = f.processSample (x);
                d[i] = x;
            }
        }
    }

private:
    struct Channel
    {
        std::array<juce::dsp::IIR::Filter<float>, 4> bands;
    };

    double fs = 44100.0;
    std::vector<Channel> channels;
    float lastLowCut = -1000.0f, lastMud = -1000.0f, lastPresence = -1000.0f, lastAir = -1000.0f;
};
} // namespace fckrp
