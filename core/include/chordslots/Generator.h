// ChordSlots core - progression model, target-driven generator, locks, voicing.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Corpus.h"
#include "Metrics.h"
#include "Performance.h"

#include <cstring>
#include <random>

namespace cs
{

constexpr int kMaxSlots = 16;
constexpr int kMaxNotes = 8;
constexpr int kStepsPerQuarter = 24;
static_assert (kStepsPerQuarter == 24, "kStepsPerQuarter must be 24");
constexpr int kStepsPerBar = 4 * kStepsPerQuarter; // 4/4 default: 96 steps
constexpr int kMaxBars = 8;

struct Meter
{
    int num = 4;
    int den = 4;
    int stepsPerBar = 96;
    std::vector<int> beatSteps;     // beat lengths: 4/4 = {24,24,24,24}, 3/4 = {24,24,24}, 6/8 = {36,36}
    std::vector<int> strongOffsets; // offsets within bar: 4/4 = {0,48}, 3/4 = {0}, 6/8 = {0,36}
};

inline int stepsPerBar (int num = 4, int den = 4) noexcept
{
    if (den <= 0) den = 4;
    return num * (kStepsPerQuarter * 4) / den;
}

inline Meter getMeter (int num = 4, int den = 4) noexcept
{
    Meter m;
    m.num = num;
    m.den = (den == 8) ? 8 : 4;
    m.stepsPerBar = stepsPerBar (m.num, m.den);

    if (m.num == 3 && m.den == 4)
    {
        m.beatSteps = { 24, 24, 24 };
        m.strongOffsets = { 0 };
    }
    else if (m.num == 6 && m.den == 8)
    {
        m.beatSteps = { 36, 36 };
        m.strongOffsets = { 0, 36 };
    }
    else
    {
        m.num = 4;
        m.den = 4;
        m.stepsPerBar = 96;
        m.beatSteps = { 24, 24, 24, 24 };
        m.strongOffsets = { 0, 48 };
    }
    return m;
}

using Rng = std::mt19937;

inline float rand01 (Rng& rng) { return std::uniform_real_distribution<float> (0.0f, 1.0f) (rng); }

//==============================================================================
struct Voicing
{
    int n = 0;
    std::array<int, kMaxNotes> notes {};
    int velocity = 96;
    int bassCount = 0;

    int computeBassCount (int upperFloor = 48) const noexcept
    {
        int c = 0;
        for (int i = 0; i < n; ++i)
            if (notes[(size_t) i] < upperFloor)
                c++;
        return c;
    }
};

struct Slot
{
    int start = 0; // steps (24 per quarter note) from the start of the loop
    Chord chord;
    Voicing voicing;
    bool locked = false; // freezes this slot's chord and voicing

    // What the user asks for (0..1)
    float targetB = 0.5f, targetT = 0.4f, targetE = 0.6f;
    // What the current chord actually measures (0..1)
    float brightness = 0.5f, tension = 0.5f, energy = 0.5f;
};

/** Layer locks: a locked layer survives Roll. */
struct Locks
{
    bool rhythm = false;  // cut positions
    bool roots = false;   // chord roots
    bool colors = false;  // chord qualities / extensions
    bool voicing = false; // inversions and register
};

struct Progression
{
    int tonic = 9; // A
    int scale = 1; // Minor
    int bars = 4;
    int timeSigNum = 4;
    int timeSigDen = 4;
    int numSlots = 4;
    int snap = 12; // cut grid in steps: 24 = 1/4, 16 = 1/4T, 12 = 1/8, 8 = 1/8T, 6 = 1/16, 4 = 1/16T, 3 = 1/32
    float adventure = 0.3f;   // 0 = diatonic triads, 1 = borrowed chords and extensions
    float syncopation = 0.3f; // how irregular randomised cuts are
    float bassMotion = 0.0f;  // 0 = keep root in bass, 1 = smooth walking bass / pedal inversions
    float voiceLeading = 0.5f; // 0 = free voicings, 1 = smooth minimal displacement
    bool phraseRepeat = false; // generates 4-slot phrase AA' with cadence variation
    int mood = -1;            // index into moods() the targets came from, -1 = custom
    int style = -1;           // -1 = Auto; otherwise a CorpusStyle value including Free
    Locks locks;
    std::array<Slot, kMaxSlots> slots {};

