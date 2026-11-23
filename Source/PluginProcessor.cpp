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

    addFloat (width, "Width", { 0.0f, 2.0f }, 1.0f, "");

    addBool (eqOn, "EQ On", true);
    addFloat (eqLowCut, "Low Cut", skewed (20.0f, 400.0f, 100.0f), 80.0f, "Hz");
    addFloat (eqMud, "Mud", { -12.0f, 6.0f, 0.1f }, 0.0f, "dB");
    addFloat (eqPresence, "Presence", { -6.0f, 12.0f, 0.1f }, 0.0f, "dB");
    addFloat (eqAir, "Air", { -6.0f, 12.0f, 0.1f }, 0.0f, "dB");

    addBool (tameOn, "Tame On", false);
    addFloat (tameDepth, "Tame Depth", { 0.0f, 1.0f }, 0.3f, "");
    addFloat (tameSens, "Tame Sensitivity", { 0.0f, 1.0f }, 0.5f, "");

    addBool (dsOn, "De-Ess On", true);
    addFloat (dsFreq, "De-Ess Freq", skewed (3000.0f, 12000.0f, 6000.0f), 6000.0f, "Hz");
    addFloat (dsThresh, "De-Ess Threshold", { -60.0f, 0.0f, 0.1f }, -30.0f, "dB");

    addBool (compOn, "Opto On", true);
    addFloat (compThresh, "Opto Threshold", { -50.0f, 0.0f, 0.1f }, -20.0f, "dB");
    addFloat (compRatio, "Opto Ratio", skewed (1.5f, 10.0f, 3.0f), 3.0f, ":1");
    addFloat (compAttack, "Opto Attack", skewed (0.5f, 100.0f, 10.0f), 10.0f, "ms");
    addFloat (compRelease, "Opto Release", skewed (0.05f, 5.0f, 0.5f), 0.5f, "s");
    addFloat (compMakeup, "Opto Gain", { 0.0f, 24.0f, 0.1f }, 3.0f, "dB");
    addFloat (compMix, "Opto Mix", { 0.0f, 1.0f }, 1.0f, "");

    addBool (satOn, "Tube On", true);
    addFloat (satDrive, "Tube Drive", { 0.0f, 1.0f }, 0.15f, "");
    addFloat (satChar, "Tube Character", { 0.0f, 1.0f }, 1.0f, "");

    addBool (dblOn, "Doubler On", false);
    addFloat (dblAmount, "Doubler Amount", { 0.0f, 1.0f }, 0.5f, "");
    addFloat (dblDetune, "Doubler Detune", { 0.0f, 1.0f }, 0.5f, "");

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


