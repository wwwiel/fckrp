#include "PluginEditor.h"

namespace fckrp
{
const juce::Colour FckrpLookAndFeel::background { 0xff141417 };
const juce::Colour FckrpLookAndFeel::panel      { 0xff1f1f24 };
const juce::Colour FckrpLookAndFeel::accent     { 0xffff5a36 };
const juce::Colour FckrpLookAndFeel::text       { 0xffeeeae4 };
const juce::Colour FckrpLookAndFeel::dim        { 0xff7c7a80 };

FckrpLookAndFeel::FckrpLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, dim.withAlpha (0.4f));
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
}

void FckrpLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = std::max (2.0f, radius * 0.12f);

    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    g.setColour (dim.withAlpha (0.35f));
    g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float capR = radius - track * 2.5f;
    g.setColour (panel.brighter (0.15f));
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

    const auto tip = centre.getPointOnCircumference (capR * 0.8f, angle);
    g.setColour (text);
    g.drawLine ({ centre.getPointOnCircumference (capR * 0.25f, angle), tip }, std::max (1.5f, track * 0.6f));
}

void FckrpLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const auto dot = r.withSizeKeepingCentre (12.0f, 12.0f);
    g.setColour (b.getToggleState() ? accent : dim.withAlpha (highlighted ? 0.8f : 0.5f));
    g.fillEllipse (dot);
}
} // namespace fckrp

using LNF = fckrp::FckrpLookAndFeel;

FckrpEditor::FckrpEditor (FckrpProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    using namespace fckrp::ids;
    setLookAndFeel (&lnf);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        processor.setCurrentProgram (presetBox.getSelectedId() - 1);
    };
    addAndMakeVisible (presetBox);

    polishKnob = makeKnob (polish, "POLISH", true);
    spaceKnob = makeKnob (space, "SPACE", true);
    outputKnob = makeKnob (output, "OUTPUT");
    widthKnob = makeKnob (width, "WIDTH");

    addSection ("GATE", gateOn, { { gateThresh, "Threshold" } });
    addSection ("EQ", eqOn, { { eqLowCut, "Low Cut" }, { eqMud, "Mud" }, { eqPresence, "Presence" }, { eqAir, "Air" } });
    addSection ("TAME", tameOn, { { tameDepth, "Depth" }, { tameSens, "Sensitivity" } });
    addSection ("DE-ESS", dsOn, { { dsFreq, "Freq" }, { dsThresh, "Threshold" } });
    addSection ("OPTO", compOn, { { compThresh, "Threshold" }, { compRatio, "Ratio" }, { compAttack, "Attack" },
                                  { compRelease, "Release" }, { compMakeup, "Gain" }, { compMix, "Mix" } });
    addSection ("TUBE", satOn, { { satDrive, "Drive" } });
    addSection ("DOUBLER", dblOn, { { dblAmount, "Amount" }, { dblDetune, "Detune" } });
    addSection ("DELAY", dlyOn, { { dlyTime, "Time" }, { dlyFeedback, "Feedback" }, { dlyMix, "Mix" } });
    addSection ("HALL", revOn, { { revSize, "Size" }, { revDamp, "Damping" }, { revMix, "Mix" } });

    setSize (1100, 780);
    startTimerHz (30);
}

FckrpEditor::~FckrpEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

std::unique_ptr<FckrpEditor::Knob> FckrpEditor::makeKnob (const char* paramId, const juce::String& labelText, bool big)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    k->slider.setPopupDisplayEnabled (false, false, nullptr);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxTextColourId, big ? LNF::text : LNF::text.withAlpha (0.85f));
    k->attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k->slider);
    k->label.setText (labelText, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (big ? 16.0f : 12.0f, big ? juce::Font::bold : juce::Font::plain));
    k->label.setColour (juce::Label::textColourId, big ? LNF::text : LNF::dim);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    return k;
}

void FckrpEditor::addSection (const juce::String& title, const char* toggleId,
                              std::initializer_list<std::pair<const char*, const char*>> knobs)
{
    auto s = std::make_unique<Section>();
    s->title = title;
    s->toggleAttachment = std::make_unique<ButtonAttachment> (processor.apvts, toggleId, s->toggle);
    addAndMakeVisible (s->toggle);
    for (const auto& [id, name] : knobs)
        s->knobs.push_back (makeKnob (id, name));
    sections.push_back (std::move (s));
}