    Meter meter() const noexcept { return getMeter (timeSigNum, timeSigDen); }
    int stepsPerBar() const noexcept { return meter().stepsPerBar; }
    int lengthSteps() const noexcept { return bars * stepsPerBar(); }
    int slotEnd (int i) const noexcept { return i + 1 < numSlots ? slots[(size_t) (i + 1)].start : lengthSteps(); }
    int maxSlots() const noexcept { return std::min (kMaxSlots, lengthSteps() / std::max (1, snap)); }
};

//==============================================================================
// Rhythm

inline void layoutEven (Progression& p)
{
    const int len = p.lengthSteps();
    p.numSlots = std::clamp (p.numSlots, 1, p.maxSlots());
    int prev = -p.snap;
    for (int i = 0; i < p.numSlots; ++i)
    {
        int s = (int) std::lround ((double) i * len / p.numSlots / p.snap) * p.snap;
        s = std::max (s, prev + p.snap);
        p.slots[(size_t) i].start = s;
        prev = s;
    }
    p.slots[0].start = 0;
}

/** Makes the cut list valid again after any edit: starts at 0, strictly
    increasing, every slot at least one step long, all inside the loop. */
inline void sanitizeRhythm (Progression& p)
{
    const int len = p.lengthSteps();
    p.numSlots = std::clamp (p.numSlots, 1, std::min (kMaxSlots, len));
    p.slots[0].start = 0;
    for (int i = 1; i < p.numSlots; ++i)
    {
        auto& s = p.slots[(size_t) i].start;
        s = std::max (s, p.slots[(size_t) (i - 1)].start + 1);
    }
    for (int i = p.numSlots - 1; i >= 1; --i)
    {
        auto& s = p.slots[(size_t) i].start;
        const int limit = (i + 1 < p.numSlots ? p.slots[(size_t) (i + 1)].start : len) - 1;
        s = std::min (s, limit);
    }
}

inline void setMeter (Progression& p, int newNum, int newDen)
{
    newDen = (newDen == 8) ? 8 : 4;
    newNum = std::clamp (newNum, 1, 16);
    if (newNum == p.timeSigNum && newDen == p.timeSigDen)
        return;

    const int oldBarLen = p.stepsPerBar();
    p.timeSigNum = newNum;
    p.timeSigDen = newDen;
    const int newBarLen = p.stepsPerBar();

    for (int i = 1; i < p.numSlots; ++i)
    {
        const int bar = p.slots[(size_t) i].start / oldBarLen;
        const int offset = p.slots[(size_t) i].start % oldBarLen;
        const int remappedOffset = std::clamp ((int) std::lround ((double) offset * newBarLen / oldBarLen / p.snap) * p.snap, 0, newBarLen - 1);
        p.slots[(size_t) i].start = bar * newBarLen + remappedOffset;
    }
    sanitizeRhythm (p);
}

inline void randomizeRhythm (Progression& p, Rng& rng)
{
    layoutEven (p);
    const int len = p.lengthSteps();
    const int unit = p.snap;
    const auto m = p.meter();
    const int barLen = m.stepsPerBar;

    for (int i = 1; i < p.numSlots; ++i)
    {
        if (rand01 (rng) >= p.syncopation)
            continue;

        // Anticipations ("pushes") are the most idiomatic displacement.
        const float r = rand01 (rng);
        const int shift = (r < 0.45f ? -1 : r < 0.70f ? 1 : r < 0.85f ? -2 : 2) * unit;
        const int lo = p.slots[(size_t) (i - 1)].start + unit;
        const int hi = (i + 1 < p.numSlots ? p.slots[(size_t) (i + 1)].start : len) - unit;
        int s = p.slots[(size_t) i].start + shift;

        // Weight towards strong beat positions in the meter
        if (rand01 (rng) < 0.5f)
        {
            const int bar = s / barLen;
            const int offset = s % barLen;
            for (int strong : m.strongOffsets)
            {
                if (std::abs (offset - strong) <= unit)
                {
                    s = bar * barLen + strong;
                    break;
                }
            }
        }

        if (s >= lo && s <= hi)
            p.slots[(size_t) i].start = s;
    }
    sanitizeRhythm (p);
}

/** Drags cut i (the start of slot i, i >= 1) to a new step, snapped. */
inline void moveCut (Progression& p, int i, int newStart)
{
    if (i < 1 || i >= p.numSlots)
        return;
    const int len = p.lengthSteps();
    int s = (int) std::lround ((double) newStart / p.snap) * p.snap;
    const int lo = p.slots[(size_t) (i - 1)].start + 1;
    const int hi = (i + 1 < p.numSlots ? p.slots[(size_t) (i + 1)].start : len) - 1;
    p.slots[(size_t) i].start = std::clamp (s, lo, hi);
}

//==============================================================================
// Candidate chords

struct Candidate
{
    Chord chord;
    float brightness = 0.5f, tension = 0.5f;
    int outside = 0;
    int complexity = 0;
};

/** Every chord the generator may use in this key at this adventure level. */
inline std::vector<Candidate> buildCandidates (int tonic, int scaleIdx, float adventure)
{
    std::vector<Candidate> out;
    const unsigned sm = scaleMask (scaleIdx);
    const int maxOutside = adventure < 0.2f ? 0 : (adventure < 0.6f ? 1 : 2);

    for (int root = 0; root < 12; ++root)
    {
        const bool rootInScale = (sm & (1u << mod12 (root - tonic))) != 0;
        if (! rootInScale && adventure < 0.4f)
            continue;

        for (int type = 0; type < numChordTypes(); ++type)
        {
            const Chord c { root, type };
            const int outside = tonesOutsideScale (c, tonic, scaleIdx);
            if (outside > maxOutside)
                continue;

            const ChordMetrics m = chordMetrics (c, tonic, scaleIdx);
            out.push_back ({ c, m.brightness, m.tension, outside, chordTypes()[(size_t) type].complexity });
        }
    }
    return out;
}

//==============================================================================
// Moods: nothing more than named target curves. No tagged progression database.

struct Mood
{
    const char* name;
    int scale;               // suggested scale index
    float adventure;
    float syncopation;
    std::vector<float> b, t, e; // control points spread across the loop
    CorpusStyle defaultStyle = CorpusStyle::PopRock;
};

inline const std::vector<Mood>& moods()
{
    static const std::vector<Mood> m = {
        { "Euphoric",    0, 0.30f, 0.25f, { 0.70f, 0.78f, 0.85f, 0.80f }, { 0.22f, 0.45f, 0.60f, 0.40f }, { 0.70f, 0.80f, 0.90f, 1.00f }, CorpusStyle::PopRock },
        { "Uplifting",   1, 0.30f, 0.30f, { 0.35f, 0.60f, 0.70f, 0.62f }, { 0.25f, 0.38f, 0.45f, 0.62f }, { 0.75f, 0.80f, 0.90f, 0.95f }, CorpusStyle::MinorLoops },
        { "Epic",        1, 0.35f, 0.20f, { 0.30f, 0.58f, 0.66f, 0.40f }, { 0.22f, 0.40f, 0.62f, 0.68f }, { 0.70f, 0.80f, 0.90f, 1.00f }, CorpusStyle::MinorLoops },
        { "Melancholic", 1, 0.40f, 0.20f, { 0.30f, 0.25f, 0.55f, 0.28f }, { 0.25f, 0.50f, 0.42f, 0.60f }, { 0.40f, 0.45f, 0.50f, 0.40f }, CorpusStyle::MinorLoops },
        { "Dark",        3, 0.50f, 0.35f, { 0.25f, 0.15f, 0.30f, 0.10f }, { 0.25f, 0.60f, 0.55f, 0.75f }, { 0.65f, 0.70f, 0.70f, 0.80f }, CorpusStyle::MinorLoops },
        { "Tense",       7, 0.65f, 0.45f, { 0.30f, 0.35f, 0.20f, 0.40f }, { 0.45f, 0.65f, 0.80f, 0.90f }, { 0.50f, 0.60f, 0.70f, 0.85f }, CorpusStyle::MinorLoops },
        { "Dreamy",      4, 0.60f, 0.15f, { 0.85f, 0.70f, 0.90f, 0.75f }, { 0.40f, 0.55f, 0.48f, 0.55f }, { 0.30f, 0.30f, 0.35f, 0.30f }, CorpusStyle::SoulRnb },
        { "Hard",        1, 0.15f, 0.50f, { 0.28f, 0.35f, 0.22f, 0.45f }, { 0.22f, 0.50f, 0.55f, 0.65f }, { 1.00f, 1.00f, 1.00f, 1.00f }, CorpusStyle::MinorLoops },
    };
    return m;
}

inline CorpusStyle styleForProgression (const Progression& p) noexcept
{
    if (p.style >= 0)
        return (CorpusStyle) p.style;

    if (p.mood >= 0 && p.mood < (int) moods().size())
    {
        if (moods()[(size_t) p.mood].defaultStyle == CorpusStyle::SoulRnb)
            return CorpusStyle::SoulRnb;
    }

    return scaleHasMinorThird (p.scale) ? CorpusStyle::MinorLoops : CorpusStyle::PopRock;
}

//==============================================================================
// Cost model

inline float transitionCost (Chord a, Chord b, int tonic, float adventure, int scaleIdx, CorpusStyle style)
{
    float c = 0.0f;
    if (a == b)
        c += 3.0f; // repeating the same chord is rarely what you want from a new slot
    else if (a.root == b.root)
        c += (a.isSlash() != b.isSlash() || a.bassInt != b.bassInt) ? 0.2f : 0.6f;

    // Hand-authored style prior (provisional)
    const float corpusCost = corpusTransitionCost (a, b, tonic, scaleIdx, style);

    // Adventure modulates prior strength:
    // Low adventure strictly guides progressions toward common songwriting transitions.
    // High adventure relaxes the prior to allow adventurous harmonic discoveries.
    const float priorWeight = (1.0f - adventure * 0.75f);
    c += priorWeight * 2.2f * corpusCost;

    // Tame settings also prefer close relatives
    c += (1.0f - adventure) * 0.3f * chordDistance (a, b);
    return 0.03f * c;
}

inline float transitionCost (Chord a, Chord b, int tonic, float adventure, int scaleIdx = 0)
{
    const CorpusStyle style = scaleHasMinorThird (scaleIdx) ? CorpusStyle::MinorLoops : CorpusStyle::PopRock;
    return transitionCost (a, b, tonic, adventure, scaleIdx, style);
}

inline float slotCost (const Progression& p, int slot, const Candidate& cand,
                       const Chord* prev, const Chord* next, const Chord* prev2 = nullptr)
{
    const Slot& s = p.slots[(size_t) slot];
    const float dB = cand.brightness - s.targetB;
    const float dT = cand.tension - s.targetT;
    float cost = dB * dB + dT * dT;

    // Low adventure keeps to plain triads in the scale; the price of colour
    // fades quadratically as the dial goes up.
    const float tame = (1.0f - p.adventure) * (1.0f - p.adventure);
    cost += tame * (0.06f * (float) cand.complexity + 0.08f * (float) cand.outside);

    if (slot == 0 && cand.chord.root == p.tonic)
        cost -= 0.012f; // a gentle pull towards starting at home

    // A-B-A Anti-Oscillation Penalty: discourage ping-ponging back to the chord 2 slots ago
    if (prev2 != nullptr && prev != nullptr && cand.chord == *prev2 && cand.chord != *prev)
        cost += (1.0f - p.adventure) * 0.08f;

    const CorpusStyle style = styleForProgression (p);
    if (prev != nullptr)
        cost += transitionCost (*prev, cand.chord, p.tonic, p.adventure, p.scale, style);
    if (next != nullptr)
        cost += transitionCost (cand.chord, *next, p.tonic, p.adventure, p.scale, style);
    return cost;
}

/** Samples an index with probability proportional to exp(-cost / temperature). */
inline int sampleSoftmin (const std::vector<float>& costs, float temperature, Rng& rng)
{
    if (costs.empty())
        return -1;
    const float lo = *std::min_element (costs.begin(), costs.end());
    std::vector<double> w (costs.size());
    double total = 0.0;
    for (size_t i = 0; i < costs.size(); ++i)
    {
        w[i] = std::exp (-(double) (costs[i] - lo) / temperature);
        total += w[i];
    }
    double r = (double) rand01 (rng) * total;
    for (size_t i = 0; i < costs.size(); ++i)
    {
        r -= w[i];
        if (r <= 0.0)
            return (int) i;
    }
    return (int) costs.size() - 1;
}

inline bool slotChordIsFixed (const Progression& p, int i)
{
    return p.slots[(size_t) i].locked || (p.locks.roots && p.locks.colors);
}

/** Candidates allowed in one slot once the root/colour locks are applied. */
inline std::vector<Candidate> slotCandidates (const Progression& p, int i, const std::vector<Candidate>& all)
{
    const Chord cur = p.slots[(size_t) i].chord;
    std::vector<Candidate> v;
    for (const auto& c : all)
    {
        if (p.locks.roots && c.chord.root != cur.root)
            continue;
        if (p.locks.colors && c.chord.type != cur.type)
            continue;
        v.push_back (c);
    }
    if (v.empty())
    {
        // The lock pins something the current key/adventure no longer offers.
        // Keep what is there rather than breaking the lock.
        const ChordMetrics m = chordMetrics (cur, p.tonic, p.scale);
        v.push_back ({ cur, m.brightness, m.tension, tonesOutsideScale (cur, p.tonic, p.scale),
                       chordTypes()[(size_t) cur.type].complexity });
    }
    return v;
}

/** Picks chords for every unlocked slot so that their measured brightness and
    tension follow the per-slot targets. Gibbs sampling over the loop: each slot
    is resampled given both neighbours, so the last chord also leads back into
    the first. */
inline void generateChords (Progression& p, Rng& rng, float temperature = 0.012f)
{
    const auto all = buildCandidates (p.tonic, p.scale, p.adventure);
    if (all.empty())
        return;

    const int n = p.numSlots;
    std::vector<std::vector<Candidate>> pools ((size_t) n);
    for (int i = 0; i < n; ++i)
        if (! slotChordIsFixed (p, i))
            pools[(size_t) i] = slotCandidates (p, i, all);

    std::vector<float> costs;
    for (int sweep = 0; sweep < 4; ++sweep)
    {
        for (int i = 0; i < n; ++i)
        {
            if (slotChordIsFixed (p, i))
                continue;

            const auto& pool = pools[(size_t) i];
            const int pi = (i + n - 1) % n, ni = (i + 1) % n, p2i = (i + n - 2) % n;
            // On the first sweep, slots to the right have not been chosen yet
            // (unless they are fixed), so they should not influence the pick.
            const bool usePrev = n > 1 && (sweep > 0 || pi < i || slotChordIsFixed (p, pi));
            const bool useNext = n > 1 && (sweep > 0 || slotChordIsFixed (p, ni));
            const bool usePrev2 = n > 2 && (sweep > 0 || p2i < i || slotChordIsFixed (p, p2i));
            const Chord* prev = usePrev ? &p.slots[(size_t) pi].chord : nullptr;
            const Chord* next = useNext ? &p.slots[(size_t) ni].chord : nullptr;
            const Chord* prev2 = usePrev2 ? &p.slots[(size_t) p2i].chord : nullptr;

            costs.resize (pool.size());
            for (size_t k = 0; k < pool.size(); ++k)
                costs[k] = slotCost (p, i, pool[k], prev, next, prev2);

            const int pick = sampleSoftmin (costs, temperature, rng);
            if (pick >= 0)
                p.slots[(size_t) i].chord = pool[(size_t) pick].chord;
        }
    }

    // Phrase repeat structure (AA'): if enabled and slot count is even (>= 4),
    // phrase A (first half) repeats in second half with variation on final cadence slot.
    if (p.phraseRepeat && n >= 4 && (n % 2 == 0))
    {
        const int h = n / 2;
        for (int i = h; i < n - 1; ++i)
        {
            if (! slotChordIsFixed (p, i))
                p.slots[(size_t) i].chord = p.slots[(size_t) (i - h)].chord;
        }
    }
}

/** Best-fitting replacements for one slot, best first (for the alternatives menu). */
inline std::vector<Chord> alternatives (const Progression& p, int slot, int maxCount)
{
    // Offer a slightly wider vocabulary than Roll would use on its own.
    const auto all = buildCandidates (p.tonic, p.scale, std::max (p.adventure, 0.45f));
    const int n = p.numSlots;
    const Chord* prev = n > 1 ? &p.slots[(size_t) ((slot + n - 1) % n)].chord : nullptr;
    const Chord* next = n > 1 ? &p.slots[(size_t) ((slot + 1) % n)].chord : nullptr;

    std::vector<std::pair<float, Chord>> scored;
    for (const auto& c : all)
        if (c.chord != p.slots[(size_t) slot].chord)
            scored.push_back ({ slotCost (p, slot, c, prev, next), c.chord });

    std::sort (scored.begin(), scored.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<Chord> out;
    for (int i = 0; i < (int) scored.size() && i < maxCount; ++i)
        out.push_back (scored[(size_t) i].second);
    return out;
}

//==============================================================================
// Voicing

constexpr int kUpperFloor = 48; // notes below this are bass doublings

/** Primary bass note of a voicing (highest note below kUpperFloor, or -1 if none). */
inline int primaryBassNote (const Voicing& v, int upperFloor = kUpperFloor) noexcept
{
    int best = -1;
    for (int i = 0; i < v.n; ++i)
        if (v.notes[(size_t) i] < upperFloor)
            if (best < 0 || v.notes[(size_t) i] > best)
                best = v.notes[(size_t) i];
    return best;
}

/** Finds a bass octave in ~28..47 closest to refBassNote (or ~40 if none),
    guaranteeing it stays below upperFloorCeiling and below kUpperFloor. */
inline int pickBassRegister (int bassPc, int refBassNote = -1, int upperFloorCeiling = kUpperFloor) noexcept
{
    constexpr int kMinBass = 28; // E1
    constexpr int kMaxBass = 47; // B2 (< kUpperFloor = 48)
    const int target = refBassNote >= 0 ? refBassNote : 40; // Default ~E2
    const int ceiling = std::min (upperFloorCeiling - 2, kUpperFloor - 1);

    int bestNote = -1;
    int bestDist = 1000;

    for (int oct = 1; oct <= 4; ++oct)
    {
        const int note = oct * 12 + bassPc;
        if (note >= kMinBass && note <= kMaxBass && note <= ceiling)
        {
            const int d = std::abs (note - target);
            if (d < bestDist)
            {
                bestDist = d;
                bestNote = note;
            }
        }
    }

    if (bestNote < 0)
    {
        for (int oct = 1; oct <= 4; ++oct)
        {
            const int note = oct * 12 + bassPc;
            if (note >= 24 && note <= ceiling)
            {
                const int d = std::abs (note - target);
                if (d < bestDist)
                {
                    bestDist = d;
                    bestNote = note;
                }
            }
        }
    }

    return bestNote >= 0 ? bestNote : (36 + bassPc <= kUpperFloor - 1 ? 36 + bassPc : 24 + bassPc);
}

/** Re-voices ONLY the bass note(s) of an existing voicing, leaving upper structure untouched. */
inline void revoiceBass (Voicing& v, Chord c, int refBassNote = -1, float energy = 0.5f, int upperFloor = kUpperFloor)
{
    std::vector<int> upper;
    for (int i = 0; i < v.n; ++i)
        if (v.notes[(size_t) i] >= upperFloor)
            upper.push_back (v.notes[(size_t) i]);

    if (upper.empty())
        return;

    const int bass = pickBassRegister (c.bassPc(), refBassNote, upper.front());
    std::vector<int> all = upper;
    all.push_back (bass);
    if (energy > 0.85f && bass - 12 >= 24)
        all.push_back (bass - 12);

    std::sort (all.begin(), all.end());
    all.erase (std::unique (all.begin(), all.end()), all.end());

    v.n = std::min ((int) all.size(), kMaxNotes);
    for (int i = 0; i < v.n; ++i)
        v.notes[(size_t) i] = std::clamp (all[(size_t) i], 0, 127);
    v.bassCount = v.computeBassCount (upperFloor);
}

inline float voiceLeadingDistance (const Voicing& a, const std::vector<int>& b)
{
    std::vector<int> au;
    for (int i = 0; i < a.n; ++i)
        if (a.notes[(size_t) i] >= kUpperFloor)
            au.push_back (a.notes[(size_t) i]);
    if (au.empty() || b.empty())
        return 0.0f;

    auto oneWay = [] (const std::vector<int>& from, const std::vector<int>& to)
    {
        float sum = 0.0f;
        for (int x : from)
        {
            int best = 1000;
            for (int y : to)
                best = std::min (best, std::abs (x - y));
            sum += (float) best;
        }
        return sum / (float) from.size();
    };
    return 0.5f * (oneWay (au, b) + oneWay (b, au));
}

/** Builds a playable voicing.
    @param ref        voicing to stay close to (previous chord), or nullptr
    @param centre     desired mean pitch of the upper structure (MIDI)
    @param energy     0..1: adds bass and doublings, raises velocity
    @param refWeight  how strongly to stick to ref
    @param jitter     amount of randomness in the choice (0 = deterministic) */
inline Voicing makeVoicing (Chord c, const Voicing* ref, float centre, float energy,
                            Rng& rng, float refWeight = 1.0f, float jitter = 0.7f)
{
    std::vector<int> pcs;
    for (int iv : chordTypes()[(size_t) c.type].iv)
    {
        const int pc = mod12 (c.root + iv);
        if (std::find (pcs.begin(), pcs.end(), pc) == pcs.end())
            pcs.push_back (pc);
    }
    std::sort (pcs.begin(), pcs.end());
    const int k = (int) pcs.size();

    std::vector<int> best;
    float bestCost = 1.0e9f;

    for (int inv = 0; inv < k; ++inv)
        for (int base : { 36, 48, 60, 72 })
        {
            std::vector<int> notes;
            int note = base + pcs[(size_t) inv];
            notes.push_back (note);
            for (int j = 1; j < k; ++j)
            {
                note += mod12 (pcs[(size_t) ((inv + j) % k)] - pcs[(size_t) ((inv + j - 1) % k)]);
                notes.push_back (note);
            }
            if (notes.front() < kUpperFloor || notes.back() > 96)
                continue;

            float mean = 0.0f;
            for (int x : notes)
                mean += (float) x;
            mean /= (float) k;

            float cost = 0.5f * std::abs (mean - centre);
            if (ref != nullptr && ref->n > 0)
                cost += refWeight * voiceLeadingDistance (*ref, notes);
            cost += jitter * rand01 (rng);

            if (cost < bestCost)
            {
                bestCost = cost;
                best = notes;
            }
        }

    if (best.empty()) // cannot happen with the ranges above, but never return silence
        for (int j = 0; j < k; ++j)
            best.push_back (60 + mod12 (pcs[(size_t) j] - pcs[0]) + pcs[0] % 12);

    std::vector<int> all = best;
    const int refBass = ref != nullptr ? primaryBassNote (*ref) : -1;
    const int bass = pickBassRegister (c.bassPc(), refBass, best.front());
    all.push_back (bass);
    if (energy > 0.85f && bass - 12 >= 24)
        all.push_back (bass - 12);
    if (energy > 0.6f)
    {
        int top = 60 + c.root;
        while (top <= best.back())
            top += 12;
        if (top <= 100)
            all.push_back (top);
    }

    std::sort (all.begin(), all.end());
    all.erase (std::unique (all.begin(), all.end()), all.end());

    Voicing v;
    v.n = std::min ((int) all.size(), kMaxNotes);
    for (int i = 0; i < v.n; ++i)
        v.notes[(size_t) i] = std::clamp (all[(size_t) i], 0, 127);
    v.bassCount = v.computeBassCount (kUpperFloor);
    v.velocity = std::clamp ((int) std::lround (45.0f + 75.0f * energy), 1, 127);
    return v;
}

inline float centreForBrightness (float targetB) { return 57.0f + 14.0f * targetB; }

/** Recomputes the measured metrics of one slot from its chord and voicing. */
inline void updateActuals (Progression& p, int i)
{
    Slot& s = p.slots[(size_t) i];
    int upper[kMaxNotes];
    int n = 0;
    for (int j = 0; j < s.voicing.n; ++j)
        if (s.voicing.notes[(size_t) j] >= kUpperFloor)
            upper[n++] = s.voicing.notes[(size_t) j];

    s.brightness = combineBrightness (qualityBrightness (s.chord.type),
                                      modalBrightness (s.chord, p.tonic, p.scale),
                                      registerBrightness (upper, n));
    s.tension = tensionOf (s.chord, p.tonic, p.scale);

    // Energy is a performance property (loudness and density), not a harmonic one.
    const float vel = clamp01 (((float) s.voicing.velocity - 45.0f) / 75.0f);
    const float density = clamp01 (((float) s.voicing.n - 2.0f) / 4.5f);
    s.energy = clamp01 (0.6f * vel + 0.4f * density);
}

inline void updateActuals (Progression& p)
{
    for (int i = 0; i < p.numSlots; ++i)
        updateActuals (p, i);
}

/** Voices the whole progression with smooth voice leading.
    @param changed  per-slot flags: true if the chord changed since the stored
                    voicing was made (nullptr = treat every slot as changed) */
inline void voiceAll (Progression& p, Rng& rng, const bool* changed = nullptr)
{
    const int n = p.numSlots;
    for (int i = 0; i < n; ++i)
    {
        Slot& s = p.slots[(size_t) i];
        const bool chordChanged = changed == nullptr || changed[i] || s.voicing.n == 0;
        if (s.locked && s.voicing.n > 0)
            continue;

        if (p.locks.voicing && s.voicing.n > 0)
        {
            if (chordChanged) // keep the shape: stay as close as possible to the old voicing
            {
                const Voicing old = s.voicing;
                s.voicing = makeVoicing (s.chord, &old, centreForBrightness (s.targetB), s.targetE, rng, 3.0f, 0.0f);
            }
            else
            {
                s.voicing.velocity = std::clamp ((int) std::lround (45.0f + 75.0f * s.targetE), 1, 127);
            }
            continue;
        }

        const Voicing* ref = nullptr;
        if (i > 0)
            ref = &p.slots[(size_t) (i - 1)].voicing;
        else if (n > 1 && p.slots[(size_t) (n - 1)].voicing.n > 0)
            ref = &p.slots[(size_t) (n - 1)].voicing;

        s.voicing = makeVoicing (s.chord, ref, centreForBrightness (s.targetB), s.targetE, rng);
    }
    updateActuals (p);
}

//==============================================================================
// Top-level operations

/** Circular DP pass to choose bass inversions for smooth bass voice leading. */
inline void generateBassLine (Progression& p)
{
    const int n = p.numSlots;
    if (n <= 0)
        return;

    std::vector<std::vector<int>> slotOpts ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const Slot& s = p.slots[(size_t) i];
        if (s.locked || p.locks.voicing)
        {
            slotOpts[(size_t) i] = { s.chord.bassInt };
        }
        else if (p.bassMotion < 0.05f)
        {
            slotOpts[(size_t) i] = { 0 };
        }
        else
        {
            std::vector<int> opts = { 0 };
            for (int iv : chordTypes()[(size_t) s.chord.type].iv)
            {
                const int ivNorm = mod12 (iv);
                if (ivNorm != 0 && std::find (opts.begin(), opts.end(), ivNorm) == opts.end())
                    opts.push_back (ivNorm);
            }
            slotOpts[(size_t) i] = opts;
        }
    }

    if (n == 1)
    {
        p.slots[0].chord.bassInt = slotOpts[0][0];
        return;
    }

    struct CandidateBass
    {
        int bassInt;
        int pitch;
    };

    std::vector<std::vector<CandidateBass>> cands ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const int root = p.slots[(size_t) i].chord.root;
        for (int bInt : slotOpts[(size_t) i])
        {
            const int pc = mod12 (root + bInt);
            const int pitch = pickBassRegister (pc, 40);
            cands[(size_t) i].push_back ({ bInt, pitch });
        }
    }

