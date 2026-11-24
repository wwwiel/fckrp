#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <memory>

using namespace mcheck;

static float linToDb (float x) { return x > 1.0e-10f ? 20.0f * std::log10 (x) : -200.0f; }

juce::AudioProcessorValueTreeState::ParameterLayout CheckProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto db = [] (float v, int) { return String (v, 1) + " dB"; };
    auto ms = [] (float v, int) { return String ((int) std::lround (v)) + " ms"; };

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::fixOn, 1 }, "Fix an", true));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::eqOn, 1 }, "EQ an", false));
    for (int b = 0; b < numEqBands; ++b)
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::eqBand (b), 1 }, String ("EQ ") + String::fromUTF8 (eqNames[(size_t) b]),
            NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::compOn, 1 }, "Komp an", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::compThresh, 1 }, "Komp Threshold",
                                                       NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -14.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::compRatio, 1 }, "Komp Ratio",
                                                       NormalisableRange<float> (1.0f, 8.0f, 0.1f, 0.5f), 2.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + ":1"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::compAttack, 1 }, "Komp Attack",
                                                       NormalisableRange<float> (1.0f, 100.0f, 0.1f, 0.5f), 30.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::compRelease, 1 }, "Komp Release",
                                                       NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.5f), 200.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::compMakeup, 1 }, "Komp Makeup",
                                                       NormalisableRange<float> (0.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::compMix, 1 }, "Komp Mix",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String ((int) std::lround (v * 100)) + " %"; })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::stereoOn, 1 }, "Stereo an", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::monoBelow, 1 }, "Mono unter",
                                                       NormalisableRange<float> (0.0f, 300.0f, 1.0f), 120.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return v < 1.0f ? String ("aus") : String ((int) v) + " Hz"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::width, 1 }, "Breite",
                                                       NormalisableRange<float> (0.0f, 1.5f, 0.01f), 1.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String ((int) std::lround (v * 100)) + " %"; })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::limOn, 1 }, "Limiter an", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::limGain, 1 }, "Limiter Gain",
                                                       NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::limCeiling, 1 }, "Limiter Ceiling",
                                                       NormalisableRange<float> (-3.0f, 0.0f, 0.1f), -1.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::limRelease, 1 }, "Limiter Release",
                                                       NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.5f), 150.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    return layout;
}

CheckProcessor::CheckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "PARAMS", createLayout())
{
    truePeak.setCeiling (std::pow (10.0f, -1.0f / 20.0f));
    prepareToPlay (48000.0, 512);
}

CheckProcessor::~CheckProcessor()
{
    if (referenceThread.joinable())
        referenceThread.join();
}

bool CheckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void CheckProcessor::prepareToPlay (double sampleRate, int)
{
    // Hosts call this again on many occasions; only start over when the rate changes,
    // so a running measurement isn't lost.
    if (! prepared || std::abs (sampleRate - currentRate) > 0.5)
    {
        prepared = true;
        currentRate = sampleRate;
        loudness.prepare (sampleRate);
        truePeak.prepare (sampleRate);
        spectrum.prepare (sampleRate);
        eq.prepare (sampleRate);
        comp.prepare (sampleRate);
        stereo.prepare (sampleRate);
        limiter.prepare (sampleRate);
        setLatencySamples (limiter.latency());
    }
}

void CheckProcessor::doReset()
{
    loudness.reset();
    truePeak.reset();
    spectrum.reset();
}

float CheckProcessor::getParam (const juce::String& id) const { return params.getRawParameterValue (id)->load(); }

void CheckProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = params.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void CheckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Start a fresh measurement when playback starts (if wanted) or on request.
    bool playing = wasPlaying;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            playing = pos->getIsPlaying();
    if (playing && ! wasPlaying && autoReset.load())
        resetRequested.store (true);
    wasPlaying = playing;

    if (resetRequested.exchange (false))
        doReset();
    if (spectrumResetRequested.exchange (false))
        spectrum.reset();

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    float* channels[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };

    // Fix chain. With every module off, audio is only delayed by the limiter look-ahead.
    const bool fix = getParam (ids::fixOn) > 0.5f;
    if (fix && getParam (ids::eqOn) > 0.5f)
    {
        std::array<float, numEqBands> g {};
        for (int b = 0; b < numEqBands; ++b)
            g[(size_t) b] = getParam (ids::eqBand (b));
        eq.setGains (g);
        eq.process (channels, numCh, n);
    }
    if (fix && getParam (ids::compOn) > 0.5f)
    {
        comp.setParams (getParam (ids::compThresh), getParam (ids::compRatio), getParam (ids::compAttack),
                        getParam (ids::compRelease), getParam (ids::compMakeup), getParam (ids::compMix));
        comp.process (channels, numCh, n);
    }
    else
        comp.gr.store (0.0f);
    if (fix && getParam (ids::stereoOn) > 0.5f)
    {
        stereo.setParams (getParam (ids::monoBelow), getParam (ids::width));
        stereo.process (channels, numCh, n);
    }
    const bool lim = fix && getParam (ids::limOn) > 0.5f;
    limiter.setParams (getParam (ids::limGain), getParam (ids::limCeiling), getParam (ids::limRelease));
    limiter.process (channels, numCh, n, lim);

    // Analysis of what leaves the plugin.
    const float* out[2] = { channels[0], channels[1] };
    loudness.process (out, numCh, n);
    truePeak.process (out, numCh, n);
    spectrum.process (out, numCh, n);
}

