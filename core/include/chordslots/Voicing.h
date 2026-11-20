// ChordSlots core - voicing rules, style dictionary and voice leading distance.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Theory.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>

namespace cs
{

constexpr int kMaxNotes = 8;
constexpr int kUpperFloor = 48; // notes below this are bass doublings

using Rng = std::mt19937;
inline float rand01 (Rng& rng) { return std::uniform_real_distribution<float> (0.0f, 1.0f) (rng); }

inline float centreForBrightness (float targetB) { return 57.0f + 14.0f * targetB; }

//==============================================================================
// Low-Interval Limits
// An adjacent pair in sorted notes is a violation when:
// - interval in {1, 2} and lowerPitch < 52 (E3)
// - interval = 3 and lowerPitch < 48 (C3)
// - interval = 4 and lowerPitch < 46 (Bb2)
// - interval = 5 and lowerPitch < 41 (F2)

struct LowIntervalLimit
{
    int interval;
    int minLowerPitch;
};

inline constexpr LowIntervalLimit kLowIntervalLimits[] = {
    { 1, 52 }, // m2 below E3
    { 2, 52 }, // M2 below E3
    { 3, 48 }, // m3 below C3
    { 4, 46 }, // M3 below Bb2
    { 5, 41 }  // P4 below F2
};

inline bool breaksLowIntervalLimit (int lowerPitch, int intervalSemitones) noexcept
{
    for (const auto& limit : kLowIntervalLimits)
    {
        if (limit.interval == intervalSemitones)
            return lowerPitch < limit.minLowerPitch;
    }
    return false;
}

inline bool violatesLowIntervalLimit (const int* sortedNotes, int n) noexcept
{
    if (n < 2 || sortedNotes == nullptr)
        return false;
    for (int k = 0; k + 1 < n; ++k)
    {
        const int L = sortedNotes[k];
        const int interval = sortedNotes[k + 1] - sortedNotes[k];
        if (breaksLowIntervalLimit (L, interval))
            return true;
    }
    return false;
}

inline int countLowIntervalViolations (const std::vector<int>& notes)
{
    int v = 0;
    for (size_t k = 0; k + 1 < notes.size(); ++k)
    {
        const int interval = notes[k + 1] - notes[k];
        if (breaksLowIntervalLimit (notes[k], interval))
            ++v;
    }
    return v;
}

//==============================================================================
// Voicing Style Dictionary
enum class VoicingStyle
{
    Close = 0,
    Piano,
    Spread,
    Drop2,
    Drop3,
    Rootless,
    Stacked
};

inline const char* voicingStyleName (VoicingStyle style) noexcept
{
    switch (style)
    {
        case VoicingStyle::Close:    return "Close";
        case VoicingStyle::Piano:    return "Piano";
        case VoicingStyle::Spread:   return "Spread";
        case VoicingStyle::Drop2:    return "Drop 2";
        case VoicingStyle::Drop3:    return "Drop 3";
        case VoicingStyle::Rootless: return "Rootless";
        case VoicingStyle::Stacked:  return "Stacked";
    }
    return "Piano";
}

inline const char* voicingStyleName (int style) noexcept
{
    return voicingStyleName ((VoicingStyle) std::clamp (style, 0, 6));
}

constexpr int kNumVoicingStyles = 7;

/** Per-style linear density scales fit on every mood with seeds 0..14 and
    checked on held-out seeds 15..24. Piano keeps its original (notes-3)/5 map. */
inline float normalizedVoicingDensity (int noteCount, VoicingStyle style) noexcept
{
    int minNotes = 3;
    int maxNotes = 8;
    switch (style)
    {
        case VoicingStyle::Close:    minNotes = 2; maxNotes = 6; break;
        // Preserve Piano's original metric exactly: clamp((notes - 3) / 5).
        case VoicingStyle::Piano:    minNotes = 3; maxNotes = 8; break;
        case VoicingStyle::Spread:   minNotes = 3; maxNotes = 7; break;
        case VoicingStyle::Drop2:    minNotes = 3; maxNotes = 7; break;
        case VoicingStyle::Drop3:    minNotes = 3; maxNotes = 7; break;
        case VoicingStyle::Rootless: minNotes = 2; maxNotes = 5; break;
        case VoicingStyle::Stacked:  minNotes = 3; maxNotes = 7; break;
    }
    return std::clamp ((float) (noteCount - minNotes) / (float) (maxNotes - minNotes), 0.0f, 1.0f);
}

//==============================================================================
struct Voicing
{
    int n = 0;
    std::array<int, kMaxNotes> notes {};
    int velocity = 96;
    int bassCount = 0;
    int transposeOffset = 0; // Moves the bass/upper split with the stored notes.

