#pragma once

// The master "fix" chain: EQ -> glue compressor -> stereo (bass mono, width) -> limiter.
// Every module can be switched off; with all modules off the audio passes through
// unchanged (only delayed by the limiter look-ahead, which the host compensates).

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mcheck
{
constexpr int numEqBands = 7;
// One band per advice region: Sub, Bass, Low mids, Mids, Presence, Highs, Air.
constexpr std::array<float, numEqBands> eqFreqs { 60.0f, 120.0f, 350.0f, 1000.0f, 3200.0f, 7000.0f, 12000.0f };
constexpr std::array<const char*, numEqBands> eqNames { "SUB", "BASS", "U-MITTE", "MITTE", u8"PRÄSENZ", u8"HÖHEN", "AIR" };

class MasterEq
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& ch : state)
            for (auto& s : ch)
                s = {};
        gains.fill (1000.0f); // force recalculation
    }

    void setGains (const std::array<float, numEqBands>& g)
    {
        for (int b = 0; b < numEqBands; ++b)
            if (std::abs (g[(size_t) b] - gains[(size_t) b]) > 1.0e-4f)
            {
                gains[(size_t) b] = g[(size_t) b];
                design (b);
            }
    }

    void process (float* const* data, int numChannels, int n) noexcept
    {
        for (int b = 0; b < numEqBands; ++b)
        {
            if (std::abs (gains[(size_t) b]) < 0.01f)
                continue;
            const auto& c = coeffs[(size_t) b];
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& s = state[(size_t) ch][(size_t) b];
                float* x = data[ch];
                for (int i = 0; i < n; ++i)
                {
                    const double in = x[i];
                    const double y = c[0] * in + s.z1;
                    s.z1 = c[1] * in - c[3] * y + s.z2;
                    s.z2 = c[2] * in - c[4] * y;
                    x[i] = (float) y;
                }
            }
        }
    }

private:
    // RBJ cookbook shelves and peaks, normalised as {b0, b1, b2, a1, a2}.
    void design (int b)
    {
        const double pi = 3.14159265358979323846;
        const double f = std::min ((double) eqFreqs[(size_t) b], fs * 0.45);
        const double A = std::pow (10.0, gains[(size_t) b] / 40.0);
        const double w = 2.0 * pi * f / fs, cw = std::cos (w), sw = std::sin (w);
        double b0, b1, b2, a0, a1, a2;
        if (b == 0 || b == numEqBands - 1)
        {
            const double S = 0.8;
            const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
            const double k = 2.0 * std::sqrt (A) * alpha;
            if (b == 0) // low shelf
            {
                b0 = A * ((A + 1) - (A - 1) * cw + k);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - k);
                a0 = (A + 1) + (A - 1) * cw + k;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - k;
            }
            else // high shelf
            {
                b0 = A * ((A + 1) + (A - 1) * cw + k);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - k);
                a0 = (A + 1) - (A - 1) * cw + k;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - k;
            }
        }
        else
        {
            const double Q = 0.9; // broad, musical
            const double alpha = sw / (2.0 * Q);
            b0 = 1 + alpha * A;
            b1 = -2 * cw;
            b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;
            a1 = -2 * cw;
            a2 = 1 - alpha / A;
        }
        coeffs[(size_t) b] = { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    struct State { double z1 = 0, z2 = 0; };
    double fs = 48000.0;
    std::array<float, numEqBands> gains {};
    std::array<std::array<double, 5>, numEqBands> coeffs {};
    std::array<std::array<State, numEqBands>, 2> state {};
};

// Stereo-linked RMS compressor with soft knee, for gentle "glue" (2:1, 1-3 dB).
class GlueCompressor
{
public:
    void prepare (double sampleRate) { fs = sampleRate; env = 0; peakEnv = 0; gr.store (0.0f); }

    // peak_: 0 = pure RMS detection (smooth, inaudible), 1 = pure peak detection.
    void setParams (float thresholdDb, float ratio_, float attackMs, float releaseMs, float makeupDb, float mix_,
                    float peak_ = 0.2f)
    {
        peakBlend = juce::jlimit (0.0f, 1.0f, peak_);
        peakAtt = (float) std::exp (-1.0 / (0.001 * fs));
        threshold = thresholdDb;
        ratio = std::max (1.0f, ratio_);
        att = (float) std::exp (-1.0 / (std::max (0.1f, attackMs) * 0.001 * fs));
        rel = (float) std::exp (-1.0 / (std::max (5.0f, releaseMs) * 0.001 * fs));
        makeup = std::pow (10.0f, makeupDb / 20.0f);
        mix = juce::jlimit (0.0f, 1.0f, mix_);
    }