    const float wRoot = (1.0f - p.bassMotion) * 7.0f;
    std::vector<int> bestPath;
    float bestTotalCost = 1.0e9f;

    for (size_t startChoice = 0; startChoice < cands[0].size(); ++startChoice)
    {
        std::vector<std::vector<float>> dp ((size_t) n);
        std::vector<std::vector<int>> parent ((size_t) n);

        for (int i = 0; i < n; ++i)
        {
            dp[(size_t) i].assign (cands[(size_t) i].size(), 1.0e9f);
            parent[(size_t) i].assign (cands[(size_t) i].size(), -1);
        }

        const float startPen = cands[0][startChoice].bassInt != 0 ? wRoot + 2.0f * (1.0f - p.bassMotion) : 0.0f;
        dp[0][startChoice] = startPen;

        for (int i = 1; i < n; ++i)
        {
            for (size_t cur = 0; cur < cands[(size_t) i].size(); ++cur)
            {
                const auto& cCur = cands[(size_t) i][cur];
                const float invPen = cCur.bassInt != 0 ? wRoot : 0.0f;

                for (size_t prev = 0; prev < cands[(size_t) (i - 1)].size(); ++prev)
                {
                    if (dp[(size_t) (i - 1)][prev] >= 1.0e8f)
                        continue;

                    const auto& cPrev = cands[(size_t) (i - 1)][prev];
                    const int leap = std::abs (cCur.pitch - cPrev.pitch);
                    const float leapCost = (float) leap + (leap > 7 ? 4.0f : 0.0f);
                    const float total = dp[(size_t) (i - 1)][prev] + leapCost + invPen;

                    if (total < dp[(size_t) i][cur])
                    {
                        dp[(size_t) i][cur] = total;
                        parent[(size_t) i][cur] = (int) prev;
                    }
                }
            }
        }

        const auto& c0 = cands[0][startChoice];
        for (size_t endChoice = 0; endChoice < cands[(size_t) (n - 1)].size(); ++endChoice)
        {
            if (dp[(size_t) (n - 1)][endChoice] >= 1.0e8f)
                continue;

            const auto& cEnd = cands[(size_t) (n - 1)][endChoice];
            const int wrapLeap = std::abs (c0.pitch - cEnd.pitch);
            const float wrapCost = (float) wrapLeap + (wrapLeap > 7 ? 4.0f : 0.0f);
            const float loopTotal = dp[(size_t) (n - 1)][endChoice] + wrapCost;

            if (loopTotal < bestTotalCost)
            {
                bestTotalCost = loopTotal;
                std::vector<int> path ((size_t) n);
                int curr = (int) endChoice;
                for (int i = n - 1; i >= 1; --i)
                {
                    path[(size_t) i] = cands[(size_t) i][(size_t) curr].bassInt;
                    curr = parent[(size_t) i][(size_t) curr];
                }
                path[0] = c0.bassInt;
                bestPath = path;
            }
        }
    }

