// Task 5 probe: Brightness listening pairs, built with the engine's own voicing and Brightness.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Build (from the worktree root):
//   g++ -std=c++17 -O2 -I core/include tools/listening/brightness/make_pairs.cpp -o /tmp/make_pairs
// Usage:
//   make_pairs              JSON with 12 pairs (6 in C major, 6 in A minor) on stdout
//   make_pairs check        reads lines "tonic scale root suffix bass n note1..noteN" on stdin and
//                           prints the engine's measured slot Brightness for each line
//
// Voicing: Progression + layoutEven + voiceAll, the same path the product uses (targetB 0.5 unless
// stated, targetE 0.6). Brightness: updateActuals, i.e. combineBrightness(contextQuality, modal,
// registerBrightness(notes >= kUpperFloor)).
//
// Pair types
//   register  identical chords; the top upper voices of each slot move up an octave by a count chosen
//             so the mean upper pitch rises by 7-12 semitones (closest to 9).
//   colour    chord qualities change (roots kept, at least 3 of 4 chords); the voicing of the changed
//             loop is searched over target centre and seed until the mean upper pitch is within 1
//             semitone of the base loop and the loop Brightness differs by at least 0.25.
#include "chordslots/Generator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace cs;

static const char* kNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

static int typeIndex (const std::string& suffix)
{
    const auto& t = chordTypes();
    for (size_t i = 0; i < t.size(); ++i)
        if (suffix == t[i].suffix)
            return (int) i;
    std::fprintf (stderr, "unknown chord suffix '%s'\n", suffix.c_str());
    std::exit (1);
}

static Chord mk (int root, const std::string& suffix)
{
    Chord c;
    c.root = mod12 (root);
    c.type = typeIndex (suffix);
    c.bassInt = 0;
    return c;
}

static std::string chordName (const Chord& c)
{
    return std::string (kNames[c.root]) + chordTypes()[(size_t) c.type].suffix;
}

struct Spec
{
    const char* key;
    int tonic, scale;
    const char* type; // "register" or "colour"
    std::vector<Chord> chords;
};

static std::vector<int> sortedNotes (const Voicing& v)
{
    std::vector<int> n (v.notes.begin(), v.notes.begin() + v.n);
    std::sort (n.begin(), n.end());
    return n;
}

// A doubled pitch (same MIDI note twice) is a single sounding note in the stimulus, so it is removed
// before Brightness is measured. The MIDI file and the engine number then describe the same notes.
static void dedupeVoicings (Progression& p)
{
    for (int i = 0; i < p.numSlots; ++i)
    {
        Voicing& v = p.slots[(size_t) i].voicing;
        std::vector<int> n = sortedNotes (v);
        n.erase (std::unique (n.begin(), n.end()), n.end());
        v.n = (int) n.size();
        for (size_t j = 0; j < n.size(); ++j)
            v.notes[j] = n[j];
    }
    updateActuals (p);
}

static Progression build (int tonic, int scale, const std::vector<Chord>& chords, unsigned seed, float targetB)
{
    Progression p;
    p.tonic = tonic;
    p.scale = scale;
    p.bars = 4;
    p.numSlots = 4;
    layoutEven (p);
    for (int i = 0; i < 4; ++i)
    {
        Slot& s = p.slots[(size_t) i];
        s.chord = chords[(size_t) i];
        s.targetB = targetB;
        s.targetT = 0.4f;
        s.targetE = 0.6f;
    }
    Rng rng (seed);
    voiceAll (p, rng, nullptr);
    dedupeVoicings (p);
    return p;
}

struct LoopStats
{
    double upperMean = 0.0;
    double brightness = 0.0;
    int upperCount = 0;
};

static LoopStats statsOf (const Progression& p)
{
    LoopStats st;
    double sum = 0.0;
    int count = 0;
    double bsum = 0.0;
    for (int i = 0; i < p.numSlots; ++i)
    {
        const Slot& s = p.slots[(size_t) i];
        for (int j = 0; j < s.voicing.n; ++j)
            if (s.voicing.notes[(size_t) j] >= kUpperFloor)
            {
                sum += s.voicing.notes[(size_t) j];
                ++count;
            }
        bsum += s.brightness;
    }
    st.upperMean = count > 0 ? sum / count : 0.0;
    st.brightness = bsum / p.numSlots;
    st.upperCount = count;
    return st;
}

