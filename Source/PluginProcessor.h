#pragma once

#include "Parameters.h"
#include "dsp/DeEsser.h"
#include "dsp/Gate.h"
#include "dsp/OptoCompressor.h"
#include "dsp/TubeSaturation.h"
#include "dsp/VocalDelay.h"
#include "dsp/VocalEq.h"
#include "dsp/VocalReverb.h"

class FckrpProcessor : public juce::AudioProcessor
{
public:
    FckrpProcessor();
    ~FckrpProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter values read by the editor.
    std::atomic<float> compGainReductionDb { 0.0f };
    std::atomic<float> deEssGainReductionDb { 0.0f };
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };

private:
    static constexpr int maxChunk = 512;

    void processChunk (juce::AudioBuffer<float>& buffer);
    void resetDsp();

    float get (const char* id) const { return params.at (id)->load(); }
    bool isOn (const char* id) const { return get (id) > 0.5f; }

    // Keyed by the address of the ID constant, so lookups on the audio
    // thread never build strings.
    std::map<const char*, std::atomic<float>*> params;

    fckrp::Gate gate;
    fckrp::VocalEq eq;
    fckrp::DeEsser deEsser;
    fckrp::OptoCompressor compressor;
    fckrp::TubeSaturation saturation;
    fckrp::VocalDelay delay;
    fckrp::VocalReverb reverb;
    juce::SmoothedValue<float> outputGain { 1.0f };

    int currentProgram = 0;
    bool isPrepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FckrpProcessor)
};
