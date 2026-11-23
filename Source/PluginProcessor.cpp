#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace fckrp
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ids;
    using FloatParam = juce::AudioParameterFloat;
    using BoolParam = juce::AudioParameterBool;
    using Attr = juce::AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addFloat = [&p] (const char* id, const char* name, juce::NormalisableRange<float> range,
                          float def, const char* unit)
    {
        const juce::String u (unit);
        auto toText = [u] (float v, int) -> juce::String
        {
            if (u == "dB")  return juce::String (v, 1) + " dB";
            if (u == "Hz")  return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
            if (u == "ms")  return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
            if (u == "s")   return juce::String (v, 2) + " s";
            if (u == ":1")  return juce::String (v, 1) + ":1";
            return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        };
        auto fromText = [u] (const juce::String& t) -> float
        {
            const float v = t.getFloatValue();
            if (u.isEmpty())                                  return v / 100.0f;
            if (u == "Hz" && t.containsIgnoreCase ("k"))      return v * 1000.0f;
            return v;
        };
        p.push_back (std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name, range, def,
                                                   Attr().withLabel (unit)
                                                         .withStringFromValueFunction (toText)
                                                         .withValueFromStringFunction (fromText)));
    };
    auto addBool = [&p] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<BoolParam> (juce::ParameterID { id, 1 }, name, def));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    addFloat (polish, "Polish", { 0.0f, 1.0f }, 0.3f, "");
    addFloat (space, "Space", { 0.0f, 1.0f }, 0.3f, "");
    addFloat (output, "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, "dB");

    addBool (gateOn, "Gate On", true);
    addFloat (gateThresh, "Gate Threshold", { -80.0f, -20.0f, 0.1f }, -60.0f, "dB");

    addBool (eqOn, "EQ On", true);
    addFloat (eqLowCut, "Low Cut", skewed (20.0f, 400.0f, 100.0f), 80.0f, "Hz");
    addFloat (eqMud, "Mud", { -12.0f, 6.0f, 0.1f }, 0.0f, "dB");
    addFloat (eqPresence, "Presence", { -6.0f, 12.0f, 0.1f }, 0.0f, "dB");
    addFloat (eqAir, "Air", { -6.0f, 12.0f, 0.1f }, 0.0f, "dB");

    addBool (dsOn, "De-Ess On", true);
    addFloat (dsFreq, "De-Ess Freq", skewed (3000.0f, 12000.0f, 6000.0f), 6000.0f, "Hz");
    addFloat (dsThresh, "De-Ess Threshold", { -60.0f, 0.0f, 0.1f }, -30.0f, "dB");

    addBool (compOn, "Opto On", true);
    addFloat (compThresh, "Opto Threshold", { -50.0f, 0.0f, 0.1f }, -20.0f, "dB");
    addFloat (compRatio, "Opto Ratio", skewed (1.5f, 10.0f, 3.0f), 3.0f, ":1");
    addFloat (compAttack, "Opto Attack", skewed (0.5f, 100.0f, 10.0f), 10.0f, "ms");
    addFloat (compRelease, "Opto Release", skewed (0.05f, 5.0f, 0.5f), 0.5f, "s");
    addFloat (compMakeup, "Opto Gain", { 0.0f, 24.0f, 0.1f }, 3.0f, "dB");

    addBool (satOn, "Tube On", true);
    addFloat (satDrive, "Tube Drive", { 0.0f, 1.0f }, 0.15f, "");

    addBool (dlyOn, "Delay On", true);
    addFloat (dlyTime, "Delay Time", skewed (20.0f, 1500.0f, 300.0f), 375.0f, "ms");
    addFloat (dlyFeedback, "Delay Feedback", { 0.0f, 0.9f }, 0.25f, "");
    addFloat (dlyMix, "Delay Mix", { 0.0f, 1.0f }, 0.10f, "");

    addBool (revOn, "Hall On", true);
    addFloat (revSize, "Hall Size", { 0.0f, 1.0f }, 0.5f, "");
    addFloat (revDamp, "Hall Damping", { 0.0f, 1.0f }, 0.5f, "");
    addFloat (revMix, "Hall Mix", { 0.0f, 1.0f }, 0.15f, "");

    return { p.begin(), p.end() };
}
} // namespace fckrp

