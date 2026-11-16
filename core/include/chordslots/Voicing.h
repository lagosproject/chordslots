// ChordSlots core - voicing rules, style dictionary and voice leading distance.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Theory.h"

#include <algorithm>
#include <array>
#include <cmath>
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

//==============================================================================
struct Voicing
{
    int n = 0;
    std::array<int, kMaxNotes> notes {};
    int velocity = 96;
    int bassCount = 0;

    int computeBassCount (int upperFloor = kUpperFloor) const noexcept
    {
        int c = 0;
        for (int i = 0; i < n; ++i)
            if (notes[(size_t) i] < upperFloor)
                c++;
        return c;
    }
};

inline bool violatesLowIntervalLimit (const Voicing& v)
{
    if (v.n < 2)
        return false;
    std::array<int, kMaxNotes> sorted;
    for (int i = 0; i < v.n; ++i)
        sorted[(size_t) i] = v.notes[(size_t) i];
    std::sort (sorted.begin(), sorted.begin() + v.n);
    return violatesLowIntervalLimit (sorted.data(), v.n);
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

//==============================================================================
// Assignment-based Voice Leading Distance
inline float voiceLeadingDistance (const std::vector<int>& u1, const std::vector<int>& u2)
{
    std::vector<int> a;
    for (int x : u1)
        if (x >= kUpperFloor)
            a.push_back (x);
    std::vector<int> b;
    for (int x : u2)
        if (x >= kUpperFloor)
            b.push_back (x);

    if (a.empty() || b.empty())
        return 0.0f;

    const auto& L = (a.size() >= b.size()) ? a : b;
    const auto& S = (a.size() >= b.size()) ? b : a;
    const int N = (int) L.size();
    const int M = (int) S.size();

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

inline float voiceLeadingDistance (const Voicing& a, const std::vector<int>& b)
{
    std::vector<int> au;
    for (int i = 0; i < a.n; ++i)
        if (a.notes[(size_t) i] >= kUpperFloor)
            au.push_back (a.notes[(size_t) i]);
    return voiceLeadingDistance (au, b);
}

inline float voiceLeadingDistance (const Voicing& a, const Voicing& b)
{
    std::vector<int> au, bu;
    for (int i = 0; i < a.n; ++i)
        if (a.notes[(size_t) i] >= kUpperFloor)
            au.push_back (a.notes[(size_t) i]);
    for (int i = 0; i < b.n; ++i)
        if (b.notes[(size_t) i] >= kUpperFloor)
            bu.push_back (b.notes[(size_t) i]);
    return voiceLeadingDistance (au, bu);
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

    if (pcs.size() > 4 && a.isP5 && a.fifth >= 0)
    {
        auto it = std::find (pcs.begin(), pcs.end(), a.fifth);
        if (it != pcs.end())
            pcs.erase (it);
    }
    if (pcs.size() > 4)
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
    if (! allowRoot)
    {
        auto it = std::find (pcs.begin(), pcs.end(), a.root);
        if (it != pcs.end())
            pcs.erase (it);
    }
    return pcs;
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
                lhShells.push_back ({ n7 });
            for (int n3 : p3s)
                lhShells.push_back ({ n3 });
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
        }
        else
        {
            auto p3s = findInOctave (a.thirdOrSus, 48, 60);
            auto p5s = findInOctave (a.fifth, 48, 60);
            for (int n3 : p3s)
                lhShells.push_back ({ n3 });
            for (int n5 : p5s)
                lhShells.push_back ({ n5 });
            for (int n3 : p3s)
                for (int n5 : p5s)
                {
                    int lo = std::min (n3, n5);
                    int hi = std::max (n3, n5);
                    if (hi - lo >= 3 && ! breaksLowIntervalLimit (lo, hi - lo))
                        lhShells.push_back ({ lo, hi });
                }
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

        auto rhPcs = getPitchClassesAfterOmissions (c, false);
        if (rhPcs.empty())
        {
            for (int pc : a.allPcs)
                if (pc != a.root)
                    rhPcs.push_back (pc);
            if (rhPcs.empty())
                rhPcs.push_back (a.root);
        }
        std::sort (rhPcs.begin(), rhPcs.end());
        const int K = (int) rhPcs.size();

        std::vector<std::vector<int>> rhCands;
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
                        rhCands.push_back (rh);
                }
            }
        }
        if (rhCands.empty())
        {
            rhCands.push_back ({ 60 + rhPcs[0], 64 + rhPcs[0 % K], 67 + rhPcs[0 % K] });
        }

        for (const auto& lh : lhShells)
        {
            for (const auto& rh : rhCands)
            {
                std::vector<int> u = lh;
                u.insert (u.end(), rh.begin(), rh.end());
                std::sort (u.begin(), u.end());
                u.erase (std::unique (u.begin(), u.end()), u.end());

                if (u.front() < 48 || u.back() > 96 || u.size() > 6)
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
                    if (n2 > 74) continue;
                    for (int p3 : pcs)
                    {
                        int n3 = n2 + mod12 (p3 - p2);
                        if (n3 - n2 < 3) n3 += 12;
                        if (n3 > 82) continue;
                        for (int p4 : pcs)
                        {
                            int n4 = n3 + mod12 (p4 - p3);
                            if (n4 - n3 < 3) n4 += 12;
                            if (n4 > 88) continue;

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

                            cands.push_back ({ n1, n2, n3, n4 });
                        }
                    }
                }
            }
        }
    }
    else if (style == VoicingStyle::Drop2 || style == VoicingStyle::Drop3)
    {
        auto pcs = getPitchClassesAfterOmissions (c, true);
        while (pcs.size() < 4)
            pcs.push_back (a.root);
        std::sort (pcs.begin(), pcs.end());

        for (int inv = 0; inv < 4; ++inv)
        {
            for (int base : { 48, 60, 72 })
            {
                std::vector<int> notes;
                int note = base + pcs[(size_t) inv];
                notes.push_back (note);
                for (int j = 1; j < 4; ++j)
                {
                    int step = mod12 (pcs[(size_t) ((inv + j) % 4)] - pcs[(size_t) ((inv + j - 1) % 4)]);
                    if (step == 0) step = 12;
                    note += step;
                    notes.push_back (note);
                }
                if (notes.front() < 48 || notes.back() > 96)
                    continue;

                std::vector<int> dropped = notes;
                if (style == VoicingStyle::Drop2)
                    dropped[2] -= 12; // 2nd voice from top
                else
                    dropped[1] -= 12; // 3rd voice from top

                std::sort (dropped.begin(), dropped.end());
                if (dropped.front() >= 48 && dropped.back() <= 96)
                    cands.push_back (dropped);
            }
        }
    }
    else if (style == VoicingStyle::Rootless)
    {
        auto pcs = getPitchClassesAfterOmissions (c, false);
        if (pcs.empty())
        {
            for (int x : a.allPcs)
                if (x != a.root) pcs.push_back (x);
            if (pcs.empty()) pcs.push_back (mod12 (a.root + 7));
        }

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
                for (size_t step = 1; step < pcs.size() || u.size() < 4; ++step)
                {
                    int targetPc = pcs[(idxA + step) % pcs.size()];
                    int n = curr + mod12 (targetPc - curr);
                    if (n <= curr) n += 12;
                    u.push_back (n);
                    curr = n;
                }
                if (u.back() <= 96)
                    cands.push_back (u);
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
                for (size_t step = 1; step < pcs.size() || u.size() < 4; ++step)
                {
                    int targetPc = pcs[(idxB + step) % pcs.size()];
                    int n = curr + mod12 (targetPc - curr);
                    if (n <= curr) n += 12;
                    u.push_back (n);
                    curr = n;
                }
                if (u.back() <= 96)
                    cands.push_back (u);
            }
        }
    }
    else if (style == VoicingStyle::Stacked)
    {
        auto pcs = getPitchClassesAfterOmissions (c, true);
        std::sort (pcs.begin(), pcs.end());
        const int k = std::min ((int) pcs.size(), 4);

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
                int topDoubled = notes.back() + 12;
                if (notes.front() >= 48 && topDoubled <= 96)
                {
                    notes.push_back (topDoubled);
                    cands.push_back (notes);
                }
            }
        }
    }

    return cands;
}