    if (! bestPath.empty())
    {
        for (int i = 0; i < n; ++i)
            p.slots[(size_t) i].chord.bassInt = bestPath[(size_t) i];
    }
}

/** How well one slot matches its targets, 0..1. */
inline float slotFit (const Slot& s)
{
    const float dB = s.brightness - s.targetB;
    const float dT = s.tension - s.targetT;
    return clamp01 (1.0f - std::sqrt ((dB * dB + dT * dT) * 0.5f) * 2.0f);
}

/** The "mood score": how closely the progression follows the target curves. */
inline float fitScore (const Progression& p)
{
    float sum = 0.0f;
    for (int i = 0; i < p.numSlots; ++i)
        sum += slotFit (p.slots[(size_t) i]);
    return p.numSlots > 0 ? sum / (float) p.numSlots : 0.0f;
}

/** Generates candidate voicings for voice-leading DP optimization. */
inline std::vector<Voicing> candidateVoicingsForSlot (const Progression& p, int slot)
{
    const Slot& s = p.slots[(size_t) slot];
    std::vector<int> pcs;
    for (int iv : chordTypes()[(size_t) s.chord.type].iv)
    {
        const int pc = mod12 (s.chord.root + iv);
        if (std::find (pcs.begin(), pcs.end(), pc) == pcs.end())
            pcs.push_back (pc);
    }
    std::sort (pcs.begin(), pcs.end());
    const int k = (int) pcs.size();
    if (k == 0)
        return { s.voicing };

    const float centre = centreForBrightness (s.targetB);
    std::vector<Voicing> candidates;

    for (int inv = 0; inv < k; ++inv)
    {
        for (int base : { 48, 60, 72 })
        {
            std::vector<int> notes;
            int note = base + pcs[(size_t) inv];
            notes.push_back (note);
            for (int j = 1; j < k; ++j)
            {
                note += mod12 (pcs[(size_t) ((inv + j) % k)] - pcs[(size_t) ((inv + j - 1) % k)]);
                notes.push_back (note);
            }
            if (notes.front() < kUpperFloor || notes.back() > 96)
                continue;

            float mean = 0.0f;
            for (int x : notes) mean += (float) x;
            mean /= (float) k;

            if (std::abs (mean - centre) > 14.0f)
                continue;

            std::vector<int> all = notes;
            const int bass = pickBassRegister (s.chord.bassPc(), -1, notes.front());
            all.push_back (bass);
            if (s.targetE > 0.85f && bass - 12 >= 24)
                all.push_back (bass - 12);
            if (s.targetE > 0.6f)
            {
                int top = 60 + s.chord.root;
                while (top <= notes.back()) top += 12;
                if (top <= 100) all.push_back (top);
            }
            std::sort (all.begin(), all.end());
            all.erase (std::unique (all.begin(), all.end()), all.end());

            Voicing v;
            v.n = std::min ((int) all.size(), kMaxNotes);
            for (int idx = 0; idx < v.n; ++idx)
                v.notes[(size_t) idx] = std::clamp (all[(size_t) idx], 0, 127);
            v.bassCount = v.computeBassCount (kUpperFloor);
            v.velocity = std::clamp ((int) std::lround (45.0f + 75.0f * s.targetE), 1, 127);
            candidates.push_back (v);
        }
    }
    if (candidates.empty())
        candidates.push_back (s.voicing);
    return candidates;
}

