#pragma once

// Loudness measurement after ITU-R BS.1770-4 / EBU R128:
//  - K-weighting (pre-filter + RLB high-pass), computed for any sample rate
//  - momentary (400 ms), short-term (3 s) and gated integrated loudness
//  - loudness range (LRA, EBU Tech 3342)
// Everything runs on the audio thread, nothing allocates after prepare().

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mcheck
{
inline double energyToLufs (double e) { return e > 1.0e-20 ? -0.691 + 10.0 * std::log10 (e) : -200.0; }
inline double lufsToEnergy (double l) { return std::pow (10.0, (l + 0.691) / 10.0); }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

struct KWeighting
{
    Biquad shelf, highpass;

    void prepare (double fs)
    {
        const double pi = 3.14159265358979323846;
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            highpass.b0 = 1.0;
            highpass.b1 = -2.0;
            highpass.b2 = 1.0;
            highpass.a1 = 2.0 * (K * K - 1.0) / a0;
            highpass.a2 = (1.0 - K / Q + K * K) / a0;
        }
        reset();
    }
    void reset() { shelf.reset(); highpass.reset(); }
    inline double process (double x) noexcept { return highpass.process (shelf.process (x)); }
};

// Loudness values are kept in a histogram (0.1 LU bins) so a whole song can be
// measured with fixed memory. Each bin also sums the exact block energies, so
// the integrated value is exact; only the gate threshold is rounded to 0.1 LU.
class LoudnessHistogram
{
public:
    static constexpr double minLufs = -70.0, maxLufs = 10.0, step = 0.1;
    static constexpr int numBins = 800;

    void reset() { counts.fill (0); energies.fill (0.0); total = 0; }

    void add (double energy)
    {
        const double l = energyToLufs (energy);
        if (l < minLufs)
            return;
        int bin = (int) ((l - minLufs) / step);
        bin = bin < 0 ? 0 : (bin >= numBins ? numBins - 1 : bin);
        counts[(size_t) bin]++;
        energies[(size_t) bin] += energy;
        ++total;
    }

    int64_t size() const { return total; }

    // Mean energy of all blocks at or above the given loudness.
    double meanEnergyAbove (double lufs, int64_t* countOut = nullptr) const
    {
        int first = (int) std::ceil ((lufs - minLufs) / step - 1.0e-9);
        first = first < 0 ? 0 : first;
        double e = 0;
        int64_t n = 0;
        for (int b = first; b < numBins; ++b)
        {
            e += energies[(size_t) b];
            n += counts[(size_t) b];
        }
        if (countOut != nullptr)
            *countOut = n;
        return n > 0 ? e / (double) n : 0.0;
    }

    // Loudness value at the given percentile (0..1) of blocks at or above a gate.
    double percentile (double gateLufs, double p) const
    {
        int first = (int) std::ceil ((gateLufs - minLufs) / step - 1.0e-9);
        first = first < 0 ? 0 : first;
        int64_t n = 0;
        for (int b = first; b < numBins; ++b)
            n += counts[(size_t) b];
        if (n == 0)
            return -200.0;
        const double target = p * (double) (n - 1);
        int64_t acc = 0;
        for (int b = first; b < numBins; ++b)
        {
            acc += counts[(size_t) b];
            if ((double) acc > target)
                return minLufs + (b + 0.5) * step;
        }
        return maxLufs;
    }

private:
    std::array<int64_t, numBins> counts {};
    std::array<double, numBins> energies {};
    int64_t total = 0;
};

