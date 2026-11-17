// ChordSlots core - fixed, deterministic bass pattern generation.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>

namespace cs
{

constexpr int kMaxBassHitsPerSlot = 64;

enum class BassStyle : int
{
    Sustain = 0,
    OctavePump,
    OffbeatEighths,
    Legato808,
    Walking
};

struct BassHit
{
    int stepOffset = 0;       // 24 steps per quarter note
    int durationSteps = 0;
    int pitch = 0;             // MIDI note
    float velocityScale = 1.0f;
    bool glide = false;        // overlapping gate intended for mono synth portamento
};

/**
 * Writes a bass pattern to caller-owned fixed storage. No allocation or global
 * state is used. `rootMidi` is the current slot's bass note; `nextRootMidi` is
 * the following slot's bass note. `scaleMask` is a 12-bit pitch-class mask
 * used for the walking approach note; zero selects a chromatic semitone below
 * the next root.
 */
inline int getBassHits (BassStyle style, int rootMidi, int nextRootMidi,
                        int slotSteps, BassHit* outHits, int maxHits,
                        uint16_t scaleMask = 0) noexcept
{
    if (outHits == nullptr || maxHits <= 0 || slotSteps <= 0)
        return 0;

    rootMidi = std::clamp (rootMidi, 0, 127);
    nextRootMidi = std::clamp (nextRootMidi, 0, 127);
    int count = 0;
    const auto add = [&] (int offset, int duration, int pitch, float velocity, bool glide = false)
    {
        if (count < maxHits)
            outHits[count++] = { offset, duration, std::clamp (pitch, 0, 127), velocity, glide };
    };

    if (style == BassStyle::Sustain)
    {
        add (0, slotSteps, rootMidi, 1.0f);
    }
    else if (style == BassStyle::OctavePump)
    {
        for (int offset = 0; offset < slotSteps && count < maxHits; offset += 12)
        {
            const int pitch = (offset / 12) % 2 == 0 ? rootMidi : rootMidi + 12;
            add (offset, std::min (12, slotSteps - offset), pitch,
                 offset % 24 == 0 ? 1.0f : 0.82f);
        }
    }
    else if (style == BassStyle::OffbeatEighths)
    {
        for (int offset = 12; offset < slotSteps && count < maxHits; offset += 24)
            add (offset, std::min (10, slotSteps - offset), rootMidi,
                 offset % 48 == 12 ? 1.0f : 0.85f);
        // Very short slots still get a playable bass note.
        if (count == 0)
            add (0, slotSteps, rootMidi, 1.0f);
    }
    else if (style == BassStyle::Legato808)
    {
        // One onset per slot; only overlap at the boundary when the next pitch
        // differs, creating the slide for mono synths. Consumers preserve it.
        const bool changesPitch = rootMidi != nextRootMidi;
        add (0, slotSteps + (changesPitch ? 1 : 0), rootMidi, 1.0f, changesPitch);
    }
    else if (style == BassStyle::Walking)
    {
        // Root on each quarter. On the final beat, lead into the following
        // root using the nearest lower chromatic or scale tone.
        for (int offset = 0; offset < slotSteps && count < maxHits; offset += 24)
        {
            int pitch = rootMidi;
            if (offset > 0 && offset + 24 >= slotSteps)
            {
                pitch = nextRootMidi - 1;
                if (scaleMask != 0)
                {
                    for (int candidate = nextRootMidi - 1; candidate >= nextRootMidi - 12; --candidate)
                    {
                        const int pitchClass = (candidate % 12 + 12) % 12;
                        if ((scaleMask & (uint16_t (1u) << pitchClass)) != 0)
                        {
                            pitch = candidate;
                            break;
                        }
                    }
                }
            }
            add (offset, std::min (22, slotSteps - offset), pitch,
                 offset == 0 ? 1.0f : 0.88f);
        }
    }

    return count;
}

} // namespace cs
