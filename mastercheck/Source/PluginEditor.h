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

// The master fix chain controls: EQ, compressor, stereo, limiter.
class FixPanel : public juce::Component
{
public:
    FixPanel (CheckProcessor&, std::function<void (const juce::String&)> notify);
    void resized() override;
    void paint (juce::Graphics&) override;
    void updateMeters();

private:
    struct Knob
    {
        juce::Slider slider;
        juce::String label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& label, bool vertical);
    void setupToggle (juce::ToggleButton&, const juce::String& paramId, const juce::String& text);
    void placeKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> knobs);

    CheckProcessor& processor;
    std::function<void (const juce::String&)> notify;
    juce::ToggleButton fixToggle, eqToggle, compToggle, densToggle, stereoToggle, limToggle;
    std::array<Knob, mcheck::numEqBands> eqKnobs;
    Knob thresh, ratio, attack, release, makeup, peak, densAmount, densDrive, monoBelow, width, sideMid, sideAir,
        limGain, ceiling, limRelease, limLink;
    juce::TextButton autoEqButton, eqZeroButton, autoLoudButton;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    juce::Rectangle<int> eqArea, compArea, densArea, stereoArea, limArea;
    float compGr = 0, limGr = 0;
};

class CheckEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit CheckEditor (CheckProcessor&);
    ~CheckEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Pulls fresh data and redraws (also used by the offline snapshot test).
    void refresh();

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    void showMessage (const juce::String&);

private:
    void timerCallback() override;
    void drawTiles (juce::Graphics&, juce::Rectangle<int>);
    void drawHistory (juce::Graphics&, juce::Rectangle<int>);
    void drawSpectrum (juce::Graphics&, juce::Rectangle<int>);

    CheckProcessor& processor;
    juce::LookAndFeel_V4 lnf;

    juce::ComboBox targetBox, curveBox;
    juce::TextButton resetButton, learnButton;
    std::unique_ptr<juce::FileChooser> chooser;
    FixPanel fixPanel;
    juce::String toast;
    juce::uint32 toastUntil = 0;
    bool dragging = false;
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
    static bool isAudioFile (const juce::String& path);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CheckEditor)
};
