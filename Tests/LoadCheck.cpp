// Loads a built fckrp plugin file the way a DAW does, opens it, runs audio
// through it and opens its editor. Exit code 0 = the plugin loads and works.
//
//   fckrp_loadcheck /path/to/fckrp.vst3

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::printf ("usage: fckrp_loadcheck <plugin path>\n");
        return 2;
    }

    const juce::String path = juce::File (juce::String::fromUTF8 (argv[1])).getFullPathName();

    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* format : formats.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (found, path);

    if (found.isEmpty())
    {
        std::printf ("FAIL  no plugin found in %s\n", path.toRawUTF8());
        return 1;
    }

    const auto& desc = *found.getFirst();
    std::printf ("found: %s (%s) by %s, %d in / %d out\n", desc.name.toRawUTF8(), desc.pluginFormatName.toRawUTF8(),
                 desc.manufacturerName.toRawUTF8(), desc.numInputChannels, desc.numOutputChannels);

    juce::String error;
    auto instance = formats.createPluginInstance (desc, 48000.0, 512, error);
    if (instance == nullptr)
    {
        std::printf ("FAIL  could not create instance: %s\n", error.toRawUTF8());
        return 1;
    }

    instance->enableAllBuses();
    instance->setPlayConfigDetails (2, 2, 48000.0, 512);
    instance->prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    bool finite = true;
    for (int block = 0; block < 200; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.3f * std::sin ((float) (block * 512 + i) * 0.05f));
        instance->processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float x = buffer.getSample (ch, i);
                finite = finite && std::isfinite (x);
                peak = std::max (peak, std::abs (x));
            }
    }
    instance->releaseResources();

    std::printf ("processed audio: peak %.3f, %s\n", peak, finite ? "no NaN/Inf" : "NaN/Inf found");
    if (! finite || peak <= 0.0001f)
    {
        std::printf ("FAIL  audio output is broken\n");
        return 1;
    }

    std::printf ("parameters: %d, presets: %d\n", (int) instance->getParameters().size(), instance->getNumPrograms());

    if (instance->hasEditor())
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorIfNeeded());
        if (editor == nullptr)
        {
            std::printf ("FAIL  editor did not open\n");
            return 1;
        }
        std::printf ("editor: %d x %d\n", editor->getWidth(), editor->getHeight());
    }

    std::printf ("PASS  plugin loads and runs\n");
    return 0;
}
