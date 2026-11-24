// Offline tests for fckrp check: measurement accuracy against EBU test signals,
// pass-through, advice on shaped noise, stereo checks, state, and a UI snapshot.
//   mastercheck_tests [--editor]

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cstdio>
#include <random>

using namespace mcheck;

static int failures = 0;
static void check (bool ok, const char* what, double value = 0.0)
{
    std::printf ("%s  %s (%.3f)\n", ok ? "PASS" : "FAIL", what, value);
    if (! ok)
        ++failures;
}

static constexpr double fs = 48000.0;

// Feeds a stereo signal in blocks of changing size.
template <typename Gen>
static void feed (CheckProcessor& p, double seconds, Gen gen)
{
    const int total = (int) (seconds * fs);
    const int sizes[] = { 512, 37, 4096, 1, 1024, 9000, 128 };
    juce::MidiBuffer midi;
    int done = 0, s = 0;
    while (done < total)
    {
        const int n = std::min (sizes[s++ % 7], total - done);
        juce::AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i)
        {
            float l, r;
            gen (done + i, l, r);
            b.setSample (0, i, l);
            b.setSample (1, i, r);
        }
        p.processBlock (b, midi);
        done += n;
    }
}

static auto sine (double freq, double dbfs, double phase = 0.0)
{
    const double a = std::pow (10.0, dbfs / 20.0);
    return [=] (int i, float& l, float& r)
    {
        l = r = (float) (a * std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / fs + phase));
    };
}

static void testLufsSine()
{
    CheckProcessor p;
    p.prepareToPlay (fs, 512);
    feed (p, 20.0, sine (997.0, -23.0));
    check (std::abs (p.loudness.integrated.load() + 23.0f) < 0.1f, "EBU 3341 #1: -23 dBFS sine reads -23 LUFS", p.loudness.integrated.load());
    check (std::abs (p.loudness.momentary.load() + 23.0f) < 0.1f, "momentary -23", p.loudness.momentary.load());
    check (std::abs (p.loudness.shortTerm.load() + 23.0f) < 0.1f, "short-term -23", p.loudness.shortTerm.load());

    CheckProcessor q;
    q.prepareToPlay (fs, 512);
    feed (q, 20.0, sine (997.0, -33.0));
    check (std::abs (q.loudness.integrated.load() + 33.0f) < 0.1f, "EBU 3341 #2: -33 dBFS sine reads -33 LUFS", q.loudness.integrated.load());
}

static void testGating()
{
    // EBU 3341 #4: -72, -36, -23, -36, -72 dBFS (10/10/20/10/10 s) -> -23 LUFS
    CheckProcessor p;
    p.prepareToPlay (fs, 512);
    const double levels[] = { -72, -36, -23, -36, -72 };
    const double lens[] = { 10, 10, 20, 10, 10 };
    for (int k = 0; k < 5; ++k)
        feed (p, lens[k], sine (997.0, levels[k]));
    check (std::abs (p.loudness.integrated.load() + 23.0f) < 0.1f, "EBU 3341 #4: gating ignores quiet parts", p.loudness.integrated.load());
}

static void testLra()
{
    // EBU 3342 #1: 20 s at -20, 20 s at -30 -> LRA 10 LU
    CheckProcessor p;
    p.prepareToPlay (fs, 512);
    feed (p, 20.0, sine (1000.0, -20.0));
    feed (p, 20.0, sine (1000.0, -30.0));
    check (std::abs (p.loudness.lra.load() - 10.0f) < 1.0f, "EBU 3342 #1: LRA 10 LU", p.loudness.lra.load());
}

static void testTruePeak()
{
    // Sine at fs/4 with 45 degree phase: samples hit only 0.707 of the real peak.
    CheckProcessor p;
    p.prepareToPlay (fs, 512);
    feed (p, 2.0, sine (12000.0, -0.5, juce::MathConstants<double>::pi / 4.0));
    const float tp = 20.0f * std::log10 (p.truePeak.maxTruePeak.load());
    const float sp = 20.0f * std::log10 (p.truePeak.maxSamplePeak.load());
    check (std::abs (tp + 0.5f) < 0.3f, "true peak finds the peak between samples (-0.5 dBTP)", tp);
    check (std::abs (sp + 3.51f) < 0.1f, "sample peak reads -3.5 dBFS", sp);
    check (p.truePeak.overs.load() > 0, "counts overs above -1 dBTP", p.truePeak.overs.load());

    CheckProcessor q;
    q.prepareToPlay (fs, 512);
    feed (q, 2.0, sine (1000.0, -6.0));
    const float tq = 20.0f * std::log10 (q.truePeak.maxTruePeak.load());
    check (std::abs (tq + 6.0f) < 0.15f, "1 kHz sine at -6 dBFS reads -6 dBTP", tq);
}

