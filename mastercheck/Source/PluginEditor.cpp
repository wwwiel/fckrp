#include "PluginEditor.h"

#include <cmath>

using namespace mcheck;
namespace col = mcheck::colours;

namespace
{
constexpr int editorWidth = 1240, editorHeight = 850;

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::String lufsText (float v) { return v < -69.0f ? juce::String ("--") : num (v); }

float freqToX (float f, juce::Rectangle<float> r)
{
    const float t = std::log (f / 20.0f) / std::log (20000.0f / 20.0f);
    return r.getX() + t * r.getWidth();
}

float xToFreq (float x, juce::Rectangle<float> r)
{
    const float t = (x - r.getX()) / r.getWidth();
    return 20.0f * std::pow (1000.0f, t);
}
} // namespace

// ---------------------------------------------------------------------------------------------

void HintsView::setHints (std::vector<Hint> h, int width)
{
    hints = std::move (h);
    layouts.clear();
    int y = 8;
    for (auto& hint : hints)
    {
        juce::AttributedString as;
        as.append (hint.title + "   ", font (15.0f, true), col::forLevel (hint.level));
        as.append (hint.text, font (14.0f), hint.level == 0 ? col::text.withAlpha (0.75f) : col::text);
        as.setWordWrap (juce::AttributedString::byWord);
        juce::TextLayout tl;
        tl.createLayout (as, (float) (width - 44));
        y += (int) std::ceil (tl.getHeight()) + 12;
        layouts.push_back (std::move (tl));
    }
    setSize (width, y);
    repaint();
}

void HintsView::paint (juce::Graphics& g)
{
    float y = 8.0f;
    for (size_t i = 0; i < hints.size(); ++i)
    {
        const auto& tl = layouts[i];
        g.setColour (col::forLevel (hints[i].level));
        g.fillEllipse (14.0f, y + 4.0f, 10.0f, 10.0f);
        tl.draw (g, { 34.0f, y, (float) getWidth() - 44.0f, tl.getHeight() });
        y += tl.getHeight() + 12.0f;
    }
}

// ---------------------------------------------------------------------------------------------

FixPanel::FixPanel (CheckProcessor& p, std::function<void (const juce::String&)> n)
    : processor (p), notify (std::move (n))
{
    using namespace mcheck::ids;
    setupToggle (fixToggle, fixOn, u8 ("Fix aktiv (aus = Vergleich ohne)"));
    setupToggle (eqToggle, eqOn, "");
    setupToggle (compToggle, compOn, "");
    setupToggle (densToggle, densOn, "");
    setupToggle (stereoToggle, stereoOn, "");
    setupToggle (limToggle, limOn, "");

    for (int b = 0; b < numEqBands; ++b)
    {
        auto& k = eqKnobs[(size_t) b];
        setupKnob (k, eqBand (b), u8 (eqNames[(size_t) b]), true);
        // Narrow sliders: show just the number ("-2,0").
        k.slider.textFromValueFunction = [] (double v) { return signedNum (v, 1); };
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 16);
        k.slider.updateText();
    }
    setupKnob (thresh, compThresh, "THRESH", false);
    setupKnob (ratio, compRatio, "RATIO", false);
    setupKnob (attack, compAttack, "ATTACK", false);
    setupKnob (release, compRelease, "RELEASE", false);
    setupKnob (makeup, compMakeup, "MAKEUP", false);
    setupKnob (peak, compPeak, "PEAK", false);
    setupKnob (densAmount, mcheck::ids::densAmount, "MENGE", false);
    setupKnob (densDrive, mcheck::ids::densDrive, "DRIVE", false);
    setupKnob (monoBelow, mcheck::ids::monoBelow, "MONO <", false);
    setupKnob (width, mcheck::ids::width, "BREITE", false);
    setupKnob (sideMid, mcheck::ids::sideMid, "S-MITTE", false);
    setupKnob (sideAir, mcheck::ids::sideAir, "S-AIR", false);
    setupKnob (limGain, mcheck::ids::limGain, "GAIN", false);
    setupKnob (ceiling, limCeiling, "CEILING", false);
    setupKnob (limRelease, mcheck::ids::limRelease, "RELEASE", false);
    setupKnob (limLink, mcheck::ids::limLink, "LINK", false);
    thresh.slider.setTooltip (u8 ("Ziel: nur 1–2 dB Reduktion"));
    peak.slider.setTooltip (u8 ("0 % = reine RMS-Kompression (unhörbar), 100 % = Peaks"));
    densAmount.slider.setTooltip (u8 ("Holt leise Details hoch, Peaks bleiben – lauter ohne den Limiter zu quälen"));
    monoBelow.slider.setTooltip (u8 ("Alles darunter wird mono (Bass/808)"));
    sideMid.slider.setTooltip (u8 ("Side-Glocke bei 2,5 kHz: breiter, ohne die Mitte zu ändern"));
    sideAir.slider.setTooltip (u8 ("Air ab 10 kHz nur auf der Side"));
    limLink.slider.setTooltip (u8 ("60–80 % klingt meist natürlicher als 100 %"));

    autoEqButton.setButtonText ("Auto-EQ");
    autoEqButton.setTooltip (u8 ("Stellt den EQ nach der Analyse ein (ca. 60 % der Abweichung)"));
    autoEqButton.onClick = [this] { notify (processor.applyAutoEq()); };
    addAndMakeVisible (autoEqButton);

    eqZeroButton.setButtonText ("0");
    eqZeroButton.setTooltip (u8 ("Alle EQ-Bänder auf 0 dB"));
    eqZeroButton.onClick = [this]
    {
        for (int b = 0; b < numEqBands; ++b)
            eqKnobs[(size_t) b].slider.setValue (0.0, juce::sendNotificationSync);
    };
    addAndMakeVisible (eqZeroButton);

    autoLoudButton.setButtonText ("Auto-LUFS");
    autoLoudButton.setTooltip (u8 ("Stellt den Limiter-Gain so ein, dass dein Ziel-LUFS erreicht wird"));
    autoLoudButton.onClick = [this] { notify (processor.applyAutoLoudness()); };
    addAndMakeVisible (autoLoudButton);
}