// Register pair: lower the top k upper voices of every slot by an octave. The engine voicings already
// reach about C5-G5, so an upward octave would leave the register used elsewhere. Moved notes must stay
// at or above kUpperFloor so the bass/upper split is unchanged. k is chosen per slot so the slot's mean
// upper pitch falls by the value in [7, 12] closest to 9. Returns false if no such k exists.
static bool lowerUpper (Progression& b)
{
    const Progression orig = b;
    for (int i = 0; i < b.numSlots; ++i)
    {
        Slot& s = b.slots[(size_t) i];
        std::vector<int> notes = sortedNotes (s.voicing);
        std::vector<size_t> idx;
        for (size_t j = 0; j < notes.size(); ++j)
            if (notes[j] >= kUpperFloor)
                idx.push_back (j);
        const int m = (int) idx.size();
        if (m == 0)
            return false;

        const double upperBefore = [&] { double t = 0; for (size_t j : idx) t += notes[j]; return t / m; }();
        int bestK = -1;
        double bestErr = 1e9;
        std::vector<int> bestNotes;
        for (int k = 1; k <= m; ++k)
        {
            // moved notes must stay in the upper register
            std::vector<int> trial = notes;
            bool valid = true;
            for (int q = 0; q < k && valid; ++q)
            {
                int& x = trial[idx[(size_t) (m - k + q)]];
                x -= 12;
                valid = x >= kUpperFloor;
            }
            if (! valid)
                continue;
            // an octave doubling that lands on another voice is one sounding note, so it is merged
            std::sort (trial.begin(), trial.end());
            trial.erase (std::unique (trial.begin(), trial.end()), trial.end());
            double upperAfter = 0;
            int cnt = 0;
            for (int x : trial)
                if (x >= kUpperFloor)
                {
                    upperAfter += x;
                    ++cnt;
                }
            if (cnt == 0)
                continue;
            const double d = upperBefore - upperAfter / cnt;
            const double err = std::fabs (d - 9.0);
            if (d > 0.0 && err < bestErr)
            {
                bestErr = err;
                bestK = k;
                bestNotes = trial;
            }
        }
        if (bestK < 0)
            return false;

        s.voicing.n = (int) bestNotes.size();
        for (size_t j = 0; j < bestNotes.size(); ++j)
            s.voicing.notes[j] = bestNotes[j];
    }
    // The 7-12 semitone band applies to the loop's mean upper pitch, pooled over the slots.
    const LoopStats before = statsOf (orig), after = statsOf (b);
    const double delta = before.upperMean - after.upperMean;
    if (delta < 7.0 || delta > 12.0)
        return false;
    updateActuals (b);
    return true;
}

static float approxBrightness (const std::vector<Chord>& chords, int tonic, int scale)
{
    double sum = 0.0;
    for (const auto& c : chords)
        sum += combineBrightness (contextQualityBrightness (c, tonic, scale), modalBrightness (c, tonic, scale), 0.5f);
    return (float) (sum / chords.size());
}

// Alternatives for the colour change. dim, aug and m7b5 are left out: their Brightness sits at the
// clipping limits (0 or 1) in the engine (audit H3), so a difference involving them would reflect clipping.
static std::vector<std::string> kAlt = { "", "m", "sus2", "sus4", "maj7", "m7", "7", "6", "m6" };

struct ColourResult
{
    bool found = false;
    std::vector<Chord> chords;
    unsigned seed = 0;
    float targetB = 0.0f;
    int tries = 0, prescreened = 0, voiced = 0;
};

struct ColourParams
{
    int minChanged = 3;
    float prescreenGap = 0.2f; // approximate Brightness gap needed before voicing
    double minGap = 0.25;      // measured loop Brightness gap, lower bound
    double maxGap = 1e9;       // measured loop Brightness gap, upper bound
    bool brighter = false;     // if true the changed loop must be brighter than the base loop (matched set)
};