static void testPassThrough()
{
    CheckProcessor p;
    p.prepareToPlay (fs, 256);
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    juce::MidiBuffer midi;
    // With all fix modules off (default) the audio only gets the reported latency.
    const int lat = p.getLatencySamples();
    std::vector<float> inL, inR, outL, outR;
    for (int n : { 256, 3000, 1, 77, 16384 })
    {
        juce::AudioBuffer<float> b (2, n);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                b.setSample (ch, i, d (rng));
        inL.insert (inL.end(), b.getReadPointer (0), b.getReadPointer (0) + n);
        inR.insert (inR.end(), b.getReadPointer (1), b.getReadPointer (1) + n);
        p.processBlock (b, midi);
        outL.insert (outL.end(), b.getReadPointer (0), b.getReadPointer (0) + n);
        outR.insert (outR.end(), b.getReadPointer (1), b.getReadPointer (1) + n);
    }
    bool same = lat > 0 && lat < 200;
    for (size_t i = 0; i + (size_t) lat < outL.size(); ++i)
        same = same && outL[i + (size_t) lat] == inL[i] && outR[i + (size_t) lat] == inR[i];
    for (int i = 0; i < lat; ++i)
        same = same && outL[(size_t) i] == 0.0f;
    check (same, "modules off: audio passes bit for bit, only delayed by the reported latency", lat);

    juce::AudioBuffer<float> bad (2, 512);
    bad.clear();
    bad.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
    bad.setSample (1, 20, std::numeric_limits<float>::infinity());
    p.processBlock (bad, midi);
    feed (p, 5.0, sine (1000.0, -20.0));
    check (std::isfinite (p.loudness.integrated.load()) && p.loudness.integrated.load() > -30.0f,
           "NaN/Inf from the host doesn't break the meter", p.loudness.integrated.load());
}

// Noise shaped to a band curve (dB per 1/3 octave), with optional band changes.
struct ShapedNoise
{
    std::vector<float> mid, side;
    int len = 1 << 20;

    static std::vector<float> make (const std::array<float, numBands>& curve, std::function<float (double)> extraDb,
                                    double minFreq, unsigned seed)
    {
        const int order = 20, n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) n * 2, 0.0f);
        std::mt19937 rng (seed);
        std::uniform_real_distribution<double> ph (0.0, 2.0 * juce::MathConstants<double>::pi);
        const double binHz = fs / n;
        const double bw = std::pow (2.0, 1.0 / 6.0) - std::pow (2.0, -1.0 / 6.0);
        for (int k = 1; k < n / 2; ++k)
        {
            const double f = k * binHz;
            if (f < std::max (minFreq, 18.0) || f > 20000.0)
                continue;
            // Interpolate the curve in log frequency.
            double db;
            const double lf = std::log2 (f);
            if (f <= bandCentres[0])
                db = curve[0] - 12.0 * (std::log2 (bandCentres[0]) - lf);
            else if (f >= bandCentres[numBands - 1])
                db = curve[numBands - 1] - 12.0 * (lf - std::log2 (bandCentres[numBands - 1]));
            else
            {
                int b = 0;
                while (bandCentres[(size_t) b + 1] < f)
                    ++b;
                const double t = (lf - std::log2 (bandCentres[(size_t) b])) / (std::log2 (bandCentres[(size_t) b + 1]) - std::log2 (bandCentres[(size_t) b]));
                db = curve[(size_t) b] + t * (curve[(size_t) b + 1] - curve[(size_t) b]);
            }
            db += extraDb (f);
            const double density = std::pow (10.0, db / 10.0) / (f * bw);
            const double mag = std::sqrt (density);
            const double p = ph (rng);
            data[(size_t) (2 * k)] = (float) (mag * std::cos (p));
            data[(size_t) (2 * k + 1)] = (float) (mag * std::sin (p));
        }
        fft.performRealOnlyInverseTransform (data.data());
        std::vector<float> out (data.begin(), data.begin() + n);
        return out; // unnormalised, so mid and side keep the same spectral scale
    }
};