void FixPanel::setupKnob (Knob& k, const juce::String& id, const juce::String& label, bool vertical)
{
    k.label = label;
    k.slider.setSliderStyle (vertical ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 16);
    k.slider.setColour (juce::Slider::textBoxTextColourId, col::text);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, col::accent);
    k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, col::dim.withAlpha (0.35f));
    k.slider.setColour (juce::Slider::thumbColourId, col::text);
    k.slider.setColour (juce::Slider::trackColourId, col::accent);
    k.slider.setColour (juce::Slider::backgroundColourId, col::dim.withAlpha (0.3f));
    k.slider.setDoubleClickReturnValue (true, (double) processor.params.getParameterRange (id).convertFrom0to1 (
                                                  processor.params.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.params, id, k.slider);
}

void FixPanel::setupToggle (juce::ToggleButton& b, const juce::String& id, const juce::String& text)
{
    b.setButtonText (text);
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.params, id, b));
}

void FixPanel::placeKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> knobs)
{
    const int w = area.getWidth() / (int) knobs.size();
    for (auto* k : knobs)
        k->slider.setBounds (area.removeFromLeft (w).withTrimmedTop (16).reduced (2, 0));
}

void FixPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 0);
    auto top = r.removeFromTop (30).reduced (0, 4);
    fixToggle.setBounds (top.removeFromRight (280));

    const int gap = 10;
    eqArea = r.removeFromLeft (276);
    r.removeFromLeft (gap);
    compArea = r.removeFromLeft (330);
    r.removeFromLeft (gap);
    densArea = r.removeFromLeft (104);
    r.removeFromLeft (gap);
    stereoArea = r.removeFromLeft (206);
    r.removeFromLeft (gap);
    limArea = r;

    auto title = [] (juce::Rectangle<int> a) { return a.removeFromTop (26); };
    {
        auto t = title (eqArea);
        eqToggle.setBounds (t.removeFromLeft (24));
        eqZeroButton.setBounds (t.removeFromRight (30).reduced (0, 2));
        t.removeFromRight (6);
        autoEqButton.setBounds (t.removeFromRight (90).reduced (0, 2));
    }
    compToggle.setBounds (title (compArea).removeFromLeft (24));
    densToggle.setBounds (title (densArea).removeFromLeft (24));
    stereoToggle.setBounds (title (stereoArea).removeFromLeft (24));
    {
        auto t = title (limArea);
        limToggle.setBounds (t.removeFromLeft (24));
        autoLoudButton.setBounds (t.removeFromRight (92).reduced (0, 2));
    }

    auto content = [] (juce::Rectangle<int> a) { return a.withTrimmedTop (28).withTrimmedBottom (8); };
    {
        auto a = content (eqArea);
        const int w = a.getWidth() / numEqBands;
        for (auto& k : eqKnobs)
            k.slider.setBounds (a.removeFromLeft (w).withTrimmedTop (16).reduced (1, 0));
    }
    placeKnobs (content (compArea), { &thresh, &ratio, &attack, &release, &makeup, &peak });
    placeKnobs (content (densArea), { &densAmount, &densDrive });
    placeKnobs (content (stereoArea), { &monoBelow, &width, &sideMid, &sideAir });
    placeKnobs (content (limArea), { &limGain, &ceiling, &limRelease, &limLink });
}

void FixPanel::updateMeters()
{
    compGr = processor.comp.gr.load();
    limGr = std::max (processor.limiter.gr.load(), limGr * 0.85f);
}

