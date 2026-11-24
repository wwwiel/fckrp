#pragma once

#include "Advisor.h"
#include "dsp/Loudness.h"
#include "dsp/MasterChain.h"
#include "dsp/Spectrum.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <thread>

namespace mcheck::ids
{
inline constexpr const char* fixOn = "fixOn";
inline constexpr const char* eqOn = "eqOn";
inline constexpr const char* compOn = "compOn";
inline constexpr const char* compThresh = "compThresh";
inline constexpr const char* compRatio = "compRatio";
inline constexpr const char* compAttack = "compAttack";
inline constexpr const char* compRelease = "compRelease";
inline constexpr const char* compMakeup = "compMakeup";
inline constexpr const char* compMix = "compMix";
inline constexpr const char* stereoOn = "stereoOn";
inline constexpr const char* monoBelow = "monoBelow";
inline constexpr const char* width = "width";
inline constexpr const char* limOn = "limOn";
inline constexpr const char* limGain = "limGain";
inline constexpr const char* limCeiling = "limCeiling";
inline constexpr const char* limRelease = "limRelease";
inline constexpr const char* limLink = "limLink";
inline constexpr const char* compPeak = "compPeak";
inline constexpr const char* densOn = "densOn";
inline constexpr const char* densAmount = "densAmount";
inline constexpr const char* densDrive = "densDrive";
inline constexpr const char* sideMid = "sideMid";
inline constexpr const char* sideAir = "sideAir";
inline juce::String eqBand (int b) { return "eq" + juce::String (b); }
} // namespace mcheck::ids

// fckrp check: analyser + master fix chain for the end of the master.
// The analysis measures the output, so you see the result of the fix chain.
class CheckProcessor : public juce::AudioProcessor
{
public:
    CheckProcessor();
    ~CheckProcessor() override;

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

    // Analyses an audio file (WAV, AIFF, FLAC, MP3 ...) in the background and uses it as reference.
    void loadReferenceFile (const juce::File&);
    bool isLoadingReference() const { return loadingReference.load(); }
    juce::String getReferenceStatus();
    // Same, synchronously (used by the background job and the tests). Returns an error text or empty.
    juce::String analyseReferenceFile (const juce::File&);

    // One-click helpers based on the current analysis of the output.
    juce::String applyAutoEq();
    juce::String applyAutoLoudness();

    mcheck::Measurements getMeasurements();
    mcheck::Settings getSettings();
    void setTarget (int t);
    void setCurve (int c);

    std::atomic<bool> autoReset { true };

    juce::AudioProcessorValueTreeState params;

    mcheck::LoudnessMeter loudness;
    mcheck::TruePeakMeter truePeak;
    mcheck::SpectrumAnalyser spectrum;
    mcheck::MasterEq eq;
    mcheck::GlueCompressor comp;
    mcheck::StereoTool stereo;
    mcheck::DensityShaper density;
    mcheck::TruePeakLimiter limiter;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void doReset();
    void setParam (const juce::String& id, float value);
    float getParam (const juce::String& id) const;

    std::atomic<bool> resetRequested { false }, spectrumResetRequested { false }, learning { false };
    std::atomic<bool> loadingReference { false };
    bool wasPlaying = false, prepared = false;
    float compGrSmooth = 0, limGrSmooth = 0;
    std::atomic<float> compGrSong { 0.0f }, limGrSong { 0.0f };
    double currentRate = 48000.0;

    juce::CriticalSection settingsLock;
    mcheck::Settings settings;
    juce::String referenceStatus;
    std::thread referenceThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CheckProcessor)
};