    void process (float* const* data, int numChannels, int n) noexcept
    {
        const float knee = 6.0f;
        float maxGr = 0;
        for (int i = 0; i < n; ++i)
        {
            float p = 0;
            for (int ch = 0; ch < numChannels; ++ch)
                p = std::max (p, data[ch][i] * data[ch][i]);
            // RMS-like detector: smooth the power with attack/release.
            env = p > env ? att * env + (1 - att) * p : rel * env + (1 - rel) * p;
            // Peak detector: fast attack, same release.
            peakEnv = p > peakEnv ? peakAtt * peakEnv + (1 - peakAtt) * p : rel * peakEnv + (1 - rel) * p;
            const float rmsDb = env > 1.0e-12f ? 10.0f * std::log10 (env) + 3.0f : -120.0f; // +3: sine RMS -> peak scale
            const float pkDb = peakEnv > 1.0e-12f ? 10.0f * std::log10 (peakEnv) : -120.0f;
            const float lvl = rmsDb + peakBlend * (pkDb - rmsDb);
            const float over = lvl - threshold;
            float reduce = 0;
            if (over > knee * 0.5f)
                reduce = over * (1.0f - 1.0f / ratio);
            else if (over > -knee * 0.5f)
            {
                const float t = over + knee * 0.5f;
                reduce = (1.0f - 1.0f / ratio) * t * t / (2.0f * knee);
            }
            maxGr = std::max (maxGr, reduce);
            const float g = std::pow (10.0f, -reduce / 20.0f) * makeup;
            const float wet = mix * g + (1.0f - mix);
            for (int ch = 0; ch < numChannels; ++ch)
                data[ch][i] *= wet;
        }
        gr.store (maxGr);
    }

    std::atomic<float> gr { 0.0f };

private:
    double fs = 48000.0;
    float threshold = -18, ratio = 2, att = 0.99f, rel = 0.999f, makeup = 1, mix = 1;
    float peakBlend = 0.2f, peakAtt = 0.98f;
    float env = 0, peakEnv = 0;
};

// Makes the low end mono below a crossover (side signal high-passed with a
// 4th-order Linkwitz-Riley, so the sum stays flat) and sets the overall width.
class StereoTool
{
public:
    void prepare (double sampleRate) { fs = sampleRate; freq = -1; for (auto& s : hp) s = {}; }

    // sideMidDb: bell at 2.5 kHz on the side only (wider mids without changing the mono balance).
    // sideAirDb: high shelf at 10 kHz on the side only.
    void setSideEq (float sideMidDb, float sideAirDb)
    {
        if (std::abs (sideMidDb - sideMid) > 1.0e-3f)
        {
            sideMid = sideMidDb;
            midBell = bell (2500.0, sideMid, 0.7);
        }
        if (std::abs (sideAirDb - sideAir) > 1.0e-3f)
        {
            sideAir = sideAirDb;
            airShelf = highShelf (10000.0, sideAir);
        }
    }

    void setParams (float monoBelowHz, float width_)
    {
        width = juce::jlimit (0.0f, 2.0f, width_);
        if (std::abs (monoBelowHz - freq) > 0.01f)
        {
            freq = monoBelowHz;
            const double pi = 3.14159265358979323846;
            const double w = std::tan (pi * std::min ((double) freq, fs * 0.45) / fs);
            const double q = 0.7071067811865476; // two Butterworth stages = LR4
            const double norm = 1.0 / (1.0 + w / q + w * w);
            c = { norm, -2.0 * norm, norm, 2.0 * (w * w - 1.0) * norm, (1.0 - w / q + w * w) * norm };
        }
    }

    void process (float* const* data, int numChannels, int n) noexcept
    {
        if (numChannels < 2)
            return;
        for (int i = 0; i < n; ++i)
        {
            const double l = data[0][i], r = data[1][i];
            const double m = 0.5 * (l + r);
            double s = 0.5 * (l - r);
            if (freq > 1.0f)
                for (auto& st : hp)
                {
                    const double y = c[0] * s + st.z1;
                    st.z1 = c[1] * s - c[3] * y + st.z2;
                    st.z2 = c[2] * s - c[4] * y;
                    s = y;
                }
            if (std::abs (sideMid) > 0.01f)
                s = run (midBell, midState, s);
            if (std::abs (sideAir) > 0.01f)
                s = run (airShelf, airState, s);
            s *= width;
            data[0][i] = (float) (m + s);
            data[1][i] = (float) (m - s);
        }
    }

private:
    struct State { double z1 = 0, z2 = 0; };
    using Coeffs = std::array<double, 5>;

    static double run (const Coeffs& c, State& st, double x)
    {
        const double y = c[0] * x + st.z1;
        st.z1 = c[1] * x - c[3] * y + st.z2;
        st.z2 = c[2] * x - c[4] * y;
        return y;
    }

