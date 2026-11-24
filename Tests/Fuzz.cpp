// Stress test: random block sizes, random parameter changes every block,
// several sample rates. Prints the first condition that produces loud output.
#include "PluginProcessor.h"
#include "Presets.h"
#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Random rng (1234);
    int failures = 0;
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int preset = 0; preset < 7; ++preset)
            for (int mode = 0; mode < 3; ++mode)
            {
                FckrpProcessor p;
                p.setPlayConfigDetails (2, 2, fs, 512);
                p.prepareToPlay (fs, mode == 2 ? 4096 : 512);
                p.setCurrentProgram (preset);
                juce::MidiBuffer midi;
                int64_t t = 0;
                float worst = 0.0f;
                juce::String what;
                for (int blk = 0; blk < 600; ++blk)
                {
                    const int n = mode == 0 ? 512 : 1 + rng.nextInt (4096);
                    if (mode == 2 && blk % 7 == 0)
                    {
                        auto params = p.getParameters();
                        auto* prm = params[rng.nextInt (params.size())];
                        prm->setValueNotifyingHost (rng.nextFloat());
                    }
                    juce::AudioBuffer<float> buf (2, n);
                    for (int i = 0; i < n; ++i, ++t)
                    {
                        const float v = 0.25f * std::sin ((float) t * 0.05f) * (std::fmod ((double) t / fs, 2.0) < 1.3 ? 1.0f : 0.0f);
                        buf.setSample (0, i, v);
                        buf.setSample (1, i, v * 0.9f);
                    }
                    p.processBlock (buf, midi);
                    const float pk = buf.getMagnitude (0, n);
                    if (pk > worst) { worst = pk; what = "block " + juce::String (blk) + " size " + juce::String (n); }
                }
                if (worst > 1.2f)
                {
                    ++failures;
                    std::printf ("LOUD fs=%d preset=%d mode=%d peak=%.2f at %s\n", (int) fs, preset, mode, worst, what.toRawUTF8());
                }
            }
    std::printf ("%s\n", failures ? "FUZZ FOUND PROBLEMS" : "FUZZ OK");
    return failures ? 1 : 0;
}