mcheck::Measurements CheckProcessor::getMeasurements()
{
    mcheck::Measurements m;
    m.integrated = loudness.integrated.load();
    m.shortTerm = loudness.shortTerm.load();
    m.momentary = loudness.momentary.load();
    m.lra = loudness.lra.load();
    m.maxShortTerm = loudness.maxShortTerm.load();
    m.measuredSeconds = loudness.measuredSeconds.load();
    m.maxTruePeakDb = linToDb (truePeak.maxTruePeak.load());
    m.maxSamplePeakDb = linToDb (truePeak.maxSamplePeak.load());
    m.overs = truePeak.overs.load();
    m.clips = truePeak.clips.load();
    m.correlation = spectrum.correlationLong.load();
    m.monoDropDb = spectrum.monoDropDb.load();
    spectrum.getSnapshot (m.spectrum);
    return m;
}

mcheck::Settings CheckProcessor::getSettings()
{
    const juce::ScopedLock sl (settingsLock);
    return settings;
}

void CheckProcessor::setTarget (int t)
{
    const juce::ScopedLock sl (settingsLock);
    settings.target = juce::jlimit (0, mcheck::numTargets - 1, t);
}

void CheckProcessor::setCurve (int c)
{
    const juce::ScopedLock sl (settingsLock);
    settings.curve = juce::jlimit (0, mcheck::numCurves - 1, c);
}

void CheckProcessor::startLearning()
{
    learning.store (true);
    requestReset();
}

void CheckProcessor::finishLearning()
{
    if (! learning.exchange (false))
        return;
    const auto m = getMeasurements();
    if (m.spectrum.frames >= 20 && m.integrated > -69.0f)
    {
        const auto p = mcheck::bandPowers (m.spectrum.longTerm, m.spectrum.sampleRate, mcheck::SpectrumAnalyser::fftSize);
        const juce::ScopedLock sl (settingsLock);
        for (int b = 0; b < mcheck::numBands; ++b)
            settings.reference.bands[(size_t) b] = p[(size_t) b] > 1.0e-20f ? 10.0f * std::log10 (p[(size_t) b]) : -120.0f;
        settings.reference.lufs = m.integrated;
        settings.reference.plr = m.maxTruePeakDb - m.integrated;
        settings.reference.name = u8 ("Live gelernt");
        settings.reference.valid = true;
        settings.curve = 2;
    }
    requestReset();
}

juce::String CheckProcessor::analyseReferenceFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return u8 ("Datei kann nicht gelesen werden (WAV, AIFF, FLAC, MP3, M4A gehen).");
    if (reader->lengthInSamples < (juce::int64) (reader->sampleRate * 10.0))
        return u8 ("Referenz ist zu kurz (mindestens 10 Sekunden).");

    // Own analysers so the running measurement isn't disturbed.
    auto lm = std::make_unique<LoudnessMeter>();
    auto tp = std::make_unique<TruePeakMeter>();
    auto sa = std::make_unique<SpectrumAnalyser>();
    lm->prepare (reader->sampleRate);
    tp->prepare (reader->sampleRate);
    sa->prepare (reader->sampleRate);

    const int chunk = 16384;
    const int numCh = (int) juce::jmin (2u, reader->numChannels);
    juce::AudioBuffer<float> buf (2, chunk);
    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += chunk)
    {
        const int n = (int) juce::jmin ((juce::int64) chunk, reader->lengthInSamples - pos);
        buf.clear();
        reader->read (&buf, 0, n, pos, true, numCh > 1);
        const float* ch[2] = { buf.getReadPointer (0), buf.getReadPointer (numCh > 1 ? 1 : 0) };
        lm->process (ch, numCh, n);
        tp->process (ch, numCh, n);
        sa->process (ch, numCh, n);
    }

    SpectrumSnapshot snap;
    sa->getSnapshot (snap);
    const float lufs = lm->integrated.load();
    if (snap.frames < 20 || lufs < -60.0f)
        return u8 ("In der Datei ist kaum Signal.");

    const auto p = bandPowers (snap.longTerm, snap.sampleRate, SpectrumAnalyser::fftSize);
    Reference ref;
    for (int b = 0; b < numBands; ++b)
        ref.bands[(size_t) b] = p[(size_t) b] > 1.0e-20f ? 10.0f * std::log10 (p[(size_t) b]) : -120.0f;
    ref.lufs = lufs;
    ref.plr = linToDb (tp->maxTruePeak.load()) - lufs;
    ref.name = file.getFileNameWithoutExtension();
    ref.valid = true;

    const juce::ScopedLock sl (settingsLock);
    settings.reference = ref;
    settings.curve = 2;
    return {};
}