    int computeBassCount (int upperFloor = kUpperFloor) const noexcept
    {
        int c = 0;
        for (int i = 0; i < n; ++i)
            if (notes[(size_t) i] < upperFloor + transposeOffset)
                c++;
        return c;
    }
};

inline bool violatesLowIntervalLimit (const Voicing& v)
{
    if (v.n < 2)
        return false;
    std::array<int, kMaxNotes> sorted;
    const int n = std::clamp (v.n, 0, kMaxNotes);
    for (int i = 0; i < n; ++i)
        sorted[(size_t) i] = v.notes[(size_t) i];
    for (int i = 1; i < n; ++i)
    {
        const int value = sorted[(size_t) i];
        int j = i;
        while (j > 0 && sorted[(size_t) (j - 1)] > value)
        {
            sorted[(size_t) j] = sorted[(size_t) (j - 1)];
            --j;
        }
        sorted[(size_t) j] = value;
    }
    return violatesLowIntervalLimit (sorted.data(), n);
}

inline bool violatesLowIntervalLimit (const std::vector<int>& notes)
{
    if (notes.size() < 2)
        return false;
    std::vector<int> sorted = notes;
    std::sort (sorted.begin(), sorted.end());
    return violatesLowIntervalLimit (sorted.data(), (int) sorted.size());
}

/** Primary bass note of a voicing (highest note below upperFloor, or -1 if none). */
inline int primaryBassNote (const Voicing& v, int upperFloor = kUpperFloor) noexcept
{
    int best = -1;
    for (int i = 0; i < v.n; ++i)
        if (v.notes[(size_t) i] < upperFloor + v.transposeOffset)
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
        if (v.notes[(size_t) i] >= upperFloor + v.transposeOffset)
            upper.push_back (v.notes[(size_t) i]);

    if (upper.empty())
        return;

    const int offset = v.transposeOffset;
    int bass = pickBassRegister (mod12 (c.bassPc() - offset),
                                 refBassNote < 0 ? -1 : refBassNote - offset,
                                 upper.front() - offset) + offset;
    if (bass < 0)
        bass = mod12 (bass);
    if (bass >= upperFloor + offset || bass >= upper.front() || bass > 127)
        return;
    std::vector<int> all = upper;
    all.push_back (bass);
    if (energy > 0.85f && bass - 12 >= std::max (0, 24 + offset) && (int) all.size() < kMaxNotes)
        all.push_back (bass - 12);

    std::sort (all.begin(), all.end());
    all.erase (std::unique (all.begin(), all.end()), all.end());

    v.n = std::min ((int) all.size(), kMaxNotes);
    for (int i = 0; i < v.n; ++i)
        v.notes[(size_t) i] = std::clamp (all[(size_t) i], 0, 127);
    v.bassCount = v.computeBassCount (upperFloor);
}

//==============================================================================
// Assignment-based Voice Leading Distance
inline float upperVoiceDistance (const std::vector<int>& a, const std::vector<int>& b)
{
    if (a.empty() || b.empty())
        return 0.0f;

    const auto& L = (a.size() >= b.size()) ? a : b;
    const auto& S = (a.size() >= b.size()) ? b : a;
    const int N = std::min ((int) L.size(), 8);
    const int M = std::min ((int) S.size(), N);

    std::array<float, 8> extraCost {};
    for (int i = 0; i < N && i < 8; ++i)
    {
        int nearestDist = 10000;
        for (int j = 0; j < M; ++j)
            nearestDist = std::min (nearestDist, std::abs (L[(size_t) i] - S[(size_t) j]));
        extraCost[(size_t) i] = (float) nearestDist + 2.0f;
    }

    const int numMasks = 1 << N;
    std::array<float, 256> dp;
    dp.fill (1.0e9f);
    dp[0] = 0.0f;

    for (int mask = 0; mask < numMasks; ++mask)
    {
        if (dp[(size_t) mask] >= 1.0e8f)
            continue;
        int sIdx = 0;
        for (int b_bit = 0; b_bit < N; ++b_bit)
            if (mask & (1 << b_bit))
                ++sIdx;
        if (sIdx >= M)
            continue;

        for (int lIdx = 0; lIdx < N; ++lIdx)
        {
            if (! (mask & (1 << lIdx)))
            {
                const int nextMask = mask | (1 << lIdx);
                const float cost = dp[(size_t) mask] + (float) std::abs (L[(size_t) lIdx] - S[(size_t) sIdx]);
                if (cost < dp[(size_t) nextMask])
                    dp[(size_t) nextMask] = cost;
            }
        }
    }

    float bestTotalCost = 1.0e9f;
    for (int mask = 0; mask < numMasks; ++mask)
    {
        int pop = 0;
        for (int b_bit = 0; b_bit < N; ++b_bit)
            if (mask & (1 << b_bit))
                ++pop;
        if (pop != M)
            continue;

        float total = dp[(size_t) mask];
        for (int lIdx = 0; lIdx < N; ++lIdx)
        {
            if (! (mask & (1 << lIdx)))
                total += extraCost[(size_t) lIdx];
        }
        if (total < bestTotalCost)
            bestTotalCost = total;
    }

    return bestTotalCost / (float) N;
}

inline std::vector<int> upperNotes (const Voicing& v)
{
    return { v.notes.begin() + v.computeBassCount(), v.notes.begin() + v.n };
}

inline float voiceLeadingDistance (const std::vector<int>& a, const std::vector<int>& b)
{
    const auto upper = [] (const std::vector<int>& notes)
    {
        std::vector<int> result;
        for (int note : notes)
            if (note >= kUpperFloor)
                result.push_back (note);
        return result;
    };
    return upperVoiceDistance (upper (a), upper (b));
}

inline float voiceLeadingDistance (const Voicing& a, const std::vector<int>& b)
{
    return upperVoiceDistance (upperNotes (a), b);
}

inline float voiceLeadingDistance (const Voicing& a, const Voicing& b)
{
    return upperVoiceDistance (upperNotes (a), upperNotes (b));
}

//==============================================================================
// Chord Tone Analysis & Omission Rules
struct ChordToneAnalysis
{
    int root = 0;
    int thirdOrSus = -1;
    int seventhOrSixth = -1;
    int fifth = -1;
    bool isP5 = false;
    std::vector<int> allPcs;
    std::vector<int> essentialPcs; // All tones that must remain in full voicing
};

inline ChordToneAnalysis analyzeChordTones (Chord c)
{
    ChordToneAnalysis a;
    a.root = mod12 (c.root);
    const auto& t = chordTypes()[(size_t) c.type];

    for (int iv : t.iv)
    {
        int pc = mod12 (c.root + iv);
        if (std::find (a.allPcs.begin(), a.allPcs.end(), pc) == a.allPcs.end())
            a.allPcs.push_back (pc);
    }

    if (hasMajorThird (c.type))
        a.thirdOrSus = mod12 (c.root + 4);
    else if (hasMinorThird (c.type))
        a.thirdOrSus = mod12 (c.root + 3);
    else if (isSus (c.type))
    {
        for (int iv : t.iv)
            if (iv == 2 || iv == 5)
            {
                a.thirdOrSus = mod12 (c.root + iv);
                break;
            }
    }

    if (seventhKind (c.type) != SeventhKind::None)
    {
        for (int iv : t.iv)
        {
            int m = mod12 (iv);
            if (m == 10 || m == 11 || (m == 9 && seventhKind (c.type) == SeventhKind::Diminished7))
            {
                a.seventhOrSixth = mod12 (c.root + iv);
                break;
            }
        }
    }
    else
    {
        for (int iv : t.iv)
        {
            if (mod12 (iv) == 9)
            {
                a.seventhOrSixth = mod12 (c.root + iv);
                break;
            }
        }
    }

    if (hasPerfectFifth (c.type))
    {
        a.fifth = mod12 (c.root + 7);
        a.isP5 = true;
    }
    else if (hasDimFifth (c.type))
    {
        a.fifth = mod12 (c.root + 6);
        a.isP5 = false;
    }
    else if (hasAugFifth (c.type))
    {
        a.fifth = mod12 (c.root + 8);
        a.isP5 = false;
    }

    for (int pc : a.allPcs)
    {
        if (pc == a.root)
            continue; // root is provided by bass
        if (a.isP5 && pc == a.fifth)
            continue; // perfect 5th may be omitted
        a.essentialPcs.push_back (pc);
    }

    return a;
}

inline std::vector<int> getPitchClassesAfterOmissions (Chord c, bool allowRoot = true)
{
    const auto a = analyzeChordTones (c);
    std::vector<int> pcs = a.allPcs;

    if (! allowRoot)
    {
        auto it = std::find (pcs.begin(), pcs.end(), a.root);
        if (it != pcs.end())
            pcs.erase (it);

        // For altered dominant (7#9, 7b9), drop the perfect 5th if present
        const auto& t = chordTypes()[(size_t) c.type];
        if ((std::strcmp (t.suffix, "7#9") == 0 || std::strcmp (t.suffix, "7b9") == 0) && a.isP5 && a.fifth >= 0)
        {
            auto fit = std::find (pcs.begin(), pcs.end(), a.fifth);
            if (fit != pcs.end())
                pcs.erase (fit);
        }
    }

    if (pcs.size() > 4 && a.isP5 && a.fifth >= 0)
    {
        auto it = std::find (pcs.begin(), pcs.end(), a.fifth);
        if (it != pcs.end())
            pcs.erase (it);
    }
    if (pcs.size() > 4 && allowRoot)
    {
        auto it = std::find (pcs.begin(), pcs.end(), a.root);
        if (it != pcs.end())
            pcs.erase (it);
    }
    if (pcs.size() > 4 && hasMajorThird (c.type))
    {
        int nat11 = mod12 (c.root + 5);
        auto it = std::find (pcs.begin(), pcs.end(), nat11);
        if (it != pcs.end())
            pcs.erase (it);
    }
    return pcs;
}

//==============================================================================
// Gap and Cluster Rules
inline bool hasAdjacentGapViolation (const std::vector<int>& upper, VoicingStyle style) noexcept
{
    if (upper.size() < 2)
        return false;
    for (size_t i = 0; i + 1 < upper.size(); ++i)
    {
        const int gap = upper[i + 1] - upper[i];
        if (gap > 12)
            return true;
        if (style == VoicingStyle::Spread && gap < 3)
            return true;
    }
    return false;
}

inline bool hasAdjacentSemitoneCluster (const std::vector<int>& upper) noexcept
{
    if (upper.size() < 2)
        return false;
    for (size_t i = 0; i + 1 < upper.size(); ++i)
    {
        if (upper[i + 1] - upper[i] == 1)
            return true;
    }
    return false;
}

//==============================================================================
// Candidate Voice Displacement Helper
inline std::vector<std::vector<int>> expandCandidateDisplacements (
    const std::vector<std::vector<int>>& baseCands,
    VoicingStyle style,
    int minPitch = kUpperFloor,
    int maxPitch = 96,
    size_t maxCands = 64)
{
    std::vector<std::vector<int>> result;
    result.reserve (maxCands);

    auto addCandidate = [&] (const std::vector<int>& notes) -> bool
    {
        if (notes.empty()) return false;
        if (notes.front() < minPitch || notes.back() > maxPitch) return false;
        for (size_t k = 0; k + 1 < notes.size(); ++k)
            if (notes[k] == notes[k + 1]) return false;
        if (hasAdjacentGapViolation (notes, style)) return false;
        if (style != VoicingStyle::Close && violatesLowIntervalLimit (notes)) return false;

        for (const auto& existing : result)
            if (existing == notes) return false;

        result.push_back (notes);
        return result.size() >= maxCands;
    };

    // 1. Base candidates without semitones first
    for (const auto& base : baseCands)
    {
        if (! hasAdjacentSemitoneCluster (base))
            if (addCandidate (base)) return result;
    }

    // 2. Single-voice displacements without semitones
    for (const auto& base : baseCands)
    {
        const int m = (int) base.size();
        for (int i = 0; i < m; ++i)
        {
            for (int delta : { -12, 12 })
            {
                std::vector<int> cand = base;
                cand[(size_t) i] += delta;
                std::sort (cand.begin(), cand.end());
                if (! hasAdjacentSemitoneCluster (cand))
                    if (addCandidate (cand)) return result;
            }
        }
    }

    // 3. Pair-of-voices displacements without semitones
    for (const auto& base : baseCands)
    {
        const int m = (int) base.size();
        for (int i = 0; i < m; ++i)
        {
            for (int j = i + 1; j < m; ++j)
            {
                for (int d1 : { -12, 12 })
                {
                    for (int d2 : { -12, 12 })
                    {
                        std::vector<int> cand = base;
                        cand[(size_t) i] += d1;
                        cand[(size_t) j] += d2;
                        std::sort (cand.begin(), cand.end());
                        if (! hasAdjacentSemitoneCluster (cand))
                            if (addCandidate (cand)) return result;
                    }
                }
            }
        }
    }

    // 4. Remaining base candidates
    for (const auto& base : baseCands)
    {
        if (addCandidate (base)) return result;
    }

    // 5. Remaining single-voice displacements
    for (const auto& base : baseCands)
    {
        const int m = (int) base.size();
        for (int i = 0; i < m; ++i)
        {
            for (int delta : { -12, 12 })
            {
                std::vector<int> cand = base;
                cand[(size_t) i] += delta;
                std::sort (cand.begin(), cand.end());
                if (addCandidate (cand)) return result;
            }
        }
    }

    return result;
}

//==============================================================================
// Style Upper Structure Generators
inline std::vector<std::vector<int>> generateUpperCandidates (Chord c, VoicingStyle style)
{
    std::vector<std::vector<int>> cands;
    const auto a = analyzeChordTones (c);

    if (style == VoicingStyle::Close)
    {
        std::vector<int> pcs = a.allPcs;
        std::sort (pcs.begin(), pcs.end());
        const int k = (int) pcs.size();
        for (int inv = 0; inv < k; ++inv)
        {
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
                if (! hasAdjacentGapViolation (notes, style))
                    cands.push_back (notes);
            }
        }
    }
    else if (style == VoicingStyle::Piano)
    {
        auto findInOctave = [] (int pc, int minPitch, int maxPitch) -> std::vector<int>
        {
            std::vector<int> res;
            if (pc < 0) return res;
            for (int p = minPitch; p <= maxPitch; ++p)
                if (mod12 (p) == mod12 (pc))
                    res.push_back (p);
            return res;
        };

        std::vector<std::vector<int>> lhShells;
        if (a.seventhOrSixth >= 0)
        {
            auto p7s = findInOctave (a.seventhOrSixth, 48, 60);
            auto p3s = findInOctave (a.thirdOrSus, 48, 60);
            for (int n7 : p7s)
            {
                for (int n3 : p3s)
                {
                    int lo = std::min (n3, n7);
                    int hi = std::max (n3, n7);
                    if (hi - lo >= 3 && ! breaksLowIntervalLimit (lo, hi - lo))
                        lhShells.push_back ({ lo, hi });
                }
                for (int n3 : p3s)
                {
                    int n10 = n3 + 12;
                    if (n10 <= 60 && n10 > n7)
                        if (n10 - n7 >= 3 && ! breaksLowIntervalLimit (n7, n10 - n7))
                            lhShells.push_back ({ n7, n10 });
                }
            }
            for (int n7 : p7s)
                lhShells.push_back ({ n7 });
            for (int n3 : p3s)
                lhShells.push_back ({ n3 });
        }
        else
        {
            auto p3s = findInOctave (a.thirdOrSus, 48, 60);
            auto p5s = findInOctave (a.fifth, 48, 60);
            for (int n3 : p3s)
                for (int n5 : p5s)
                {
                    int lo = std::min (n3, n5);
                    int hi = std::max (n3, n5);
                    if (hi - lo >= 3 && ! breaksLowIntervalLimit (lo, hi - lo))
                        lhShells.push_back ({ lo, hi });
                }
            for (int n3 : p3s)
                lhShells.push_back ({ n3 });
            for (int n5 : p5s)
                lhShells.push_back ({ n5 });
            if (lhShells.empty())
            {
                for (int pc : a.allPcs)
                    if (pc != a.root)
                        for (int p : findInOctave (pc, 48, 60))
                            lhShells.push_back ({ p });
            }
        }
        if (lhShells.empty())
            lhShells.push_back ({ 48 + mod12 (a.fifth >= 0 ? a.fifth : a.thirdOrSus) });

        const bool isMaj7Sharp11 = (std::strcmp (chordTypes()[(size_t) c.type].suffix, "maj7#11") == 0);

        for (const auto& lh : lhShells)
        {
            bool lhHas3 = false;
            bool lhHas7 = false;
            for (int n : lh)
            {
                if (a.thirdOrSus >= 0 && mod12 (n) == a.thirdOrSus)
                    lhHas3 = true;
                if (a.seventhOrSixth >= 0 && mod12 (n) == a.seventhOrSixth)
                    lhHas7 = true;
            }

            // Piano must not double 3rd or 7th across hands.
            // RH also never contains the root.
            auto buildRhPcs = [&] (bool dropP5) -> std::vector<int>
            {
                std::vector<int> rhPcs;
                for (int pc : a.allPcs)
                {
                    if (pc == a.root)
                        continue;
                    if (lhHas3 && pc == a.thirdOrSus)
                        continue;
                    if (lhHas7 && pc == a.seventhOrSixth)
                        continue;
                    if (dropP5 && a.isP5 && pc == a.fifth)
                        continue;
                    rhPcs.push_back (pc);
                }
                return rhPcs;
            };

            for (bool dropP5 : (isMaj7Sharp11 ? std::vector<bool>{ false, true } : std::vector<bool>{ false }))
            {
                std::vector<int> rhPcs = buildRhPcs (dropP5);
                if (rhPcs.empty())
                {
                    if (a.fifth >= 0 && mod12 (a.fifth) != a.root && ! dropP5)
                        rhPcs.push_back (a.fifth);
                    else
                        continue;
                }

                std::sort (rhPcs.begin(), rhPcs.end());
                const int K = (int) rhPcs.size();

                std::vector<std::vector<int>> baseRhCands;
                for (int inv = 0; inv < K; ++inv)
                {
                    for (int base : { 60, 72 })
                    {
                        for (int targetSize : { 3, 4 })
                        {
                            std::vector<int> rh;
                            int note = base + rhPcs[(size_t) inv];
                            rh.push_back (note);
                            for (int j = 1; j < targetSize; ++j)
                            {
                                int step = mod12 (rhPcs[(size_t) ((inv + j) % K)] - rhPcs[(size_t) ((inv + j - 1) % K)]);
                                if (step == 0) step = 12;
                                note += step;
                                rh.push_back (note);
                            }
                            if (rh.front() >= 60 && rh.back() <= 96)
                            {
                                bool ok = true;
                                for (size_t g = 0; g + 1 < rh.size(); ++g)
                                    if (rh[g + 1] - rh[g] > 12) { ok = false; break; }
                                if (ok)
                                    baseRhCands.push_back (rh);
                            }
                        }
                    }
                }

                // Displace single voices of RH by +-12
                std::vector<std::vector<int>> rhCands = baseRhCands;
                for (const auto& baseRh : baseRhCands)
                {
                    for (size_t i = 0; i < baseRh.size(); ++i)
                    {
                        for (int delta : { -12, 12 })
                        {
                            std::vector<int> disp = baseRh;
                            disp[i] += delta;
                            std::sort (disp.begin(), disp.end());
                            if (disp.front() >= 60 && disp.back() <= 96)
                            {
                                bool ok = true;
                                for (size_t g = 0; g + 1 < disp.size(); ++g)
                                    if (disp[g] == disp[g + 1] || disp[g + 1] - disp[g] > 12) { ok = false; break; }
                                if (ok && std::find (rhCands.begin(), rhCands.end(), disp) == rhCands.end())
                                    rhCands.push_back (disp);
                            }
                        }
                    }
                }

                for (const auto& rh : rhCands)
                {
                    std::vector<int> u = lh;
                    u.insert (u.end(), rh.begin(), rh.end());
                    std::sort (u.begin(), u.end());
                    u.erase (std::unique (u.begin(), u.end()), u.end());

                    if (u.front() < 48 || u.back() > 96 || u.size() < 4 || u.size() > 6)
                        continue;

                    // Gap rule: no two adjacent upper notes > 12 apart
                    if (hasAdjacentGapViolation (u, style))
                        continue;

                    // Ensure all essential tones are present in u
                    bool hasAllEssentials = true;
                    for (int epc : a.essentialPcs)
                    {
                        bool found = false;
                        for (int note : u)
                            if (mod12 (note) == epc) { found = true; break; }
                        if (! found) { hasAllEssentials = false; break; }
                    }
                    if (hasAllEssentials)
                        cands.push_back (u);
                }
            }
        }

        std::vector<std::vector<int>> deduped;
        for (const auto& u : cands)
        {
            if (! hasAdjacentSemitoneCluster (u))
            {
                if (std::find (deduped.begin(), deduped.end(), u) == deduped.end())
                {
                    deduped.push_back (u);
                    if (deduped.size() >= 64) break;
                }
            }
        }
        if (deduped.empty())
        {
            for (const auto& u : cands)
            {
                if (std::find (deduped.begin(), deduped.end(), u) == deduped.end())
                {
                    deduped.push_back (u);
                    if (deduped.size() >= 64) break;
                }
            }
        }
        return deduped;
    }
    else if (style == VoicingStyle::Spread)
    {
        auto pcs = getPitchClassesAfterOmissions (c, true);
        std::sort (pcs.begin(), pcs.end());

        for (int p1 : pcs)
        {
            for (int oct1 : { 4, 5 })
            {
                int n1 = oct1 * 12 + p1;
                if (n1 < 52 || n1 > 64) continue;
                for (int p2 : pcs)
                {
                    int n2 = n1 + mod12 (p2 - p1);
                    if (n2 - n1 < 3) n2 += 12;
                    if (n2 - n1 > 12 || n2 > 74) continue;
                    for (int p3 : pcs)
                    {
                        int n3 = n2 + mod12 (p3 - p2);
                        if (n3 - n2 < 3) n3 += 12;
                        if (n3 - n2 > 12 || n3 > 82) continue;
                        for (int p4 : pcs)
                        {
                            int n4 = n3 + mod12 (p4 - p3);
                            if (n4 - n3 < 3) n4 += 12;
                            if (n4 - n3 > 12 || n4 > 88) continue;

                            if (a.thirdOrSus >= 0 && a.seventhOrSixth >= 0)
                            {
                                bool midHas3 = (p2 == a.thirdOrSus || p3 == a.thirdOrSus);
                                bool midHas7 = (p2 == a.seventhOrSixth || p3 == a.seventhOrSixth);
                                if (! midHas3 && ! midHas7) continue;
                            }
                            bool hasEssentials = true;
                            for (int epc : a.essentialPcs)
                                if (p1 != epc && p2 != epc && p3 != epc && p4 != epc)
                                {
                                    hasEssentials = false;
                                    break;
                                }
                            if (! hasEssentials) continue;

                            std::vector<int> u = { n1, n2, n3, n4 };
                            if (! hasAdjacentGapViolation (u, style))
                                cands.push_back (u);
                        }
                    }
                }
            }
        }
    }
    else if (style == VoicingStyle::Drop2 || style == VoicingStyle::Drop3)
    {
        auto pcs = getPitchClassesAfterOmissions (c, true);
        std::vector<std::vector<int>> closePositions;

        if (pcs.size() >= 4)
        {
            std::sort (pcs.begin(), pcs.end());
            pcs.resize (4);
            const int K = 4;
            for (int inv = 0; inv < K; ++inv)
            {
                for (int base : { 36, 48, 60, 72 })
                {
                    std::vector<int> close;
                    int note = base + pcs[(size_t) inv];
                    close.push_back (note);
                    for (int j = 1; j < K; ++j)
                    {
                        int step = mod12 (pcs[(size_t) ((inv + j) % K)] - pcs[(size_t) ((inv + j - 1) % K)]);
                        if (step == 0) step = 12;
                        note += step;
                        close.push_back (note);
                    }
                    if (close.back() - close.front() <= 12)
                        closePositions.push_back (close);
                }
            }
        }
        else
        {
            // Triad: build 4-note close position doubling root at the top
            int rPc = a.root;
            int tPc = a.thirdOrSus >= 0 ? a.thirdOrSus : mod12 (rPc + 4);
            int fPc = a.fifth >= 0 ? a.fifth : mod12 (rPc + 7);

            for (int base : { 36, 48, 60, 72 })
            {
                int n0 = base + rPc;
                int n1 = n0 + mod12 (tPc - rPc);
                int n2 = n1 + mod12 (fPc - tPc);
                int n3 = n0 + 12;
                if (n3 - n0 <= 12)
                    closePositions.push_back ({ n0, n1, n2, n3 });
            }
        }

        for (const auto& close : closePositions)
        {
            std::vector<int> dropped = close;
            if (style == VoicingStyle::Drop2)
                dropped[2] -= 12; // 2nd voice from top
            else
                dropped[1] -= 12; // 3rd voice from top

            std::sort (dropped.begin(), dropped.end());
            if (dropped.front() >= 48 && dropped.back() <= 96)
            {
                if (! hasAdjacentGapViolation (dropped, style))
                    cands.push_back (dropped);
            }
        }
    }
    else if (style == VoicingStyle::Rootless)
    {
        auto pcs = getPitchClassesAfterOmissions (c, false);
        const int ninth = mod12 (a.root + 2);
        if (a.allPcs.size() == 3
            && std::find (a.allPcs.begin(), a.allPcs.end(), ninth) == a.allPcs.end()
            && std::find (pcs.begin(), pcs.end(), ninth) == pcs.end())
            pcs.push_back (ninth);
        std::sort (pcs.begin(), pcs.end());
        if (pcs.empty())
        {
            for (int x : a.allPcs)
                if (x != a.root) pcs.push_back (x);
            if (pcs.empty()) pcs.push_back (mod12 (a.root + 7));
        }

        std::vector<std::vector<int>> baseCands;

        // Form A: lowest note is 3rd/sus in 48-62
        int startA = a.thirdOrSus >= 0 ? a.thirdOrSus : pcs[0];
        auto itA = std::find (pcs.begin(), pcs.end(), startA);
        size_t idxA = (itA != pcs.end()) ? (size_t) (itA - pcs.begin()) : 0;

        for (int oct = 3; oct <= 5; ++oct)
        {
            int n0 = oct * 12 + startA;
            if (n0 >= 48 && n0 <= 62)
            {
                std::vector<int> u = { n0 };
                int curr = n0;
                bool gapOk = true;
                for (size_t step = 1; step < pcs.size(); ++step)
                {
                    int targetPc = pcs[(idxA + step) % pcs.size()];
                    int n = curr + mod12 (targetPc - curr);
                    if (n <= curr) n += 12;
                    if (n - curr > 12) { gapOk = false; break; }
                    u.push_back (n);
                    curr = n;
                }
                if (gapOk && u.back() <= 96 && ! hasAdjacentGapViolation (u, style))
                    baseCands.push_back (u);
            }
        }

        // Form B: lowest note is 7th/6th (or 5th) in 48-62
        int startB = a.seventhOrSixth >= 0 ? a.seventhOrSixth : (a.fifth >= 0 ? a.fifth : pcs[0]);
        auto itB = std::find (pcs.begin(), pcs.end(), startB);
        size_t idxB = (itB != pcs.end()) ? (size_t) (itB - pcs.begin()) : 0;

        for (int oct = 3; oct <= 5; ++oct)
        {
            int n0 = oct * 12 + startB;
            if (n0 >= 48 && n0 <= 62)
            {
                std::vector<int> u = { n0 };
                int curr = n0;
                bool gapOk = true;
                for (size_t step = 1; step < pcs.size(); ++step)
                {
                    int targetPc = pcs[(idxB + step) % pcs.size()];
                    int n = curr + mod12 (targetPc - curr);
                    if (n <= curr) n += 12;
                    if (n - curr > 12) { gapOk = false; break; }
                    u.push_back (n);
                    curr = n;
                }
                if (gapOk && u.back() <= 96 && ! hasAdjacentGapViolation (u, style))
                    baseCands.push_back (u);
            }
        }

        return expandCandidateDisplacements (baseCands, style, 48, 96, 64);
    }
    else if (style == VoicingStyle::Stacked)
    {
        auto pcs = getPitchClassesAfterOmissions (c, true);
        std::sort (pcs.begin(), pcs.end());
        const int k = std::min ((int) pcs.size(), 4);

        std::vector<std::vector<int>> baseCands;

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
                if (notes.front() >= 48 && notes.back() <= 96)
                {
                    if (! hasAdjacentGapViolation (notes, style))
                        baseCands.push_back (notes);
                }
                int topDoubled = notes.back() + 12;
                if (notes.front() >= 48 && topDoubled <= 96)
                {
                    std::vector<int> withTop = notes;
                    withTop.push_back (topDoubled);
                    if (! hasAdjacentGapViolation (withTop, style))
                        baseCands.push_back (withTop);
                }
            }
        }

        return expandCandidateDisplacements (baseCands, style, 48, 96, 64);
    }

    return cands;
}

