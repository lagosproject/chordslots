// ChordSlots core - progression model, target-driven generator, locks, voicing.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Corpus.h"
#include "BassPatterns.h"
#include "Metrics.h"
#include "Performance.h"
#include "RingInference.h"
#include "Voicing.h"

#include <cstring>
#include <limits>
#include <optional>
#include <random>

namespace cs
{

constexpr int kMaxSlots = 16;
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

//==============================================================================
struct Slot
{
    int start = 0; // steps (24 per quarter note) from the start of the loop
    Chord chord;
    Voicing voicing;
    bool locked = false; // freezes this slot's chord and voicing
    bool cutLocked = false;

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

struct MelodyNote
{
    double start = 0.0, end = 0.0; // fractions of the imported phrase (loop for legacy sessions)
    int pitch = 60;
    int velocity = 90;
};

constexpr size_t kMaxMelodyNotes = 4096;

enum class Ending { Free, ResolveToTonic, LeadBackToStart };

struct Progression
{
    int tonic = 9; // A
    int scale = 1; // Minor
    int bars = 4;
    int timeSigNum = 4;
    int timeSigDen = 4;
    int numSlots = 4;
    int snap = 12; // cut grid in steps: 24 = 1/4, 16 = 1/4T, 12 = 1/8, 8 = 1/8T, 6 = 1/16, 4 = 1/16T, 3 = 1/32
    std::vector<MelodyNote> melody;
    std::string melodyFilename;
    double melodySourceSteps = 0.0; // zero for legacy loop-relative melodies
    double melodySourceBpm = 0.0; // zero when the MIDI file has no usable tempo event
    int melodySourceTonic = -1;
    int melodySourceScale = -1;
    bool autoMelodyKey = false;
    bool fitMelodyToLoop = false;
    float melodyFit = 0.75f;
    float variety = 0.5f;     // sampling temperature for a full Roll
    float adventure = 0.3f;   // 0 = diatonic triads, 1 = borrowed chords and extensions
    Ending ending = Ending::Free;
    float syncopation = 0.3f; // how irregular randomised cuts are
    BassStyle bassStyle = BassStyle::Sustain;
    float bassMotion = 0.0f;  // 0 = keep root in bass, 1 = smooth walking bass / pedal inversions
    float voiceLeading = 0.5f; // 0 = free voicings, 1 = smooth minimal displacement
    bool phraseRepeat = false; // generates 4-slot phrase AA' with cadence variation
    int mood = -1;            // index into moods() the targets came from, -1 = custom
    int style = -1;           // -1 = Auto; otherwise a CorpusStyle value including Free
    int voicingStyle = (int) VoicingStyle::Piano;
    Locks locks;
    std::array<Slot, kMaxSlots> slots {};

    Meter meter() const noexcept { return getMeter (timeSigNum, timeSigDen); }
    int stepsPerBar() const noexcept { return meter().stepsPerBar; }
    int lengthSteps() const noexcept { return bars * stepsPerBar(); }
    double melodyLengthSteps() const noexcept
    {
        return ! fitMelodyToLoop && melodySourceSteps > 0.0 ? melodySourceSteps : lengthSteps();
    }
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
    const bool pinned = std::any_of (p.slots.begin() + 1, p.slots.begin() + p.numSlots,
                                    [] (const Slot& s) { return s.cutLocked; });
    if (! pinned && p.numSlots <= p.maxSlots())
        layoutEven (p);
    else
    {
        for (int left = 0, right = 1; right <= p.numSlots; ++right)
        {
            if (right < p.numSlots && ! p.slots[(size_t) right].cutLocked)
                continue;
            const int start = p.slots[(size_t) left].start;
            const int end = p.slotEnd (right - 1);
            for (int i = left + 1; i < right; ++i)
            {
                const double even = start + (double) (end - start) * (i - left) / (right - left);
                const int snapped = (int) std::lround (even / p.snap) * p.snap;
                p.slots[(size_t) i].start = std::clamp (snapped, p.slots[(size_t) (i - 1)].start + 1,
                                                       end - (right - i));
            }
            left = right;
        }
    }
    const int len = p.lengthSteps();
    const int unit = p.snap;
    const auto m = p.meter();
    const int barLen = m.stepsPerBar;