void FixPanel::paint (juce::Graphics& g)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (col::text);
    g.setFont (font (15.0f, true));
    g.drawText (u8 ("MASTER-FIX"), 14, 6, 200, 20, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (font (12.0f));
    g.drawText (u8 ("Reihenfolge: EQ → Komp → Dichte → Stereo → Limiter (max. 3 dB). Die Analyse oben misst das Ergebnis."), 130, 6, 560, 20,
                juce::Justification::centredLeft);

    auto section = [&] (juce::Rectangle<int> a, const juce::String& name, const juce::String& extra)
    {
        g.setColour (col::background.withAlpha (0.45f));
        g.fillRoundedRectangle (a.toFloat(), 6.0f);
        g.setColour (col::text);
        g.setFont (font (13.0f, true));
        g.drawText (name, a.getX() + 26, a.getY() + 4, 110, 18, juce::Justification::centredLeft);
        if (extra.isNotEmpty())
        {
            g.setColour (col::accent);
            g.setFont (font (12.0f, true));
            g.drawText (extra, a.getX() + 26 + 96, a.getY() + 4, a.getWidth() - 26 - 96 - 4 - (a == limArea ? 96 : 0), 18,
                        juce::Justification::centredLeft);
        }
    };
    section (eqArea, "EQ", {});
    section (compArea, "KOMPRESSOR", compGr > 0.05f ? u8 ("GR -") + num (compGr) + " dB" : juce::String());
    section (densArea, "DICHTE", {});
    section (stereoArea, "STEREO / SIDE", {});
    section (limArea, "LIMITER", limGr > 0.05f ? u8 ("GR -") + num (limGr) + " dB" + (limGr > 3.0f ? u8 (" (zu viel)") : juce::String()) : juce::String());

    g.setColour (col::dim);
    g.setFont (font (10.5f, true));
    auto label = [&] (const Knob& k)
    {
        const auto b = k.slider.getBounds();
        g.drawText (k.label, b.getX() - 6, b.getY() - 15, b.getWidth() + 12, 14, juce::Justification::centred);
    };
    g.setFont (font (9.5f, true));
    for (auto& k : eqKnobs)
    {
        const auto b = k.slider.getBounds();
        g.drawFittedText (k.label, b.getX() - 1, b.getY() - 15, b.getWidth() + 2, 14, juce::Justification::centred, 1, 0.75f);
    }
    g.setFont (font (10.5f, true));
    for (auto* k : { &thresh, &ratio, &attack, &release, &makeup, &peak, &densAmount, &densDrive, &monoBelow, &width,
                     &sideMid, &sideAir, &limGain, &ceiling, &limRelease, &limLink })
        label (*k);
}

// ---------------------------------------------------------------------------------------------

CheckEditor::CheckEditor (CheckProcessor& p)
    : AudioProcessorEditor (&p), processor (p), fixPanel (p, [this] (const juce::String& m) { showMessage (m); })
{
    lnf.setColour (juce::ComboBox::backgroundColourId, col::panel);
    lnf.setColour (juce::ComboBox::outlineColourId, col::dim.withAlpha (0.4f));
    lnf.setColour (juce::ComboBox::textColourId, col::text);
    lnf.setColour (juce::ComboBox::arrowColourId, col::accent);
    lnf.setColour (juce::PopupMenu::backgroundColourId, col::panel);
    lnf.setColour (juce::PopupMenu::textColourId, col::text);
    lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    lnf.setColour (juce::TextButton::buttonColourId, col::panel.brighter (0.1f));
    lnf.setColour (juce::TextButton::buttonOnColourId, col::accent);
    lnf.setColour (juce::TextButton::textColourOffId, col::text);
    lnf.setColour (juce::TextButton::textColourOnId, col::background);
    lnf.setColour (juce::ToggleButton::textColourId, col::text);
    lnf.setColour (juce::ToggleButton::tickColourId, col::accent);
    lnf.setColour (juce::ToggleButton::tickDisabledColourId, col::dim);
    lnf.setColour (juce::ScrollBar::thumbColourId, col::dim.withAlpha (0.6f));
    setLookAndFeel (&lnf);

    for (int i = 0; i < numTargets; ++i)
        targetBox.addItem (u8 ("Ziel: ") + u8 (loudnessTargets[(size_t) i].name), i + 1);
    addAndMakeVisible (fixPanel);
    targetBox.onChange = [this] { processor.setTarget (targetBox.getSelectedId() - 1); refresh(); };
    addAndMakeVisible (targetBox);

    for (int i = 0; i < numCurves; ++i)
        curveBox.addItem (u8 (curveNames[(size_t) i]), i + 1);
    curveBox.onChange = [this] { processor.setCurve (curveBox.getSelectedId() - 1); refresh(); };
    addAndMakeVisible (curveBox);

    resetButton.setButtonText ("Reset");
    resetButton.setTooltip (u8 ("Messung neu starten"));
    resetButton.onClick = [this] { processor.requestReset(); };
    addAndMakeVisible (resetButton);

    learnButton.setButtonText (u8 ("Referenz laden …"));
    learnButton.setTooltip (u8 ("Song auswählen oder einfach eine Audiodatei auf das Plugin ziehen"));
    learnButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (u8 ("Referenz-Song wählen"), juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile())
                                      processor.loadReferenceFile (f);
                              });
    };
    addAndMakeVisible (learnButton);

    autoResetToggle.setButtonText (u8 ("Neu bei Play"));
    autoResetToggle.setToggleState (processor.autoReset.load(), juce::dontSendNotification);
    autoResetToggle.onClick = [this] { processor.autoReset.store (autoResetToggle.getToggleState()); };
    addAndMakeVisible (autoResetToggle);

    hintsViewport.setViewedComponent (&hintsView, false);
    hintsViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (hintsViewport);

    setSize (editorWidth, editorHeight);
    refresh();
    startTimerHz (15);
}

