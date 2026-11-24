#pragma once

// Spectrum and stereo analysis on the audio thread.
//  - 8192-point FFT, Hann window, 75 % overlap
//  - fast spectrum (for the live display) and the long-term average of the
//    whole song (for the advice), gated so silence doesn't count
//  - per-bin mid and side power, so the bass width can be checked
//  - correlation and mono-compatibility from running sums

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mcheck
{
struct SpectrumSnapshot
{
    double sampleRate = 48000.0;
    int numBins = 0;              // fftSize / 2 + 1
    std::vector<float> fast;      // power per bin (linear), smoothed over ~0.3 s
    std::vector<float> longTerm;  // average power per bin over the measured song
    std::vector<float> midLong, sideLong;
    int64_t frames = 0;           // long-term frames counted
};

class SpectrumAnalyser
{
public:
    static constexpr int order = 13;
    static constexpr int fftSize = 1 << order;
    static constexpr int hop = fftSize / 4;
    static constexpr int numBins = fftSize / 2 + 1;

    SpectrumAnalyser() : fft (order)
    {
        window.resize (fftSize);
        double wsum = 0;
        for (int i = 0; i < fftSize; ++i)
        {
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) fftSize);
            wsum += window[(size_t) i];
        }
        // Scale so a full-scale sine reads about 0 dB in its bin.
        norm = (float) (1.0 / ((wsum / 2.0) * (wsum / 2.0)));
        for (auto& b : ring)
            b.assign (fftSize, 0.0f);
        for (auto& b : work)
            b.assign (fftSize * 2, 0.0f);
        fastPow.assign (numBins, 0.0f);
        sumPow.assign (numBins, 0.0);
        sumMid.assign (numBins, 0.0);
        sumSide.assign (numBins, 0.0);
        shared.fast.assign (numBins, 0.0f);
        shared.longTerm.assign (numBins, 0.0f);
        shared.midLong.assign (numBins, 0.0f);
        shared.sideLong.assign (numBins, 0.0f);
        shared.numBins = numBins;
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // Fast display smoothing: about 0.3 s regardless of sample rate.
        const double hopsPerSecond = fs / hop;
        fastCoeff = (float) std::exp (-1.0 / (0.3 * hopsPerSecond));
        corrCoeff = std::exp (-1.0 / (0.3 * fs));
        reset();
    }

    void reset()
    {
        for (auto& b : ring)
            std::fill (b.begin(), b.end(), 0.0f);
        writePos = 0;
        sinceHop = 0;
        std::fill (fastPow.begin(), fastPow.end(), 0.0f);
        std::fill (sumPow.begin(), sumPow.end(), 0.0);
        std::fill (sumMid.begin(), sumMid.end(), 0.0);
        std::fill (sumSide.begin(), sumSide.end(), 0.0);
        frames = 0;
        lr = ll = rr = 0;
        fastLr = fastLl = fastRr = 0;
        correlationFast.store (1.0f);
        correlationLong.store (1.0f);
        monoDropDb.store (0.0f);
        publish (true);
    }

    void process (const float* const* data, int numChannels, int numSamples) noexcept
    {
        const float* l = data[0];
        const float* r = numChannels > 1 ? data[1] : data[0];
        for (int i = 0; i < numSamples; ++i)
        {
            float a = l[i], b = r[i];
            if (! std::isfinite (a)) a = 0.0f;
            if (! std::isfinite (b)) b = 0.0f;
            ring[0][(size_t) writePos] = a;
            ring[1][(size_t) writePos] = b;
            writePos = (writePos + 1) % fftSize;

            // Running correlation, ~0.3 s, for the live meter.
            fastLr = fastLr * corrCoeff + (double) a * b;
            fastLl = fastLl * corrCoeff + (double) a * a;
            fastRr = fastRr * corrCoeff + (double) b * b;

            if (++sinceHop >= hop)
            {
                sinceHop = 0;
                analyseFrame();
            }
        }
        const double den = std::sqrt (fastLl * fastRr);
        if (den > 1.0e-9)
            correlationFast.store ((float) (fastLr / den));
    }

    // Copies the latest analysis. Returns false if nothing new could be read.
    bool getSnapshot (SpectrumSnapshot& out)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        out = shared;
        return true;
    }

    std::atomic<float> correlationFast { 1.0f }, correlationLong { 1.0f }, monoDropDb { 0.0f };