/** Global circular Viterbi DP pass to minimize upper-voice displacement across slot boundaries. */
inline void optimizeVoiceLeading (Progression& p)
{
    const int n = p.numSlots;
    if (n <= 1 || p.locks.voicing || p.voiceLeading < 0.05f)
        return;

    const float initialFit = fitScore (p);
    std::vector<std::vector<Voicing>> cands ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        if (p.slots[(size_t) i].locked)
            cands[(size_t) i] = { p.slots[(size_t) i].voicing };
        else
            cands[(size_t) i] = candidateVoicingsForSlot (p, i);
    }

    auto transitionCost = [&p] (const Voicing& a, const Voicing& b) -> float
    {
        std::vector<int> bUpper;
        for (int j = 0; j < b.n; ++j)
            if (b.notes[(size_t) j] >= kUpperFloor)
                bUpper.push_back (b.notes[(size_t) j]);

        float dist = voiceLeadingDistance (a, bUpper);
        int aSop = a.n > 0 ? a.notes[(size_t) (a.n - 1)] : 60;
        int bSop = b.n > 0 ? b.notes[(size_t) (b.n - 1)] : 60;
        int sopLeap = std::abs (bSop - aSop);
        float sopCost = (sopLeap > 2) ? 0.5f * (float) (sopLeap - 2) : 0.0f;

        int common = 0;
        for (int x : bUpper)
        {
            for (int i = 0; i < a.n; ++i)
                if (a.notes[(size_t) i] == x)
                {
                    ++common;
                    break;
                }
        }
        float commonBonus = 0.7f * (float) common;
        return p.voiceLeading * (dist + sopCost - commonBonus);
    };

    float bestTotalCost = 1.0e9f;
    std::vector<Voicing> bestPath;

    for (size_t startChoice = 0; startChoice < cands[0].size(); ++startChoice)
    {
        std::vector<std::vector<float>> dp ((size_t) n);
        std::vector<std::vector<int>> parent ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            dp[(size_t) i].assign (cands[(size_t) i].size(), 1.0e9f);
            parent[(size_t) i].assign (cands[(size_t) i].size(), -1);
        }

        dp[0][startChoice] = 0.0f;

        for (int i = 1; i < n; ++i)
        {
            for (size_t cur = 0; cur < cands[(size_t) i].size(); ++cur)
            {
                const auto& vCur = cands[(size_t) i][cur];
                for (size_t prev = 0; prev < cands[(size_t) (i - 1)].size(); ++prev)
                {
                    if (dp[(size_t) (i - 1)][prev] >= 1.0e8f)
                        continue;
                    const auto& vPrev = cands[(size_t) (i - 1)][prev];
                    const float cost = dp[(size_t) (i - 1)][prev] + transitionCost (vPrev, vCur);
                    if (cost < dp[(size_t) i][cur])
                    {
                        dp[(size_t) i][cur] = cost;
                        parent[(size_t) i][cur] = (int) prev;
                    }
                }
            }
        }

        const auto& v0 = cands[0][startChoice];
        for (size_t endChoice = 0; endChoice < cands[(size_t) (n - 1)].size(); ++endChoice)
        {
            if (dp[(size_t) (n - 1)][endChoice] >= 1.0e8f)
                continue;
            const auto& vEnd = cands[(size_t) (n - 1)][endChoice];
            const float loopTotal = dp[(size_t) (n - 1)][endChoice] + transitionCost (vEnd, v0);
            if (loopTotal < bestTotalCost)
            {
                bestTotalCost = loopTotal;
                std::vector<Voicing> path ((size_t) n);
                int curr = (int) endChoice;
                for (int i = n - 1; i >= 1; --i)
                {
                    path[(size_t) i] = cands[(size_t) i][(size_t) curr];
                    curr = parent[(size_t) i][(size_t) curr];
                }
                path[0] = v0;
                bestPath = path;
            }
        }
    }

    if (! bestPath.empty())
    {
        const auto originalVoicings = p.slots;
        for (int i = 0; i < n; ++i)
            p.slots[(size_t) i].voicing = bestPath[(size_t) i];
        updateActuals (p);

        // Fit non-regression guarantee: if Fit score drops by more than 0.02, revert to initial
        const float newFit = fitScore (p);
        if (newFit < initialFit - 0.02f)
        {
            p.slots = originalVoicings;
            updateActuals (p);
        }
    }
}