    Coeffs bell (double f, double db, double q) const
    {
        const double pi = 3.14159265358979323846;
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * std::min (f, fs * 0.45) / fs;
        const double alpha = std::sin (w) / (2.0 * q), cw = std::cos (w);
        const double a0 = 1 + alpha / A;
        return { (1 + alpha * A) / a0, -2 * cw / a0, (1 - alpha * A) / a0, -2 * cw / a0, (1 - alpha / A) / a0 };
    }

    Coeffs highShelf (double f, double db) const
    {
        const double pi = 3.14159265358979323846;
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * std::min (f, fs * 0.45) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / 2.0 * std::sqrt (2.0), k = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1) - (A - 1) * cw + k;
        return { A * ((A + 1) + (A - 1) * cw + k) / a0, -2 * A * ((A - 1) + (A + 1) * cw) / a0,
                 A * ((A + 1) + (A - 1) * cw - k) / a0, 2 * ((A - 1) - (A + 1) * cw) / a0,
                 ((A + 1) - (A - 1) * cw - k) / a0 };
    }

    double fs = 48000.0;
    float freq = -1, width = 1, sideMid = 1000, sideAir = 1000;
    Coeffs c {}, midBell { 1, 0, 0, 0, 0 }, airShelf { 1, 0, 0, 0, 0 };
    std::array<State, 2> hp {};
    State midState, airState;
};

// "Density": an upward shaping curve (own design, inflator-style). Quiet parts are
// lifted by up to ~3.5 dB while full-scale peaks stay where they are, so the master
// gets louder and denser without the limiter working harder.
class DensityShaper
{
public:
    void setParams (float amount_, float driveDb)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount_);
        drive = std::pow (10.0f, driveDb / 20.0f);
    }

    static float curve (float x)
    {
        const float a = std::abs (x);
        if (a >= 1.0f)
            return x; // above full scale: leave it to the limiter
        const float y = 1.5f * a - 0.5f * a * a * a;
        return x < 0 ? -y : y;
    }

    void process (float* const* data, int numChannels, int n) noexcept
    {
        if (amount <= 0.0f)
            return;
        const float inv = 1.0f / drive;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float x = data[ch][i];
                const float shaped = curve (x * drive) * inv;
                data[ch][i] = x + amount * (shaped - x);
            }
    }

private:
    float amount = 0.5f, drive = 1.0f;
};

// Look-ahead limiter with true-peak detection (4x interpolated). The gain is the
// running minimum of the needed gain over the look-ahead window, then a box filter
// of the same length, so it has fully reached its value when the peak arrives.
class TruePeakLimiter
{
public:
    static constexpr double lookaheadMs = 1.5;

    void prepare (double sampleRate, int maxChannels = 2)
    {
        fs = sampleRate;
        look = std::max (8, (int) std::lround (fs * lookaheadMs * 0.001));
        delayLen = look + 1 + interpDelay;
        delay.assign ((size_t) maxChannels, std::vector<float> ((size_t) delayLen, 0.0f));
        for (auto& gp : paths)
            gp.prepare (look);
        pos = 0;
        t = 0;
        interp.prepare (fs);
        gr.store (0.0f);
    }

    int latency() const { return delayLen; }

    // link: 1 = both channels get the same gain, 0 = fully independent.
    void setParams (float driveDb, float ceilingDb, float releaseMs, float link_ = 0.7f)
    {
        link = juce::jlimit (0.0f, 1.0f, link_);
        drive = std::pow (10.0f, driveDb / 20.0f);
        ceiling = std::pow (10.0f, ceilingDb / 20.0f);
        relCoeff = (float) (1.0 - std::exp (-1.0 / (std::max (10.0f, releaseMs) * 0.001 * fs)));
    }

    // active=false: only the delay is applied (keeps latency constant).
    void process (float* const* data, int numChannels, int n, bool active) noexcept
    {
        float minGain = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float own[2] = { 1.0f, 1.0f };
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float pk = interp.push (ch, data[ch][i] * (active ? drive : 1.0f));
                own[ch] = pk > ceiling ? ceiling / pk : 1.0f;
            }
            const float linked = std::min (own[0], numChannels > 1 ? own[1] : own[0]);
            float g[2];
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float need = linked + (1.0f - link) * (own[ch] - linked);
                g[ch] = paths[(size_t) ch].push (need, t, look, relCoeff);
                minGain = std::min (minGain, g[ch]);
            }
            ++t;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& d = delay[(size_t) ch];
                const float in = data[ch][i] * (active ? drive : 1.0f);
                const float out = d[(size_t) pos];
                d[(size_t) pos] = in;
                // Safety clip catches the tiny rest the smooth gain can leave.
                float y = active ? out * g[ch] : out;
                if (active)
                    y = juce::jlimit (-ceiling, ceiling, y);
                data[ch][i] = y;
            }
            pos = (pos + 1) % delayLen;
        }
        // Timing: the interpolator reports a peak about 6 samples late, so the audio is
        // delayed by look + 1 + 6 and the minimum window is slightly longer than look.
        gr.store (active ? -20.0f * std::log10 (std::max (1.0e-6f, minGain)) : 0.0f);
    }

    std::atomic<float> gr { 0.0f };