void FckrpEditor::paint (juce::Graphics& g)
{
    g.fillAll (LNF::background);

    // Header
    g.setColour (LNF::text);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("fckrp", 24, 12, 200, 36, juce::Justification::centredLeft);
    g.setColour (LNF::accent);
    g.fillRect (24, 50, 54, 3);
    g.setColour (LNF::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("vocal chain", 100, 22, 200, 20, juce::Justification::centredLeft);

    // Section panels
    for (auto& s : sections)
    {
        g.setColour (LNF::panel);
        g.fillRoundedRectangle (s->bounds.toFloat(), 8.0f);
        g.setColour (LNF::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (s->title, s->bounds.getX() + 12, s->bounds.getY() + 8, s->bounds.getWidth() - 40, 20,
                    juce::Justification::centredLeft);
    }

    // Gain-reduction meters (Opto and Tame)
    auto drawGr = [&g] (juce::Rectangle<int> area, float db, float fullScale)
    {
        if (area.isEmpty())
            return;
        g.setColour (LNF::background);
        g.fillRoundedRectangle (area.toFloat(), 3.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, db / fullScale);
        auto bar = area.toFloat().reduced (2.0f);
        g.setColour (LNF::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 2.0f);
        g.setColour (LNF::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("-" + juce::String (db, 1) + " dB", area.translated (0, 14), juce::Justification::centred);
    };
    drawGr (compMeterArea, compGr, 20.0f);
    drawGr (tameMeterArea, tameCut, 12.0f);

    // Input and output level meters
    auto drawLevel = [&g] (juce::Rectangle<int> area, float level, const char* label)
    {
        if (area.isEmpty())
            return;
        g.setColour (LNF::panel);
        g.fillRoundedRectangle (area.toFloat(), 3.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (level + 60.0f) / 60.0f);
        auto bar = area.toFloat().reduced (2.0f);
        g.setColour (level > -0.5f ? juce::Colours::red : LNF::text.withAlpha (0.8f));
        g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * frac), 2.0f);
        g.setColour (LNF::dim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (label, area.getX() - 8, area.getBottom() + 3, area.getWidth() + 16, 12, juce::Justification::centred);
    };
    drawLevel (inMeterArea, inLevel, "IN");
    drawLevel (outMeterArea, outLevel, "OUT");
}

void FckrpEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (48);
    presetBox.setBounds (header.removeFromRight (240).withSizeKeepingCentre (240, 28));

    area.removeFromTop (8);
    auto macroRow = area.removeFromTop (150);
    const int bigKnob = 120;
    auto centreBlock = macroRow.withSizeKeepingCentre (bigKnob * 2 + 80, macroRow.getHeight());
    auto placeBig = [] (FckrpEditor::Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (20));
        k.slider.setBounds (r);
    };
    placeBig (*polishKnob, centreBlock.removeFromLeft (bigKnob + 20));
    placeBig (*spaceKnob, centreBlock.removeFromRight (bigKnob + 20));

    auto widthArea = macroRow.removeFromLeft (120);
    widthKnob->label.setBounds (widthArea.removeFromTop (20));
    widthKnob->slider.setBounds (widthArea.reduced (10, 10));

    auto outArea = macroRow.removeFromRight (140);
    outMeterArea = outArea.removeFromRight (12).withTrimmedTop (10).withTrimmedBottom (18);
    outArea.removeFromRight (4);
    inMeterArea = outArea.removeFromRight (12).withTrimmedTop (10).withTrimmedBottom (18);
    outputKnob->label.setBounds (outArea.removeFromTop (20));
    outputKnob->slider.setBounds (outArea.reduced (10, 10));

    area.removeFromTop (12);
    const int gap = 8;
    const int n = (int) sections.size();
    const int w = (area.getWidth() - gap * (n - 1)) / n;
    const int knobH = 72;

    for (int i = 0; i < n; ++i)
    {
        auto& s = *sections[(size_t) i];
        auto r = area.removeFromLeft (w);
        area.removeFromLeft (gap);
        s.bounds = r;

        s.toggle.setBounds (r.getRight() - 30, r.getY() + 6, 24, 24);
        auto inner = r.reduced (6).withTrimmedTop (28);

        if (s.title == "OPTO")
            compMeterArea = inner.removeFromBottom (34).withTrimmedBottom (20).reduced (4, 0);
        if (s.title == "TAME")
            tameMeterArea = inner.removeFromBottom (34).withTrimmedBottom (20).reduced (4, 0);

        for (auto& k : s.knobs)
        {
            auto kr = inner.removeFromTop (knobH);
            k->label.setBounds (kr.removeFromTop (14));
            k->slider.setBounds (kr);
        }
    }
}

void FckrpEditor::timerCallback()
{
    // Meters fall back smoothly instead of jumping.
    const float gr = processor.compGainReductionDb.load();
    compGr = gr > compGr ? gr : compGr * 0.85f + gr * 0.15f;
    const float lvl = processor.outputLevelDb.load();
    outLevel = lvl > outLevel ? lvl : outLevel - 1.5f;
    const float il = processor.inputLevelDb.load();
    inLevel = il > inLevel ? il : inLevel - 1.5f;

    if (presetBox.getSelectedId() != processor.getCurrentProgram() + 1)
        presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);

    const float tc = processor.tameCutDb.load();
    tameCut = tc > tameCut ? tc : tameCut * 0.85f + tc * 0.15f;

    repaint (compMeterArea.expanded (2, 20));
    repaint (tameMeterArea.expanded (2, 20));
    repaint (outMeterArea.expanded (10, 16));
    repaint (inMeterArea.expanded (10, 16));
}
