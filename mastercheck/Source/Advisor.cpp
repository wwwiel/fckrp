#include "Advisor.h"

#include <algorithm>
#include <cmath>

namespace mcheck
{
const std::array<float, numBands> bandCentres { 25, 31.5f, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630,
                                                800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000,
                                                12500, 16000 };

const std::array<LoudnessTarget, numTargets> loudnessTargets { {
    { "Spotify (-14 LUFS)", -14.0f, 1.0f },
    { "Rap laut (-9 LUFS)", -9.0f, 1.5f },
    { "Maximal laut (-7 LUFS)", -7.0f, 1.5f },
} };

const std::array<const char*, numCurves> curveNames { "Kurve: Rap / Trap", "Kurve: Pop / R&B", "Kurve: Eigene Referenz" };

// Average tonal balance of finished masters, as power per 1/3-octave band in dB.
// Only the shape matters; the measurement is shifted onto it. These are a
// starting point; a learned reference track is more accurate.
const std::array<std::array<float, numBands>, 2> builtInCurves { {
    // Rap / Trap: strong sub and 808, presence for the vocal, soft top end
    { -3.0f, -1.0f, 0.0f, 0.5f, 0.5f, 0.0f, -1.0f, -2.5f, -4.0f, -5.5f, -7.0f, -8.0f, -9.0f, -10.0f, -11.0f,
      -12.0f, -13.0f, -14.0f, -15.0f, -16.0f, -17.0f, -18.2f, -19.5f, -21.0f, -22.5f, -24.0f, -26.0f, -28.5f, -32.0f },
    // Pop / R&B: less sub, a bit brighter
    { -6.0f, -3.5f, -2.0f, -1.0f, -0.5f, 0.0f, -0.5f, -1.5f, -2.5f, -3.5f, -4.5f, -5.5f, -6.5f, -7.5f, -8.5f,
      -9.5f, -10.5f, -11.5f, -12.5f, -13.5f, -14.5f, -15.5f, -16.7f, -18.0f, -19.5f, -21.0f, -23.0f, -25.5f, -29.0f },
} };

juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::String num (double v, int decimals)
{
    if (std::abs (v) < 0.5 * std::pow (10.0, -decimals))
        v = 0.0; // no "-0,0"
    if (decimals <= 0)
        return juce::String ((int) std::lround (v)); // String (double, 0) would print full precision
    return juce::String (v, decimals).replaceCharacter ('.', ',');
}

juce::String signedNum (double v, int decimals)
{
    const auto s = num (v, decimals);
    return (v > 0 && s != num (0.0, decimals)) ? "+" + s : s;
}

juce::String freqText (double hz)
{
    if (hz >= 1000.0)
    {
        const double k = hz / 1000.0;
        return (k >= 10.0 ? num (k, 0) : num (k, 1)) + " kHz";
    }
    return juce::String ((int) std::lround (hz)) + " Hz";
}

static float toDb (double p) { return p > 1.0e-20 ? (float) (10.0 * std::log10 (p)) : -200.0f; }

std::array<float, numBands> bandPowers (const std::vector<float>& bins, double sampleRate, int fftSize)
{
    std::array<float, numBands> out {};
    const double binHz = sampleRate / fftSize;
    const double edge = std::pow (2.0, 1.0 / 6.0);
    const int n = (int) bins.size();
    for (int b = 0; b < numBands; ++b)
    {
        const double lo = bandCentres[(size_t) b] / edge, hi = bandCentres[(size_t) b] * edge;
        int k0 = (int) std::ceil (lo / binHz), k1 = (int) std::ceil (hi / binHz);
        k0 = std::clamp (k0, 1, n);
        k1 = std::clamp (k1, k0 + 1, n);
        double s = 0;
        for (int k = k0; k < k1 && k < n; ++k)
            s += bins[(size_t) k];
        // Narrow low bands may contain fewer bins than their width: scale to the band width.
        const double covered = (k1 - k0) * binHz;
        s *= (hi - lo) / std::max (covered, binHz);
        out[(size_t) b] = (float) s;
    }
    return out;
}

static const std::array<float, numBands>& curveFor (const Settings& s)
{
    if (s.curve == 2 && s.reference.valid)
        return s.reference.bands;
    return builtInCurves[(size_t) std::clamp (s.curve, 0, 1)];
}

BandAnalysis analyseBands (const SpectrumSnapshot& snap, const Settings& settings)
{
    BandAnalysis a;
    a.seconds = (float) ((double) snap.frames * SpectrumAnalyser::hop / std::max (1.0, snap.sampleRate));
    if (snap.frames < 4 || snap.longTerm.empty())
        return a;

    const auto p = bandPowers (snap.longTerm, snap.sampleRate, SpectrumAnalyser::fftSize);
    const auto& curve = curveFor (settings);
    std::vector<float> diffs;
    for (int b = 0; b < numBands; ++b)
    {
        a.measured[(size_t) b] = toDb (p[(size_t) b]);
        a.target[(size_t) b] = curve[(size_t) b];
        // Wider tolerance where hearing and playback systems vary most.
        const float f = bandCentres[(size_t) b];
        a.tolerance[(size_t) b] = f < 60.0f ? 4.0f : (f < 200.0f ? 3.0f : (f > 10000.0f ? 4.0f : 2.5f));
        if (f >= 40.0f && f <= 12500.0f && a.measured[(size_t) b] > -190.0f)
            diffs.push_back (a.measured[(size_t) b] - a.target[(size_t) b]);
    }
    if (diffs.size() < 10)
        return a;

    // Line up by the median difference so one loud region doesn't shift everything.
    std::sort (diffs.begin(), diffs.end());
    const float offset = diffs.size() % 2 == 1 ? diffs[diffs.size() / 2]
                                               : 0.5f * (diffs[diffs.size() / 2 - 1] + diffs[diffs.size() / 2]);
    for (int b = 0; b < numBands; ++b)
    {
        a.measured[(size_t) b] -= offset;
        a.deviation[(size_t) b] = a.measured[(size_t) b] - a.target[(size_t) b];
    }
    a.valid = true;
    return a;
}

std::vector<Resonance> findResonances (const SpectrumSnapshot& snap)
{
    std::vector<Resonance> out;
    if (snap.frames < 20 || snap.longTerm.empty())
        return out;

    // Resample onto a 1/24-octave grid, then compare each point with its 1-octave neighbourhood.
    const double binHz = snap.sampleRate / SpectrumAnalyser::fftSize;
    const int n = (int) snap.longTerm.size();
    const double f0 = 150.0, f1 = std::min (15000.0, snap.sampleRate * 0.45);
    const int perOct = 24;
    const int points = (int) (std::log2 (f1 / f0) * perOct) + 1;
    std::vector<float> grid ((size_t) points), freqs ((size_t) points);
    for (int i = 0; i < points; ++i)
    {
        const double f = f0 * std::pow (2.0, (double) i / perOct);
        const double lo = f * std::pow (2.0, -0.5 / perOct), hi = f * std::pow (2.0, 0.5 / perOct);
        int k0 = (int) std::floor (lo / binHz + 0.5), k1 = (int) std::floor (hi / binHz + 0.5);
        k0 = std::clamp (k0, 1, n - 1);
        k1 = std::clamp (k1, k0, n - 1);
        double s = 0;
        for (int k = k0; k <= k1; ++k)
            s += snap.longTerm[(size_t) k];
        grid[(size_t) i] = toDb (s / (k1 - k0 + 1));
        freqs[(size_t) i] = (float) f;
    }

    std::vector<Resonance> cands;
    const int half = perOct / 2;
    for (int i = half; i < points - half; ++i)
    {
        if (freqs[(size_t) i] < 300.0f)
            continue;
        double s = 0;
        for (int j = i - half; j <= i + half; ++j)
            if (std::abs (j - i) > 2)
                s += grid[(size_t) j];
        const float broad = (float) (s / (2 * half - 4));
        const float excess = grid[(size_t) i] - broad;
        const bool isMax = grid[(size_t) i] >= grid[(size_t) (i - 1)] && grid[(size_t) i] >= grid[(size_t) (i + 1)];
        if (isMax && excess > 7.0f)
            cands.push_back ({ freqs[(size_t) i], excess });
    }
    std::sort (cands.begin(), cands.end(), [] (auto& a, auto& b) { return a.excessDb > b.excessDb; });
    for (auto& c : cands)
    {
        bool near = false;
        for (auto& o : out)
            near = near || std::abs (std::log2 (c.freq / o.freq)) < 1.0 / 3.0;
        if (! near)
            out.push_back (c);
        if (out.size() >= 3)
            break;
    }
    return out;
}

float bassSideToMidDb (const SpectrumSnapshot& snap, float maxFreq)
{
    if (snap.frames < 4 || snap.midLong.empty())
        return -200.0f;
    const double binHz = snap.sampleRate / SpectrumAnalyser::fftSize;
    double m = 0, s = 0;
    for (int k = 1; k < (int) snap.midLong.size() && k * binHz <= maxFreq; ++k)
        if (k * binHz >= 25.0)
        {
            m += snap.midLong[(size_t) k];
            s += snap.sideLong[(size_t) k];
        }
    if (m < 1.0e-14)
        return -200.0f;
    return toDb (s / m);
}

namespace
{
struct Region
{
    float lo, hi;
    const char* name;
    const char* tooLoud;  // what it sounds like when too loud
    const char* tooQuiet;
    const char* sources;  // what usually lives there
    bool shelf;           // suggest a shelf instead of a bell
};

const Region regions[] = {
    { 20, 60, "Sub (20–60 Hz)", "dröhnt, frisst Headroom und Lautheit", "wenig Fundament", "808 / Sub-Bass", true },
    { 60, 250, "Bass (60–250 Hz)", "dick und matschig", "dünn, kein Druck", "Kick, 808, Bass", false },
    { 250, 500, "Untere Mitten (250–500 Hz)", "mulmig, boxy", "hohl, dünn", "Vocal-Körper, Snare, Keys", false },
    { 500, 2000, "Mitten (500 Hz–2 kHz)", "nasal, blechern", "hohl, weit weg", "Vocal, Melodie, Snare", false },
    { 2000, 5000, "Präsenz (2–5 kHz)", "hart, aggressiv, nervt bei Lautstärke", "Vocal undeutlich, geht unter",
      "Vocal, Snare, Leads", false },
    { 5000, 10000, "Höhen (5–10 kHz)", "scharf, zischelig (S-Laute, Hi-Hats)", "dumpf", "S-Laute, Hi-Hats, Becken", false },
    { 10000, 17000, "Air (10–16 kHz)", "spitz, zu hell", "wenig Glanz", "Hi-Hats, Vocal-Air", true },
};

void addLoudnessHints (std::vector<Hint>& hints, const Measurements& m, const Settings& s)
{
    const auto& t = loudnessTargets[(size_t) std::clamp (s.target, 0, numTargets - 1)];
    const float i = m.integrated;
    const float diff = i - t.lufs;
    const juce::String early = m.measuredSeconds < 30.0f ? u8 (" (Erst ") + num (m.measuredSeconds, 0)
                                                               + u8 (" s gemessen – spiel den ganzen Song.)")
                                                         : juce::String();
    const float tp = m.maxTruePeakDb;
    const float plr = tp - i;

    if (diff > t.tolerance)
        hints.push_back ({ diff > 3.0f ? 2 : 1, u8 ("Lauter als dein Ziel"),
                           u8 ("Integriert ") + num (i) + u8 (" LUFS, Ziel ") + num (t.lufs, 0)
                               + u8 (". Nimm am Limiter ca. ") + num (diff) + u8 (" dB Gain zurück. Lauter bringt auf Spotify nichts, es wird eh runtergedreht – du verlierst nur Punch.")
                               + early });
    else if (diff < -t.tolerance)
    {
        juce::String txt = u8 ("Integriert ") + num (i) + u8 (" LUFS, Ziel ") + num (t.lufs, 0)
                           + u8 (". Gib am Limiter ca. ") + num (-diff) + u8 (" dB mehr Gain rein (Ceiling bleibt bei -1 dB).");
        if (plr < 7.0f)
            txt << u8 (" Achtung: Die Dynamik ist schon knapp. Hol die Lautheit lieber über einen sanften Bus-Kompressor (2:1, 2–3 dB) als nur über den Limiter.");
        hints.push_back ({ 1, u8 ("Leiser als dein Ziel"), txt + early });
    }
    else
        hints.push_back ({ 0, u8 ("Lautheit passt"),
                           u8 ("Integriert ") + num (i) + u8 (" LUFS, Ziel ") + num (t.lufs, 0) + "." + early });

    if (i > -14.0f)
        hints.push_back ({ 0, u8 ("So macht es Spotify"),
                           u8 ("Spotify dreht deinen Song um ca. ") + num (i + 14.0f) + u8 (" dB leiser (Normalisierung auf -14 LUFS). Danach klingt der Master mit mehr Punch oft voller als der lautere.") });
    else if (i < -15.0f)
        hints.push_back ({ 0, u8 ("So macht es Spotify"),
                           u8 ("Spotify macht leise Songs nur so weit lauter, wie die Peaks es erlauben (bis -1 dBTP). Dein Song kann dort leiser wirken als andere.") });

    if (s.reference.valid)
        hints.push_back ({ 0, u8 ("Deine Referenz"),
                           u8 ("Referenz: ") + num (s.reference.lufs) + u8 (" LUFS, PLR ") + num (s.reference.plr)
                               + u8 (" dB. Du: ") + num (i) + u8 (" LUFS, PLR ") + num (plr) + " dB." });

    // True peak
    if (tp > -1.0f)
    {
        juce::String txt = u8 ("Höchster True Peak ") + signedNum (tp) + u8 (" dBTP");
        if (m.overs > 0)
            txt << " (" << m.overs << u8 (" Mal über -1 dB)");
        txt << u8 (". Stell Ceiling/Output am Limiter auf -1,0 dB und schalte True Peak (ISP) an. Hat dein Limiter das nicht: Ceiling -1,5 bis -2 dB.");
        if (m.clips > 0)
            txt << " " << m.clips << u8 (" Samples liegen bei 0 dBFS – das ist echtes Clipping.");
        hints.push_back ({ 2, u8 ("True Peak zu hoch"), txt });
    }
    else if (tp > -2.0f && i > -14.0f)
        hints.push_back ({ 1, u8 ("True Peak knapp"),
                           u8 ("True Peak ") + num (tp) + u8 (" dBTP. Spotify empfiehlt bei Masters lauter als -14 LUFS max. -2 dBTP, weil beim Umwandeln (Ogg/AAC) neue Peaks entstehen. Ceiling auf -2 dB ist sicherer.") });
    else
        hints.push_back ({ 0, u8 ("True Peak ok"), u8 ("Höchster True Peak ") + num (tp) + u8 (" dBTP.") });

    // Dynamics
    const bool loudTarget = t.lufs > -12.0f;
    if (plr < 5.0f)
        hints.push_back ({ 2, u8 ("Zu stark limitiert"),
                           u8 ("PLR nur ") + num (plr) + u8 (" dB: Der Master ist platt gedrückt, Kick und 808 verlieren Punch und es kann verzerren. Nimm 1–3 dB Limiter-Gain zurück.") });
    else if (plr < 6.5f)
        hints.push_back ({ 1, u8 ("Dynamik knapp"),
                           u8 ("PLR ") + num (plr) + u8 (" dB. Das ist schon sehr laut gemastert – nicht noch mehr reindrücken. Gut für Rap: 6–9 dB.") });
    else if (plr > 13.0f && loudTarget)
        hints.push_back ({ 1, u8 ("Viel Dynamik"),
                           u8 ("PLR ") + num (plr) + u8 (" dB. Für Rap kannst du mehr komprimieren oder limitieren, bis ca. 7–9 dB.") });
    else
        hints.push_back ({ 0, u8 ("Dynamik ok"), u8 ("PLR ") + num (plr) + u8 (" dB (Rap meist 6–9, Pop 8–11).") });

    if (m.measuredSeconds >= 20.0f)
    {
        if (m.lra > 9.0f)
            hints.push_back ({ 1, u8 ("Lautstärke schwankt stark"),
                               u8 ("LRA ") + num (m.lra) + u8 (" LU: Parts sind sehr unterschiedlich laut. Hook und Verse angleichen oder Bus-Kompressor (2:1, langsames Release).") });
        else
            hints.push_back ({ 0, u8 ("Gleichmäßig"), u8 ("LRA ") + num (m.lra) + u8 (" LU (Rap meist 3–7).") });
    }
}

void addSpectrumHints (std::vector<Hint>& hints, const Measurements& m, const Settings& s)
{
    const auto a = analyseBands (m.spectrum, s);
    if (! a.valid || a.seconds < 5.0f)
        return;

    const bool customCurve = s.curve == 2 && s.reference.valid;
    bool anyBand = false;
    for (const auto& r : regions)
    {
        double sum = 0, tol = 0;
        int n = 0, worst = -1;
        for (int b = 0; b < numBands; ++b)
        {
            const float f = bandCentres[(size_t) b];
            if (f < r.lo || f >= r.hi || f > m.spectrum.sampleRate * 0.45)
                continue;
            sum += a.deviation[(size_t) b];
            tol += a.tolerance[(size_t) b];
            ++n;
            if (worst < 0 || std::abs (a.deviation[(size_t) b]) > std::abs (a.deviation[(size_t) worst]))
                worst = b;
        }
        if (n == 0)
            continue;
        const double dev = sum / n, t = tol / n;
        if (std::abs (dev) <= t)
            continue;
        anyBand = true;
        // Suggest a bit less than the full difference: the reference is an average, not a rule.
        const double amount = std::min (6.0, std::round ((std::abs (dev) - 1.0) * 2.0) / 2.0);
        const float f = bandCentres[(size_t) worst];
        const bool loud = dev > 0;
        const juce::String where = r.shelf ? (r.lo < 100 ? u8 (" mit Low-Shelf unter ") + freqText (r.hi)
                                                         : u8 (" mit High-Shelf ab ") + freqText (r.lo))
                                           : u8 (" breit (Q ~1) bei ") + freqText (f);
        juce::String txt = u8 ("ca. ") + signedNum (dev) + u8 (" dB ") + (loud ? u8 ("zu laut") : u8 ("zu leise")) + " ("
                           + u8 (loud ? r.tooLoud : r.tooQuiet) + u8 (") gegenüber ")
                           + (customCurve ? u8 ("deiner Referenz") : u8 ("der Kurve")) + u8 (". Tipp: ")
                           + (loud ? u8 ("EQ") + where + u8 (" um ca. -") + num (std::max (0.5, amount)) + " dB absenken."
                                   : u8 ("EQ") + where + u8 (" um ca. +") + num (std::max (0.5, amount)) + u8 (" dB anheben – oder dort weniger wegnehmen."))
                           + u8 (" Meist: ") + u8 (r.sources) + u8 (" – am besten direkt in der Spur fixen.");
        hints.push_back ({ std::abs (dev) > t + 2.0 ? 2 : 1, u8 (r.name), txt });
    }
    if (! anyBand)
        hints.push_back ({ 0, u8 ("Klangbalance passt"),
                           u8 ("Kein Frequenzbereich weicht deutlich von ")
                               + (customCurve ? u8 ("deiner Referenz") : u8 ("der Kurve")) + u8 (" ab.") });

    if (a.seconds >= 10.0f)
        for (const auto& r : findResonances (m.spectrum))
            hints.push_back ({ 1, u8 ("Spitze bei ") + freqText (r.freq),
                               u8 ("Bei ") + freqText (r.freq) + u8 (" steht ein schmaler Peak ca. ") + num (r.excessDb, 0)
                                   + u8 (" dB über der Umgebung. Wenn's da klingelt oder pfeift: schmal absenken (Q 4–8, ca. -")
                                   + num (std::min (6.0, std::round (r.excessDb * 0.4 * 2.0) / 2.0))
                                   + u8 (" dB). Kann auch ein Ton aus dem Beat sein – dann lassen.") });
}

void addStereoHints (std::vector<Hint>& hints, const Measurements& m)
{
    if (m.spectrum.frames < 20)
        return;
    const float c = m.correlation, mono = m.monoDropDb;
    bool problem = false;
    if (c < 0.0f)
    {
        hints.push_back ({ 2, u8 ("Phasenproblem"),
                           u8 ("Korrelation ") + num (c, 2) + u8 (": Links und rechts löschen sich teilweise aus. In Mono (Handy, Club) fehlt dann viel. Prüf Stereo-Widener, Chorus und Samples mit gedrehter Phase.") });
        problem = true;
    }
    else if (c < 0.3f)
    {
        hints.push_back ({ 1, u8 ("Sehr breit"),
                           u8 ("Korrelation ") + num (c, 2) + u8 (". Klingt breit, aber in Mono geht was verloren. Widener etwas zurück.") });
        problem = true;
    }
    if (mono < -3.0f)
    {
        hints.push_back ({ mono < -5.0f ? 2 : 1, u8 ("In Mono leiser"),
                           u8 ("In Mono ist der Song ca. ") + num (-mono) + u8 (" dB leiser. Normal ist 0 bis 2 dB.") });
        problem = true;
    }
    const float bass = bassSideToMidDb (m.spectrum);
    if (bass > -12.0f)
    {
        hints.push_back ({ bass > -6.0f ? 2 : 1, u8 ("Bass ist stereo"),
                           u8 ("Unter 120 Hz ist der Side-Anteil nur ") + num (-bass, 0)
                               + u8 (" dB unter der Mitte. Mach Bass und 808 mono (Stereo-Imager unter 120 Hz auf mono), sonst verliert der Master Druck und Lautheit.") });
        problem = true;
    }
    if (! problem)
        hints.push_back ({ 0, u8 ("Stereo ok"),
                           u8 ("Korrelation ") + num (c, 2) + u8 (", Mono ") + num (mono) + u8 (" dB, Bass mono genug.") });
}
} // namespace

std::vector<Hint> makeHints (const Measurements& m, const Settings& s)
{
    std::vector<Hint> hints;
    if (m.measuredSeconds < 3.0f || m.integrated < -69.0f)
    {
        hints.push_back ({ 0, u8 ("Warte auf Audio"),
                           u8 ("Spiel deinen Song ab, am besten komplett von Anfang an. Bei Play startet die Messung automatisch neu.") });
        return hints;
    }
    addLoudnessHints (hints, m, s);
    addSpectrumHints (hints, m, s);
    addStereoHints (hints, m);
    std::stable_sort (hints.begin(), hints.end(), [] (const Hint& a, const Hint& b) { return a.level > b.level; });
    return hints;
}

int overallLevel (const std::vector<Hint>& hints)
{
    int l = 0;
    for (auto& h : hints)
        l = std::max (l, h.level);
    return l;
}
} // namespace mcheck