private:
    // Gain computer for one channel: sliding minimum over the look-ahead window
    // (monotonic queue), instant attack / smooth release, then a box filter.
    struct GainPath
    {
        std::vector<float> box, dqVal;
        std::vector<int64_t> dqIdx;
        int64_t dqHead = 0, dqTail = 0;
        double boxSum = 0;
        int boxPos = 0;
        float released = 1.0f;

        void prepare (int look)
        {
            box.assign ((size_t) look, 1.0f);
            dqVal.assign ((size_t) look + 8, 1.0f);
            dqIdx.assign ((size_t) look + 8, 0);
            dqHead = dqTail = 0;
            boxSum = look;
            boxPos = 0;
            released = 1.0f;
        }

        float push (float need, int64_t t, int look, float relCoeff) noexcept
        {
            const int qs = (int) dqVal.size();
            while (dqTail > dqHead && dqVal[(size_t) ((dqTail - 1) % qs)] >= need)
                --dqTail;
            dqVal[(size_t) (dqTail % qs)] = need;
            dqIdx[(size_t) (dqTail % qs)] = t;
            ++dqTail;
            while (dqIdx[(size_t) (dqHead % qs)] <= t - (look + 3))
                ++dqHead;
            const float held = dqVal[(size_t) (dqHead % qs)];
            released = held < released ? held : released + (held - released) * relCoeff;
            boxSum += released - box[(size_t) boxPos];
            box[(size_t) boxPos] = released;
            boxPos = (boxPos + 1) % look;
            return (float) (boxSum / look);
        }
    };

    // 4x windowed-sinc interpolation, like the true-peak meter.
    struct Interp
    {
        static constexpr int taps = 12;
        int factor = 4;
        std::vector<float> c;
        std::array<std::array<float, (taps + 1) * 2>, 2> h {};
        std::array<int, 2> p {};

        void prepare (double fs)
        {
            factor = fs < 96000.0 ? 4 : (fs < 192000.0 ? 2 : 1);
            const int n = taps * factor + 1;
            const double centre = (n - 1) / 2.0, pi = 3.14159265358979323846;
            c.assign ((size_t) (factor * (taps + 1)), 0.0f);
            for (int k = 0; k < n; ++k)
            {
                const double tt = (k - centre) / factor;
                const double s = std::abs (tt) < 1.0e-12 ? 1.0 : std::sin (pi * tt) / (pi * tt);
                const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (k + 0.5) / n);
                c[(size_t) ((k % factor) * (taps + 1) + k / factor)] = (float) (s * w);
            }
            for (int ph = 0; ph < factor; ++ph)
            {
                double s = 0;
                for (int k = 0; k <= taps; ++k)
                    s += c[(size_t) (ph * (taps + 1) + k)];
                for (int k = 0; k <= taps; ++k)
                    c[(size_t) (ph * (taps + 1) + k)] = (float) (c[(size_t) (ph * (taps + 1) + k)] / s);
            }
            for (auto& x : h)
                x.fill (0.0f);
            p = { 0, 0 };
        }

        float push (int ch, float x)
        {
            auto& hh = h[(size_t) ch];
            int& q = p[(size_t) ch];
            hh[(size_t) q] = x;
            hh[(size_t) (q + taps + 1)] = x;
            float m = std::abs (x);
            for (int ph = 0; ph < factor; ++ph)
            {
                const float* cc = &c[(size_t) (ph * (taps + 1))];
                float y = 0;
                for (int k = 0; k <= taps; ++k)
                    y += cc[k] * hh[(size_t) (q + taps + 1 - k)];
                m = std::max (m, std::abs (y));
            }
            q = (q + 1) % (taps + 1);
            return m;
        }
    };

    double fs = 48000.0;
    static constexpr int interpDelay = 6;
    int look = 72, delayLen = 79;
    std::vector<std::vector<float>> delay;
    std::array<GainPath, 2> paths;
    int64_t t = 0;
    int pos = 0;
    float drive = 1, ceiling = 0.891f, relCoeff = 0.001f, link = 0.7f;
    Interp interp;
};
} // namespace mcheck