/** The main button: re-rolls every layer that is not locked. */
inline void roll (Progression& p, Rng& rng)
{
    if (! p.locks.rhythm)
        randomizeRhythm (p, rng);
    else
        sanitizeRhythm (p);

    bool changed[kMaxSlots] = {};
    Chord before[kMaxSlots];
    for (int i = 0; i < p.numSlots; ++i)
        before[i] = p.slots[(size_t) i].chord;

    generateChords (p, rng);

    if (! p.locks.voicing)
        generateBassLine (p);
    else
    {
        for (int i = 0; i < p.numSlots; ++i)
            sanitizeBass (p.slots[(size_t) i].chord);
    }

    for (int i = 0; i < p.numSlots; ++i)
        changed[i] = before[i] != p.slots[(size_t) i].chord;

    if (p.locks.voicing)
        voiceAll (p, rng, changed);
    else
        voiceAll (p, rng, nullptr);

    if (! p.locks.voicing && p.voiceLeading > 0.05f)
        optimizeVoiceLeading (p);
}

/** Re-voices without touching chords or rhythm (after target edits). */
inline void revoice (Progression& p, Rng& rng)
{
    bool changed[kMaxSlots] = {};
    if (p.locks.voicing)
        voiceAll (p, rng, changed);
    else
        voiceAll (p, rng, nullptr);

    if (! p.locks.voicing && p.voiceLeading > 0.05f)
        optimizeVoiceLeading (p);
}

inline void setChord (Progression& p, int slot, Chord c, Rng& rng)
{
    if (slot < 0 || slot >= p.numSlots)
        return;
    Slot& s = p.slots[(size_t) slot];
    sanitizeBass (c);
    s.chord = c;
    const Voicing old = s.voicing;
    const int n = p.numSlots;
    const Voicing* ref = nullptr;
    if (slot > 0 && p.slots[(size_t) (slot - 1)].voicing.n > 0)
        ref = &p.slots[(size_t) (slot - 1)].voicing;
    else if (n > 1 && p.slots[(size_t) (n - 1)].voicing.n > 0)
        ref = &p.slots[(size_t) (n - 1)].voicing;
    else if (old.n > 0)
        ref = &old;

    s.voicing = makeVoicing (c, ref, centreForBrightness (s.targetB), s.targetE, rng, 2.0f, 0.0f);
    updateActuals (p, slot);
}