static ColourResult searchColour (const Spec& spec, const Progression& base, unsigned rngSeed, const ColourParams& cp = ColourParams())
{
    ColourResult res;
    const LoopStats a = statsOf (base);
    std::mt19937 rng (rngSeed);
    std::uniform_real_distribution<float> u01 (0.0f, 1.0f);
    const float targets[] = { 0.3f, 0.4f, 0.5f, 0.6f, 0.7f };
    double bestErr = 1e9;

    for (int t = 0; t < 40000 && res.voiced < 400; ++t)
    {
        ++res.tries;
        std::vector<Chord> cand = spec.chords;
        int changed = 0;
        for (auto& c : cand)
        {
            if (u01 (rng) < 0.75f)
            {
                std::string cur = chordTypes()[(size_t) c.type].suffix;
                std::vector<std::string> opts;
                for (const auto& s : kAlt)
                    if (s != cur)
                        opts.push_back (s);
                c = mk (c.root, opts[(size_t) (rng() % opts.size())]);
                ++changed;
            }
        }
        if (changed < cp.minChanged || cand == spec.chords)
            continue;
        const float approxGap = approxBrightness (cand, spec.tonic, spec.scale) - approxBrightness (spec.chords, spec.tonic, spec.scale);
        if (std::fabs (approxGap) < cp.prescreenGap || (cp.brighter && approxGap < 0.0f))
            continue;
        if (cp.maxGap < 1e8 && std::fabs (approxGap) > (float) cp.maxGap + 0.08f)
            continue;
        ++res.prescreened;

        for (float tb : targets)
            for (unsigned seed = 1; seed <= 6; ++seed)
            {
                Progression b = build (spec.tonic, spec.scale, cand, seed, tb);
                bool clipped = false;
                for (int i = 0; i < b.numSlots; ++i)
                    clipped = clipped || b.slots[(size_t) i].brightness <= 0.001f || b.slots[(size_t) i].brightness >= 0.999f;
                if (clipped)
                    continue;
                const LoopStats sb = statsOf (b);
                const double dUp = std::fabs (sb.upperMean - a.upperMean);
                const double dB = std::fabs (sb.brightness - a.brightness);
                ++res.voiced;
                const bool signOk = ! cp.brighter || sb.brightness > a.brightness;
                if (signOk && dUp <= 1.0 && dB >= cp.minGap && dB <= cp.maxGap && dUp < bestErr)
                {
                    bestErr = dUp;
                    res.found = true;
                    res.chords = cand;
                    res.seed = seed;
                    res.targetB = tb;
                }
            }
    }
    return res;
}

static void printLoop (const char* label, const Progression& p, const std::vector<Chord>& chords, const char* name, bool last)
{
    const LoopStats st = statsOf (p);
    std::printf ("    \"%s\": {\"label\": \"%s\", \"slots\": [", name, label);
    for (int i = 0; i < p.numSlots; ++i)
    {
        const Slot& s = p.slots[(size_t) i];
        const std::vector<int> notes = sortedNotes (s.voicing);
        std::printf ("%s{\"chord\": \"%s\", \"root\": %d, \"suffix\": \"%s\", \"pcs\": [", i ? ", " : "", chordName (chords[(size_t) i]).c_str(),
                     chords[(size_t) i].root, chordTypes()[(size_t) chords[(size_t) i].type].suffix);
        std::vector<int> pcs;
        for (int iv : chordTypes()[(size_t) chords[(size_t) i].type].iv)
            pcs.push_back (mod12 (chords[(size_t) i].root + iv));
        std::sort (pcs.begin(), pcs.end());
        pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
        for (size_t j = 0; j < pcs.size(); ++j)
            std::printf ("%s%d", j ? ", " : "", pcs[j]);
        std::printf ("], \"notes\": [");
        for (size_t j = 0; j < notes.size(); ++j)
            std::printf ("%s%d", j ? ", " : "", notes[j]);
        std::printf ("], \"brightness\": %.6f}", s.brightness);
    }
    std::printf ("], \"upper_mean\": %.4f, \"brightness\": %.6f}%s\n", st.upperMean, st.brightness, last ? "" : ",");
}

static int runCheck()
{
    std::string line;
    while (std::getline (std::cin, line))
    {
        std::istringstream ss (line);
        int tonic = 0, scale = 0, root = 0, bass = 0, n = 0;
        std::string suffix;
        if (! (ss >> tonic >> scale >> root >> suffix >> bass >> n))
            continue;
        if (suffix == "-") // placeholder for the major triad, whose suffix is empty
            suffix.clear();
        Progression p;
        p.tonic = tonic;
        p.scale = scale;
        p.numSlots = 1;
        p.slots[0].chord = mk (root, suffix);
        p.slots[0].chord.bassInt = bass;
        Voicing v;
        v.n = n;
        for (int j = 0; j < n; ++j)
            ss >> v.notes[(size_t) j];
        p.slots[0].voicing = v;
        updateActualsLocal (p, 0);
        std::printf ("%.6f\n", p.slots[0].brightness);
    }
    return 0;
}