class LoudnessMeter
{
public:
    static constexpr int historySize = 600; // short-term values, one per 100 ms = 60 s

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        stepLen = std::max (1, (int) std::lround (fs * 0.1));
        for (auto& k : kw)
            k.prepare (fs);
        reset();
    }

    void reset()
    {
        for (auto& k : kw)
            k.reset();
        stepEnergy.fill (0.0);
        stepPos = 0;
        stepsDone = 0;
        acc = 0;
        accCount = 0;
        integratedHist.reset();
        shortTermHist.reset();
        momentary.store (-200.0f);
        shortTerm.store (-200.0f);
        integrated.store (-200.0f);
        lra.store (0.0f);
        maxMomentary.store (-200.0f);
        maxShortTerm.store (-200.0f);
        measuredSeconds.store (0.0f);
        for (auto& h : history)
            h.store (-200.0f);
        historyWrite.store (0);
    }

    // channels: 1 or 2 (more are ignored). Left/right weights are 1.0 (BS.1770).
    void process (const float* const* data, int numChannels, int numSamples) noexcept
    {
        numChannels = numChannels > 2 ? 2 : numChannels;
        for (int i = 0; i < numSamples; ++i)
        {
            double sum = 0;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const double y = kw[(size_t) ch].process ((double) data[ch][i]);
                sum += y * y;
            }
            acc += sum;
            if (++accCount >= stepLen)
                finishStep();
        }
    }

    std::atomic<float> momentary { -200.0f }, shortTerm { -200.0f }, integrated { -200.0f }, lra { 0.0f };
    std::atomic<float> maxMomentary { -200.0f }, maxShortTerm { -200.0f };
    std::atomic<float> measuredSeconds { 0.0f }; // time above the -70 LUFS gate
    std::array<std::atomic<float>, historySize> history {};
    std::atomic<int> historyWrite { 0 };

private:
    void finishStep() noexcept
    {
        // Guard against NaN/Inf from the host so one bad sample can't poison the song.
        double e = acc / (double) stepLen;
        if (! std::isfinite (e))
            e = 0.0;
        acc = 0;
        accCount = 0;
        for (auto& k : kw)
            if (! std::isfinite (k.shelf.z1) || ! std::isfinite (k.highpass.z1))
                k.reset();

        stepEnergy[(size_t) stepPos] = e;
        stepPos = (stepPos + 1) % 30;
        ++stepsDone;

        auto meanOfLast = [this] (int n)
        {
            double s = 0;
            for (int k = 1; k <= n; ++k)
                s += stepEnergy[(size_t) ((stepPos - k + 30) % 30)];
            return s / n;
        };

        if (stepsDone >= 4)
        {
            const double m = meanOfLast (4);
            const auto ml = (float) energyToLufs (m);
            momentary.store (ml);
            if (ml > maxMomentary.load())
                maxMomentary.store (ml);
            integratedHist.add (m);
            if (energyToLufs (m) >= -70.0)
                measuredSeconds.store (measuredSeconds.load() + 0.1f);

            // Integrated loudness: absolute gate -70, relative gate -10 LU.
            const double absMean = integratedHist.meanEnergyAbove (-70.0);
            if (absMean > 0)
            {
                const double rel = energyToLufs (absMean) - 10.0;
                const double gated = integratedHist.meanEnergyAbove (std::max (-70.0, rel));
                integrated.store ((float) energyToLufs (gated));
            }
        }

        if (stepsDone >= 30)
        {
            const double s = meanOfLast (30);
            const auto sl = (float) energyToLufs (s);
            shortTerm.store (sl);
            if (sl > maxShortTerm.load())
                maxShortTerm.store (sl);
            shortTermHist.add (s);

            // LRA: absolute gate -70, relative gate -20 LU, 10th to 95th percentile.
            const double absMean = shortTermHist.meanEnergyAbove (-70.0);
            if (absMean > 0)
            {
                const double gate = std::max (-70.0, energyToLufs (absMean) - 20.0);
                const double lo = shortTermHist.percentile (gate, 0.10);
                const double hi = shortTermHist.percentile (gate, 0.95);
                lra.store ((float) std::max (0.0, hi - lo));
            }
        }
        else if (stepsDone >= 4)
        {
            // Until 3 s have passed, show the loudness of what we have so far.
            shortTerm.store ((float) energyToLufs (meanOfLast (stepsDone)));
        }

        const int w = historyWrite.load();
        history[(size_t) w].store (shortTerm.load());
        historyWrite.store ((w + 1) % historySize);
    }

    double fs = 48000.0;
    int stepLen = 4800;
    std::array<KWeighting, 2> kw;
    std::array<double, 30> stepEnergy {};
    int stepPos = 0;
    int64_t stepsDone = 0;
    double acc = 0;
    int accCount = 0;
    LoudnessHistogram integratedHist, shortTermHist;
};