/** Changes ONLY the bass note of a slot, preserving upper voice structure. */
inline void setBass (Progression& p, int slot, int bassInt)
{
    if (slot < 0 || slot >= p.numSlots)
        return;
    Slot& s = p.slots[(size_t) slot];
    s.chord.bassInt = bassInt;
    sanitizeBass (s.chord);

    const int n = p.numSlots;
    const int prevSlot = (slot + n - 1) % n;
    const int prevBass = n > 1 ? primaryBassNote (p.slots[(size_t) prevSlot].voicing) : -1;
    revoiceBass (s.voicing, s.chord, prevBass, s.targetE);
    updateActuals (p, slot);
}

/** Rerolls ONLY the root of a single slot, scored by slotCost. */
inline void rollSlotRoot (Progression& p, int slot, Rng& rng)
{
    if (slot < 0 || slot >= p.numSlots)
        return;
    const Slot& s = p.slots[(size_t) slot];
    const auto all = buildCandidates (p.tonic, p.scale, std::max (p.adventure, 0.45f));
    std::vector<Candidate> candidates;
    for (const auto& c : all)
    {
        if (c.chord.type == s.chord.type && c.chord.root != s.chord.root)
            candidates.push_back (c);
    }
    if (candidates.empty())
    {
        for (const auto& c : all)
        {
            if (c.chord.root != s.chord.root)
                candidates.push_back (c);
        }
    }
    if (candidates.empty())
        return;

    const int n = p.numSlots;
    const int pi = (slot + n - 1) % n, ni = (slot + 1) % n, p2i = (slot + n - 2) % n;
    const Chord* prev = n > 1 ? &p.slots[(size_t) pi].chord : nullptr;
    const Chord* next = n > 1 ? &p.slots[(size_t) ni].chord : nullptr;
    const Chord* prev2 = n > 2 ? &p.slots[(size_t) p2i].chord : nullptr;

    std::vector<float> costs (candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i)
        costs[i] = slotCost (p, slot, candidates[i], prev, next, prev2);

    const int pick = sampleSoftmin (costs, 0.004f, rng);
    if (pick >= 0)
    {
        Chord nextChord = candidates[(size_t) pick].chord;
        nextChord.bassInt = s.chord.bassInt;
        sanitizeBass (nextChord);
        setChord (p, slot, nextChord, rng);
    }
}

/** Rerolls ONLY the chord type/color of a single slot, keeping root and scored by slotCost. */
inline void rollSlotColor (Progression& p, int slot, Rng& rng)
{
    if (slot < 0 || slot >= p.numSlots)
        return;
    const Slot& s = p.slots[(size_t) slot];
    const auto all = buildCandidates (p.tonic, p.scale, std::max (p.adventure, 0.45f));
    std::vector<Candidate> candidates;
    for (const auto& c : all)
    {
        if (c.chord.root == s.chord.root && c.chord.type != s.chord.type)
            candidates.push_back (c);
    }
    if (candidates.empty())
    {
        for (const auto& c : all)
        {
            if (c.chord.type != s.chord.type)
                candidates.push_back (c);
        }
    }
    if (candidates.empty())
        return;

    const int n = p.numSlots;
    const int pi = (slot + n - 1) % n, ni = (slot + 1) % n, p2i = (slot + n - 2) % n;
    const Chord* prev = n > 1 ? &p.slots[(size_t) pi].chord : nullptr;
    const Chord* next = n > 1 ? &p.slots[(size_t) ni].chord : nullptr;
    const Chord* prev2 = n > 2 ? &p.slots[(size_t) p2i].chord : nullptr;

    std::vector<float> costs (candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i)
        costs[i] = slotCost (p, slot, candidates[i], prev, next, prev2);

    const int pick = sampleSoftmin (costs, 0.004f, rng);
    if (pick >= 0)
    {
        Chord nextChord = candidates[(size_t) pick].chord;
        nextChord.bassInt = s.chord.bassInt;
        sanitizeBass (nextChord);
        setChord (p, slot, nextChord, rng);
    }
}

/** Cycles or rerolls the bass note / inversion for a single slot. */
inline void rollSlotBass (Progression& p, int slot)
{
    if (slot < 0 || slot >= p.numSlots)
        return;
    const Slot& s = p.slots[(size_t) slot];
    const auto& t = chordTypes()[(size_t) s.chord.type];
    std::vector<int> validBass = { 0 }; // root position
    for (int iv : t.iv)
    {
        int ivNorm = mod12 (iv);
        if (ivNorm != 0 && std::find (validBass.begin(), validBass.end(), ivNorm) == validBass.end())
            validBass.push_back (ivNorm);
    }
    auto it = std::find (validBass.begin(), validBass.end(), s.chord.bassInt);
    int nextIdx = 0;
    if (it != validBass.end())
        nextIdx = (int) (((size_t) (it - validBass.begin()) + 1u) % validBass.size());
    setBass (p, slot, validBass[(size_t) nextIdx]);
}

/** Safe melody notes for a chord in key context (chord tones + non-clashing scale degrees). */
inline std::vector<int> safeMelodyNotes (Chord c, int tonic, int scaleIdx)
{
    std::vector<int> notes;
    for (int iv : chordTypes()[(size_t) c.type].iv)
    {
        int pc = mod12 (c.root + iv);
        if (std::find (notes.begin(), notes.end(), pc) == notes.end())
            notes.push_back (pc);
    }
    for (int pcRel : scales()[(size_t) scaleIdx].pcs)
    {
        int pc = mod12 (tonic + pcRel);
        if (std::find (notes.begin(), notes.end(), pc) == notes.end())
            notes.push_back (pc);
    }
    std::sort (notes.begin(), notes.end());
    return notes;
}

/** Changes the number of slots, keeping targets/chords by position. */
inline void setNumSlots (Progression& p, int newCount)
{
    newCount = std::clamp (newCount, 1, p.maxSlots());
    const int old = std::max (1, p.numSlots);
    if (newCount == old)
        return;

    const auto prev = p.slots;
    for (int i = 0; i < newCount; ++i)
    {
        p.slots[(size_t) i] = prev[(size_t) std::min (old - 1, i * old / newCount)];
        p.slots[(size_t) i].locked = false;
    }
    p.numSlots = newCount;
    layoutEven (p);
}

inline void setBars (Progression& p, int newBars)
{
    newBars = std::clamp (newBars, 1, kMaxBars);
    if (newBars == p.bars)
        return;
    const double ratio = (double) newBars / p.bars;
    p.bars = newBars;
    p.numSlots = std::min (p.numSlots, p.maxSlots());
    for (int i = 0; i < p.numSlots; ++i)
        p.slots[(size_t) i].start = (int) std::lround (p.slots[(size_t) i].start * ratio / p.snap) * p.snap;
    sanitizeRhythm (p);
}

/** Adds a cut inside slot i at the given step: the slot is split in two and
    both halves keep its chord and targets. Returns false if it does not fit. */
inline bool splitSlot (Progression& p, int i, int step)
{
    if (i < 0 || i >= p.numSlots || p.numSlots >= kMaxSlots)
        return false;
    step = (int) std::lround ((double) step / p.snap) * p.snap;
    if (step <= p.slots[(size_t) i].start || step >= p.slotEnd (i))
        return false;

    for (int j = p.numSlots; j > i + 1; --j)
        p.slots[(size_t) j] = p.slots[(size_t) (j - 1)];
    p.slots[(size_t) (i + 1)] = p.slots[(size_t) i];
    p.slots[(size_t) (i + 1)].start = step;
    p.slots[(size_t) (i + 1)].locked = false;
    ++p.numSlots;
    return true;
}