// ---- Matched set (pairs 13-22) --------------------------------------------------------------------
// The first set cannot compare the two effects at equal size: register pairs differ by about 0.09 engine
// Brightness, colour pairs by 0.25-0.33. This set matches them.
//   colour_small  chord qualities change (at least 2 of 4 chords), register matched within 1 semitone, and the
//                 engine Brightness gap is 0.075-0.105, the same size as a register pair.
//   conflict      the two cues point in opposite directions with the same engine size. File X has the brighter
//                 chord qualities at a register about 8 semitones lower; file Y has the darker qualities at the
//                 original register. The colour gap (register matched) is 0.075-0.105 and the net engine
//                 Brightness gap is within 0.03. If listeners pick the higher register, register wins the
//                 perceived Brightness; if they pick the brighter chords, colour wins.
static const double kGapLo = 0.075, kGapHi = 0.105;

struct ConflictResult
{
    bool found = false;
    std::vector<Chord> darkChords;
    unsigned seed = 0;
    float targetB = 0.0f;
};

static ConflictResult searchConflict (const Spec& spec, const Progression& baseHigh, const Progression& baseLow, unsigned rngSeed)
{
    ConflictResult res;
    const LoopStats a = statsOf (baseHigh); // brighter chords, original register
    const LoopStats al = statsOf (baseLow); // brighter chords, register lowered
    std::mt19937 rng (rngSeed);
    std::uniform_real_distribution<float> u01 (0.0f, 1.0f);
    const float targets[] = { 0.3f, 0.4f, 0.5f, 0.6f, 0.7f };
    double bestScore = 1e9;
    int voiced = 0;
    for (int t = 0; t < 60000 && voiced < 600; ++t)
    {
        std::vector<Chord> cand = spec.chords;
        int changed = 0;
        for (auto& c : cand)
            if (u01 (rng) < 0.6f)
            {
                std::string cur = chordTypes()[(size_t) c.type].suffix;
                std::vector<std::string> opts;
                for (const auto& sfx : kAlt)
                    if (sfx != cur)
                        opts.push_back (sfx);
                c = mk (c.root, opts[(size_t) (rng() % opts.size())]);
                ++changed;
            }
        if (changed < 2 || cand == spec.chords)
            continue;
        const float gap = approxBrightness (spec.chords, spec.tonic, spec.scale) - approxBrightness (cand, spec.tonic, spec.scale);
        if (gap < 0.04f || gap > 0.16f)
            continue;
        for (float tb : targets)
            for (unsigned seed = 1; seed <= 6; ++seed)
            {
                Progression q = build (spec.tonic, spec.scale, cand, seed, tb);
                bool clipped = false;
                for (int i = 0; i < q.numSlots; ++i)
                    clipped = clipped || q.slots[(size_t) i].brightness <= 0.001f || q.slots[(size_t) i].brightness >= 0.999f;
                if (clipped)
                    continue;
                ++voiced;
                const LoopStats sq = statsOf (q);
                const double dUp = std::fabs (sq.upperMean - a.upperMean);
                const double colourGap = a.brightness - sq.brightness;
                const double net = al.brightness - sq.brightness;
                if (dUp > 1.0 || colourGap < kGapLo || colourGap > kGapHi || std::fabs (net) > 0.03)
                    continue;
                const double score = dUp + std::fabs (net) * 10.0;
                if (score < bestScore)
                {
                    bestScore = score;
                    res.found = true;
                    res.darkChords = cand;
                    res.seed = seed;
                    res.targetB = tb;
                }
            }
    }
    return res;
}

