// ChordSlots core - voicing rules, low-interval limits and voice leading distance.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Theory.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace cs
{

constexpr int kMaxNotes = 8;
constexpr int kUpperFloor = 48; // notes below this are bass doublings

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

} // namespace cs
