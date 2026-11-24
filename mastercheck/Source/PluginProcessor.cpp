#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

static float linToDb (float x) { return x > 1.0e-10f ? 20.0f * std::log10 (x) : -200.0f; }

CheckProcessor::CheckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    truePeak.setCeiling (std::pow (10.0f, -1.0f / 20.0f));
    prepareToPlay (48000.0, 512);
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
    }
}

void CheckProcessor::doReset()
{
    loudness.reset();
    truePeak.reset();
    spectrum.reset();
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

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    // Audio is only read, never changed.
    const float* channels[2] = { buffer.getReadPointer (0), buffer.getReadPointer (numCh > 1 ? 1 : 0) };
    loudness.process (channels, numCh, n);
    truePeak.process (channels, numCh, n);
    spectrum.process (channels, numCh, n);
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
        settings.reference.valid = true;
        settings.curve = 2;
    }
    requestReset();
}

void CheckProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    const auto s = getSettings();
    juce::XmlElement xml ("FCKRPCHECK");
    xml.setAttribute ("version", 1);
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
    }
    copyXmlToBinary (xml, dest);
}

void CheckProcessor::setStateInformation (const void* data, int size)
{
    const auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("FCKRPCHECK"))
        return;
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
            settings.reference.valid = true;
        }
    }
    if (settings.curve == 2 && ! settings.reference.valid)
        settings.curve = 0;
}

juce::AudioProcessorEditor* CheckProcessor::createEditor() { return new CheckEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CheckProcessor(); }
