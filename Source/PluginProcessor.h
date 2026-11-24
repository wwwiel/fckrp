#pragma once

#include "Parameters.h"
#include "dsp/DeEsser.h"
#include "dsp/Doubler.h"
#include "dsp/ResonanceTamer.h"
#include "dsp/Gate.h"
#include "dsp/OptoCompressor.h"
#include "dsp/PunchLimiter.h"
#include "dsp/TubeSaturation.h"
#include "dsp/VocalDelay.h"
#include "dsp/VocalEq.h"
#include "dsp/VocalReverb.h"

class FckrpProcessor : public juce::AudioProcessor
{
public:
    FckrpProcessor();
    ~FckrpProcessor() override;

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
    std::atomic<float> tameCutDb { 0.0f };
    std::atomic<float> punchGainReductionDb { 0.0f };
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };

    // Diagnostics, written to a log file once a second from the message
    // thread (never from the audio thread). They show what the host really
    // sends and how loud the signal is after every module.
    enum Stage { stIn, stGate, stEq, stTame, stDeEss, stOpto, stTube, stDoubler, stDelay, stHall, stWidth, stOut, numStages };
    std::array<std::atomic<float>, numStages> stagePeak;
    std::atomic<int> diagMinBlock { 1 << 30 }, diagMaxBlock { 0 }, diagBufChannels { 0 };
    std::atomic<int> diagNumIn { 0 }, diagNumOut { 0 }, diagPrepares { 0 }, diagPrepBlock { 0 };
    std::atomic<int> diagUnprepared { 0 }, diagNonFiniteIn { 0 }, diagSafetyTrips { 0 }, diagRunawayTrips { 0 };
    std::atomic<double> diagRate { 0.0 };

private:
    static constexpr int maxChunk = 512;

    void processChunk (juce::AudioBuffer<float>& buffer);
    void resetDsp();
    void markStage (Stage s, const juce::AudioBuffer<float>& b);

    struct DiagLogger;
    std::unique_ptr<DiagLogger> diagLogger;

    float get (const char* id) const { return params.at (id)->load(); }
    bool isOn (const char* id) const { return get (id) > 0.5f; }

    // Keyed by the address of the ID constant, so lookups on the audio
    // thread never build strings.
    std::map<const char*, std::atomic<float>*> params;

    fckrp::Gate gate;
    fckrp::VocalEq eq;
    fckrp::ResonanceTamer tamer;
    fckrp::DeEsser deEsser;
    fckrp::Doubler doubler;
    fckrp::StereoWidth stereoWidth;
    juce::AudioBuffer<float> compDry;
    juce::SmoothedValue<float> compMixSmooth { 1.0f };
    fckrp::OptoCompressor compressor;
    fckrp::TubeSaturation saturation;
    fckrp::VocalDelay delay;
    fckrp::VocalReverb reverb;
    juce::SmoothedValue<float> outputGain { 1.0f }, inputGain { 1.0f };
    fckrp::PunchLimiter punch;

    int currentProgram = 0;
    bool isPrepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FckrpProcessor)
};