CheckEditor::~CheckEditor() { setLookAndFeel (nullptr); }

void CheckEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 0);
    auto header = r.removeFromTop (56).reduced (0, 12);
    header.removeFromLeft (330); // title + status
    autoResetToggle.setBounds (header.removeFromRight (120));
    header.removeFromRight (8);
    resetButton.setBounds (header.removeFromRight (70));
    header.removeFromRight (8);
    learnButton.setBounds (header.removeFromRight (190));
    header.removeFromRight (8);
    curveBox.setBounds (header.removeFromRight (190));
    header.removeFromRight (8);
    targetBox.setBounds (header.removeFromRight (std::max (150, header.getWidth())));

    auto top = r.removeFromTop (360);
    tilesArea = top.removeFromLeft (290);
    top.removeFromLeft (14);
    historyArea = top.removeFromTop (110);
    top.removeFromTop (12);
    spectrumArea = top;

    r.removeFromTop (12);
    fixPanel.setBounds (r.removeFromTop (200));
    r.removeFromTop (12);
    auto hintsArea = r.removeFromTop (r.getHeight() - 14);
    hintsViewport.setBounds (hintsArea.withTrimmedTop (30).reduced (2));
    hintsView.setHints (this->hints, hintsViewport.getWidth() - hintsViewport.getScrollBarThickness());
}

void CheckEditor::timerCallback() { refresh(); }

void CheckEditor::refresh()
{
    meas = processor.getMeasurements();
    settings = processor.getSettings();
    bands = analyseBands (meas.spectrum, settings);

    // Live spectrum as power in a 1/3-octave window around each pixel, using the same
    // offset as the long-term curve so both line up with the reference.
    rawOffset = 0;
    if (bands.valid)
    {
        const auto raw = bandPowers (meas.spectrum.longTerm, meas.spectrum.sampleRate, SpectrumAnalyser::fftSize);
        for (int b = 0; b < numBands; ++b)
            if (raw[(size_t) b] > 1.0e-20f && b == 16 /* 1 kHz */)
                rawOffset = 10.0f * std::log10 (raw[(size_t) b]) - bands.measured[(size_t) b];
    }
    const auto plot = spectrumArea.toFloat().reduced (40.0f, 10.0f).withTrimmedBottom (30.0f);
    fastBandDb.assign ((size_t) std::max (0, (int) plot.getWidth()), -200.0f);
    if (! meas.spectrum.fast.empty() && plot.getWidth() > 0)
    {
        const auto& f = meas.spectrum.fast;
        std::vector<double> prefix (f.size() + 1, 0.0);
        for (size_t k = 0; k < f.size(); ++k)
            prefix[k + 1] = prefix[k] + f[k];
        const double binHz = meas.spectrum.sampleRate / SpectrumAnalyser::fftSize;
        const double edge = std::pow (2.0, 1.0 / 6.0);
        for (size_t x = 0; x < fastBandDb.size(); ++x)
        {
            const double fc = xToFreq (plot.getX() + (float) x, plot);
            const double lo = fc / edge, hi = fc * edge;
            const int k0 = juce::jlimit (1, (int) f.size() - 1, (int) std::floor (lo / binHz + 0.5));
            const int k1 = juce::jlimit (k0, (int) f.size() - 1, (int) std::floor (hi / binHz + 0.5));
            double s = (prefix[(size_t) k1 + 1] - prefix[(size_t) k0]) * (hi - lo) / ((k1 - k0 + 1) * binHz);
            fastBandDb[x] = s > 1.0e-20 ? (float) (10.0 * std::log10 (s)) - (bands.valid ? rawOffset : 0.0f) : -200.0f;
        }
    }

    learnButton.setEnabled (! processor.isLoadingReference());
    fixPanel.updateMeters();
    fixPanel.repaint();
    targetBox.setSelectedId (settings.target + 1, juce::dontSendNotification);
    curveBox.setItemEnabled (3, settings.reference.valid);
    curveBox.setSelectedId (settings.curve + 1, juce::dontSendNotification);
    autoResetToggle.setToggleState (processor.autoReset.load(), juce::dontSendNotification);

    // Advice changes slowly; recompute it twice a second.
    if (tick++ % 7 == 0 || hints.empty())
    {
        hints = makeHints (meas, settings);
        verdict = overallLevel (hints);
        if (hintsViewport.getWidth() > 0)
            hintsView.setHints (hints, hintsViewport.getWidth() - hintsViewport.getScrollBarThickness());
    }
    repaint();
}

void CheckEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::background);

    // Header
    g.setColour (col::text);
    g.setFont (font (24.0f, true));
    g.drawText ("fckrp", 16, 12, 80, 32, juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("CHECK", 86, 12, 100, 32, juce::Justification::centredLeft);

    juce::String status;
    juce::Colour sc;
    if (processor.isLoadingReference())
    {
        status = u8 ("Analysiere Referenz …");
        sc = col::cool;
    }
    else if (meas.measuredSeconds < 3.0f)
    {
        status = u8 ("Warte auf Audio");
        sc = col::dim;
    }
    else
    {
        status = verdict == 0 ? u8 ("Spotify-ready") : (verdict == 1 ? u8 ("Fast – siehe Tipps") : u8 ("Noch nicht fertig"));
        sc = col::forLevel (verdict);
    }
    g.setFont (font (13.0f, true));
    const int sw = std::min (170, (int) font (13.0f, true).getStringWidthFloat (status) + 24);
    const juce::Rectangle<float> pill (180.0f, 18.0f, (float) sw, 22.0f);
    g.setColour (sc.withAlpha (0.18f));
    g.fillRoundedRectangle (pill, 11.0f);
    g.setColour (sc);
    g.drawFittedText (status, pill.toNearestInt().reduced (8, 0), juce::Justification::centred, 1, 0.8f);

    drawTiles (g, tilesArea);
    drawHistory (g, historyArea);
    drawSpectrum (g, spectrumArea);

    // Hints panel
    auto hr = hintsViewport.getBounds().expanded (2).withTop (hintsViewport.getY() - 32);
    g.setColour (col::panel);
    g.fillRoundedRectangle (hr.toFloat(), 8.0f);
    g.setColour (col::text);
    g.setFont (font (15.0f, true));
    g.drawText (u8 ("WAS DU TUN SOLLTEST"), hr.getX() + 14, hr.getY() + 6, 300, 22, juce::Justification::centredLeft);
    juce::String note = u8 ("Rot = fixen · Gelb = prüfen · Grün = passt · Referenz: Song aufs Plugin ziehen");
    auto noteColour = col::dim;
    const auto refStatus = processor.getReferenceStatus();
    if (juce::Time::getMillisecondCounter() < toastUntil)
    {
        note = toast;
        noteColour = col::cool;
    }
    else if (refStatus.isNotEmpty())
    {
        note = refStatus;
        noteColour = processor.isLoadingReference() ? col::cool : col::warn;
    }
    g.setColour (noteColour);
    g.setFont (font (12.5f, noteColour != col::dim));
    g.drawText (note, hr.getX() + 230, hr.getY() + 6, hr.getWidth() - 244, 22, juce::Justification::centredRight);

    if (dragging)
    {
        g.setColour (col::background.withAlpha (0.85f));
        g.fillRect (getLocalBounds());
        g.setColour (col::accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (20.0f), 12.0f, 3.0f);
        g.setFont (font (28.0f, true));
        g.drawText (u8 ("Loslassen = Song als Referenz nehmen"), getLocalBounds(), juce::Justification::centred);
    }
}