//==============================================================================
/** Builds a playable voicing using the specified style, omission rules and hard filter. */
inline Voicing makeVoicing (Chord c, const Voicing* ref, float centre, float energy,
                            Rng& rng, float refWeight = 1.0f, float jitter = 0.7f,
                            VoicingStyle style = VoicingStyle::Piano)
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
        const int bass = pickBassRegister (c.bassPc(), refBass, upper.front());
        std::vector<int> all = upper;
        all.push_back (bass);

        int subBass = -1;
        if (energy > 0.85f && bass - 12 >= 24)
            subBass = bass - 12;

        int top = -1;
        if (energy > 0.6f && style != VoicingStyle::Stacked && style != VoicingStyle::Rootless)
        {
            int candTop = 60 + c.root;
            while (candTop <= upper.back()) candTop += 12;
            if (candTop <= 96) top = candTop;
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

        sc.allNotes = all;
        sc.violations = countLowIntervalViolations (all);

        float mean = 0.0f;
        for (int x : upper) mean += (float) x;
        mean /= (float) upper.size();

        float cost = 0.5f * std::abs (mean - centre);
        if (ref != nullptr && ref->n > 0)
            cost += refWeight * voiceLeadingDistance (*ref, upper);
        cost += jitter * rand01 (rng);
        sc.cost = cost;
        return sc;
    };

    std::vector<ScoredCandidate> scored;
    for (const auto& u : cands)
        scored.push_back (buildAndScore (u));

    // Hard filter: reject any candidate that violates low-interval limit (except Close)
    std::vector<ScoredCandidate> valid;
    if (style == VoicingStyle::Close)
    {
        valid = scored;
    }
    else
    {
        for (const auto& sc : scored)
            if (sc.violations == 0)
                valid.push_back (sc);

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
                    if (sc.violations == 0)
                        valid.push_back (sc);
                }
            }
        }

        // If still none survive, fall back to the best candidate with the fewest violations
        if (valid.empty())
        {
            std::sort (scored.begin(), scored.end(), [] (const auto& x, const auto& y)
            {
                if (x.violations != y.violations)
                    return x.violations < y.violations;
                return x.cost < y.cost;
            });
            if (! scored.empty())
                valid.push_back (scored.front());
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

    Voicing v;
    v.n = std::min ((int) best.allNotes.size(), kMaxNotes);
    for (int i = 0; i < v.n; ++i)
        v.notes[(size_t) i] = best.allNotes[(size_t) i];
    v.bassCount = v.computeBassCount (kUpperFloor);
    v.velocity = std::clamp ((int) std::lround (45.0f + 75.0f * energy), 1, 127);
    return v;
}

} // namespace cs