/** Removes the cut at the start of slot i (i >= 1): the previous slot absorbs it. */
inline bool removeSlot (Progression& p, int i)
{
    if (i < 1 || i >= p.numSlots)
        return false;
    for (int j = i; j + 1 < p.numSlots; ++j)
        p.slots[(size_t) j] = p.slots[(size_t) (j + 1)];
    --p.numSlots;
    return true;
}

/** Re-picks the chord of selected slots only (bit i of mask = slot i), close
    to the best fit, leaving everything else untouched. Used when the user
    edits one slot's target. Locked slots and layer locks are respected. */
inline void rerollSlots (Progression& p, unsigned mask, Rng& rng, float temperature = 0.004f)
{
    bool wasLocked[kMaxSlots];
    Chord before[kMaxSlots];
    for (int i = 0; i < p.numSlots; ++i)
    {
        wasLocked[i] = p.slots[(size_t) i].locked;
        before[i] = p.slots[(size_t) i].chord;
        if ((mask & (1u << i)) == 0)
            p.slots[(size_t) i].locked = true;
    }

    generateChords (p, rng, temperature);

    for (int i = 0; i < p.numSlots; ++i)
    {
        p.slots[(size_t) i].locked = wasLocked[i];
        if (before[i] != p.slots[(size_t) i].chord)
        {
            if (! p.locks.voicing && ! wasLocked[i])
                p.slots[(size_t) i].chord.bassInt = 0;
            else
                sanitizeBass (p.slots[(size_t) i].chord);
            setChord (p, i, p.slots[(size_t) i].chord, rng);
        }
    }
    updateActuals (p);
}

/** Applies the energy targets (density and velocity) while keeping each
    voicing's shape. */
inline void applyEnergy (Progression& p, Rng& rng)
{
    for (int i = 0; i < p.numSlots; ++i)
    {
        Slot& s = p.slots[(size_t) i];
        if (s.locked || s.voicing.n == 0)
            continue;
        const Voicing old = s.voicing;
        s.voicing = makeVoicing (s.chord, &old, centreForBrightness (s.targetB), s.targetE, rng, 3.0f, 0.0f);
    }
    updateActuals (p);
}

/** Changes key by transposing everything (the progression stays the same,
    relative to the tonic). Moves by the shortest distance. */
inline void setTonic (Progression& p, int newTonic)
{
    newTonic = mod12 (newTonic);
    int d = mod12 (newTonic - p.tonic);
    if (d > 6)
        d -= 12;
    p.tonic = newTonic;
    for (int i = 0; i < p.numSlots; ++i)
    {
        Slot& s = p.slots[(size_t) i];
        s.chord.root = mod12 (s.chord.root + d);
        for (int j = 0; j < s.voicing.n; ++j)
            s.voicing.notes[(size_t) j] = std::clamp (s.voicing.notes[(size_t) j] + d, 0, 127);
    }
    updateActuals (p);
}

//==============================================================================
// Mood curves sampling

inline float sampleCurve (const std::vector<float>& pts, float x)
{
    if (pts.empty())
        return 0.5f;
    const float pos = x * (float) pts.size() - 0.5f;
    const int i0 = std::clamp ((int) std::floor (pos), 0, (int) pts.size() - 1);
    const int i1 = std::clamp (i0 + 1, 0, (int) pts.size() - 1);
    const float f = std::clamp (pos - (float) i0, 0.0f, 1.0f);
    return pts[(size_t) i0] + (pts[(size_t) i1] - pts[(size_t) i0]) * f;
}

/** Writes a mood's curves into the slot targets (positions follow the cuts). */
inline void applyMood (Progression& p, int moodIdx, bool alsoSetScale = true)
{
    const Mood& m = moods()[(size_t) moodIdx];
    p.mood = moodIdx;
    if (alsoSetScale)
        p.scale = m.scale;
    p.adventure = m.adventure;
    p.syncopation = m.syncopation;
    for (int i = 0; i < p.numSlots; ++i)
    {
        const float x = ((float) i + 0.5f) / (float) p.numSlots;
        Slot& s = p.slots[(size_t) i];
        s.targetB = sampleCurve (m.b, x);
        s.targetT = sampleCurve (m.t, x);
        s.targetE = sampleCurve (m.e, x);
    }
}

//==============================================================================
// Flat, trivially copyable snapshot for the audio thread and for MIDI export.

struct PlaySlot
{
    int start = 0, end = 0;
    int n = 0;
    int notes[kMaxNotes] = {};
    int velocities[kMaxNotes] = {};
    float offsetMs[kMaxNotes] = {};
    int velocity = 96;
    int bassCount = 0;
    int primaryBass = -1;
};

struct PlayData
{
    int lengthSteps = 4 * kStepsPerBar;
    int stepsPerBar = kStepsPerBar;
    int numSlots = 0;
    PlaySlot slots[kMaxSlots];

    int slotAt (double step) const noexcept
    {
        for (int i = numSlots - 1; i >= 0; --i)
            if (step >= (double) slots[i].start)
                return i;
        return numSlots > 0 ? 0 : -1;
    }
};

inline PlayData toPlayData (const Progression& p, const PerformanceParams& perf = {}, double bpm = 120.0, int pass = 0)
{
    PlayData d;
    d.lengthSteps = p.lengthSteps();
    d.stepsPerBar = p.stepsPerBar();
    d.numSlots = p.numSlots;
    const double msPerBeat = 60000.0 / std::max (20.0, bpm);
    const double msPerStep = msPerBeat / (double) kStepsPerQuarter;

    for (int i = 0; i < p.numSlots; ++i)
    {
        const Slot& s = p.slots[(size_t) i];
        PlaySlot& o = d.slots[i];
        o.start = s.start;
        o.end = p.slotEnd (i);
        o.velocity = s.voicing.velocity;

        const int bCount = s.voicing.bassCount > 0 ? s.voicing.bassCount : s.voicing.computeBassCount();
        o.bassCount = bCount;

        int pb = primaryBassNote (s.voicing);
        if (pb < 0)
            pb = pickBassRegister (s.chord.bassPc(), -1, kUpperFloor);
        o.primaryBass = pb;

        PerfInput in;
        in.n = std::min (s.voicing.n, kPerfMaxNotes);
        in.bassCount = bCount;
        for (int j = 0; j < in.n; ++j)
        {
            in.pitch[(size_t) j] = s.voicing.notes[(size_t) j];
            in.velocity[(size_t) j] = s.voicing.velocity;
        }

        const double slotLenMs = (double) (o.end - o.start) * msPerStep;
        const PerfOutput out = applyPerformance (in, perf, slotLenMs, i, pass);

        o.n = std::min (out.n, kMaxNotes);
        for (int j = 0; j < o.n; ++j)
        {
            o.notes[j] = out.notes[(size_t) j].pitch;
            o.velocities[j] = out.notes[(size_t) j].velocity;
            o.offsetMs[j] = out.notes[(size_t) j].offsetMs;
        }
    }
    return d;
}

/** A fresh, already-rolled progression. */
inline Progression makeDefaultProgression (Rng& rng, int moodIdx = 2)
{
    Progression p;
    layoutEven (p);
    applyMood (p, moodIdx);
    roll (p, rng);
    return p;
}

} // namespace cs