void CheckEditor::drawTiles (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto t = targetFor (settings);
    const bool have = meas.measuredSeconds >= 3.0f && meas.integrated > -69.0f;
    const float tp = meas.maxTruePeakDb;
    const float plr = tp - meas.integrated;

    auto tile = [&g] (juce::Rectangle<int> r, const juce::String& label, const juce::String& value, const juce::String& unit,
                      juce::Colour c, float valueSize, const juce::String& sub)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (c);
        g.fillRoundedRectangle (r.toFloat().removeFromLeft (4.0f).reduced (0.0f, 8.0f), 2.0f);
        auto in = r.reduced (14, r.getHeight() < 80 ? 5 : 8);
        g.setColour (col::dim);
        g.setFont (font (11.5f, true));
        g.drawText (label, in.removeFromTop (16), juce::Justification::centredLeft);
        if (sub.isNotEmpty())
        {
            g.setFont (font (11.0f));
            g.drawText (sub, in.removeFromBottom (14), juce::Justification::centredLeft);
        }
        g.setColour (col::text);
        g.setFont (font (valueSize, true));
        const int vw = (int) font (valueSize, true).getStringWidthFloat (value) + 4;
        g.drawText (value, in.withWidth (vw), juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (font (12.0f));
        g.drawText (unit, in.withTrimmedLeft (vw + 2), juce::Justification::centredLeft);
    };

    const float diff = meas.integrated - t.lufs;
    const int iLevel = ! have ? -1 : (std::abs (diff) <= t.tolerance ? 0 : (std::abs (diff) <= 3.0f ? 1 : 2));
    auto levelColour = [] (int l) { return l < 0 ? col::dim : col::forLevel (l); };

    auto big = area.removeFromTop (96);
    tile (big, u8 ("INTEGRIERT (GANZER SONG)"), lufsText (have ? meas.integrated : -200.0f), "LUFS",
          levelColour (iLevel), 40.0f,
          have ? u8 ("Ziel ") + num (t.lufs, 1) + u8 (" · Abweichung ") + signedNum (diff) + " dB"
                     + u8 (" · ") + num (meas.measuredSeconds, 0) + " s"
               : u8 ("Ziel ") + num (t.lufs, 1) + u8 (" LUFS · spiel den Song ab"));
    area.removeFromTop (10);

    const int w = (area.getWidth() - 10) / 2, h = (area.getHeight() - 24) / 4;
    auto cell = [&] (int c, int row)
    { return juce::Rectangle<int> (area.getX() + c * (w + 10), area.getY() + row * (h + 8), w, h); };

    tile (cell (0, 0), "SHORT-TERM (3 s)", lufsText (meas.shortTerm), "LUFS", col::dim, 21.0f,
          meas.maxShortTerm > -69.0f ? u8 ("max ") + num (meas.maxShortTerm) : juce::String());
    tile (cell (1, 0), "MOMENTARY", lufsText (meas.momentary), "LUFS", col::dim, 21.0f, {});

    const int tpLevel = ! have ? -1 : (tp > -1.0f ? 2 : ((tp > -2.0f && meas.integrated > -14.0f) ? 1 : 0));
    tile (cell (0, 1), "TRUE PEAK MAX", tp < -100.0f ? juce::String ("--") : signedNum (tp), "dBTP",
          levelColour (tpLevel), 21.0f,
          meas.overs > 0 ? juce::String (meas.overs) + u8 (" × über -1 dB") : u8 ("Ziel ≤ -1,0"));
    const bool loudTarget = t.lufs > -12.0f;
    const int plrLevel = ! have ? -1 : (plr < 5.0f ? 2 : (plr < 6.5f || (plr > 13.0f && loudTarget) ? 1 : 0));
    tile (cell (1, 1), u8 ("PLR (DYNAMIK)"), have ? num (plr) : juce::String ("--"), "dB", levelColour (plrLevel), 21.0f,
          u8 ("Rap gut: 6–9"));

    const int lraLevel = meas.measuredSeconds < 20.0f ? -1 : (meas.lra > 9.0f ? 1 : 0);
    tile (cell (0, 2), "LRA", meas.measuredSeconds >= 20.0f ? num (meas.lra) : juce::String ("--"), "LU",
          levelColour (lraLevel), 21.0f, u8 ("Schwankung"));
    tile (cell (1, 2), "SAMPLE PEAK", meas.maxSamplePeakDb < -100.0f ? juce::String ("--") : signedNum (meas.maxSamplePeakDb),
          "dBFS", meas.clips > 0 ? col::bad : col::dim, 21.0f,
          meas.clips > 0 ? juce::String (meas.clips) + " Clips" : juce::String());

    const bool st = meas.spectrum.frames >= 20;
    const float c = meas.correlation;
    const int cLevel = ! st ? -1 : (c < 0.0f ? 2 : (c < 0.3f ? 1 : 0));
    tile (cell (0, 3), "KORRELATION", st ? num (c, 2) : juce::String ("--"), "", levelColour (cLevel), 21.0f,
          u8 ("live ") + num (processor.spectrum.correlationFast.load(), 2));
    const int mLevel = ! st ? -1 : (meas.monoDropDb < -5.0f ? 2 : (meas.monoDropDb < -3.0f ? 1 : 0));
    tile (cell (1, 3), "MONO-CHECK", st ? signedNum (meas.monoDropDb) : juce::String ("--"), "dB", levelColour (mLevel),
          21.0f, u8 ("Pegel in Mono"));
}