static float scaleTo (std::vector<float>& v, float rmsDb)
{
    double s = 0;
    for (float x : v)
        s += (double) x * x;
    const float g = (float) (std::pow (10.0, rmsDb / 20.0) / std::sqrt (s / (double) v.size()));
    for (auto& x : v)
        x *= g;
    return g;
}

static bool hasHint (const std::vector<Hint>& hints, const char* title, const char* textPart = nullptr)
{
    for (auto& h : hints)
        if (h.title.contains (u8 (title)) && (textPart == nullptr || h.text.contains (u8 (textPart))))
            return true;
    return false;
}

static void printHints (const std::vector<Hint>& hints)
{
    for (auto& h : hints)
        std::printf ("      [%d] %s: %s\n", h.level, h.title.toRawUTF8(), h.text.toRawUTF8());
}

static std::vector<Hint> runNoise (std::function<float (double)> extra, int curve = 0, bool stereoBass = false,
                                   CheckProcessor* keep = nullptr)
{
    auto mid = ShapedNoise::make (builtInCurves[(size_t) curve], extra, 0.0, 11);
    auto side = ShapedNoise::make (builtInCurves[(size_t) curve], [] (double) { return 0.0f; }, stereoBass ? 0.0 : 300.0, 22);
    // Side uses the same spectral scale as the mid, so it only adds a little width.
    const float g = scaleTo (mid, -14.0f) * (stereoBass ? 0.5f : 0.3f);
    for (auto& x : side)
        x *= g;
    CheckProcessor local;
    CheckProcessor& p = keep != nullptr ? *keep : local;
    p.prepareToPlay (fs, 512);
    p.setCurve (curve);
    const int n = (int) mid.size();
    feed (p, 21.0, [&] (int i, float& l, float& r)
    {
        const float m = mid[(size_t) (i % n)], s = side[(size_t) (i % n)];
        l = m + s;
        r = m - s;
    });
    return makeHints (p.getMeasurements(), p.getSettings());
}

static void testAdvice()
{
    auto flat = runNoise ([] (double) { return 0.0f; });
    bool spectralProblem = false;
    for (auto& h : flat)
        spectralProblem = spectralProblem || (h.title.contains ("Hz") && h.level > 0);
    check (! spectralProblem && hasHint (flat, "Klangbalance passt"), "noise matching the curve gives no EQ advice");
    check (! hasHint (flat, "Bass ist stereo") && hasHint (flat, "Stereo ok"), "mono bass with some width: stereo ok");
    if (spectralProblem)
        printHints (flat);

    auto harsh = runNoise ([] (double f) { return (f > 2000.0 && f < 5000.0) ? 5.0f : 0.0f; });
    check (hasHint (harsh, "Präsenz (2–5 kHz)", "zu laut"), "+5 dB at 2-5 kHz: says Präsenz is too loud");
    printHints (harsh);

    auto dull = runNoise ([] (double f) { return (f > 5000.0 && f < 10000.0) ? -5.0f : 0.0f; });
    check (hasHint (dull, "Höhen (5–10 kHz)", "zu leise"), "-5 dB at 5-10 kHz: says highs are too quiet");

    auto wideBass = runNoise ([] (double) { return 0.0f; }, 0, true);
    check (hasHint (wideBass, "Bass ist stereo"), "stereo bass is detected");

    auto ring = runNoise ([] (double f) { return std::abs (std::log2 (f / 3150.0)) < 0.02 ? 14.0f : 0.0f; });
    check (hasHint (ring, "Spitze bei 3,1 kHz") || hasHint (ring, "Spitze bei 3,2 kHz"), "narrow peak at 3.15 kHz is found");
}

static void testStereoAndLoudnessHints()
{
    CheckProcessor p;
    p.prepareToPlay (fs, 512);
    std::mt19937 rng (3);
    std::normal_distribution<float> d (0.0f, 0.1f);
    feed (p, 6.0, [&] (int, float& l, float& r) { l = d (rng); r = -l; });
    const auto m = p.getMeasurements();
    check (m.correlation < -0.9f, "antiphase reads correlation -1", m.correlation);
    check (hasHint (makeHints (m, p.getSettings()), "Phasenproblem"), "antiphase gives the phase warning");

    CheckProcessor q;
    q.prepareToPlay (fs, 512);
    feed (q, 10.0, sine (1000.0, -0.2));
    const auto hq = makeHints (q.getMeasurements(), q.getSettings());
    check (hasHint (hq, "True Peak zu hoch"), "peaks above -1 dBTP are flagged");
    check (hasHint (hq, "Lauter als dein Ziel"), "too loud for -14 target is flagged");
}

