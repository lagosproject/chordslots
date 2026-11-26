// ChordSlots - fixed-size song arrangement primitives.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Generator.h"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace cs
{

constexpr int kMaxVariations = 8;
constexpr int kMaxSongEntries = 16;
constexpr int kMinSongTranspose = -48;
constexpr int kMaxSongTranspose = 48;

struct SongEntry
{
    int variation = 0; // Zero-based variation index
    int transpose = 0; // semitones
};

struct Song
{
    int numEntries = 0;
    SongEntry entries[kMaxSongEntries] = {};
};

static_assert (std::is_trivially_copyable<Song>::value, "Song must be safe to copy as a bounded value");

inline SongEntry sanitizeSongEntry (SongEntry entry, int variationCount = 4) noexcept
{
    entry.variation = std::clamp (entry.variation, 0, std::clamp (variationCount, 1, kMaxVariations) - 1);
    entry.transpose = std::clamp (entry.transpose, kMinSongTranspose, kMaxSongTranspose);
    return entry;
}

inline Song sanitizeSong (const Song& source, int variationCount = 4) noexcept
{
    Song result;
    result.numEntries = std::clamp (source.numEntries, 0, kMaxSongEntries);
    for (int i = 0; i < result.numEntries; ++i)
        result.entries[i] = sanitizeSongEntry (source.entries[i], variationCount);
    return result;
}

inline int songLengthSteps (const Song& source, const PlayData* variations, int variationCount = 4) noexcept
{
    if (variations == nullptr || variationCount <= 0)
        return 0;
    const Song song = sanitizeSong (source, variationCount);
    int length = 0;
    for (int i = 0; i < song.numEntries; ++i)
    {
        const int v = std::clamp (song.entries[i].variation, 0, variationCount - 1);
        const int section = std::max (0, variations[v].lengthSteps);
        if (section > 0 && length > (std::numeric_limits<int>::max)() - section)
            return (std::numeric_limits<int>::max)();
        length += section;
    }
    return length;
}

struct SongPosition
{
    int entry = -1;
    int variation = -1;
    int localStep = 0;
    int sectionStart = 0;
    int transpose = 0;
};

/** Resolve an absolute song step. At/after the end returns entry=-1. */
inline SongPosition songPositionAt (const Song& source, const PlayData* variations,
                                    int variationCount, int absoluteStep) noexcept
{
    SongPosition pos;
    if (variations == nullptr || variationCount <= 0 || absoluteStep < 0)
        return pos;
    const Song song = sanitizeSong (source, variationCount);
    long long base = 0;
    for (int i = 0; i < song.numEntries; ++i)
    {
        const int v = std::clamp (song.entries[i].variation, 0, variationCount - 1);
        const int len = std::max (0, variations[v].lengthSteps);
        if ((long long) absoluteStep < base + len)
        {
            pos.entry = i;
            pos.variation = v;
            pos.localStep = (int) ((long long) absoluteStep - base);
            pos.sectionStart = (int) base;
            pos.transpose = song.entries[i].transpose;
            return pos;
        }
        if (len > 0 && base + len > (std::numeric_limits<int>::max)())
            return pos;
        base += len;
    }
    return pos;
}

} // namespace cs
