// Offline checks for the fckrp audio engine. Exit code 0 = all passed.

#include "PluginProcessor.h"
#include "Presets.h"

#include <cstdio>
#include <limits>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

// A rough vocal stand-in: a buzzy tone with vibrato, phrase gaps,
// "s" noise bursts and a little background hiss.
void fillVocalLike (juce::AudioBuffer<float>& b, double fs, int64_t& t, juce::Random& rng)
{
    for (int i = 0; i < b.getNumSamples(); ++i, ++t)
    {
        const double sec = (double) t / fs;
        const double phrase = std::fmod (sec, 2.0);
        float v = 0.0f;
        if (phrase < 1.4)
        {
            const double f = 220.0 * (1.0 + 0.01 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * sec));
            double s = 0.0;
            for (int h = 1; h <= 12; ++h)
                s += std::sin (2.0 * juce::MathConstants<double>::pi * f * h * sec) / h;
            const float env = (float) std::min (1.0, phrase * 20.0) * (0.3f + 0.2f * (float) std::sin (sec * 7.0));
            v = (float) s * env * 0.5f;
            if (phrase > 0.9 && phrase < 1.0)
                v += (rng.nextFloat() * 2.0f - 1.0f) * 0.3f;
        }
        v += (rng.nextFloat() * 2.0f - 1.0f) * 0.001f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            b.setSample (ch, i, v);
    }
}

struct RunResult
{
    bool finite = true;
    float peak = 0.0f;
    double rms = 0.0;
};

RunResult run (FckrpProcessor& p, double fs, int blockSize, double seconds, bool silenceInput = false)
{
    const int numCh = p.getTotalNumOutputChannels();
    juce::AudioBuffer<float> buf (numCh, blockSize);
    juce::MidiBuffer midi;
    juce::Random rng (42);
    int64_t t = 0;
    RunResult r;
    double sumSq = 0.0;
    int64_t count = 0;
    const int64_t total = (int64_t) (seconds * fs);

    for (int64_t done = 0; done < total; done += blockSize)
    {
        if (silenceInput)
            buf.clear();
        else
            fillVocalLike (buf, fs, t, rng);

        p.processBlock (buf, midi);

        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = buf.getSample (ch, i);
                if (! std::isfinite (x)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (x));
                sumSq += (double) x * x;
                ++count;
            }
    }
    r.rms = std::sqrt (sumSq / (double) std::max<int64_t> (1, count));
    return r;
}

void setParam (FckrpProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void testPresets()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 512, 37 };

    for (int pi = 0; pi < (int) fckrp::getFactoryPresets().size(); ++pi)
        for (double fs : rates)
            for (int bs : blocks)
            {
                FckrpProcessor p;
                p.setPlayConfigDetails (2, 2, fs, bs);
                p.prepareToPlay (fs, bs);
                p.setCurrentProgram (pi);
                const auto r = run (p, fs, bs, 4.0);
                const auto name = p.getProgramName (pi) + " @ " + juce::String ((int) fs) + " Hz, block " + juce::String (bs);
                check (r.finite, name + ": no NaN/Inf");
                check (r.peak < 4.0f, name + ": stays bounded (peak " + juce::String (r.peak, 2) + ")");
                check (r.rms > 0.005, name + ": produces sound (rms " + juce::String (r.rms, 4) + ")");
            }
}

void testCompressor()
{
    using namespace fckrp::ids;
    const double fs = 48000.0;
    const int bs = 256;

    FckrpProcessor p;
    p.setPlayConfigDetails (2, 2, fs, bs);
    p.prepareToPlay (fs, bs);
    for (auto id : { gateOn, eqOn, dsOn, satOn, dlyOn, revOn })
        setParam (p, id, 0.0f);
    setParam (p, polish, 0.0f);
    setParam (p, space, 0.0f);
    setParam (p, compThresh, -20.0f);
    setParam (p, compRatio, 4.0f);
    setParam (p, compMakeup, 0.0f);

    auto feedSine = [&] (float amp, double seconds)
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        float lastPeak = 0.0f;
        static double phase = 0.0;
        for (int64_t n = 0; n < (int64_t) (seconds * fs); n += bs)
        {
            for (int i = 0; i < bs; ++i)
            {
                const float v = amp * (float) std::sin (phase);
                phase += 2.0 * juce::MathConstants<double>::pi * 1000.0 / fs;
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            p.processBlock (buf, midi);
            lastPeak = buf.getMagnitude (0, bs);
        }
        return lastPeak;
    };

    // -6 dBFS sine, 14 dB over threshold at 4:1 -> roughly 10 dB of reduction.
    const float outLoud = feedSine (0.5f, 2.0);
    const float gr = p.compGainReductionDb.load();
    check (gr > 6.0f && gr < 14.0f, "Opto: -6 dB sine gives ~10 dB reduction (got " + juce::String (gr, 1) + " dB)");
    check (outLoud < 0.5f, "Opto: output is quieter than input");

    // Opto release: right after the signal drops, part of the reduction is
    // still held (slow tail).
    feedSine (0.01f, 0.1);
    const float held = p.compGainReductionDb.load();
    check (held > 0.5f, "Opto: slow release tail after loud passage (" + juce::String (held, 1) + " dB still held)");

    feedSine (0.01f, 8.0);
    check (p.compGainReductionDb.load() < 0.5f, "Opto: fully recovers after quiet passage");
}