private:
    void analyseFrame() noexcept
    {
        // Window both channels, in time order starting at the oldest sample.
        double frameEnergy = 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& w = work[(size_t) ch];
            for (int i = 0; i < fftSize; ++i)
            {
                const float x = ring[(size_t) ch][(size_t) ((writePos + i) % fftSize)];
                frameEnergy += (double) x * x;
                w[(size_t) i] = x * window[(size_t) i];
            }
            std::fill (w.begin() + fftSize, w.end(), 0.0f);
            fft.performRealOnlyForwardTransform (w.data(), true);
        }
        frameEnergy /= (2.0 * fftSize);

        const bool counts = frameEnergy > 1.0e-6; // gate: about -60 dBFS RMS
        const auto& L = work[0];
        const auto& R = work[1];
        double lE = 0, rE = 0, lrE = 0;
        for (int k = 0; k < numBins; ++k)
        {
            const float lr_ = L[(size_t) (2 * k)], li = L[(size_t) (2 * k + 1)];
            const float rr_ = R[(size_t) (2 * k)], ri = R[(size_t) (2 * k + 1)];
            const float pl = (lr_ * lr_ + li * li) * norm;
            const float pr = (rr_ * rr_ + ri * ri) * norm;
            const float p = 0.5f * (pl + pr);
            fastPow[(size_t) k] = fastPow[(size_t) k] * fastCoeff + p * (1.0f - fastCoeff);
            if (counts)
            {
                const float mr = 0.5f * (lr_ + rr_), mi = 0.5f * (li + ri);
                const float sr = 0.5f * (lr_ - rr_), si = 0.5f * (li - ri);
                const double pm = (mr * mr + mi * mi) * norm;
                const double ps = (sr * sr + si * si) * norm;
                sumPow[(size_t) k] += p;
                sumMid[(size_t) k] += pm;
                sumSide[(size_t) k] += ps;
                lE += pl;
                rE += pr;
                lrE += (double) lr_ * rr_ + (double) li * ri;
            }
        }
        if (counts)
        {
            ++frames;
            lr += lrE * norm;
            ll += lE;
            rr += rE;
            const double den = std::sqrt (ll * rr);
            if (den > 1.0e-12)
                correlationLong.store ((float) (lr / den));
            const double avg = 0.5 * (ll + rr);
            double mono = 0;
            for (int k = 0; k < numBins; ++k)
                mono += sumMid[(size_t) k];
            if (avg > 1.0e-12 && mono > 1.0e-15)
                monoDropDb.store ((float) (10.0 * std::log10 (mono / avg)));
        }
        publish (false);
    }

    void publish (bool force) noexcept
    {
        if (force)
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            copyToShared();
            return;
        }
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked())
            copyToShared();
    }

    void copyToShared() noexcept
    {
        shared.sampleRate = fs;
        shared.frames = frames;
        const double inv = frames > 0 ? 1.0 / (double) frames : 0.0;
        for (int k = 0; k < numBins; ++k)
        {
            shared.fast[(size_t) k] = fastPow[(size_t) k];
            shared.longTerm[(size_t) k] = (float) (sumPow[(size_t) k] * inv);
            shared.midLong[(size_t) k] = (float) (sumMid[(size_t) k] * inv);
            shared.sideLong[(size_t) k] = (float) (sumSide[(size_t) k] * inv);
        }
    }

    juce::dsp::FFT fft;
    double fs = 48000.0;
    std::vector<float> window;
    float norm = 1.0f;
    std::array<std::vector<float>, 2> ring, work;
    int writePos = 0, sinceHop = 0;
    std::vector<float> fastPow;
    std::vector<double> sumPow, sumMid, sumSide;
    int64_t frames = 0;
    float fastCoeff = 0.8f;
    double corrCoeff = 0.99993; // ~0.3 s at 48 kHz
    double lr = 0, ll = 0, rr = 0;
    double fastLr = 0, fastLl = 0, fastRr = 0;

    juce::SpinLock lock;
    SpectrumSnapshot shared;
};
} // namespace mcheck