//==============================================================================
/** Assembles full voicing with bass, doublings, and velocity according to style and energy. */
inline Voicing assembleVoicing (Chord c, const std::vector<int>& upper, float energy,
                                VoicingStyle style, int refBassNote = -1)
{
    if (upper.empty())
    {
        Voicing fallback;
        fallback.n = 1;
        fallback.notes[0] = pickBassRegister (c.bassPc(), refBassNote);
        fallback.bassCount = 1;
        fallback.velocity = std::clamp ((int) std::lround (45.0f + 75.0f * energy), 1, 127);
        return fallback;
    }

    const int bass = pickBassRegister (c.bassPc(), refBassNote, upper.front());
    std::vector<int> all = upper;
    all.push_back (bass);

    int subBass = -1;
    if (energy > 0.85f && bass - 12 >= 24)
        subBass = bass - 12;

    int top = -1;
    if (style == VoicingStyle::Stacked && energy > 0.5f)
    {
        const int doubledTop = upper.back() + 12;
        if (doubledTop <= 96)
            top = doubledTop;
    }
    else if (energy > 0.6f && style != VoicingStyle::Stacked && style != VoicingStyle::Rootless)
    {
        int candTop = 60 + c.root;
        while (candTop <= upper.back()) candTop += 12;
        if (candTop <= 96)
        {
            const int gap = candTop - upper.back();
            bool ok = (gap <= 12);
            if (style == VoicingStyle::Spread && gap < 3)
                ok = false;
            if (style != VoicingStyle::Close && gap == 1)
                ok = false;
            if (ok)
                top = candTop;
        }
    }

    if (top >= 0) all.push_back (top);
    if (subBass >= 0) all.push_back (subBass);

    std::sort (all.begin(), all.end());
    all.erase (std::unique (all.begin(), all.end()), all.end());

    if ((int) all.size() > kMaxNotes && subBass >= 0)
    {
        auto it = std::find (all.begin(), all.end(), subBass);
        if (it != all.end()) all.erase (it);
    }
    if ((int) all.size() > kMaxNotes && top >= 0)
    {
        auto it = std::find (all.begin(), all.end(), top);
        if (it != all.end()) all.erase (it);
    }

    Voicing v;
    v.n = std::min ((int) all.size(), kMaxNotes);
    for (int i = 0; i < v.n; ++i)
        v.notes[(size_t) i] = std::clamp (all[(size_t) i], 0, 127);
    v.bassCount = v.computeBassCount (kUpperFloor);
    v.velocity = std::clamp ((int) std::lround (45.0f + 75.0f * energy), 1, 127);
    return v;
}