void CheckEditor::drawHistory (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    g.setColour (col::dim);
    g.setFont (font (11.5f, true));
    g.drawText (u8 ("LAUTHEIT ÜBER ZEIT (SHORT-TERM, LETZTE 60 s)"), area.getX() + 14, area.getY() + 6, 400, 16,
                juce::Justification::centredLeft);

    const auto r = area.toFloat().reduced (40.0f, 0.0f).withTrimmedTop (26.0f).withTrimmedBottom (10.0f);
    const float top = 0.0f, bottom = -30.0f;
    auto y = [&] (float l) { return r.getY() + (top - juce::jlimit (bottom, top, l)) / (top - bottom) * r.getHeight(); };

    g.setFont (font (10.0f));
    for (float l = 0.0f; l >= bottom; l -= 10.0f)
    {
        g.setColour (col::dim.withAlpha (0.2f));
        g.drawHorizontalLine ((int) y (l), r.getX(), r.getRight());
        g.setColour (col::dim);
        g.drawText (num (l, 0), (int) r.getX() - 36, (int) y (l) - 7, 30, 14, juce::Justification::centredRight);
    }

    const auto t = targetFor (settings);
    g.setColour (col::good.withAlpha (0.12f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX(), y (t.lufs + t.tolerance), r.getRight(),
                                                            y (t.lufs - t.tolerance)));
    g.setColour (col::good.withAlpha (0.7f));
    g.drawHorizontalLine ((int) y (t.lufs), r.getX(), r.getRight());
    g.setFont (font (10.0f, true));
    g.drawText (u8 ("Ziel"), (int) r.getRight() + 4, (int) y (t.lufs) - 7, 32, 14, juce::Justification::centredLeft);

    const int n = LoudnessMeter::historySize;
    const int w = processor.loudness.historyWrite.load();
    juce::Path p;
    bool started = false;
    for (int i = 0; i < n; ++i)
    {
        const float l = processor.loudness.history[(size_t) ((w + i) % n)].load();
        const float x = r.getX() + (float) i / (n - 1) * r.getWidth();
        if (l < -69.0f)
        {
            started = false;
            continue;
        }
        if (! started)
            p.startNewSubPath (x, y (l));
        else
            p.lineTo (x, y (l));
        started = true;
    }
    g.setColour (col::accent);
    g.strokePath (p, juce::PathStrokeType (2.0f));

    if (meas.integrated > -69.0f)
    {
        g.setColour (col::text.withAlpha (0.6f));
        const float yi = y (meas.integrated);
        for (float x = r.getX(); x < r.getRight(); x += 10.0f)
            g.drawLine (x, yi, std::min (x + 5.0f, r.getRight()), yi, 1.0f);
        g.setFont (font (10.0f));
        g.drawText (u8 ("Integriert"), (int) r.getX() + 4, (int) yi - 15, 80, 13, juce::Justification::centredLeft);
    }
}