FckrpProcessor::FckrpProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "fckrp", fckrp::createParameterLayout())
{
    for (const char* id : fckrp::ids::all)
    {
        auto* value = apvts.getRawParameterValue (id);
        jassert (value != nullptr);
        params[id] = value;
    }
}

bool FckrpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void FckrpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = std::max (1, getTotalNumOutputChannels());

    gate.prepare (sampleRate);
    eq.prepare (sampleRate, numCh);
    deEsser.prepare (sampleRate, samplesPerBlock, numCh);
    compressor.prepare (sampleRate);
    saturation.prepare (sampleRate, samplesPerBlock, numCh);
    delay.prepare (sampleRate, numCh);
    reverb.prepare (sampleRate, samplesPerBlock);
    outputGain.reset (sampleRate, 0.05);

    setLatencySamples ((int) std::round (saturation.getLatencySamples()));
}

void FckrpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    using namespace fckrp::ids;
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    inputLevelDb.store (fckrp::gainToDb (buffer.getMagnitude (0, buffer.getNumSamples())));

    // The two macro knobs push several settings at once.
    const float pol = get (polish);
    const float spc = get (space);

    if (isOn (gateOn))
    {
        gate.setThreshold (get (gateThresh));
        gate.process (buffer);
    }

    if (isOn (eqOn))
    {
        eq.setParameters (get (eqLowCut), get (eqMud),
                          get (eqPresence) + 3.0f * pol,
                          get (eqAir) + 3.0f * pol);
        eq.process (buffer);
    }

    float dsGr = 0.0f;
    if (isOn (dsOn))
    {
        deEsser.setParameters (get (dsFreq), get (dsThresh) - 4.0f * pol);
        dsGr = deEsser.process (buffer);
    }
    deEssGainReductionDb.store (dsGr);

    float compGr = 0.0f;
    if (isOn (compOn))
    {
        compressor.setParameters (get (compThresh) - 10.0f * pol, get (compRatio), get (compAttack),
                                  get (compRelease), get (compMakeup) + 4.0f * pol);
        compGr = compressor.process (buffer);
    }
    compGainReductionDb.store (compGr);

    saturation.setAmount (get (satDrive) + 0.25f * pol);
    saturation.process (buffer, isOn (satOn));

    if (isOn (dlyOn))
    {
        delay.setParameters (get (dlyTime), get (dlyFeedback), get (dlyMix) + 0.25f * spc);
        delay.process (buffer);
    }

    if (isOn (revOn))
    {
        reverb.setParameters (get (revSize) + 0.2f * spc, get (revDamp), get (revMix) + 0.35f * spc);
        reverb.process (buffer);
    }

    outputGain.setTargetValue (fckrp::dbToGain (get (output)));
    outputGain.applyGain (buffer, buffer.getNumSamples());

    outputLevelDb.store (fckrp::gainToDb (buffer.getMagnitude (0, buffer.getNumSamples())));
}

juce::AudioProcessorEditor* FckrpProcessor::createEditor()
{
    return new FckrpEditor (*this);
}

int FckrpProcessor::getNumPrograms()
{
    return (int) fckrp::getFactoryPresets().size();
}

const juce::String FckrpProcessor::getProgramName (int index)
{
    const auto& presets = fckrp::getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void FckrpProcessor::setCurrentProgram (int index)
{
    const auto& presets = fckrp::getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;
    const auto& preset = presets[(size_t) index];

    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr)
            continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        for (const auto& [id, v] : preset.values)
            if (id == rp->getParameterID())
                value = v;

        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }
}

void FckrpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FckrpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FckrpProcessor();
}