static int runMatched()
{
    std::vector<Spec> specs = {
        { "C", 0, 0, "colour_small", { mk (0, ""), mk (7, ""), mk (9, "m"), mk (5, "") } },
        { "C", 0, 0, "colour_small", { mk (9, "m"), mk (5, ""), mk (0, ""), mk (7, "") } },
        { "C", 0, 0, "colour_small", { mk (0, ""), mk (5, ""), mk (2, "m"), mk (7, "") } },
        { "Am", 9, 1, "colour_small", { mk (9, "m"), mk (5, ""), mk (7, ""), mk (4, "") } },
        { "Am", 9, 1, "colour_small", { mk (9, "m"), mk (0, ""), mk (5, ""), mk (7, "") } },
        { "Am", 9, 1, "colour_small", { mk (9, "m"), mk (4, "m"), mk (5, ""), mk (0, "") } },
        { "C", 0, 0, "conflict", { mk (0, ""), mk (9, "m"), mk (5, ""), mk (7, "") } },
        { "C", 0, 0, "conflict", { mk (5, ""), mk (7, ""), mk (0, ""), mk (9, "m") } },
        { "Am", 9, 1, "conflict", { mk (9, "m"), mk (7, ""), mk (5, ""), mk (0, "") } },
        { "Am", 9, 1, "conflict", { mk (9, "m"), mk (2, "m"), mk (0, ""), mk (7, "") } },
    };
    std::printf ("[\n");
    for (size_t k = 0; k < specs.size(); ++k)
    {
        const Spec& sp = specs[k];
        const size_t pairNo = 12 + k + 1;
        Progression base = build (sp.tonic, sp.scale, sp.chords, 1, 0.5f);
        Progression a, b;
        std::vector<Chord> aChords, bChords;
        const bool baseIsA = (k % 2 == 0);
        std::string extra;
        if (std::strcmp (sp.type, "colour_small") == 0)
        {
            ColourParams cp;
            cp.minChanged = 2;
            cp.prescreenGap = 0.05f;
            cp.minGap = kGapLo;
            cp.maxGap = kGapHi;
            cp.brighter = (k % 2 == 0); // alternate whether the changed loop is brighter or darker
            ColourResult cr = searchColour (sp, base, 4100u + (unsigned) k, cp);
            if (! cr.found)
            {
                std::fprintf (stderr, "pair %zu: no small colour match\n", pairNo);
                return 1;
            }
            Progression other = build (sp.tonic, sp.scale, cr.chords, cr.seed, cr.targetB);
            a = baseIsA ? base : other;
            b = baseIsA ? other : base;
            aChords = baseIsA ? sp.chords : cr.chords;
            bChords = baseIsA ? cr.chords : sp.chords;
        }
        else
        {
            Progression low = base;
            if (! lowerUpper (low))
            {
                std::fprintf (stderr, "pair %zu: no lowering\n", pairNo);
                return 1;
            }
            ConflictResult cf = searchConflict (sp, base, low, 7300u + (unsigned) k);
            if (! cf.found)
            {
                std::fprintf (stderr, "pair %zu: no conflict match\n", pairNo);
                return 1;
            }
            Progression dark = build (sp.tonic, sp.scale, cf.darkChords, cf.seed, cf.targetB);
            // X = brighter chords, lowered register (base spec chords); Y = darker chords, original register.
            a = baseIsA ? low : dark;
            b = baseIsA ? dark : low;
            aChords = baseIsA ? sp.chords : cf.darkChords;
            bChords = baseIsA ? cf.darkChords : sp.chords;
        }
        const LoopStats sa = statsOf (a), sb = statsOf (b);
        std::fprintf (stderr, "pair %02zu %-13s %-3s dUpper %+.2f dBright %+.4f\n", pairNo, sp.type, sp.key, sb.upperMean - sa.upperMean, sb.brightness - sa.brightness);
        std::string labelA, labelB;
        for (size_t i = 0; i < aChords.size(); ++i)
            labelA += (i ? " " : "") + chordName (aChords[i]);
        for (size_t i = 0; i < bChords.size(); ++i)
            labelB += (i ? " " : "") + chordName (bChords[i]);
        std::printf ("  {\"pair_id\": \"pair%02zu\", \"key\": \"%s\", \"type\": \"%s\", \"tonic\": %d, \"scale\": %d,\n", pairNo, sp.key, sp.type, sp.tonic, sp.scale);
        printLoop (labelA.c_str(), a, aChords, "A", false);
        printLoop (labelB.c_str(), b, bChords, "B", false);
        std::printf ("    \"delta_upper\": %.4f, \"delta_brightness\": %.6f\n", sb.upperMean - sa.upperMean, sb.brightness - sa.brightness);
        std::printf ("  }%s\n", k + 1 < specs.size() ? "," : "");
    }
    std::printf ("]\n");
    return 0;
}

