#pragma once

#include "PluginProcessor.h"

namespace fckrp
{
class FckrpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FckrpLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    static const juce::Colour background, panel, accent, text, dim;
};
} // namespace fckrp

class FckrpEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FckrpEditor (FckrpProcessor&);
    ~FckrpEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::ToggleButton toggle;
        std::unique_ptr<ButtonAttachment> toggleAttachment;
        std::vector<std::unique_ptr<Knob>> knobs;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    std::unique_ptr<Knob> makeKnob (const char* paramId, const juce::String& labelText, bool big = false);
    void addSection (const juce::String& title, const char* toggleId,
                     std::initializer_list<std::pair<const char*, const char*>> knobs);

    FckrpProcessor& processor;
    fckrp::FckrpLookAndFeel lnf;

    juce::ComboBox presetBox;
    std::unique_ptr<Knob> polishKnob, spaceKnob, outputKnob;
    std::vector<std::unique_ptr<Section>> sections;

    float compGr = 0.0f, deEssGr = 0.0f, outLevel = -100.0f;
    juce::Rectangle<int> compMeterArea, outMeterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FckrpEditor)
};