namespace
{
void atomicMax (std::atomic<float>& a, float v)
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

void atomicMin (std::atomic<int>& a, int v)
{
    int cur = a.load (std::memory_order_relaxed);
    while (v < cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

void atomicMaxInt (std::atomic<int>& a, int v)
{
    int cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

juce::File diagLogFile()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Logs/fckrp-diag.log");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("fckrp/fckrp-diag.log");
   #endif
}
} // namespace

// Writes what the host does and the level after every module to a small
// text file, once a second, but only while audio is flowing.
struct FckrpProcessor::DiagLogger : private juce::Timer
{
    explicit DiagLogger (FckrpProcessor& p) : proc (p)
    {
        file = diagLogFile();
        file.getParentDirectory().createDirectory();
        if (file.getSize() > 2 * 1024 * 1024)
            file.deleteFile();
        write ("\n=== fckrp build " __DATE__ " " __TIME__ " loaded "
               + juce::Time::getCurrentTime().toString (true, true, true, true)
               + " | host: " + juce::PluginHostType().getHostDescription()
               + " | format: " + juce::String (juce::AudioProcessor::getWrapperTypeDescription (proc.wrapperType)));
        startTimer (1000);
    }

    ~DiagLogger() override { stopTimer(); }

    void timerCallback() override
    {
        static const char* names[] = { "in", "gate", "eq", "tame", "deess", "opto", "tube", "dbl", "delay", "hall", "width", "out" };
        float peaks[numStages];
        for (int s = 0; s < numStages; ++s)
            peaks[s] = proc.stagePeak[(size_t) s].exchange (0.0f);

        const int trips = proc.diagSafetyTrips.load() + proc.diagRunawayTrips.load();
        const bool active = peaks[stIn] > 1.0e-5f || peaks[stOut] > 1.0e-5f;
        if (! active && trips == lastTrips && proc.diagPrepares.load() == lastPrepares)
            return;
        lastTrips = trips;
        lastPrepares = proc.diagPrepares.load();
        if (++lines > 3600)
            return; // about an hour of audio is plenty

        juce::String line = juce::Time::getCurrentTime().toString (false, true, true, true)
            + " sr=" + juce::String (proc.diagRate.load(), 0)
            + " prep=" + juce::String (proc.diagPrepares.load()) + "/" + juce::String (proc.diagPrepBlock.load())
            + " blk=" + juce::String (proc.diagMinBlock.exchange (1 << 30)) + ".." + juce::String (proc.diagMaxBlock.exchange (0))
            + " ch=" + juce::String (proc.diagNumIn.load()) + ">" + juce::String (proc.diagNumOut.load())
            + " buf=" + juce::String (proc.diagBufChannels.load())
            + " unprep=" + juce::String (proc.diagUnprepared.load())
            + " nanIn=" + juce::String (proc.diagNonFiniteIn.load())
            + " trips=" + juce::String (proc.diagSafetyTrips.load()) + "/" + juce::String (proc.diagRunawayTrips.load())
            + " |";
        for (int s = 0; s < numStages; ++s)
            line << " " << names[s] << "=" << juce::String (fckrp::gainToDb (peaks[s]), 1);
        write (line);
    }

    void write (const juce::String& text) { file.appendText (text + "\n", false, false, "\n"); }

    FckrpProcessor& proc;
    juce::File file;
    int lastTrips = 0, lastPrepares = -1, lines = 0;
};

FckrpProcessor::~FckrpProcessor() = default;

void FckrpProcessor::markStage (Stage s, const juce::AudioBuffer<float>& b)
{
    atomicMax (stagePeak[(size_t) s], b.getMagnitude (0, b.getNumSamples()));
}

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

    for (auto& p : stagePeak)
        p.store (0.0f);

    // Only log inside a real plugin host, not in the offline tests.
    if (wrapperType != wrapperType_Undefined)
        diagLogger = std::make_unique<DiagLogger> (*this);
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
    diagPrepares.fetch_add (1);
    diagPrepBlock.store (samplesPerBlock);
    diagRate.store (sampleRate);

    const int numCh = std::max (2, getTotalNumOutputChannels());

    // Hosts may send bigger blocks than announced here (FL Studio does), so
    // all internal buffers are sized for a fixed chunk and processBlock()
    // splits whatever arrives into chunks of at most this size.
    samplesPerBlock = maxChunk;

    gate.prepare (sampleRate);
    eq.prepare (sampleRate, numCh);
    tamer.prepare (sampleRate, numCh);
    deEsser.prepare (sampleRate, samplesPerBlock, numCh);
    compressor.prepare (sampleRate);
    saturation.prepare (sampleRate, samplesPerBlock, numCh);
    doubler.prepare (sampleRate);
    stereoWidth.prepare (sampleRate);
    compDry.setSize (numCh, samplesPerBlock);
    compMixSmooth.reset (sampleRate, 0.05);
    delay.prepare (sampleRate, numCh);
    reverb.prepare (sampleRate, samplesPerBlock);
    outputGain.reset (sampleRate, 0.05);

    setLatencySamples ((int) std::round (saturation.getLatencySamples()));
    isPrepared = true;
}

void FckrpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = buffer.getNumChannels();
    const int total = buffer.getNumSamples();

    atomicMin (diagMinBlock, total);
    atomicMaxInt (diagMaxBlock, total);
    diagBufChannels.store (numCh);
    diagNumIn.store (getTotalNumInputChannels());
    diagNumOut.store (getTotalNumOutputChannels());

    // Some hosts call this before prepareToPlay(): output silence then.
    if (! isPrepared)
    {
        diagUnprepared.fetch_add (1);
        buffer.clear();
        return;
    }

    // Never let NaN/Inf from the host into the filters.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < total; ++i)
            if (! std::isfinite (d[i]))
            {
                d[i] = 0.0f;
                diagNonFiniteIn.fetch_add (1);
            }
    }

    const float inPeak = buffer.getMagnitude (0, total);
    inputLevelDb.store (fckrp::gainToDb (inPeak));
    atomicMax (stagePeak[stIn], inPeak);

    for (int start = 0; start < total; start += maxChunk)
    {
        const int len = std::min (maxChunk, total - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numCh, start, len);
        processChunk (chunk);
    }

    // Safety net: if anything ever went wrong inside, silence it and reset
    // instead of sending noise to the speakers, and never exceed +6 dBFS.
    // A level above +60 dBFS inside the plugin can never come from music,
    // even with every gain knob turned up: that is a runaway, so mute too.
    // (Turning Output up must never trigger this, so it only reacts to
    // absurd levels, not to a loud but normal signal.)
    bool bad = false, runawayTrip = false;
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < total; ++i)
        {
            if (! std::isfinite (d[i]))
            {
                bad = true;
                break;
            }
            if (std::abs (d[i]) > 1000.0f)
                runawayTrip = true;
            d[i] = juce::jlimit (-2.0f, 2.0f, d[i]);
        }
    }

    if (bad || runawayTrip)
    {
        (bad ? diagSafetyTrips : diagRunawayTrips).fetch_add (1);
        buffer.clear();
        resetDsp();
    }

    outputLevelDb.store (fckrp::gainToDb (buffer.getMagnitude (0, total)));
}