int main (int argc, char** argv)
{
    if (argc > 1 && std::strcmp (argv[1], "check") == 0)
        return runCheck();
    if (argc > 1 && std::strcmp (argv[1], "matched") == 0)
        return runMatched();

    const float kTargetB = 0.5f;
    std::vector<Spec> specs = {
        // C major (tonic 0, Major): register pairs first, then colour pairs
        { "C", 0, 0, "register", { mk (0, ""), mk (5, ""), mk (7, ""), mk (0, "") } },
        { "C", 0, 0, "register", { mk (0, ""), mk (9, "m"), mk (5, ""), mk (7, "") } },
        { "C", 0, 0, "register", { mk (0, ""), mk (7, ""), mk (9, "m"), mk (5, "") } },
        { "C", 0, 0, "colour", { mk (0, ""), mk (9, "m"), mk (2, "m"), mk (7, "") } },
        { "C", 0, 0, "colour", { mk (5, ""), mk (0, ""), mk (7, ""), mk (9, "m") } },
        { "C", 0, 0, "colour", { mk (0, ""), mk (4, "m"), mk (9, "m"), mk (5, "") } },
        // A minor (tonic 9, Minor)
        { "Am", 9, 1, "register", { mk (9, "m"), mk (5, ""), mk (0, ""), mk (7, "") } },
        { "Am", 9, 1, "register", { mk (9, "m"), mk (2, "m"), mk (4, ""), mk (9, "m") } },
        { "Am", 9, 1, "register", { mk (9, "m"), mk (7, ""), mk (5, ""), mk (4, "") } },
        { "Am", 9, 1, "colour", { mk (9, "m"), mk (2, "m"), mk (7, ""), mk (0, "") } },
        { "Am", 9, 1, "colour", { mk (2, "m"), mk (4, ""), mk (9, "m"), mk (5, "") } },
        { "Am", 9, 1, "colour", { mk (9, "m"), mk (5, ""), mk (2, "m"), mk (4, "") } },
    };

    std::printf ("[\n");
    for (size_t k = 0; k < specs.size(); ++k)
    {
        const Spec& sp = specs[k];
        const unsigned baseSeed = 1;
        // base: the engine's own voicing of the first loop. other: the register-lowered or colour-changed version.
        Progression base = build (sp.tonic, sp.scale, sp.chords, baseSeed, kTargetB);
        Progression other;
        std::vector<Chord> otherChords = sp.chords;
        if (std::strcmp (sp.type, "register") == 0)
        {
            other = base;
            if (! lowerUpper (other))
            {
                std::fprintf (stderr, "pair %zu: no lowering satisfies 7-12 semitones\n", k + 1);
                return 1;
            }
        }
        else
        {
            ColourResult cr = searchColour (sp, base, 2026u + (unsigned) k);
            if (! cr.found)
            {
                std::fprintf (stderr, "pair %zu: no colour match found (tries %d)\n", k + 1, cr.tries);
                return 1;
            }
            otherChords = cr.chords;
            std::fprintf (stderr, "pair %02zu colour search: tries %d prescreened %d voiced %d -> seed %u targetB %.1f\n", k + 1, cr.tries, cr.prescreened, cr.voiced, cr.seed, cr.targetB);
            other = build (sp.tonic, sp.scale, otherChords, cr.seed, cr.targetB);
        }

        // Orientation alternates with the pair index so that A is not always the base voicing.
        const bool baseIsA = (k % 2 == 0);
        const Progression& a = baseIsA ? base : other;
        const Progression& b = baseIsA ? other : base;
        const std::vector<Chord>& aChords = baseIsA ? sp.chords : otherChords;
        const std::vector<Chord>& bChords = baseIsA ? otherChords : sp.chords;

        const LoopStats sa = statsOf (a);
        const LoopStats sb = statsOf (b);
        std::fprintf (stderr, "pair %02zu %-8s %-3s upperA %.2f upperB %.2f dUpper %+.2f brightA %.4f brightB %.4f dBright %+.4f\n", k + 1, sp.type, sp.key,
                      sa.upperMean, sb.upperMean, sb.upperMean - sa.upperMean, sa.brightness, sb.brightness, sb.brightness - sa.brightness);

        std::string labelA, labelB;
        for (size_t i = 0; i < aChords.size(); ++i)
            labelA += (i ? " " : "") + chordName (aChords[i]);
        for (size_t i = 0; i < bChords.size(); ++i)
            labelB += (i ? " " : "") + chordName (bChords[i]);

        std::printf ("  {\"pair_id\": \"pair%02zu\", \"key\": \"%s\", \"type\": \"%s\", \"tonic\": %d, \"scale\": %d,\n", k + 1, sp.key, sp.type, sp.tonic, sp.scale);
        printLoop (labelA.c_str(), a, aChords, "A", false);
        printLoop (labelB.c_str(), b, bChords, "B", false);
        std::printf ("    \"delta_upper\": %.4f, \"delta_brightness\": %.6f\n", sb.upperMean - sa.upperMean, sb.brightness - sa.brightness);
        std::printf ("  }%s\n", k + 1 < specs.size() ? "," : "");
    }
    std::printf ("]\n");
    return 0;
}
