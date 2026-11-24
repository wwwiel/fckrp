#pragma once

// Turns the measurements into plain German advice for the master.

#include "dsp/Spectrum.h"

#include <juce_core/juce_core.h>

#include <array>
#include <vector>

namespace mcheck
{
constexpr int numBands = 29; // 1/3 octaves, 25 Hz .. 16 kHz
extern const std::array<float, numBands> bandCentres;

struct LoudnessTarget
{
    const char* name;
    float lufs;
    float tolerance;
};
constexpr int numTargets = 4; // the last one follows the loaded reference
extern const std::array<LoudnessTarget, numTargets> loudnessTargets;

constexpr int numCurves = 3; // Rap/Trap, Pop/R&B, own reference
extern const std::array<const char*, numCurves> curveNames;
extern const std::array<std::array<float, numBands>, 2> builtInCurves;

struct Measurements
{
    float integrated = -200, shortTerm = -200, momentary = -200, lra = 0;
    float maxShortTerm = -200, measuredSeconds = 0;
    float maxTruePeakDb = -200, maxSamplePeakDb = -200;
    int overs = 0, clips = 0;
    float correlation = 1, monoDropDb = 0;
    float compGr = 0, limGr = 0; // typical gain reduction of the fix chain (dB, highest 3 s average)
    bool densityOn = false;
    SpectrumSnapshot spectrum;
};

struct Reference
{
    bool valid = false;
    juce::String name;
    std::array<float, numBands> bands {}; // dB, any offset
    float lufs = -200, plr = 0;
};

struct Settings
{
    int target = 0;
    int curve = 0;
    Reference reference;
};

struct BandAnalysis
{
    bool valid = false;
    float seconds = 0;
    std::array<float, numBands> measured {}; // dB, shifted to line up with the target
    std::array<float, numBands> target {};
    std::array<float, numBands> deviation {};
    std::array<float, numBands> tolerance {};
};

struct Hint
{
    int level = 0; // 0 ok / info, 1 check, 2 fix this
    juce::String title, text;
};

struct Resonance
{
    float freq = 0, excessDb = 0;
};

// Power (linear) of each 1/3-octave band from a per-bin power spectrum.
std::array<float, numBands> bandPowers (const std::vector<float>& bins, double sampleRate, int fftSize);
BandAnalysis analyseBands (const SpectrumSnapshot&, const Settings&);
std::vector<Resonance> findResonances (const SpectrumSnapshot&);
float bassSideToMidDb (const SpectrumSnapshot&, float maxFreq = 120.0f);

LoudnessTarget targetFor (const Settings&);

// Average deviation per advice region (same order as the fix EQ bands); 0 if unknown.
constexpr int numRegions = 7;
std::array<float, numRegions> regionDeviations (const BandAnalysis&, double sampleRate);

std::vector<Hint> makeHints (const Measurements&, const Settings&);
int overallLevel (const std::vector<Hint>&);

// Helpers for German number formatting ("-9,2", "+2,5").
juce::String num (double v, int decimals = 1);
juce::String signedNum (double v, int decimals = 1);
juce::String freqText (double hz);
juce::String u8 (const char* s);
} // namespace mcheck