void FckrpProcessor::resetDsp()
{
    diagPrepares.fetch_sub (1); // not a host call
    prepareToPlay (getSampleRate() > 0.0 ? getSampleRate() : 44100.0, maxChunk);
}

void FckrpProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    using namespace fckrp::ids;

    // The two macro knobs push several settings at once.
    const float pol = get (polish);
    const float spc = get (space);

    if (isOn (gateOn))
    {
        gate.setThreshold (get (gateThresh));
        gate.process (buffer);
    }
    markStage (stGate, buffer);

    if (isOn (eqOn))
    {
        eq.setParameters (get (eqLowCut), get (eqMud),
                          get (eqPresence) + 3.0f * pol,
                          get (eqAir) + 3.0f * pol);
        eq.process (buffer);
    }
    markStage (stEq, buffer);

    float tameCut = 0.0f;
    if (isOn (tameOn))
    {
        tamer.setParameters (get (tameDepth), get (tameSens));
        tameCut = tamer.process (buffer);
    }
    tameCutDb.store (tameCut);
    markStage (stTame, buffer);

    float dsGr = 0.0f;
    if (isOn (dsOn))
    {
        deEsser.setParameters (get (dsFreq), get (dsThresh) - 4.0f * pol);
        dsGr = deEsser.process (buffer);
    }
    deEssGainReductionDb.store (dsGr);
    markStage (stDeEss, buffer);

    float compGr = 0.0f;
    if (isOn (compOn))
    {
        compressor.setParameters (get (compThresh) - 10.0f * pol, get (compRatio), get (compAttack),
                                  get (compRelease), get (compMakeup) + 4.0f * pol);
        // Parallel compression: keep a copy of the uncompressed signal and
        // blend it back in with the Mix knob (100 % = fully compressed).
        const int n = buffer.getNumSamples();
        const int chs = std::min (buffer.getNumChannels(), compDry.getNumChannels());
        for (int ch = 0; ch < chs; ++ch)
            compDry.copyFrom (ch, 0, buffer, ch, 0, n);

        compGr = compressor.process (buffer);

        compMixSmooth.setTargetValue (get (compMix));
        for (int i = 0; i < n; ++i)
        {
            const float m = compMixSmooth.getNextValue();
            for (int ch = 0; ch < chs; ++ch)
            {
                const float wet = buffer.getSample (ch, i);
                buffer.setSample (ch, i, compDry.getSample (ch, i) + m * (wet - compDry.getSample (ch, i)));
            }
        }
    }
    compGainReductionDb.store (compGr);
    markStage (stOpto, buffer);

    saturation.setAmount (get (satDrive) + 0.25f * pol);
    saturation.setCharacter (get (satChar));
    saturation.process (buffer, isOn (satOn));
    markStage (stTube, buffer);

    if (isOn (dblOn))
    {
        doubler.setParameters (get (dblAmount), get (dblDetune));
        doubler.process (buffer);
    }
    markStage (stDoubler, buffer);

    if (isOn (dlyOn))
    {
        delay.setParameters (get (dlyTime), get (dlyFeedback), get (dlyMix) + 0.25f * spc);
        delay.process (buffer);
    }
    markStage (stDelay, buffer);

    if (isOn (revOn))
    {
        reverb.setParameters (get (revSize) + 0.2f * spc, get (revDamp), get (revMix) + 0.35f * spc);
        reverb.process (buffer);
    }
    markStage (stHall, buffer);

    stereoWidth.setWidth (get (width) + 0.25f * spc);
    stereoWidth.process (buffer);
    markStage (stWidth, buffer);

    outputGain.setTargetValue (fckrp::dbToGain (get (output)));
    outputGain.applyGain (buffer, buffer.getNumSamples());
    markStage (stOut, buffer);
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