/** Builds a playable voicing using the specified style, omission rules and hard filter. */
inline Voicing makeVoicing (Chord c, const Voicing* ref, float centre, float energy,
                            Rng& rng, float refWeight = 1.0f, float jitter = 0.7f,
                            VoicingStyle style = VoicingStyle::Piano, const Voicing* next = nullptr)
{
    const auto a = analyzeChordTones (c);
    auto cands = generateUpperCandidates (c, style);

    struct ScoredCandidate
    {
        std::vector<int> allNotes;
        std::vector<int> upperNotes;
        int violations = 0;
        float cost = 1.0e9f;
    };

    auto buildAndScore = [&] (const std::vector<int>& upper) -> ScoredCandidate
    {
        ScoredCandidate sc;
        sc.upperNotes = upper;
        const int refBass = ref != nullptr ? primaryBassNote (*ref) : -1;
        Voicing v = assembleVoicing (c, upper, energy, style, refBass);

        sc.allNotes.assign (v.notes.begin(), v.notes.begin() + v.n);
        sc.violations = countLowIntervalViolations (sc.allNotes);

        float mean = 0.0f;
        for (int x : upper) mean += (float) x;
        mean /= (float) upper.size();

        float cost = 0.5f * std::abs (mean - centre);
        if (next != nullptr)
        {
            if (ref != nullptr)
                cost += refWeight * voiceLeadingDistance (*ref, v);
            cost += refWeight * voiceLeadingDistance (v, *next);
        }
        else if (ref != nullptr && ref->n > 0)
            cost += refWeight * voiceLeadingDistance (*ref, upper);
        cost += jitter * rand01 (rng);
        sc.cost = cost;
        return sc;
    };

    std::vector<ScoredCandidate> scored;
    for (const auto& u : cands)
        scored.push_back (buildAndScore (u));

    // Hard filter:
    // 1. Reject any candidate that violates low-interval limit (except Close)
    // 2. Reject any candidate with adjacent upper gap > 12 (or in Spread, gap < 3)
    std::vector<ScoredCandidate> valid;
    for (const auto& sc : scored)
    {
        if (style != VoicingStyle::Close && sc.violations > 0)
            continue;
        if (hasAdjacentGapViolation (sc.upperNotes, style))
            continue;
        valid.push_back (sc);
    }

    // If no candidate survives, transpose the upper structure up an octave and retry
    if (valid.empty())
    {
        for (const auto& u : cands)
        {
            if (u.back() + 12 <= 96)
            {
                std::vector<int> transposed = u;
                for (int& x : transposed) x += 12;
                auto sc = buildAndScore (transposed);
                if ((style == VoicingStyle::Close || sc.violations == 0) && ! hasAdjacentGapViolation (sc.upperNotes, style))
                    valid.push_back (sc);
            }
        }
    }

    // Fallback: fewest violations without gap violations if possible
    if (valid.empty())
    {
        for (const auto& sc : scored)
            if (! hasAdjacentGapViolation (sc.upperNotes, style))
                valid.push_back (sc);
        if (valid.empty())
            valid = scored;
    }

    // Semitone cluster rule (every style except Close):
    // Reject candidates with an adjacent 1-semitone upper pair whenever another candidate without one exists
    if (style != VoicingStyle::Close && ! valid.empty())
    {
        bool anyWithoutSemitone = false;
        for (const auto& sc : valid)
        {
            if (! hasAdjacentSemitoneCluster (sc.upperNotes))
            {
                anyWithoutSemitone = true;
                break;
            }
        }
        if (anyWithoutSemitone)
        {
            std::vector<ScoredCandidate> filtered;
            for (const auto& sc : valid)
            {
                if (! hasAdjacentSemitoneCluster (sc.upperNotes))
                    filtered.push_back (sc);
            }
            valid = std::move (filtered);
        }
    }

    if (valid.empty())
    {
        std::vector<int> fb;
        auto pcs = getPitchClassesAfterOmissions (c, style != VoicingStyle::Rootless);
        for (int p : pcs)
            fb.push_back (60 + mod12 (p - 60 % 12));
        std::sort (fb.begin(), fb.end());
        valid.push_back (buildAndScore (fb));
    }

    std::sort (valid.begin(), valid.end(), [] (const auto& x, const auto& y) { return x.cost < y.cost; });
    const auto& best = valid.front();
    const int refBass = ref != nullptr ? primaryBassNote (*ref) : -1;
    return assembleVoicing (c, best.upperNotes, energy, style, refBass);
}

} // namespace cs