static void setP (CheckProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.params.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void testLimiter()
{
    // Loud shaped noise pushed 8 dB into the limiter: true peak must stay at the ceiling.
    auto mid = ShapedNoise::make (builtInCurves[0], [] (double) { return 0.0f; }, 0.0, 5);
    scaleTo (mid, -12.0f);
    CheckProcessor p;
    p.prepareToPlay (fs, 512);
    setP (p, ids::limOn, 1.0f);
    setP (p, ids::limGain, 8.0f);
    setP (p, ids::limCeiling, -1.0f);
    const int n = (int) mid.size();
    feed (p, 10.0, [&] (int i, float& l, float& r) { l = r = mid[(size_t) (i % n)]; });
    const auto m = p.getMeasurements();
    check (m.maxTruePeakDb <= -0.9f, "limiter keeps true peak at the -1 dBTP ceiling", m.maxTruePeakDb);
    check (m.integrated > -9.0f, "limiter makes it louder", m.integrated);

    // Sine bursts: check the limiter doesn't distort a clean tone much (THD-like check on level).
    CheckProcessor q;
    q.prepareToPlay (fs, 512);
    setP (q, ids::limOn, 1.0f);
    setP (q, ids::limGain, 3.0f);
    feed (q, 3.0, sine (100.0, -3.0));
    const auto mq = q.getMeasurements();
    check (mq.maxTruePeakDb <= -0.9f && mq.maxTruePeakDb > -1.5f, "sine is held right at the ceiling", mq.maxTruePeakDb);
}

static void testAutoLoudness()
{
    auto mid = ShapedNoise::make (builtInCurves[0], [] (double) { return 0.0f; }, 0.0, 7);
    scaleTo (mid, -20.0f);
    const int n = (int) mid.size();
    CheckProcessor p;
    p.prepareToPlay (fs, 512);
    p.setTarget (1); // -9 LUFS
    auto play = [&] { feed (p, 21.0, [&] (int i, float& l, float& r) { l = r = mid[(size_t) (i % n)]; }); };
    play();
    p.applyAutoLoudness();
    p.processBlock (*std::make_unique<juce::AudioBuffer<float>> (2, 16), *std::make_unique<juce::MidiBuffer>());
    play();
    p.applyAutoLoudness();
    p.processBlock (*std::make_unique<juce::AudioBuffer<float>> (2, 16), *std::make_unique<juce::MidiBuffer>());
    play();
    const auto m = p.getMeasurements();
    check (std::abs (m.integrated + 9.0f) < 1.0f, "Auto-Lautheit reaches the -9 LUFS target", m.integrated);
    check (m.maxTruePeakDb <= -0.9f, "and true peak stays under -1 dBTP", m.maxTruePeakDb);
}

static void testAutoEq()
{
    CheckProcessor p;
    runNoise ([] (double f) { return (f > 2000.0 && f < 5000.0) ? 5.0f : 0.0f; }, 0, false, &p);
    const float before = regionDeviations (analyseBands (p.getMeasurements().spectrum, p.getSettings()), fs)[4];
    p.applyAutoEq();
    const float gain = p.params.getRawParameterValue (ids::eqBand (4))->load();
    check (gain < -1.5f && gain > -6.01f, "Auto-EQ turns PRÄSENZ down", gain);
    runNoise ([] (double f) { return (f > 2000.0 && f < 5000.0) ? 5.0f : 0.0f; }, 0, false, &p);
    const float after = regionDeviations (analyseBands (p.getMeasurements().spectrum, p.getSettings()), fs)[4];
    check (after < before - 1.5f, "after Auto-EQ the presence region is closer to the curve", after);
}

static void testStereoTool()
{
    CheckProcessor p;
    p.prepareToPlay (fs, 512);
    setP (p, ids::stereoOn, 1.0f);
    setP (p, ids::monoBelow, 120.0f);
    // 50 Hz only on the left: after the tool, both sides carry it equally.
    feed (p, 6.0, [] (int i, float& l, float& r)
    {
        l = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 50.0 * i / fs);
        r = 0.0f;
    });
    const float bass = bassSideToMidDb (p.getMeasurements().spectrum);
    check (bass < -20.0f, "Mono unter 120 Hz makes the bass mono", bass);
}