void testDeEsser()
{
    using namespace fckrp::ids;
    const double fs = 48000.0;
    const int bs = 256;
    FckrpProcessor p;
    p.setPlayConfigDetails (2, 2, fs, bs);
    p.prepareToPlay (fs, bs);
    for (auto id : { gateOn, eqOn, compOn, satOn, dlyOn, revOn })
        setParam (p, id, 0.0f);
    setParam (p, polish, 0.0f);
    setParam (p, dsThresh, -30.0f);

    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    double phase = 0.0;
    float maxGr = 0.0f;
    for (int n = 0; n < (int) fs; n += bs)
    {
        for (int i = 0; i < bs; ++i)
        {
            const float v = 0.3f * (float) std::sin (phase);
            phase += 2.0 * juce::MathConstants<double>::pi * 8000.0 / fs;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
        maxGr = std::max (maxGr, p.deEssGainReductionDb.load());
    }
    check (maxGr > 3.0f, "De-esser: loud 8 kHz tone is reduced (" + juce::String (maxGr, 1) + " dB)");
}

void testSilenceTail()
{
    const double fs = 48000.0;
    const int bs = 512;
    FckrpProcessor p;
    p.setPlayConfigDetails (2, 2, fs, bs);
    p.prepareToPlay (fs, bs);
    p.setCurrentProgram (5); // Big Hook: most delay and reverb
    run (p, fs, bs, 3.0);
    const auto r = run (p, fs, bs, 10.0, true);
    check (r.finite, "Tail: no NaN/Inf in silence");
    // Measure only the last second.
    const auto last = run (p, fs, bs, 1.0, true);
    check (last.peak < 0.002f, "Tail: rings out to near silence (peak " + juce::String (last.peak, 5) + ")");
}

void testMono()
{
    const double fs = 44100.0;
    const int bs = 256;
    FckrpProcessor p;
    juce::AudioProcessor::BusesLayout mono;
    mono.inputBuses.add (juce::AudioChannelSet::mono());
    mono.outputBuses.add (juce::AudioChannelSet::mono());
    check (p.setBusesLayout (mono), "Mono: layout accepted");
    p.setPlayConfigDetails (1, 1, fs, bs);
    p.prepareToPlay (fs, bs);
    p.setCurrentProgram (1);
    const auto r = run (p, fs, bs, 3.0);
    check (r.finite && r.peak < 4.0f && r.rms > 0.005, "Mono: processes cleanly");
}

void testState()
{
    using namespace fckrp::ids;
    FckrpProcessor a;
    a.setCurrentProgram (3);
    setParam (a, compRatio, 7.5f);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    FckrpProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    const float ratio = b.apvts.getRawParameterValue (compRatio)->load();
    check (std::abs (ratio - 7.5f) < 0.01f, "State: settings survive save and reload");
    check (b.getCurrentProgram() == 3, "State: preset choice survives save and reload");
}

void testEditor()
{
    FckrpProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    check (ed != nullptr && ed->getWidth() > 0, "Editor: opens");

    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile ("fckrp_ui.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("      UI snapshot: %s\n", out.getFullPathName().toRawUTF8());
}

// Hosts like FL Studio may send blocks larger than the size given to
// prepareToPlay, and change the size from block to block.
void testOversizedAndVariableBlocks()
{
    const double fs = 44100.0;
    FckrpProcessor p;
    p.setPlayConfigDetails (2, 2, fs, 64);
    p.prepareToPlay (fs, 64);
    p.setCurrentProgram (5); // Big Hook: doubler, width, delay and hall all on

    juce::MidiBuffer midi;
    juce::Random rng (7);
    int64_t t = 0;
    bool finite = true;
    float peak = 0.0f;
    const int sizes[] = { 64, 1024, 4096, 1, 333, 2048, 8192, 17, 512 };
    for (int rep = 0; rep < 20; ++rep)
        for (int n : sizes)
        {
            juce::AudioBuffer<float> buf (2, n);
            fillVocalLike (buf, fs, t, rng);
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    const float x = buf.getSample (ch, i);
                    finite = finite && std::isfinite (x);
                    peak = std::max (peak, std::abs (x));
                }
        }
    check (finite, "Block sizes: no NaN/Inf with blocks larger than prepared");
    check (peak < 1.5f, "Block sizes: stays at normal level (peak " + juce::String (peak, 2) + ")");
}

void testUnprepared()
{
    FckrpProcessor p;
    juce::AudioBuffer<float> buf (2, 256);
    buf.applyGain (0.0f);
    for (int i = 0; i < 256; ++i) { buf.setSample (0, i, 0.5f); buf.setSample (1, i, 0.5f); }
    juce::MidiBuffer midi;
    p.processBlock (buf, midi);
    check (buf.getMagnitude (0, 256) == 0.0f, "Unprepared: outputs silence before prepareToPlay");
}

// A NaN or Inf coming in must not get stuck in the filters and reverb.
void testBadInputRecovers()
{
    const double fs = 48000.0;
    const int bs = 256;
    FckrpProcessor p;
    p.setPlayConfigDetails (2, 2, fs, bs);
    p.prepareToPlay (fs, bs);
    p.setCurrentProgram (5);
    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    buf.clear();
    buf.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
    buf.setSample (1, 20, std::numeric_limits<float>::infinity());
    p.processBlock (buf, midi);
    const auto r = run (p, fs, bs, 2.0);
    check (r.finite && r.peak < 4.0f, "Bad input: recovers after NaN/Inf input");
}

void allModulesOff (FckrpProcessor& p)
{
    using namespace fckrp::ids;
    for (auto id : { gateOn, eqOn, tameOn, dsOn, compOn, satOn, dblOn, dlyOn, revOn })
        setParam (p, id, 0.0f);
    setParam (p, polish, 0.0f);
    setParam (p, space, 0.0f);
}

// Stereo test signal: different content left and right.
void fillStereo (juce::AudioBuffer<float>& b, int64_t& t, juce::Random& rng)
{
    for (int i = 0; i < b.getNumSamples(); ++i, ++t)
    {
        const float a = 0.3f * std::sin ((float) t * 0.031f) + (rng.nextFloat() - 0.5f) * 0.1f;
        const float c = 0.3f * std::sin ((float) t * 0.047f);
        b.setSample (0, i, a);
        b.setSample (1, i, c);
    }
}

void testDoublerMonoSafe()
{
    using namespace fckrp::ids;
    const double fs = 48000.0;
    const int bs = 256;
    FckrpProcessor withDbl, without;
    for (auto* p : { &withDbl, &without })
    {
        p->setPlayConfigDetails (2, 2, fs, bs);
        p->prepareToPlay (fs, bs);
        allModulesOff (*p);
    }
    setParam (withDbl, dblOn, 1.0f);
    setParam (withDbl, dblAmount, 1.0f);
    setParam (withDbl, dblDetune, 1.0f);

    juce::AudioBuffer<float> a (2, bs), b (2, bs);
    juce::MidiBuffer midi;
    juce::Random r1 (3), r2 (3);
    int64_t t1 = 0, t2 = 0;
    float maxMonoDiff = 0.0f, maxSide = 0.0f;
    for (int blk = 0; blk < 400; ++blk)
    {
        fillStereo (a, t1, r1);
        fillStereo (b, t2, r2);
        withDbl.processBlock (a, midi);
        without.processBlock (b, midi);
        for (int i = 0; i < bs; ++i)
        {
            const float monoA = a.getSample (0, i) + a.getSample (1, i);
            const float monoB = b.getSample (0, i) + b.getSample (1, i);
            maxMonoDiff = std::max (maxMonoDiff, std::abs (monoA - monoB));
            maxSide = std::max (maxSide, std::abs ((a.getSample (0, i) - a.getSample (1, i)) - (b.getSample (0, i) - b.getSample (1, i))));
        }
    }
    check (maxSide > 0.01f, "Doubler: adds stereo doubling (side change " + juce::String (maxSide, 3) + ")");
    check (maxMonoDiff < 1.0e-4f, "Doubler: mono sum unchanged (diff " + juce::String (maxMonoDiff, 6) + ")");
}

void testWidth()
{
    using namespace fckrp::ids;
    const double fs = 48000.0;
    const int bs = 256;
    FckrpProcessor p;
    p.setPlayConfigDetails (2, 2, fs, bs);
    p.prepareToPlay (fs, bs);
    allModulesOff (p);
    setParam (p, width, 0.0f);
    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    juce::Random rng (5);
    int64_t t = 0;
    float maxDiff = 0.0f;
    for (int blk = 0; blk < 100; ++blk)
    {
        fillStereo (buf, t, rng);
        p.processBlock (buf, midi);
        if (blk > 20)
            for (int i = 0; i < bs; ++i)
                maxDiff = std::max (maxDiff, std::abs (buf.getSample (0, i) - buf.getSample (1, i)));
    }
    check (maxDiff < 1.0e-5f, "Width: 0 % makes the signal mono");
}

void testCompMix()
{
    using namespace fckrp::ids;
    const double fs = 48000.0;
    const int bs = 256;
    FckrpProcessor mixed, off;
    for (auto* p : { &mixed, &off })
    {
        p->setPlayConfigDetails (2, 2, fs, bs);
        p->prepareToPlay (fs, bs);
        allModulesOff (*p);
    }
    setParam (mixed, compOn, 1.0f);
    setParam (mixed, compMix, 0.0f);
    setParam (mixed, compThresh, -40.0f);
    juce::AudioBuffer<float> a (2, bs), b (2, bs);
    juce::MidiBuffer midi;
    juce::Random r1 (9), r2 (9);
    int64_t t1 = 0, t2 = 0;
    float maxDiff = 0.0f;
    for (int blk = 0; blk < 200; ++blk)
    {
        fillStereo (a, t1, r1);
        fillStereo (b, t2, r2);
        mixed.processBlock (a, midi);
        off.processBlock (b, midi);
        if (blk > 20)
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < bs; ++i)
                    maxDiff = std::max (maxDiff, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
    }
    check (maxDiff < 1.0e-4f, "Opto Mix: 0 % passes the uncompressed signal (diff " + juce::String (maxDiff, 6) + ")");
    check (mixed.compGainReductionDb.load() > 3.0f, "Opto Mix: compressor still works underneath");
}

void testTamer()
{
    using namespace fckrp::ids;
    const double fs = 48000.0;
    const int bs = 256;
    auto runTamer = [&] (bool withResonance)
    {
        FckrpProcessor p;
        p.setPlayConfigDetails (2, 2, fs, bs);
        p.prepareToPlay (fs, bs);
        allModulesOff (p);
        setParam (p, tameOn, 1.0f);
        setParam (p, tameDepth, 1.0f);
        setParam (p, tameSens, 0.5f);
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        juce::Random rng (11);
        double phase = 0.0;
        float maxCut = 0.0f;
        for (int blk = 0; blk < 400; ++blk)
        {
            for (int i = 0; i < bs; ++i)
            {
                float v = (rng.nextFloat() - 0.5f) * 0.2f;
                if (withResonance)
                    v += 0.3f * (float) std::sin (phase);
                phase += juce::MathConstants<double>::twoPi * 3324.0 / fs;
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            p.processBlock (buf, midi);
            if (blk > 50)
                maxCut = std::max (maxCut, p.tameCutDb.load());
        }
        return maxCut;
    };
    const float withRes = runTamer (true);
    const float noiseOnly = runTamer (false);
    check (withRes > 4.0f, "Tame: cuts a ringing resonance (" + juce::String (withRes, 1) + " dB)");
    check (noiseOnly < 2.0f, "Tame: leaves broad noise mostly alone (" + juce::String (noiseOnly, 1) + " dB)");
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const bool withEditor = argc > 1 && juce::String (argv[1]) == "--editor";

    testPresets();
    testCompressor();
    testDeEsser();
    testSilenceTail();
    testMono();
    testState();
    testOversizedAndVariableBlocks();
    testBadInputRecovers();
    testUnprepared();
    testDoublerMonoSafe();
    testWidth();
    testCompMix();
    testTamer();
    if (withEditor)
        testEditor();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