void CheckProcessor::loadReferenceFile (const juce::File& file)
{
    if (loadingReference.exchange (true))
        return;
    if (referenceThread.joinable())
        referenceThread.join();
    {
        const juce::ScopedLock sl (settingsLock);
        referenceStatus = u8 ("Analysiere ") + file.getFileName() + u8 (" …");
    }
    referenceThread = std::thread ([this, file]
    {
        const auto err = analyseReferenceFile (file);
        {
            const juce::ScopedLock sl (settingsLock);
            referenceStatus = err;
        }
        loadingReference.store (false);
    });
}

juce::String CheckProcessor::getReferenceStatus()
{
    const juce::ScopedLock sl (settingsLock);
    return referenceStatus;
}

juce::String CheckProcessor::applyAutoEq()
{
    const auto m = getMeasurements();
    const auto s = getSettings();
    const auto a = analyseBands (m.spectrum, s);
    if (! a.valid || a.seconds < 5.0f)
        return u8 ("Erst ein paar Sekunden abspielen, dann Auto-EQ drücken.");
    const auto dev = regionDeviations (a, m.spectrum.sampleRate);
    // Correct about 60 % of each deviation, so it stays musical; small ones are left alone.
    for (int b = 0; b < numEqBands; ++b)
    {
        const float d = dev[(size_t) b];
        if (std::abs (d) < 1.0f)
            continue;
        const float now = getParam (ids::eqBand (b));
        setParam (ids::eqBand (b), juce::jlimit (-6.0f, 6.0f, now - 0.6f * d));
    }
    setParam (ids::eqOn, 1.0f);
    setParam (ids::fixOn, 1.0f);
    spectrumResetRequested.store (true);
    return u8 ("Auto-EQ gesetzt. Spiel den Song nochmal, dann siehst du das Ergebnis.");
}

juce::String CheckProcessor::applyAutoLoudness()
{
    const auto m = getMeasurements();
    if (m.measuredSeconds < 10.0f || m.integrated < -60.0f)
        return u8 ("Erst mindestens 10 Sekunden abspielen (besser den ganzen Song).");
    const auto t = targetFor (getSettings());
    const bool limActive = getParam (ids::limOn) > 0.5f && getParam (ids::fixOn) > 0.5f;
    const float gainNow = limActive ? getParam (ids::limGain) : 0.0f;
    const float newGain = juce::jlimit (0.0f, 24.0f, gainNow + (t.lufs - m.integrated));
    setParam (ids::limGain, newGain);
    setParam (ids::limOn, 1.0f);
    setParam (ids::fixOn, 1.0f);
    requestReset();
    return u8 ("Limiter-Gain auf ") + num (newGain) + u8 (" dB. Spiel den Song nochmal von vorne zum Prüfen.");
}

void CheckProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    const auto s = getSettings();
    juce::XmlElement xml ("FCKRPCHECK");
    xml.setAttribute ("version", 2);
    xml.setAttribute ("target", s.target);
    xml.setAttribute ("curve", s.curve);
    xml.setAttribute ("autoReset", autoReset.load());
    if (s.reference.valid)
    {
        juce::StringArray bands;
        for (float b : s.reference.bands)
            bands.add (juce::String (b, 2));
        auto* ref = xml.createNewChildElement ("REFERENCE");
        ref->setAttribute ("bands", bands.joinIntoString (" "));
        ref->setAttribute ("lufs", s.reference.lufs);
        ref->setAttribute ("plr", s.reference.plr);
        ref->setAttribute ("name", s.reference.name);
    }
    if (auto p = params.copyState().createXml())
        xml.addChildElement (p.release());
    copyXmlToBinary (xml, dest);
}

void CheckProcessor::setStateInformation (const void* data, int size)
{
    const auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("FCKRPCHECK"))
        return;
    if (auto* p = xml->getChildByName (params.state.getType()))
        params.replaceState (juce::ValueTree::fromXml (*p));
    const juce::ScopedLock sl (settingsLock);
    settings.target = juce::jlimit (0, mcheck::numTargets - 1, xml->getIntAttribute ("target", 0));
    settings.curve = juce::jlimit (0, mcheck::numCurves - 1, xml->getIntAttribute ("curve", 0));
    autoReset.store (xml->getBoolAttribute ("autoReset", true));
    settings.reference = {};
    if (auto* ref = xml->getChildByName ("REFERENCE"))
    {
        juce::StringArray bands;
        bands.addTokens (ref->getStringAttribute ("bands"), " ", "");
        if (bands.size() == mcheck::numBands)
        {
            for (int b = 0; b < mcheck::numBands; ++b)
                settings.reference.bands[(size_t) b] = bands[b].getFloatValue();
            settings.reference.lufs = (float) ref->getDoubleAttribute ("lufs", -14.0);
            settings.reference.plr = (float) ref->getDoubleAttribute ("plr", 8.0);
            settings.reference.name = ref->getStringAttribute ("name");
            settings.reference.valid = true;
        }
    }
    if (settings.curve == 2 && ! settings.reference.valid)
        settings.curve = 0;
}

juce::AudioProcessorEditor* CheckProcessor::createEditor() { return new CheckEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CheckProcessor(); }
