// ChordSlots - Pattern bank and variation definitions.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Generator.h"
#include "Song.h"

namespace cs
{

constexpr int kNumVariations = 4; // A, B, C, D

enum class SwitchQuantize : int
{
    NextBar = 0,
    NextSlot = 1,
    Immediate = 2
};

struct PlayBank
{
    PlayData variations[kMaxVariations];
    Song song;
    int variationCount = kNumVariations;
};

} // namespace cs
