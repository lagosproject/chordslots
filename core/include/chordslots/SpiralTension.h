// ChordSlots core - Spiral Array geometry for contextual tension.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Theory.h"

#include <cmath>

namespace cs
{
// Herremans and Chew, Tension Ribbons (TENOR 2016). Coordinates, tonal
// weights and distance scaling follow Partitura 1.9's tonal_tension module.
struct SpiralPoint
{
    double x = 0, y = 0, z = 0;

    SpiralPoint operator+ (SpiralPoint b) const noexcept { return { x + b.x, y + b.y, z + b.z }; }
    SpiralPoint operator* (double weight) const noexcept { return { x * weight, y * weight, z * weight }; }
};

constexpr double kSpiralPitchStep = 0.9009674940214843; // sqrt(2/15) * pi^2 / 4
constexpr double kSpiralDistanceScale = 1.0 / (12.0 * kSpiralPitchStep); // C to B-sharp

inline SpiralPoint spiralPitch (int fifths) noexcept
{
    constexpr double x[4] { 0, 1, 0, -1 }, y[4] { 1, 0, -1, 0 };
    const int turn = (fifths % 4 + 4) % 4;
    return { x[turn], y[turn], kSpiralPitchStep * fifths };
}

inline double spiralDistanceSquared (SpiralPoint a, SpiralPoint b) noexcept
{
    const double x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    return x * x + y * y + z * z;
}

inline float spiralDistance (SpiralPoint a, SpiralPoint b) noexcept
{
    return (float) (kSpiralDistanceScale * std::sqrt (spiralDistanceSquared (a, b)));
}

inline SpiralPoint spiralCenter (const int* fifths, int n) noexcept
{
    SpiralPoint sum;
    for (int i = 0; i < n; ++i)
        sum = sum + spiralPitch (fifths[i]);
    return n > 0 ? sum * (1.0 / n) : sum;
}

inline float cloudDiameter (const int* fifths, int n) noexcept
{
    double maxDistance = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            maxDistance = std::max (maxDistance, spiralDistanceSquared (spiralPitch (fifths[i]), spiralPitch (fifths[j])));
    return (float) (kSpiralDistanceScale * std::sqrt (maxDistance));
}

inline SpiralPoint spiralTonalCenter (SpiralPoint tonic, SpiralPoint dominant, SpiralPoint subdominant) noexcept
{
    return (tonic * 0.516 + dominant * 0.315 + subdominant * 0.168) * (1.0 / 0.999);
}

inline SpiralPoint spiralTriad (int root, bool minor) noexcept
{
    return spiralTonalCenter (spiralPitch (root), spiralPitch (root + 1), spiralPitch (root + (minor ? -3 : 4)));
}

inline SpiralPoint spiralKey (int root, bool minor) noexcept
{
    const auto tonic = spiralTriad (root, minor);
    const auto dominant = spiralTriad (root + 1, false);
    const auto subdominant = spiralTriad (root - 1, minor);
    if (! minor)
        return spiralTonalCenter (tonic, dominant, subdominant);

    return spiralTonalCenter (tonic,
                             dominant * 0.75 + spiralTriad (root + 1, true) * 0.25,
                             subdominant * 0.75 + spiralTriad (root - 1, false) * 0.25);
}

inline float cloudMomentum (SpiralPoint previous, SpiralPoint current) noexcept
{
    return spiralDistance (previous, current);
}

inline float tensileStrain (SpiralPoint current, SpiralPoint key) noexcept
{
    return spiralDistance (current, key);
}

// ChordSlots MIDI notes do not carry spellings. Start with the root's spelling
// in the active key, then spell the intervals implied by the chord symbol.
inline std::array<int, 12> chordSpiralSpellings (Chord chord, int tonic, int scale)
{
    const bool minorKey = scaleHasMinorThird (scale);
    std::array<int, 12> spellings {};
    for (int pc = 0; pc < 12; ++pc)
        spellings[(size_t) pc] = fifthsOf (pc - tonic, minorKey);
    const int root = spellings[(size_t) chord.root];
    const auto& intervals = chordTypes()[(size_t) chord.type].iv;
    for (int interval : intervals)
    {
        int offset = fifthsOf (interval, interval == 6 && ! hasPerfectFifth (intervals));
        if (interval == 8 && hasAugFifth (intervals))
            offset = 8;
        if (interval == 9 && seventhKind (intervals) == SeventhKind::Diminished7)
            offset = -9;
        if (interval == 15)
            offset = 9; // sharp ninth
        spellings[(size_t) mod12 (chord.root + interval)] = root + offset;
    }
    return spellings;
}

inline SpiralPoint chordSpiralCenter (Chord chord, int tonic, int scale)
{
    const auto spellings = chordSpiralSpellings (chord, tonic, scale);
    const auto& intervals = chordTypes()[(size_t) chord.type].iv;
    SpiralPoint sum;
    for (int interval : intervals)
        sum = sum + spiralPitch (spellings[(size_t) mod12 (chord.root + interval)]);
    return sum * (1.0 / (double) intervals.size());
}

inline SpiralPoint voicingSpiralCenter (Chord chord, int tonic, int scale, const int* notes, int n)
{
    if (n <= 0)
        return chordSpiralCenter (chord, tonic, scale);
    const auto spellings = chordSpiralSpellings (chord, tonic, scale);
    SpiralPoint sum;
    for (int i = 0; i < n; ++i)
        sum = sum + spiralPitch (spellings[(size_t) mod12 (notes[i])]);
    return sum * (1.0 / n);
}
} // namespace cs