    for (int i = 1; i < p.numSlots; ++i)
    {
        if (p.slots[(size_t) i].cutLocked || rand01 (rng) >= p.syncopation)
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
    if (i < 1 || i >= p.numSlots || p.slots[(size_t) i].cutLocked)
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
inline std::vector<Candidate> buildCandidates (int tonic, int scaleIdx, float adventure,
                                               float diatonicContext = kMinorDiatonicMajorContext)
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

            const ChordMetrics m = chordMetrics (c, tonic, scaleIdx, 0.5f, diatonicContext);
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
    std::optional<CorpusStyle> defaultStyle = std::nullopt; // Auto follows the current scale
    float diatonicContext = 0.0f; // per-mood strength of kMinorDiatonicMajorContext, used only if kMoodDiatonicContext
};

/** When true, each mood brings its own strength for the minor-key diatonic
    major-chord Brightness context (Mood::diatonicContext) instead of the global
    kMinorDiatonicMajorContext. Off until validated by listening (GENERAL-PLAN). */
inline constexpr bool kMoodDiatonicContext = false;

inline const std::vector<Mood>& moods()
{
    static const std::vector<Mood> m = {
        { "Euphoric",    0, 0.30f, 0.25f, { 0.70f, 0.78f, 0.85f, 0.80f }, { 0.22f, 0.45f, 0.60f, 0.40f }, { 0.70f, 0.80f, 0.90f, 1.00f } },
        { "Uplifting",   1, 0.30f, 0.30f, { 0.35f, 0.60f, 0.70f, 0.62f }, { 0.25f, 0.38f, 0.45f, 0.62f }, { 0.75f, 0.80f, 0.90f, 0.95f } },
        { "Epic",        1, 0.35f, 0.20f, { 0.30f, 0.58f, 0.66f, 0.40f }, kUseContextualTension ? std::vector<float> { 0.24f, 0.42f, 0.62f, 0.64f } : std::vector<float> { 0.22f, 0.40f, 0.62f, 0.68f }, { 0.70f, 0.80f, 0.90f, 1.00f }, std::nullopt, 0.75f },
        { "Melancholic", 1, 0.40f, 0.20f, { 0.30f, 0.25f, 0.55f, 0.28f }, { 0.25f, 0.50f, 0.42f, 0.60f }, { 0.40f, 0.45f, 0.50f, 0.40f }, std::nullopt, 0.75f },
        { "Dark",        3, 0.50f, 0.35f, { 0.25f, 0.15f, 0.30f, 0.10f }, { 0.25f, 0.60f, 0.55f, 0.75f }, { 0.65f, 0.70f, 0.70f, 0.80f } },
        { "Tense",       7, 0.65f, 0.45f, { 0.30f, 0.35f, 0.20f, 0.40f }, { 0.45f, 0.65f, 0.80f, 0.90f }, { 0.50f, 0.60f, 0.70f, 0.85f } },
        { "Dreamy",      4, 0.60f, 0.15f, { 0.85f, 0.70f, 0.90f, 0.75f }, { 0.40f, 0.55f, 0.48f, 0.55f }, { 0.30f, 0.30f, 0.35f, 0.30f }, CorpusStyle::SoulRnb },
        { "Hard",        1, 0.15f, 0.50f, { 0.28f, 0.35f, 0.22f, 0.45f }, kUseContextualTension ? std::vector<float> { 0.24f, 0.54f, 0.55f, 0.64f } : std::vector<float> { 0.22f, 0.50f, 0.55f, 0.65f }, { 1.00f, 1.00f, 1.00f, 1.00f }, std::nullopt, 0.40f },
    };
    return m;
}

/** Diatonic-context strength in force for a progression: the preset's own value
    when kMoodDiatonicContext is on and the targets come from a preset, otherwise
    the global constant. */
inline float diatonicContextFor (const Progression& p)
{
    if constexpr (kMoodDiatonicContext)
        if (p.mood >= 0 && p.mood < (int) moods().size())
            return moods()[(size_t) p.mood].diatonicContext;
    return kMinorDiatonicMajorContext;
}

inline CorpusStyle styleForProgression (const Progression& p) noexcept
{
    if (p.style >= 0)
        return (CorpusStyle) p.style;

    if (p.mood >= 0 && p.mood < (int) moods().size())
    {
        const auto style = moods()[(size_t) p.mood].defaultStyle;
        if (style.has_value())
            return *style;
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

    // Billboard and RS200 transition prior
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

inline float rollTemperature (float variety)
{
    const float value = clamp01 (variety);
    // Piecewise logarithmic interpolation preserves the previous 0.012 midpoint.
    return value <= 0.5f ? 0.004f * std::pow (3.0f, 2.0f * value)
                         : 0.012f * std::pow (2.5f, 2.0f * value - 1.0f);
}

inline SpiralPoint progressionSpiralKey (const Progression& p)
{
    return spiralKey (0, scaleHasMinorThird (p.scale)); // relative to tonic
}

inline float tensionErrorDelta (float oldTension, float newTension, float target) noexcept
{
    const float oldError = oldTension - target, newError = newTension - target;
    return newError * newError - oldError * oldError;
}

inline float estimateContextualTension (const Progression& p, Chord chord, const Chord* previous, float estimate)
{
    const auto current = chordSpiralCenter (chord, p.tonic, p.scale);
    const float momentum = previous != nullptr
        ? cloudMomentum (chordSpiralCenter (*previous, p.tonic, p.scale), current) : 0.0f;
    return blendContextualTension (estimate, tensileStrain (current, progressionSpiralKey (p)), momentum);
}

inline float contextualEditCost (const Progression& p, int slot, const Candidate& cand,
                                  const Chord* previous, const Chord* next)
{
    if constexpr (! kUseContextualTension)
        return 0.0f;

    const auto& current = cand.chord;
    float delta = tensionErrorDelta (cand.tension, estimateContextualTension (p, current, previous, cand.tension),
                                     p.slots[(size_t) slot].targetT);
    if (next != nullptr)
    {
        const float neutral = tensionOf (*next, p.tonic, p.scale);
        delta += tensionErrorDelta (neutral, estimateContextualTension (p, *next, &current, neutral),
                                    p.slots[(size_t) ((slot + 1) % p.numSlots)].targetT);
    }
    return delta;
}

using MelodyProfile = std::array<float, 12>;

inline MelodyProfile slotMelodyProfile (const Progression& p, int slot, bool includeWhenDisabled = false)
{
    MelodyProfile profile {};
    if (p.melody.empty() || (! includeWhenDisabled && p.melodyFit <= 0.0f))
        return profile;

    const double melodyLength = p.melodyLengthSteps();
    const auto meter = p.meter();
    const int slotStart = p.slots[(size_t) slot].start, slotEnd = p.slotEnd (slot);
    for (const auto& note : p.melody)
    {
        if (! (note.start >= 0.0f && note.end <= 1.0f && note.end > note.start))
            continue;
        const double start = note.start * melodyLength;
        const double end = note.end * melodyLength;
        const double overlap = std::min<double> (slotEnd, end) - std::max<double> (slotStart, start);
        if (overlap <= 0)
            continue;
        const int offset = (int) std::lround (start) % meter.stepsPerBar;
        const auto endStrong = meter.strongOffsets.end();
        const bool strong = std::find (meter.strongOffsets.begin(), endStrong, offset) != endStrong;
        profile[(size_t) mod12 (note.pitch)] += (float) overlap * (strong ? 1.5f : 1.0f);
    }
    return profile;
}

constexpr float melodyClashCost = 1.4f;

inline float melodyNoteCost (Chord chord, int pitchClass)
{
    pitchClass = mod12 (pitchClass);
    const int interval = mod12 (pitchClass - chord.root);
    const auto& notes = chordTypes()[(size_t) chord.type].iv;
    if ((chordMask (chord) & (1u << pitchClass)) != 0)
        return 0.0f;

    const bool major = hasMajorThird (notes), minor = hasMinorThird (notes);
    const bool dominant = seventhKind (notes) == SeventhKind::Minor7 && (major || isSus (notes));
    if ((major && interval == 5) || (! dominant && interval == 1) || (dominant && interval == 11))
        return melodyClashCost;
    if (interval == 2 || (minor && interval == 5) || (major && interval == 6) || interval == 9)
        return 0.12f;
    return 0.55f;
}

inline float melodyCost (Chord chord, const MelodyProfile& profile, float weight)
{
    if (weight <= 0.0f)
        return 0.0f;

    float total = 0.0f, penalty = 0.0f;
    for (int pc = 0; pc < 12; ++pc)
    {
        const float duration = profile[(size_t) pc];
        if (duration > 0.0f)
        {
            total += duration;
            penalty += duration * melodyNoteCost (chord, pc);
        }
    }
    return total > 0.0f ? weight * penalty / total : 0.0f;
}

/** Intrinsic compatibility of a chord with the slot's melody, independent of the generator weight. */
inline float melodyMatch (Chord chord, const MelodyProfile& profile)
{
    return clamp01 (1.0f - melodyCost (chord, profile, 1.0f) / melodyClashCost);
}

inline unsigned melodyClashMask (Chord chord, const MelodyProfile& profile)
{
    unsigned clashes = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (profile[(size_t) pc] > 0.0f && melodyNoteCost (chord, pc) == melodyClashCost)
            clashes |= 1u << pc;
    return clashes;
}

inline float slotUnaryCost (const Progression& p, int slot, const Candidate& cand,
                            const MelodyProfile* melody = nullptr)
{
    const Slot& s = p.slots[(size_t) slot];
    const float dB = cand.brightness - s.targetB;
    const float dT = cand.tension - s.targetT;
    float cost = dB * dB + dT * dT;
    if (! p.melody.empty() && p.melodyFit > 0.0f)
        cost += melodyCost (cand.chord, melody != nullptr ? *melody : slotMelodyProfile (p, slot), p.melodyFit);

    // Low adventure keeps to plain triads in the scale; the price of colour
    // fades quadratically as the dial goes up.
    const float tame = (1.0f - p.adventure) * (1.0f - p.adventure);
    cost += tame * (0.06f * (float) cand.complexity + 0.08f * (float) cand.outside);

    if (slot == 0 && cand.chord.root == p.tonic)
        cost -= 0.012f; // a gentle pull towards starting at home

    return cost;
}

inline bool oscillates (Chord first, Chord middle, Chord last) noexcept
{
    return first.root == last.root && first.type == last.type
        && (first.root != middle.root || first.type != middle.type);
}

inline float slotCost (const Progression& p, int slot, const Candidate& cand,
                       const Chord* prev, const Chord* next, const Chord* prev2 = nullptr,
                       const MelodyProfile* melody = nullptr)
{
    float cost = slotUnaryCost (p, slot, cand, melody) + contextualEditCost (p, slot, cand, prev, next);
    // A-B-A Anti-Oscillation Penalty: discourage ping-ponging back to the chord 2 slots ago
    if (prev2 != nullptr && prev != nullptr && oscillates (*prev2, *prev, cand.chord))
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
        const ChordMetrics m = chordMetrics (cur, p.tonic, p.scale, 0.5f, diatonicContextFor (p));
        v.push_back ({ cur, m.brightness, m.tension, tonesOutsideScale (cur, p.tonic, p.scale),
                       chordTypes()[(size_t) cur.type].complexity });
    }
    return v;
}

namespace detail
{

inline Candidate currentCandidate (const Progression& p, int slot)
{
    const Chord chord = p.slots[(size_t) slot].chord;
    const auto metrics = chordMetrics (chord, p.tonic, p.scale, 0.5f, diatonicContextFor (p));
    return { chord, metrics.brightness, metrics.tension, tonesOutsideScale (chord, p.tonic, p.scale),
             chordTypes()[(size_t) chord.type].complexity };
}

inline std::vector<int> bassOptions (const Progression& p, int slot, Chord chord)
{
    const auto& stored = p.slots[(size_t) slot];
    if (stored.locked)
        return { stored.chord.bassInt };
    if (p.locks.voicing)
        return { isBassIntValid (chord.type, stored.chord.bassInt) ? stored.chord.bassInt : 0 };

    std::vector<int> options { 0 };
    if (p.bassMotion >= 0.05f)
        for (int interval : chordTypes()[(size_t) chord.type].iv)
        {
            const int pitchClass = mod12 (interval);
            if (std::find (options.begin(), options.end(), pitchClass) == options.end())
                options.push_back (pitchClass);
        }
    return options;
}

inline float bassLeapCost (int first, int second) noexcept
{
    const int leap = std::abs (first - second);
    return (float) leap + (leap > 7 ? 4.0f : 0.0f);
}

// Convert semitone costs from the bass-only DP into the harmonic cost scale.
constexpr float kBassCostScale = 0.03f;

inline float jointBassCost (const Progression& p, Chord a, Chord b)
{
    if (p.bassMotion < 0.05f)
        return 0.0f;
    return kBassCostScale * bassLeapCost (pickBassRegister (a.bassPc(), 40),
                                         pickBassRegister (b.bassPc(), 40));
}

inline float harmonicTransitionCost (const Progression& p, CorpusStyle style, Chord a, Chord b)
{
    a.bassInt = b.bassInt = 0;
    return transitionCost (a, b, p.tonic, p.adventure, p.scale, style);
}

inline bool hasTonicFunction (const Progression& p, Chord chord)
{
    const unsigned mask = scaleMask (p.scale);
    const auto& quality = chordProperties (chord.type);
    const bool minor = scaleHasMinorThird (p.scale);
    return chord.root == p.tonic
        && (minor ? quality.hasMinorThird : quality.hasMajorThird)
        && ((mask & (1u << 7)) != 0 ? quality.hasPerfectFifth : quality.hasDimFifth)
        && tonesOutsideScale (chord, p.tonic, p.scale) == 0;
}

// Cadential movement relative to the destination. Borrowed dominants can
// score here only if the existing candidate rules have already admitted them.
inline float preparationBonus (const Progression& p, Chord from, Chord to)
{
    const int degree = mod12 (from.root - to.root);
    const auto& quality = chordProperties (from.type);
    if (degree == 7)
        return quality.hasMajorThird ? 0.10f : 0.06f;
    if (degree == 11 && quality.hasDimFifth)
        return 0.09f;
    if (tonesOutsideScale (from, p.tonic, p.scale) == 0)
    {
        if (degree == 10 || degree == 1)
            return 0.085f; // modal bVII or bII
        if (degree == 5)
            return 0.065f; // plagal motion
    }
    return 0.0f;
}

inline float inversionCost (const Progression& p, int slot, int bassInt)
{
    return bassInt == 0 ? 0.0f : kBassCostScale * (1.0f - p.bassMotion) * (slot == 0 ? 9.0f : 7.0f);
}

struct ChordVariable
{
    std::vector<Chord> states;
    std::vector<float> costs;
};

inline ChordVariable makeChordVariable (const Progression& p, int first, int second,
                                       const std::vector<Candidate>& pool, Ending ending)
{
    ChordVariable variable;
    std::vector<std::pair<float, Chord>> options;
    const auto firstMelody = slotMelodyProfile (p, first);
    const auto secondMelody = second >= 0 ? slotMelodyProfile (p, second) : MelodyProfile {};
    for (const auto& candidate : pool)
    {
        const auto firstOptions = bassOptions (p, first, candidate.chord);
        const auto secondOptions = second < 0 ? std::vector<int> {} : bassOptions (p, second, candidate.chord);
        for (int bassInt : firstOptions)
        {
            if (second >= 0 && std::find (secondOptions.begin(), secondOptions.end(), bassInt) == secondOptions.end())
                continue;

            Chord chord = candidate.chord;
            chord.bassInt = bassInt;
            float cost = slotUnaryCost (p, first, candidate, &firstMelody) + inversionCost (p, first, bassInt);
            if constexpr (kUseContextualTension)
                if (p.numSlots == 1)
                    cost += contextualEditCost (p, first, candidate, nullptr, nullptr);
            if (second >= 0)
                cost += slotUnaryCost (p, second, candidate, &secondMelody) + inversionCost (p, second, bassInt);
            if (ending == Ending::ResolveToTonic && first == p.numSlots - 1 && hasTonicFunction (p, chord))
                cost -= 0.22f;
            options.emplace_back (cost, chord);
        }
    }

    constexpr size_t beamSize = 64;
    const auto n = std::min (options.size(), beamSize);
    std::partial_sort (options.begin(), options.begin() + n, options.end(), [] (const auto& a, const auto& b)
    {
        if (a.first != b.first)
            return a.first < b.first;
        if (a.second.root != b.second.root)
            return a.second.root < b.second.root;
        if (a.second.type != b.second.type)
            return a.second.type < b.second.type;
        return a.second.bassInt < b.second.bassInt;
    });
    for (size_t i = 0; i < n; ++i)
    {
        variable.costs.push_back (options[i].first);
        variable.states.push_back (options[i].second);
    }
    return variable;
}

inline void correctOscillations (const Progression& p, const RingInference& ring,
                                 const std::vector<ChordVariable>& variables,
                                 const std::vector<int>& nodeOf, std::vector<int>& sampled,
                                 Rng& rng, float temperature)
{
    const int n = p.numSlots;
    if (n < 3 || p.adventure >= 1.0f)
        return;

    std::vector<int> mutableNodes;
    for (int node = 0; node < (int) variables.size(); ++node)
        if (ring.size (node) > 1)
            mutableNodes.push_back (node);
    if (mutableNodes.empty())
        return;

    const auto chordAt = [&] (int slot)
    {
        const int node = nodeOf[(size_t) (slot % n)];
        return variables[(size_t) node].states[(size_t) sampled[(size_t) node]];
    };
    const auto countOscillations = [&]
    {
        int count = 0;
        for (int slot = 0; slot < n; ++slot)
            count += oscillates (chordAt (slot), chordAt (slot + 1), chordAt (slot + 2));
        return count;
    };
    int oscillations = countOscillations();
    const double penalty = 0.08 * (1.0 - p.adventure) / temperature;

    std::uniform_int_distribution<size_t> chooseNode (0, mutableNodes.size() - 1);
    for (int step = 0; step < 2 * n; ++step)
    {
        const int node = mutableNodes[chooseNode (rng)];
        const int previous = sampled[(size_t) node];
        const int proposal = RingInference::pick (ring.conditional (node, sampled), rand01 (rng));
        if (proposal == previous)
            continue;
        sampled[(size_t) node] = proposal;
        const int proposedOscillations = countOscillations();
        // The first-order conditional cancels from the MH acceptance ratio.
        const int difference = proposedOscillations - oscillations;
        if (difference <= 0 || rand01 (rng) < std::exp (-penalty * difference))
            oscillations = proposedOscillations;
        else
            sampled[(size_t) node] = previous;
    }

    // A first-order proposal can put almost all its mass on an alternating
    // pair at low temperature. A full-conditional sweep includes the ABA
    // term in the proposal, so every draw is accepted by the target model.
    for (int node : mutableNodes)
    {
        auto weights = ring.conditional (node, sampled);
        for (size_t state = 0; state < weights.size(); ++state)
        {
            sampled[(size_t) node] = (int) state;
            weights[state] *= std::exp (-penalty * countOscillations());
        }
        sampled[(size_t) node] = RingInference::pick (weights, rand01 (rng));
    }
}

} // namespace detail

/** Draws from the first-order Boltzmann distribution on the chord loop. */
inline void generateChords (Progression& p, Rng& rng, float temperature = 0.012f, bool applyEnding = true)
{
    const int n = p.numSlots;
    if (n < 1)
        return;

    const auto ending = applyEnding ? p.ending : Ending::Free;
    const auto all = buildCandidates (p.tonic, p.scale, p.adventure, diatonicContextFor (p));
    if (all.empty())
        return;

    std::vector<std::vector<Candidate>> pools ((size_t) n);
    for (int slot = 0; slot < n; ++slot)
        pools[(size_t) slot] = slotChordIsFixed (p, slot)
            ? std::vector<Candidate> { detail::currentCandidate (p, slot) }
            : slotCandidates (p, slot, all);

    const bool repeat = p.phraseRepeat && n >= 4 && n % 2 == 0;
    const int half = n / 2;
    std::vector<detail::ChordVariable> variables;
    std::vector<int> nodeOf ((size_t) n, -1);
    for (int slot = 0; slot < n; ++slot)
    {
        if (nodeOf[(size_t) slot] >= 0)
            continue;
        const int node = (int) variables.size();
        nodeOf[(size_t) slot] = node;
        const int partner = slot + half;
        if (repeat && slot < half - 1 && ! slotChordIsFixed (p, slot) && ! slotChordIsFixed (p, partner))
        {
            std::vector<Candidate> common;
            const auto& other = pools[(size_t) partner];
            for (const auto& candidate : pools[(size_t) slot])
                if (std::any_of (other.begin(), other.end(), [&] (const auto& c) { return c.chord == candidate.chord; }))
                    common.push_back (candidate);
            if (! common.empty())
            {
                auto combined = detail::makeChordVariable (p, slot, partner, common, ending);
                if (! combined.states.empty())
                {
                    nodeOf[(size_t) partner] = node;
                    variables.push_back (std::move (combined));
                    continue;
                }
            }
        }
        variables.push_back (detail::makeChordVariable (p, slot, -1, pools[(size_t) slot], ending));
    }

    std::vector<detail::RingInference::Weights> unary;
    for (const auto& variable : variables)
        unary.push_back (detail::boltzmannWeights (variable.costs, temperature));
    detail::RingInference ring (std::move (unary));
    const CorpusStyle style = styleForProgression (p);
    const size_t numTypes = chordTypes().size();
    const size_t chordCount = 12 * numTypes;
    struct EstimatedTension
    {
        SpiralPoint center;
        float strain = -1.0f;
        float neutral = 0.0f;
    };
    std::vector<EstimatedTension> estimates (kUseContextualTension ? chordCount : 0);
    const auto key = progressionSpiralKey (p);
    const auto lookupTension = [&] (Chord chord) -> const EstimatedTension&
    {
        auto& estimate = estimates[(size_t) chord.root * numTypes + (size_t) chord.type];
        if (estimate.strain < 0.0f)
        {
            estimate.center = chordSpiralCenter (chord, p.tonic, p.scale);
            estimate.strain = tensileStrain (estimate.center, key);
            chord.bassInt = 0;
            estimate.neutral = tensionOf (chord, p.tonic, p.scale);
        }
        return estimate;
    };
    std::vector<float> transitionCache (chordCount * chordCount, -1.0f);
    const auto transition = [&] (Chord a, Chord b)
    {
        const size_t src = (size_t) a.root * numTypes + (size_t) a.type;
        const size_t dst = (size_t) b.root * numTypes + (size_t) b.type;
        float& cost = transitionCache[src * chordCount + dst];
        if (cost < 0.0f)
        {
            cost = detail::harmonicTransitionCost (p, style, a, b);
        }
        return cost;
    };
    if (n > 1)
    {
        for (int slot = 0; slot < n; ++slot)
        {
            const int from = nodeOf[(size_t) slot], to = nodeOf[(size_t) ((slot + 1) % n)];
            const auto& first = variables[(size_t) from].states;
            const auto& second = variables[(size_t) to].states;
            std::vector<float> costs;
            costs.reserve (first.size() * second.size());
            for (Chord a : first)
                for (Chord b : second)
                {
                    const float harmonic = transition (a, b);
                    float cost = harmonic + detail::jointBassCost (p, a, b);
                    if (slot == n - 1 && ending == Ending::LeadBackToStart)
                        cost += harmonic - detail::preparationBonus (p, a, b);
                    if (slot == n - 2 && ending == Ending::ResolveToTonic && detail::hasTonicFunction (p, b))
                        cost -= detail::preparationBonus (p, a, b);
                    if constexpr (kUseContextualTension)
                    {
                        const auto& previous = lookupTension (a);
                        const auto& current = lookupTension (b);
                        const float momentum = cloudMomentum (previous.center, current.center);
                        const float measured = blendContextualTension (current.neutral, current.strain, momentum);
                        cost += tensionErrorDelta (current.neutral, measured, p.slots[(size_t) ((slot + 1) % n)].targetT);
                    }
                    costs.push_back (cost);
                }
            ring.addEdge (from, to, detail::boltzmannWeights (costs, temperature));
        }
    }

    auto sampled = ring.sample ([&] { return rand01 (rng); });
    if (sampled.empty())
        return;
    detail::correctOscillations (p, ring, variables, nodeOf, sampled, rng, temperature);
    for (int slot = 0; slot < n; ++slot)
    {
        const int node = nodeOf[(size_t) slot];
        p.slots[(size_t) slot].chord = variables[(size_t) node].states[(size_t) sampled[(size_t) node]];
    }
}

/** Scores distinct chord identities by the best joint state with the other slots fixed. */
inline std::vector<std::pair<float, Chord>> harmonicAlternatives (const Progression& p, int slot, int maxCount)
{
    const int n = p.numSlots;
    if (slot < 0 || slot >= n || maxCount <= 0)
        return {};

    // The manual picker retains its wider palette at low Adventure.
    const auto all = buildCandidates (p.tonic, p.scale, std::max (p.adventure, 0.45f), diatonicContextFor (p));
    const auto current = p.slots[(size_t) slot].chord;
    const auto neighbour = [&] (int offset) { return p.slots[(size_t) ((slot + n + offset) % n)].chord; };
    const Chord previous = neighbour (-1), next = neighbour (1);
    const auto style = styleForProgression (p);
    const auto melody = slotMelodyProfile (p, slot);
    std::vector<std::pair<float, Chord>> scored;
    for (const auto& candidate : all)
    {
        if (candidate.chord.root == current.root && candidate.chord.type == current.type)
            continue;

        const Chord chord = candidate.chord;
        float harmonic = slotUnaryCost (p, slot, candidate, &melody);
        harmonic += contextualEditCost (p, slot, candidate, n > 1 ? &previous : nullptr, n > 1 ? &next : nullptr);
        if (n > 1)
            harmonic += detail::harmonicTransitionCost (p, style, previous, chord)
                      + detail::harmonicTransitionCost (p, style, chord, next);
        if (n > 2)
        {
            const int count = oscillates (neighbour (-2), previous, chord)
                            + oscillates (previous, chord, next)
                            + oscillates (chord, next, neighbour (2));
            harmonic += (1.0f - p.adventure) * 0.08f * (float) count;
        }

        Chord best = chord;
        float bestCost = std::numeric_limits<float>::infinity();
        for (int bassInt : detail::bassOptions (p, slot, chord))
        {
            Chord proposed = chord;
            proposed.bassInt = bassInt;
            float cost = harmonic + detail::inversionCost (p, slot, bassInt);
            if (n > 1)
                cost += detail::jointBassCost (p, previous, proposed) + detail::jointBassCost (p, proposed, next);
            if (cost < bestCost)
            {
                bestCost = cost;
                best = proposed;
            }
        }
        scored.emplace_back (bestCost, best);
    }

    const auto count = std::min (scored.size(), (size_t) maxCount);
    std::partial_sort (scored.begin(), scored.begin() + count, scored.end(), [] (const auto& a, const auto& b)
    {
        if (a.first != b.first) return a.first < b.first;
        if (a.second.root != b.second.root) return a.second.root < b.second.root;
        return a.second.type < b.second.type;
    });
    scored.resize (count);
    return scored;
}

inline std::vector<Chord> alternatives (const Progression& p, int slot, int maxCount)
{
    std::vector<Chord> out;
    for (const auto& entry : harmonicAlternatives (p, slot, maxCount))
        out.push_back (entry.second);
    return out;
}

struct VoicedAlternative
{
    Chord chord;
    Voicing voicing;
    float cost;
};

/** Returns the requested number of alternatives per scale group, in display order. */
inline std::vector<VoicedAlternative> inspectAlternatives (const Progression& p, int slot,
                                                         size_t inScale, size_t outside, Rng rng)
{
    const auto harmonic = harmonicAlternatives (p, slot, std::numeric_limits<int>::max());
    if (harmonic.empty())
        return {};

    const auto& target = p.slots[(size_t) slot];
    const int n = p.numSlots;
    const auto* previous = n > 1 ? &p.slots[(size_t) ((slot + n - 1) % n)].voicing : nullptr;
    const auto* next = n > 1 ? &p.slots[(size_t) ((slot + 1) % n)].voicing : nullptr;
    const auto style = (VoicingStyle) p.voicingStyle;
    std::vector<VoicedAlternative> groups[2];
    constexpr float maxMovementCost = 0.02f;

    for (auto [cost, chord] : harmonic)
    {
        const bool isOutside = scaleMembership (chord, p.tonic, p.scale).outsideChord != 0;
        auto& group = groups[isOutside];
        const size_t limit = isOutside ? outside : inScale;
        if (limit == 0 || (group.size() == limit && cost >= group.back().cost))
            continue;

        sanitizeBass (chord);
        auto previewRng = rng;
        const auto v = makeVoicing (chord, previous != nullptr ? previous : &target.voicing,
                                    centreForBrightness (target.targetB), target.targetE,
                                    previewRng, 2.0f, 0.0f, style, next);
        const float movement = previous != nullptr
            ? voiceLeadingDistance (*previous, v) + voiceLeadingDistance (v, *next) : 0.0f;
        // Only harmonically close candidates can trade places. Since the added cost
        // is non-negative, the harmonic score alone can rule out expensive rows.
        const VoicedAlternative entry { chord, v, cost + maxMovementCost * movement / (4.0f + movement) };
        const auto pos = std::upper_bound (group.begin(), group.end(), entry,
                                           [] (const auto& a, const auto& b) { return a.cost < b.cost; });
        group.insert (pos, entry);
        if (group.size() > limit)
            group.pop_back();
    }

    groups[0].insert (groups[0].end(), groups[1].begin(), groups[1].end());
    return std::move (groups[0]);
}

//==============================================================================
inline float measuredContextualTension (const Progression& p, int slot)
{
    const auto& current = p.slots[(size_t) slot];
    const auto center = [&] (const Slot& s)
    {
        return voicingSpiralCenter (s.chord, p.tonic, p.scale, s.voicing.notes.data(), s.voicing.n);
    };
    const auto cloud = center (current);
    const auto& previous = p.slots[(size_t) ((slot + p.numSlots - 1) % p.numSlots)];
    const float momentum = p.numSlots > 1 ? cloudMomentum (center (previous), cloud) : 0.0f;
    const float local = voicedTension (current.chord, p.tonic, p.scale, current.voicing.notes.data(), current.voicing.n);
    return blendContextualTension (local, tensileStrain (cloud, progressionSpiralKey (p)), momentum);
}

/** Recomputes the measured metrics of one slot from its chord and voicing. */
inline void updateActualsLocal (Progression& p, int i)
{
    Slot& s = p.slots[(size_t) i];
    int upper[kMaxNotes];
    int n = 0;
    for (int j = 0; j < s.voicing.n; ++j)
        if (s.voicing.notes[(size_t) j] >= kUpperFloor + s.voicing.transposeOffset)
            upper[n++] = s.voicing.notes[(size_t) j];

    s.brightness = combineBrightness (contextQualityBrightness (s.chord, p.tonic, p.scale, kMinorDominantContext, diatonicContextFor (p)),
                                      modalBrightness (s.chord, p.tonic, p.scale),
                                      registerBrightness (upper, n));
    s.tension = kUseContextualTension ? measuredContextualTension (p, i)
                                      : tensionOf (s.chord, p.tonic, p.scale);

    // Energy is a performance property (loudness and density), not a harmonic one.
    const float vel = clamp01 (((float) s.voicing.velocity - 45.0f) / 75.0f);
    const float density = normalizedVoicingDensity (s.voicing.n, (VoicingStyle) p.voicingStyle);
    s.energy = clamp01 (0.6f * vel + 0.4f * density);
}

inline void updateActuals (Progression& p, int i)
{
    updateActualsLocal (p, i);
    if constexpr (kUseContextualTension)
        if (p.numSlots > 1)
        {
            const int next = (i + 1) % p.numSlots;
            p.slots[(size_t) next].tension = measuredContextualTension (p, next);
        }
}

inline void updateActuals (Progression& p)
{
    for (int i = 0; i < p.numSlots; ++i)
        updateActualsLocal (p, i);
}

inline void voiceRolledSlot (Progression& p, int i, Rng& rng, bool chordChanged)
{
    Slot& s = p.slots[(size_t) i];
    if (s.locked && s.voicing.n > 0)
        return;

    const auto style = (VoicingStyle) p.voicingStyle;
    const float centre = centreForBrightness (s.targetB);
    if (p.locks.voicing && s.voicing.n > 0)
    {
        if (chordChanged)
        {
            const Voicing old = s.voicing;
            s.voicing = makeVoicing (s.chord, &old, centre, s.targetE, rng, 3.0f, 0.0f, style);
        }
        else
            s.voicing.velocity = std::clamp ((int) std::lround (45.0f + 75.0f * s.targetE), 1, 127);
        return;
    }

    const auto* ref = p.numSlots > 1 ? &p.slots[(size_t) ((i + p.numSlots - 1) % p.numSlots)].voicing : nullptr;
    s.voicing = makeVoicing (s.chord, ref, centre, s.targetE, rng, 1.0f, 0.7f, style);
}

/** Voices the whole progression with smooth voice leading.
    @param changed  per-slot flags: true if the chord changed since the stored
                    voicing was made (nullptr = treat every slot as changed) */
inline void voiceAll (Progression& p, Rng& rng, const bool* changed = nullptr)
{
    for (int i = 0; i < p.numSlots; ++i)
        voiceRolledSlot (p, i, rng, changed == nullptr || changed[i]);
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
        slotOpts[(size_t) i] = detail::bassOptions (p, i, p.slots[(size_t) i].chord);

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
                    const float leapCost = detail::bassLeapCost (cCur.pitch, cPrev.pitch);
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
            const float wrapCost = detail::bassLeapCost (c0.pitch, cEnd.pitch);
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
    VoicingStyle style = (VoicingStyle) p.voicingStyle;
    auto uppers = generateUpperCandidates (s.chord, style);
    if (uppers.empty())
        return { s.voicing };

    const float centre = centreForBrightness (s.targetB);
    struct Scored
    {
        Voicing v;
        float dist;
        bool hasSemitone;
    };
    std::vector<Scored> scored;

    for (const auto& notes : uppers)
    {
        float mean = 0.0f;
        for (int x : notes) mean += (float) x;
        mean /= (float) notes.size();

        const float dist = std::abs (mean - centre);
        if (dist > 14.0f)
            continue;

        if (hasAdjacentGapViolation (notes, style))
            continue;

        Voicing v = assembleVoicing (s.chord, notes, s.targetE, style, -1);
        if (style != VoicingStyle::Close && violatesLowIntervalLimit (v))
            continue;

        scored.push_back ({ v, dist, hasAdjacentSemitoneCluster (notes) });
    }

    if (style != VoicingStyle::Close && ! scored.empty())
    {
        bool anyClean = false;
        for (const auto& sc : scored)
            if (! sc.hasSemitone) { anyClean = true; break; }
        if (anyClean)
        {
            std::vector<Scored> cleanScored;
            for (const auto& sc : scored)
                if (! sc.hasSemitone) cleanScored.push_back (sc);
            scored = std::move (cleanScored);
        }
    }

    std::sort (scored.begin(), scored.end(), [] (const Scored& a, const Scored& b) {
        return a.dist < b.dist;
    });

    std::vector<Voicing> candidates;
    constexpr size_t kMaxCands = 8;
    for (size_t i = 0; i < scored.size() && i < kMaxCands; ++i)
        candidates.push_back (scored[i].v);

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

    generateChords (p, rng, rollTemperature (p.variety));

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

    s.voicing = makeVoicing (c, ref, centreForBrightness (s.targetB), s.targetE, rng, 2.0f, 0.0f, (VoicingStyle) p.voicingStyle);
    updateActuals (p, slot);
}

enum class InversionDirection { Up, Down };

inline bool canInvertChord (const Voicing& voicing, InversionDirection direction) noexcept
{
    const int bassCount = voicing.computeBassCount();
    if (voicing.n - bassCount < 2)
        return false;
    return direction == InversionDirection::Up
         ? voicing.notes[(size_t) bassCount] <= 115
         : voicing.notes[(size_t) (voicing.n - 1)] >= std::max (0, kUpperFloor + voicing.transposeOffset) + 12;
}

/** Moves an outer chord voice by one octave, leaving the separate bass in place. */
inline void invertChord (Progression& p, int slot, InversionDirection direction)
{
    if (slot < 0 || slot >= p.numSlots)
        return;

    auto& voicing = p.slots[(size_t) slot].voicing;
    if (! canInvertChord (voicing, direction))
        return;

    const auto first = voicing.notes.begin() + voicing.computeBassCount();
    const auto end = voicing.notes.begin() + voicing.n;
    if (direction == InversionDirection::Up)
        *first += 12;
    else
        *(end - 1) -= 12;
    std::sort (first, end);
    // A unison on one MIDI channel is a single sounding note.
    voicing.n = (int) (std::unique (first, end) - voicing.notes.begin());
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
    const auto all = buildCandidates (p.tonic, p.scale, std::max (p.adventure, 0.45f), diatonicContextFor (p));
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
    const auto melody = slotMelodyProfile (p, slot);
    for (size_t i = 0; i < candidates.size(); ++i)
        costs[i] = slotCost (p, slot, candidates[i], prev, next, prev2, &melody);

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
    const auto all = buildCandidates (p.tonic, p.scale, std::max (p.adventure, 0.45f), diatonicContextFor (p));
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
    const auto melody = slotMelodyProfile (p, slot);
    for (size_t i = 0; i < candidates.size(); ++i)
        costs[i] = slotCost (p, slot, candidates[i], prev, next, prev2, &melody);

    const int pick = sampleSoftmin (costs, 0.004f, rng);
    if (pick >= 0)
    {
        Chord nextChord = candidates[(size_t) pick].chord;
        nextChord.bassInt = s.chord.bassInt;
        sanitizeBass (nextChord);
        setChord (p, slot, nextChord, rng);
    }
}

/** Cycles the bass note for a single slot. */
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

inline bool splitSlot (Progression&, int, int);
inline bool removeSlot (Progression&, int);

/** Changes the number of slots, keeping targets/chords by position. */
inline void setNumSlots (Progression& p, int newCount)
{
    newCount = std::clamp (newCount, 1, p.maxSlots());
    const int old = std::max (1, p.numSlots);
    if (newCount == old)
        return;
    if (std::any_of (p.slots.begin() + 1, p.slots.begin() + old,
                     [] (const Slot& s) { return s.cutLocked; }))
    {
        for (int i = old - 1; i >= 1 && p.numSlots > newCount; --i)
            removeSlot (p, i);

        while (p.numSlots < newCount)
        {
            int bestSlot = -1, bestWidth = 0, split = 0;
            for (int i = 0; i < p.numSlots; ++i)
            {
                const int start = p.slots[(size_t) i].start;
                const int end = p.slotEnd (i);
                const int first = (start / p.snap + 1) * p.snap;
                if (first >= end || end - start <= bestWidth)
                    continue;
                bestSlot = i;
                bestWidth = end - start;
                split = std::clamp (((start + end) / 2 / p.snap) * p.snap, first,
                                    ((end - 1) / p.snap) * p.snap);
            }
            if (bestSlot < 0 || ! splitSlot (p, bestSlot, split))
                break;
        }
        return;
    }

    const auto prev = p.slots;
    for (int i = 0; i < newCount; ++i)
    {
        p.slots[(size_t) i] = prev[(size_t) std::min (old - 1, i * old / newCount)];
        p.slots[(size_t) i].locked = false;
    }
    p.numSlots = newCount;
    layoutEven (p);
    if constexpr (kUseContextualTension)
        updateActuals (p);
}

inline void setBars (Progression& p, int newBars)
{
    newBars = std::clamp (newBars, 1, kMaxBars);
    if (newBars == p.bars)
        return;
    const int oldCount = p.numSlots;
    const double ratio = (double) newBars / p.bars;
    p.bars = newBars;
    if (std::none_of (p.slots.begin() + 1, p.slots.begin() + p.numSlots,
                      [] (const Slot& s) { return s.cutLocked; }))
        p.numSlots = std::min (p.numSlots, p.maxSlots());
    for (int i = 0; i < p.numSlots; ++i)
        p.slots[(size_t) i].start = (int) std::lround (p.slots[(size_t) i].start * ratio / p.snap) * p.snap;
    sanitizeRhythm (p);
    if constexpr (kUseContextualTension)
        if (p.numSlots != oldCount)
            updateActuals (p);
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
    p.slots[(size_t) (i + 1)].cutLocked = false;
    ++p.numSlots;
    if constexpr (kUseContextualTension)
        updateActuals (p, i + 1);
    return true;
}

/** Removes the cut at the start of slot i (i >= 1): the previous slot absorbs it. */
inline bool removeSlot (Progression& p, int i)
{
    if (i < 1 || i >= p.numSlots || p.slots[(size_t) i].cutLocked)
        return false;
    for (int j = i; j + 1 < p.numSlots; ++j)
        p.slots[(size_t) j] = p.slots[(size_t) (j + 1)];
    --p.numSlots;
    if constexpr (kUseContextualTension)
    {
        const int changed = i % p.numSlots;
        p.slots[(size_t) changed].tension = measuredContextualTension (p, changed);
    }
    return true;
}

/** Re-picks the chord of selected slots only (bit i of mask = slot i), close
    to the best fit, leaving everything else untouched. Used when the user
    edits one slot's target. Locked slots and layer locks are respected. */
inline void rerollSlots (Progression& p, unsigned mask, Rng& rng, float temperature = 0.004f,
                         bool revoiceRepeated = false, bool applyEnding = false)
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

    generateChords (p, rng, temperature, applyEnding);

    for (int i = 0; i < p.numSlots; ++i)
    {
        p.slots[(size_t) i].locked = wasLocked[i];
        const bool chordChanged = before[i] != p.slots[(size_t) i].chord;
        if ((mask & (1u << i)) == 0 || wasLocked[i])
            continue;
        if (revoiceRepeated || (chordChanged && p.locks.voicing))
            voiceRolledSlot (p, i, rng, chordChanged);
        else if (chordChanged)
            setChord (p, i, p.slots[(size_t) i].chord, rng);
    }
    updateActuals (p);
}

inline void rollSelectedSlot (Progression& p, int slot, Rng& rng)
{
    if (slot >= 0 && slot < p.numSlots)
        rerollSlots (p, 1u << slot, rng, 0.004f, true, slot == p.numSlots - 1);
}

/** Applies the energy targets (density and velocity) while keeping each
    voicing's shape. Only slots whose bit is set in `mask` are touched (default: all). */
inline void applyEnergy (Progression& p, Rng& rng, unsigned mask = ~0u)
{
    for (int i = 0; i < p.numSlots; ++i)
    {
        Slot& s = p.slots[(size_t) i];
        if ((mask & (1u << i)) == 0 || s.locked || s.voicing.n == 0)
            continue;
        const Voicing old = s.voicing;
        s.voicing = makeVoicing (s.chord, &old, centreForBrightness (s.targetB), s.targetE, rng, 3.0f, 0.0f, (VoicingStyle) p.voicingStyle);
    }
    updateActuals (p);
}

inline void setTonalContext (Progression& p, int tonic, int scale)
{
    p.tonic = mod12 (tonic);
    p.scale = scale;
    updateActuals (p);
}

inline bool hasRelativeKey (const Progression& p) noexcept
{
    return p.scale == 0 || p.scale == 1;
}

inline void goToRelative (Progression& p)
{
    if (hasRelativeKey (p))
        setTonalContext (p, p.tonic + (p.scale == 0 ? -3 : 3), 1 - p.scale);
}

inline int playbackBassNote (const Slot& slot) noexcept
{
    const int bass = primaryBassNote (slot.voicing);
    if (bass >= 0)
        return bass;
    const int offset = slot.voicing.transposeOffset;
    return pickBassRegister (mod12 (slot.chord.bassPc() - offset)) + offset;
}

inline bool canTransposeProgression (const Progression& p, int semitones) noexcept
{
    if (semitones < -127 || semitones > 127)
        return false;

    const auto fits = [semitones] (int note)
    {
        return note + semitones >= 0 && note + semitones <= 127;
    };
    for (int i = 0; i < p.numSlots; ++i)
    {
        const auto& slot = p.slots[(size_t) i];
        for (int j = 0; j < slot.voicing.n; ++j)
            if (! fits (slot.voicing.notes[(size_t) j]))
                return false;

        const int bass = playbackBassNote (slot);
        if (! fits (bass))
            return false;
        if (p.bassStyle != BassStyle::Sustain)
        {
            const int next = playbackBassNote (p.slots[(size_t) ((i + 1) % p.numSlots)]);
            BassHit before[kMaxBassHitsPerSlot], after[kMaxBassHitsPerSlot];
            const int steps = p.slotEnd (i) - slot.start;
            const int count = getBassHits (p.bassStyle, bass, next, steps, before, kMaxBassHitsPerSlot);
            getBassHits (p.bassStyle, bass + semitones, next + semitones, steps, after, kMaxBassHitsPerSlot);
            for (int j = 0; j < count; ++j)
                if (after[j].pitch != before[j].pitch + semitones)
                    return false;
        }
    }
    return true;
}

inline bool transposeProgression (Progression& p, int semitones)
{
    if (! canTransposeProgression (p, semitones))
        return false;
    for (int i = 0; i < p.numSlots; ++i)
    {
        Slot& s = p.slots[(size_t) i];
        s.chord.root = mod12 (s.chord.root + semitones);
        for (int j = 0; j < s.voicing.n; ++j)
            s.voicing.notes[(size_t) j] += semitones;
        s.voicing.transposeOffset = std::clamp (s.voicing.transposeOffset + semitones, -127, 127);
    }
    setTonalContext (p, p.tonic + semitones, p.scale);
    return true;
}

/** Transposes to a tonic by the shortest distance. */
inline void setTonic (Progression& p, int newTonic)
{
    const int distance = mod12 (mod12 (newTonic) - p.tonic);
    transposeProgression (p, distance > 6 ? distance - 12 : distance);
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
    struct ArpNote { uint8_t pitch = 0; int8_t voice = -1; };
    int start = 0, end = 0;
    int n = 0;
    int notes[kMaxNotes] = {};
    int velocities[kMaxNotes] = {};
    float offsetMs[kMaxNotes] = {};
    int velocity = 96;
    int bassCount = 0;
    int primaryBass = -1;
    int numHits = 0;
    int arpRate = 0, arpDuration = 0, arpDirection = 1;
    int pulseRate = 0;
    float pulseLegacyLength = 1.0f, pulseAccent = 1.0f;
    bool arpBounce = false;
    int numArpNotes = 0;
    ArpNote arpNotes[kPerfMaxNotes * kMaxArpOctaves] {};
    bool sustainBassUnderComping = false;
    CompHit hits[kMaxCompHitsPerSlot] = {};
    int numBassHits = 0;
    BassHit bassHits[kMaxBassHitsPerSlot] = {};

    CompHit hitAt (int index) const noexcept
    {
        if (arpRate > 0)
            return arpHit (index, end - start, arpRate, arpDuration, arpDirection);
        if (pulseRate > 0)
            return pulseHit (index, end - start, pulseRate, pulseLegacyLength, pulseAccent);
        return hits[index];
    }

    ArpNote arpNoteFor (const CompHit& hit) const noexcept
    {
        return hit.arpVoice != 0 && numArpNotes > 0
            ? arpNotes[arpSequenceIndex (hit.arpVoice, numArpNotes, arpBounce)] : ArpNote {};
    }
};

static_assert (kMaxBars * kStepsPerBar / 12 <= kMaxCompHitsPerSlot,
               "kMaxCompHitsPerSlot must hold the densest comp pattern across max bars");
static_assert (std::is_trivially_copyable<CompHit>::value, "CompHit must be trivially copyable");
static_assert (std::is_trivially_copyable<PlaySlot>::value, "PlaySlot must be trivially copyable");

struct PlayData
{
    int lengthSteps = 4 * kStepsPerBar;
    int stepsPerBar = kStepsPerBar;
    BassStyle bassStyle = BassStyle::Sustain;
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

static_assert (std::is_trivially_copyable<PlayData>::value, "PlayData must be trivially copyable");

inline PlayData toPlayData (const Progression& p, const PerformanceParams& perf = {}, double bpm = 120.0, int pass = 0)
{
    PlayData d;
    d.lengthSteps = p.lengthSteps();
    d.stepsPerBar = p.stepsPerBar();
    d.numSlots = p.numSlots;
    d.bassStyle = p.bassStyle;
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

        const int pb = playbackBassNote (s);
        o.primaryBass = pb;
        if (p.bassStyle != BassStyle::Sustain)
        {
            const auto& next = p.slots[(size_t) ((i + 1) % p.numSlots)];
            const int nextBass = playbackBassNote (next);
            o.numBassHits = getBassHits (p.bassStyle, pb, nextBass, o.end - o.start, o.bassHits, kMaxBassHitsPerSlot);
        }

        if (isArpeggio (perf.compStyle))
        {
            o.arpRate = std::max (1, perf.arpRateSteps);
            o.arpDuration = arpDurationSteps (o.arpRate, perf.arpLength);
            o.arpDirection = perf.compStyle == CompStyle::ArpDown ? -1 : 1;
            o.arpBounce = perf.compStyle == CompStyle::ArpUpDown;
            o.numHits = (o.end - o.start + o.arpRate - 1) / o.arpRate;
        }
        else if (isPulse (perf.compStyle))
        {
            o.pulseRate = pulseRateFor (perf);
            o.pulseLegacyLength = perf.compHitLength;
            o.pulseAccent = perf.compPatternAccent;
            o.numHits = (o.end - o.start + o.pulseRate - 1) / o.pulseRate;
        }
        else
        {
            o.numHits = getCompHits (perf.compStyle, o.end - o.start, o.hits, kMaxCompHitsPerSlot,
                                     p.timeSigNum, p.timeSigDen, o.start);
            shapeCompHits (o.hits, o.numHits, perf);
        }
        o.sustainBassUnderComping = isArpeggio (perf.compStyle) || (o.numHits > 1 && ! isShldRhythm (perf.compStyle));

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

        // In combined-channel mode a bass octave that lands on a sustained
        // upper chord pitch would send a same-pitch note-off into that voice.
        // Keep the pump in the root octave for only those colliding hits; split
        // Ch 1/2 routing remains unchanged and collision-free by channel.
        if (p.bassStyle == BassStyle::OctavePump)
            for (int h = 0; h < o.numBassHits; ++h)
                for (int j = bCount; j < out.n; ++j)
                    if (o.bassHits[h].pitch == out.notes[(size_t) j].pitch)
                    {
                        o.bassHits[h].pitch = pb;
                        break;
                    }

        o.n = std::min (out.n, kMaxNotes);
        for (int j = 0; j < o.n; ++j)
        {
            o.notes[j] = out.notes[(size_t) j].pitch;
            o.velocities[j] = out.notes[(size_t) j].velocity;
            o.offsetMs[j] = out.notes[(size_t) j].offsetMs;
        }
        if (o.arpRate > 0)
        {
            const int octaves = std::clamp (perf.arpOctaves, 1, kMaxArpOctaves);
            for (int octave = 0; octave < octaves; ++octave)
                for (int j = bCount; j < o.n; ++j)
                {
                    const int pitch = o.notes[j] + 12 * octave;
                    if (pitch <= 127)
                        o.arpNotes[o.numArpNotes++] = { (uint8_t) pitch, (int8_t) j };
                }
            if (octaves > 1)
            {
                auto* end = o.arpNotes + o.numArpNotes;
                std::sort (o.arpNotes, end, [] (auto a, auto b)
                {
                    return a.pitch < b.pitch || (a.pitch == b.pitch && a.voice < b.voice);
                });
                o.numArpNotes = (int) (std::unique (o.arpNotes, end, [] (auto a, auto b) { return a.pitch == b.pitch; })
                                      - o.arpNotes);
            }
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
