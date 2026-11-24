#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace mcheck
{
namespace colours
{
const juce::Colour background { 0xff141417 };
const juce::Colour panel { 0xff1f1f24 };
const juce::Colour accent { 0xffff5a36 };
const juce::Colour text { 0xffeeeae4 };
const juce::Colour dim { 0xff7c7a80 };
const juce::Colour good { 0xff4cc38a };
const juce::Colour warn { 0xffe8b341 };
const juce::Colour bad { 0xffff4d4d };
const juce::Colour cool { 0xff4d9dff };
inline juce::Colour forLevel (int level) { return level >= 2 ? bad : (level == 1 ? warn : good); }
} // namespace colours

class HintsView : public juce::Component
{
public:
    void setHints (std::vector<Hint> h, int width);
    void paint (juce::Graphics&) override;

private:
    std::vector<Hint> hints;
    std::vector<juce::TextLayout> layouts;
};
} // namespace mcheck

class CheckEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CheckEditor (CheckProcessor&);
    ~CheckEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Pulls fresh data and redraws (also used by the offline snapshot test).
    void refresh();

private:
    void timerCallback() override;
    void drawTiles (juce::Graphics&, juce::Rectangle<int>);
    void drawHistory (juce::Graphics&, juce::Rectangle<int>);
    void drawSpectrum (juce::Graphics&, juce::Rectangle<int>);

    CheckProcessor& processor;
    juce::LookAndFeel_V4 lnf;

    juce::ComboBox targetBox, curveBox;
    juce::TextButton resetButton, learnButton;
    juce::ToggleButton autoResetToggle;
    juce::Viewport hintsViewport;
    mcheck::HintsView hintsView;

    mcheck::Measurements meas;
    mcheck::Settings settings;
    mcheck::BandAnalysis bands;
    std::vector<mcheck::Hint> hints;
    std::vector<float> fastBandDb; // per pixel column of the spectrum
    float rawOffset = 0;
    int verdict = 0;
    int tick = 0;

    juce::Rectangle<int> tilesArea, historyArea, spectrumArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CheckEditor)
};
