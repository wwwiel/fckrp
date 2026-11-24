#pragma once

#include "Common.h"

namespace fckrp
{
// Resonance tamer: a dynamic EQ that only cuts harsh, ringing frequencies
// while they stick out, and leaves the vocal alone otherwise.
//
// Design (original): eight narrow bands, spaced evenly on a log scale from
// 1.2 kHz to 9 kHz, where vocal harshness lives. Each band measures its own
// level and compares it with the bands two steps away on either side. A real resonance is a narrow
// peak, so a band that is much louder than its neighbours is turned down,
// by exactly as much as it sticks out above the "Sensitivity" threshold,
// scaled by "Depth". Broad, natural brightness raises all bands together and
// is therefore not touched.
//
// Each band is a state-variable band-pass with unity peak gain; the cut is
// applied as y = x + (gain - 1) * bandpass(x), so the gain can change every
// sample without recalculating filter coefficients (no zipper noise).
class ResonanceTamer
{
public:
    static constexpr int numBands = 8;

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        const double lo = 1200.0, hi = std::min (9000.0, sampleRate * 0.4);
        for (int b = 0; b < numBands; ++b)
        {
            const double f = lo * std::pow (hi / lo, (double) b / (numBands - 1));
            auto& band = bands[(size_t) b];
            band.g = (float) std::tan (juce::MathConstants<double>::pi * f / sampleRate);
            band.k = 1.0f / q;
            band.a1 = 1.0f / (1.0f + band.g * (band.g + band.k));
            band.a2 = band.g * band.a1;
            band.a3 = band.g * band.a2;
            band.state.assign ((size_t) numChannels, { 0.0f, 0.0f });
            band.env = 0.0f;
            band.gain = 1.0f;
        }
        attCoef = timeCoef (0.002, fs);
        relCoef = timeCoef (0.060, fs);
        gainAtt = timeCoef (0.003, fs);
        gainRel = timeCoef (0.080, fs);
    }

    // depth 0..1 -> up to 12 dB of cut. sensitivity 0..1 -> a band must
    // stick out 9 dB (low) down to 1 dB (high) above its neighbours.
    void setParameters (float depth, float sensitivity)
    {
        maxCutDb = 12.0f * juce::jlimit (0.0f, 1.0f, depth);
        thresholdDb = 9.0f - 8.0f * juce::jlimit (0.0f, 1.0f, sensitivity);
    }

    // Returns the deepest cut in dB during the block (for metering).
    float process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = std::min (buffer.getNumChannels(), (int) bands[0].state.size());
        const int n = buffer.getNumSamples();
        float maxCut = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // 1) Run every band on every channel, cascaded, and measure.
            float levels[numBands];
            for (int b = 0; b < numBands; ++b)
            {
                auto& band = bands[(size_t) b];
                float peak = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float x = buffer.getSample (ch, i);
                    auto& s = band.state[(size_t) ch];
                    const float v3 = x - s[1];
                    const float v1 = band.a1 * s[0] + band.a2 * v3;
                    const float v2 = s[1] + band.a2 * s[0] + band.a3 * v3;
                    s[0] = 2.0f * v1 - s[0];
                    s[1] = 2.0f * v2 - s[1];
                    const float bp = band.k * v1;           // unity gain at the centre
                    peak = std::max (peak, std::abs (bp));
                    buffer.setSample (ch, i, x + (band.gain - 1.0f) * bp);
                }
                band.env = (peak > band.env ? attCoef : relCoef) * (band.env - peak) + peak;
                levels[b] = gainToDb (band.env + 1.0e-6f);
            }

            // 2) Compare each band with its neighbours and update its gain.
            for (int b = 0; b < numBands; ++b)
            {
                // Compare with the bands two steps away on each side, so a
                // resonance that falls between two band centres (and shows
                // up in both) still stands out clearly.
                float sum = 0.0f;
                int count = 0;
                for (int o : { -2, 2 })
                    if (b + o >= 0 && b + o < numBands) { sum += levels[b + o]; ++count; }
                const float ref = sum / (float) count;

                const float excess = levels[b] - ref - thresholdDb;
                // Ignore near-silence so noise floors are never "tamed".
                const float cutDb = (excess > 0.0f && levels[b] > -60.0f) ? std::min (excess, maxCutDb) : 0.0f;
                const float target = dbToGain (-cutDb);
                auto& band = bands[(size_t) b];
                band.gain = (target < band.gain ? gainAtt : gainRel) * (band.gain - target) + target;
                maxCut = std::max (maxCut, -gainToDb (band.gain));
            }
        }
        return maxCut;
    }

private:
    struct Band
    {
        float g = 0.1f, k = 0.25f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
        std::vector<std::array<float, 2>> state;
        float env = 0.0f, gain = 1.0f;
    };

    static constexpr float q = 4.0f;
    double fs = 44100.0;
    std::array<Band, numBands> bands;
    float attCoef = 0.0f, relCoef = 0.0f, gainAtt = 0.0f, gainRel = 0.0f;
    float maxCutDb = 4.0f, thresholdDb = 5.0f;
};
} // namespace fckrp
