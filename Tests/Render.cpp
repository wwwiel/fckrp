// Offline render: runs a WAV file through fckrp with a preset and optional
// parameter overrides, and writes the result. Used to tune presets on a
// real voice without a DAW.
//
//   fckrp_render in.wav out.wav "Preset name" [param_id value]...
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "Presets.h"
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 4)
    {
        std::printf ("usage: fckrp_render in.wav out.wav \"Preset name\" [param_id value]...\n");
        return 2;
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (argv[1])));
    if (reader == nullptr)
    {
        std::printf ("cannot read %s\n", argv[1]);
        return 1;
    }

    const double fs = reader->sampleRate;
    const int len = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> in (2, len);
    reader->read (&in, 0, len, 0, true, true);   // mono files are copied to both channels

    FckrpProcessor p;
    const int bs = 256;
    p.setPlayConfigDetails (2, 2, fs, bs);

    int program = -1;
    const auto& presets = fckrp::getFactoryPresets();
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == juce::String (argv[3]))
            program = (int) i;
    if (program < 0)
    {
        std::printf ("unknown preset %s\n", argv[3]);
        return 1;
    }
    p.setCurrentProgram (program);
    for (int a = 4; a + 1 < argc; a += 2)
    {
        auto* prm = p.apvts.getParameter (argv[a]);
        if (prm == nullptr) { std::printf ("unknown parameter %s\n", argv[a]); return 1; }
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) std::atof (argv[a + 1])));
    }
    p.prepareToPlay (fs, bs);

    const int latency = p.getLatencySamples();
    const int total = len + latency;
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> block (2, bs);
    juce::MidiBuffer midi;
    for (int start = 0; start < total; start += bs)
    {
        const int n = std::min (bs, total - start);
        block.clear();
        for (int ch = 0; ch < 2; ++ch)
            if (start < len)
                block.copyFrom (ch, 0, in, ch, start, std::min (n, len - start));
        juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), 2, 0, n);
        p.processBlock (view, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, start, block, ch, 0, n);
    }

    juce::File outFile (argv[2]);
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (outFile), fs, 2, 24, {}, 0));
    if (writer == nullptr)
    {
        std::printf ("cannot write %s\n", argv[2]);
        return 1;
    }
    writer->writeFromAudioSampleBuffer (out, latency, len);
    std::printf ("rendered %s with \"%s\", latency %d samples, peak %.1f dBFS\n", argv[2], argv[3], latency,
                 fckrp::gainToDb (out.getMagnitude (latency, len)));
    return 0;
}