static void testReferenceFile()
{
    // Write shaped noise (Pop curve) to a WAV, load it as reference.
    auto mid = ShapedNoise::make (builtInCurves[1], [] (double) { return 0.0f; }, 0.0, 9);
    scaleTo (mid, -11.0f);
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mcheck_ref_test.wav");
    file.deleteFile();
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (file), fs, 2, 24, {}, 0));
        juce::AudioBuffer<float> b (2, (int) mid.size());
        for (int i = 0; i < (int) mid.size(); ++i)
            b.setSample (0, i, mid[(size_t) i]), b.setSample (1, i, mid[(size_t) i]);
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    CheckProcessor p;
    const auto err = p.analyseReferenceFile (file);
    const auto s = p.getSettings();
    check (err.isEmpty() && s.reference.valid && s.curve == 2 && s.reference.name == "mcheck_ref_test",
           "dropped WAV becomes the reference");
    // Mid noise ~ -11 dB RMS; just check it's a plausible loud value.
    check (s.reference.lufs > -16.0f && s.reference.lufs < -6.0f, "reference loudness measured", s.reference.lufs);
    p.setTarget (3);
    check (std::abs (targetFor (p.getSettings()).lufs - s.reference.lufs) < 0.01f, "target 'Wie Referenz' uses its LUFS");

    // Same material measured against it: no EQ advice.
    auto h = runNoise ([] (double) { return 0.0f; }, 1, false, &p);
    p.setCurve (2);
    h = makeHints (p.getMeasurements(), p.getSettings());
    check (hasHint (h, "Klangbalance passt"), "song matching the dropped reference gets no EQ advice");
    file.deleteFile();

    check (p.analyseReferenceFile (juce::File ("/nonexistent.wav")).isNotEmpty(), "bad file gives an error, no crash");
}

static void testState()
{
    CheckProcessor p;
    p.setTarget (1);
    // Learn a reference from shaped noise.
    p.startLearning();
    runNoise ([] (double) { return 0.0f; }, 1, false, &p);
    p.startLearning(); // runNoise resets via prepare? keep learning flag set
    runNoise ([] (double) { return 0.0f; }, 1, false, &p);
    p.finishLearning();
    const auto s = p.getSettings();
    check (s.reference.valid && s.curve == 2, "learning a reference switches to it");
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    CheckProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    const auto t = q.getSettings();
    check (t.target == 1 && t.curve == 2 && t.reference.valid && std::abs (t.reference.lufs - s.reference.lufs) < 0.05f,
           "settings and reference survive save/load", t.reference.lufs);

    // Measuring against your own reference: the same material shows no EQ advice.
    auto self = runNoise ([] (double) { return 0.0f; }, 1, false, &q);
    q.setCurve (2);
    const auto h = makeHints (q.getMeasurements(), q.getSettings());
    check (hasHint (h, "Klangbalance passt"), "the reference itself matches the learned curve");
    if (! hasHint (h, "Klangbalance passt"))
        printHints (h);
}

static void testEditor()
{
    CheckProcessor p;
    auto hints = runNoise ([] (double f) { return (f > 2000.0 && f < 5000.0) ? 4.0f : 0.0f; }, 0, true, &p);
    setP (p, ids::eqOn, 1.0f);
    setP (p, ids::eqBand (4), -2.0f);
    setP (p, ids::limOn, 1.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    if (auto* ce = dynamic_cast<CheckEditor*> (ed.get()))
        ce->refresh();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile ("mastercheck_ui.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    check (png.writeImageToStream (img, os), "editor renders a snapshot");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool withEditor = argc > 1 && juce::String (argv[1]) == "--editor";
    testLufsSine();
    testGating();
    testLra();
    testTruePeak();
    testPassThrough();
    testAdvice();
    testStereoAndLoudnessHints();
    testLimiter();
    testAutoLoudness();
    testAutoEq();
    testStereoTool();
    testReferenceFile();
    testState();
    if (withEditor)
        testEditor();
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