// True peak after BS.1770-4 Annex 2: oversample (4x below 96 kHz, 2x below
// 192 kHz) with a windowed-sinc interpolator and take the largest absolute value.
class TruePeakMeter
{
public:
    void prepare (double fs)
    {
        factor = fs < 96000.0 ? 4 : (fs < 192000.0 ? 2 : 1);
        const int taps = tapsPerPhase * factor + 1;
        const double centre = (taps - 1) / 2.0;
        const double pi = 3.14159265358979323846;
        coeffs.assign ((size_t) (factor * (tapsPerPhase + 1)), 0.0f);
        for (int n = 0; n < taps; ++n)
        {
            const double t = (n - centre) / factor;
            const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
            const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (n + 0.5) / taps); // Hann
            const int phase = n % factor, k = n / factor;
            coeffs[(size_t) (phase * (tapsPerPhase + 1) + k)] = (float) (sinc * w);
        }
        // Normalise each phase to unity gain at DC.
        for (int p = 0; p < factor; ++p)
        {
            double s = 0;
            for (int k = 0; k <= tapsPerPhase; ++k)
                s += coeffs[(size_t) (p * (tapsPerPhase + 1) + k)];
            if (std::abs (s) > 1.0e-9)
                for (int k = 0; k <= tapsPerPhase; ++k)
                    coeffs[(size_t) (p * (tapsPerPhase + 1) + k)] = (float) (coeffs[(size_t) (p * (tapsPerPhase + 1) + k)] / s);
        }
        reset();
    }

    void reset()
    {
        for (auto& h : hist)
            h.fill (0.0f);
        pos = 0;
        maxTruePeak.store (0.0f);
        maxSamplePeak.store (0.0f);
        overs.store (0);
        clips.store (0);
        current = 0;
        currentPeak.store (0.0f);
    }

    void setCeiling (float linear) { ceiling = linear; }

    void process (const float* const* data, int numChannels, int numSamples) noexcept
    {
        numChannels = numChannels > 2 ? 2 : numChannels;
        float blockTp = 0, blockSp = 0;
        int newOvers = 0, newClips = 0;
        for (int i = 0; i < numSamples; ++i)
        {
            float frameTp = 0;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float x = data[ch][i];
                if (! std::isfinite (x))
                    x = 0.0f;
                auto& h = hist[(size_t) ch];
                h[(size_t) pos] = x;
                h[(size_t) (pos + histLen)] = x; // mirrored so the filter reads contiguously
                const float ax = std::abs (x);
                blockSp = std::max (blockSp, ax);
                if (ax >= 0.99999f)
                    ++newClips;
                float tp = ax;
                for (int p = 0; p < factor; ++p)
                {
                    const float* c = &coeffs[(size_t) (p * (tapsPerPhase + 1))];
                    float y = 0;
                    for (int k = 0; k <= tapsPerPhase; ++k)
                        y += c[k] * h[(size_t) (pos + histLen - k)];
                    tp = std::max (tp, std::abs (y));
                }
                frameTp = std::max (frameTp, tp);
            }
            if (frameTp > ceiling && ! aboveCeiling)
                ++newOvers;
            aboveCeiling = frameTp > ceiling;
            blockTp = std::max (blockTp, frameTp);
            pos = (pos + 1) % histLen;
        }
        if (blockTp > maxTruePeak.load())
            maxTruePeak.store (blockTp);
        if (blockSp > maxSamplePeak.load())
            maxSamplePeak.store (blockSp);
        if (newOvers > 0)
            overs.store (overs.load() + newOvers);
        if (newClips > 0)
            clips.store (clips.load() + newClips);

        // Display value: peak hold with a slow fall.
        current = std::max (blockTp, current * std::pow (0.5f, (float) numSamples / 24000.0f));
        currentPeak.store (current);
    }

    std::atomic<float> maxTruePeak { 0.0f }, maxSamplePeak { 0.0f }, currentPeak { 0.0f };
    std::atomic<int> overs { 0 }, clips { 0 };

private:
    static constexpr int tapsPerPhase = 12;
    static constexpr int histLen = tapsPerPhase + 1;
    int factor = 4;
    std::vector<float> coeffs;
    std::array<std::array<float, histLen * 2>, 2> hist {};
    int pos = 0;
    float ceiling = 0.891f;
    bool aboveCeiling = false;
    float current = 0;
};
} // namespace mcheck