void CheckEditor::drawSpectrum (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    g.setColour (col::dim);
    g.setFont (font (11.5f, true));
    const bool custom = settings.curve == 2 && settings.reference.valid;
    g.drawText (u8 ("KLANGBALANCE: DEIN SONG (ORANGE) GEGEN ")
                    + (custom ? u8 ("REFERENZ „") + settings.reference.name + u8 ("“") : u8 ("ZIELKURVE"))
                    + u8 (" (WEISS)"),
                area.getX() + 14, area.getY() + 6, area.getWidth() - 28, 16, juce::Justification::centredLeft);

    const auto r = area.toFloat().reduced (40.0f, 10.0f).withTrimmedBottom (30.0f).withTrimmedTop (16.0f);
    // Target curve with tolerance (before measuring: the raw curve shape)
    const auto& rawCurve = custom ? settings.reference.bands : builtInCurves[(size_t) juce::jlimit (0, 1, settings.curve)];
    std::array<float, numBands> target {}, tol {};
    float maxT = -200.0f;
    for (int b = 0; b < numBands; ++b)
    {
        target[(size_t) b] = bands.valid ? bands.target[(size_t) b] : rawCurve[(size_t) b];
        tol[(size_t) b] = bands.valid ? bands.tolerance[(size_t) b] : 2.5f;
        maxT = std::max (maxT, target[(size_t) b]);
    }
    const float top = maxT + 9.0f, range = 48.0f;
    auto y = [&] (float db) { return r.getY() + juce::jlimit (0.0f, 1.0f, (top - db) / range) * r.getHeight(); };

    // Grid
    g.setFont (font (10.0f));
    for (float f : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = freqToX (f, r);
        g.setColour (col::dim.withAlpha (0.15f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (col::dim);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20,
                    (int) r.getBottom() + 2, 40, 12, juce::Justification::centred);
    }

    juce::Path tolPath, targetPath;
    for (int b = 0; b < numBands; ++b)
    {
        const float x = freqToX (bandCentres[(size_t) b], r);
        if (b == 0)
        {
            tolPath.startNewSubPath (x, y (target[0] + tol[0]));
            targetPath.startNewSubPath (x, y (target[0]));
        }
        else
        {
            tolPath.lineTo (x, y (target[(size_t) b] + tol[(size_t) b]));
            targetPath.lineTo (x, y (target[(size_t) b]));
        }
    }
    for (int b = numBands - 1; b >= 0; --b)
        tolPath.lineTo (freqToX (bandCentres[(size_t) b], r), y (target[(size_t) b] - tol[(size_t) b]));
    tolPath.closeSubPath();
    g.setColour (col::text.withAlpha (0.07f));
    g.fillPath (tolPath);
    g.setColour (col::text.withAlpha (0.7f));
    g.strokePath (targetPath, juce::PathStrokeType (1.5f));

    // Live spectrum (thin)
    if (bands.valid && ! fastBandDb.empty())
    {
        juce::Path live;
        bool started = false;
        for (size_t i = 0; i < fastBandDb.size(); ++i)
        {
            const float v = fastBandDb[i];
            const float x = r.getX() + (float) i;
            if (v < -150.0f || x > r.getRight())
            {
                started = false;
                continue;
            }
            if (! started)
                live.startNewSubPath (x, y (v));
            else
                live.lineTo (x, y (v));
            started = true;
        }
        g.setColour (col::cool.withAlpha (0.45f));
        g.strokePath (live, juce::PathStrokeType (1.0f));
    }

    // Long-term measurement with band dots coloured by deviation
    if (bands.valid)
    {
        juce::Path m;
        for (int b = 0; b < numBands; ++b)
        {
            const float x = freqToX (bandCentres[(size_t) b], r);
            if (b == 0)
                m.startNewSubPath (x, y (bands.measured[0]));
            else
                m.lineTo (x, y (bands.measured[(size_t) b]));
        }
        g.setColour (col::accent);
        g.strokePath (m, juce::PathStrokeType (2.5f));
        for (int b = 0; b < numBands; ++b)
        {
            const float d = std::abs (bands.deviation[(size_t) b]), t = bands.tolerance[(size_t) b];
            const int level = d <= t ? 0 : (d <= t + 2.0f ? 1 : 2);
            const float x = freqToX (bandCentres[(size_t) b], r);
            g.setColour (col::forLevel (level));
            g.fillEllipse (x - 3.5f, y (bands.measured[(size_t) b]) - 3.5f, 7.0f, 7.0f);
        }
    }
    else
    {
        g.setColour (col::dim);
        g.setFont (font (14.0f));
        g.drawText (u8 ("Spiel den Song ab – nach ein paar Sekunden erscheint hier die Klangbalance."), r.toNearestInt(),
                    juce::Justification::centred);
    }

    // Region summary along the bottom
    struct R { float lo, hi; const char* name; };
    const R regs[] = { { 20, 60, "Sub" }, { 60, 250, "Bass" }, { 250, 500, "U-Mitten" }, { 500, 2000, "Mitten" },
                       { 2000, 5000, u8"Präsenz" }, { 5000, 10000, u8"Höhen" }, { 10000, 20000, "Air" } };
    const float ly = r.getBottom() + 14.0f;
    for (const auto& reg : regs)
    {
        const float x0 = freqToX (reg.lo, r), x1 = freqToX (reg.hi, r);
        g.setColour (col::dim.withAlpha (0.3f));
        g.drawVerticalLine ((int) x0, ly, ly + 16.0f);
        juce::String label = u8 (reg.name);
        juce::Colour c = col::dim;
        if (bands.valid)
        {
            double s = 0, t = 0;
            int n = 0;
            for (int b = 0; b < numBands; ++b)
                if (bandCentres[(size_t) b] >= reg.lo && bandCentres[(size_t) b] < reg.hi)
                {
                    s += bands.deviation[(size_t) b];
                    t += bands.tolerance[(size_t) b];
                    ++n;
                }
            if (n > 0)
            {
                const double d = s / n, tt = t / n;
                label << " " << signedNum (d);
                c = col::forLevel (std::abs (d) <= tt ? 0 : (std::abs (d) <= tt + 2.0 ? 1 : 2));
            }
        }
        g.setColour (c);
        g.setFont (font (11.0f, true));
        g.drawFittedText (label, juce::Rectangle<float> (x0, ly, x1 - x0, 16.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

bool CheckEditor::isAudioFile (const juce::String& path)
{
    const auto ext = juce::File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3" || ext == ".m4a" || ext == ".ogg";
}

bool CheckEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f))
            return true;
    return false;
}

void CheckEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragging = false;
    for (auto& f : files)
        if (isAudioFile (f))
        {
            processor.loadReferenceFile (juce::File (f));
            break;
        }
    repaint();
}

void CheckEditor::showMessage (const juce::String& m)
{
    toast = m;
    toastUntil = juce::Time::getMillisecondCounter() + 6000;
    repaint();
}
