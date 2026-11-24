#pragma once

#include "Advisor.h"
#include "dsp/Loudness.h"
#include "dsp/Spectrum.h"

#include <juce_audio_processors/juce_audio_processors.h>

// fckrp check: an analyser for the end of the master chain.
// Audio passes through untouched; everything here only measures.
class CheckProcessor : public juce::AudioProcessor
{
public:
    CheckProcessor();
    ~CheckProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "fckrp check"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Called from the editor (message thread).
    void requestReset() { resetRequested.store (true); }
    void startLearning();
    void finishLearning();
    bool isLearning() const { return learning.load(); }

    mcheck::Measurements getMeasurements();
    mcheck::Settings getSettings();
    void setTarget (int t);
    void setCurve (int c);

    std::atomic<bool> autoReset { true };

    mcheck::LoudnessMeter loudness;
    mcheck::TruePeakMeter truePeak;
    mcheck::SpectrumAnalyser spectrum;

private:
    void doReset();

    std::atomic<bool> resetRequested { false }, learning { false };
    bool wasPlaying = false, prepared = false;
    double currentRate = 48000.0;

    juce::CriticalSection settingsLock;
    mcheck::Settings settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CheckProcessor)
};
