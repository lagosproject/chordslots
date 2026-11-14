// Headless unit tests for cs::TriggerEngine
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "chordslots/Trigger.h"
#include <cstdio>
#include <cassert>

static int checks = 0;
static int failures = 0;

#define CHECK(cond) \
    do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (false)

void testMapping()
{
    // White keys mapping: C3 (60) is slot 0, D3 (62) is 1, E3 (64) is 2, F3 (65) is 3, G3 (67) is 4, A3 (69) is 5, B3 (71) is 6, C4 (72) is 7
    CHECK (cs::TriggerEngine::noteToSlot (60, 60, cs::TriggerLayout::WhiteKeys, 8) == 0);
    CHECK (cs::TriggerEngine::noteToSlot (61, 60, cs::TriggerLayout::WhiteKeys, 8) == -1); // Black key
    CHECK (cs::TriggerEngine::noteToSlot (62, 60, cs::TriggerLayout::WhiteKeys, 8) == 1);
    CHECK (cs::TriggerEngine::noteToSlot (63, 60, cs::TriggerLayout::WhiteKeys, 8) == -1); // Black key
    CHECK (cs::TriggerEngine::noteToSlot (64, 60, cs::TriggerLayout::WhiteKeys, 8) == 2);
    CHECK (cs::TriggerEngine::noteToSlot (65, 60, cs::TriggerLayout::WhiteKeys, 8) == 3);
    CHECK (cs::TriggerEngine::noteToSlot (67, 60, cs::TriggerLayout::WhiteKeys, 8) == 4);
    CHECK (cs::TriggerEngine::noteToSlot (69, 60, cs::TriggerLayout::WhiteKeys, 8) == 5);
    CHECK (cs::TriggerEngine::noteToSlot (71, 60, cs::TriggerLayout::WhiteKeys, 8) == 6);
    CHECK (cs::TriggerEngine::noteToSlot (72, 60, cs::TriggerLayout::WhiteKeys, 8) == 7);
    CHECK (cs::TriggerEngine::noteToSlot (74, 60, cs::TriggerLayout::WhiteKeys, 8) == -1); // Out of range (slot 8 >= numSlots 8)
    CHECK (cs::TriggerEngine::noteToSlot (59, 60, cs::TriggerLayout::WhiteKeys, 8) == -1); // Below root

    // Chromatic mapping: C1 (36) is slot 0, C#1 (37) is slot 1, D1 (38) is slot 2...
    CHECK (cs::TriggerEngine::noteToSlot (36, 36, cs::TriggerLayout::Chromatic, 16) == 0);
    CHECK (cs::TriggerEngine::noteToSlot (37, 36, cs::TriggerLayout::Chromatic, 16) == 1);
    CHECK (cs::TriggerEngine::noteToSlot (38, 36, cs::TriggerLayout::Chromatic, 16) == 2);
    CHECK (cs::TriggerEngine::noteToSlot (51, 36, cs::TriggerLayout::Chromatic, 16) == 15);
    CHECK (cs::TriggerEngine::noteToSlot (52, 36, cs::TriggerLayout::Chromatic, 16) == -1); // Out of range
}

void testHeldNoteStackAndFallback()
{
    cs::TriggerEngine engine;
    cs::TriggerConfig cfg;
    cfg.rootNote = 60;
    cfg.layout = cs::TriggerLayout::WhiteKeys;
    cfg.numSlots = 8;

    // Press C3 (60)
    auto a1 = engine.onNoteOn (60, 95, cfg);
    CHECK (a1.startSlot == 0);
    CHECK (a1.startVelocity == 95);
    CHECK (a1.releaseCount == 0);
    CHECK (engine.getActiveSlot() == 0);
    CHECK (engine.getNumHeldKeys() == 1);

    // Simulate sounding notes [48, 60, 64, 67]
    const int chord0[] = { 48, 60, 64, 67 };
    engine.setSoundingNotes (chord0, 4);

    // Press D3 (62) while C3 (60) is still held -> switch to slot 1
    auto a2 = engine.onNoteOn (62, 105, cfg);
    CHECK (a2.startSlot == 1);
    CHECK (a2.startVelocity == 105);
    // Release previous sounding notes first!
    CHECK (a2.releaseCount == 4);
    CHECK (a2.releaseNotes[0] == 48 && a2.releaseNotes[3] == 67);
    CHECK (engine.getActiveSlot() == 1);
    CHECK (engine.getNumHeldKeys() == 2);

    const int chord1[] = { 50, 62, 65, 69 };
    engine.setSoundingNotes (chord1, 4);

    // Release D3 (62) -> last-note priority should fall back to C3 (60)!
    auto a3 = engine.onNoteOff (62, cfg);
    CHECK (a3.startSlot == 0);
    CHECK (a3.startVelocity == 95);
    CHECK (a3.releaseCount == 4);
    CHECK (a3.releaseNotes[0] == 50 && a3.releaseNotes[3] == 69);
    CHECK (engine.getActiveSlot() == 0);
    CHECK (engine.getNumHeldKeys() == 1);

    engine.setSoundingNotes (chord0, 4);

    // Release C3 (60) -> no keys left -> release chord
    auto a4 = engine.onNoteOff (60, cfg);
    CHECK (a4.startSlot == -1);
    CHECK (a4.releaseCount == 4);
    CHECK (a4.releaseNotes[0] == 48);
    CHECK (engine.getActiveSlot() == -1);
    CHECK (engine.getNumHeldKeys() == 0);
    CHECK (! a4.isTriggerActive);
}

void testSustainPedal()
{
    cs::TriggerEngine engine;
    cs::TriggerConfig cfg;
    cfg.rootNote = 60;
    cfg.layout = cs::TriggerLayout::WhiteKeys;
    cfg.numSlots = 8;

    // Press C3 (60)
    engine.onNoteOn (60, 100, cfg);
    const int chord0[] = { 48, 60, 64, 67 };
    engine.setSoundingNotes (chord0, 4);

    // Press Sustain Pedal down (CC 64 = 127)
    auto aPedalDown = engine.onControlChange (64, 127);
    CHECK (aPedalDown.isTriggerActive);
    CHECK (aPedalDown.releaseCount == 0);

    // Release C3 (60) -> key is released, but sustain holds it!
    auto aKeyOff = engine.onNoteOff (60, cfg);
    CHECK (aKeyOff.releaseCount == 0); // No release yet!
    CHECK (aKeyOff.startSlot == -1);
    CHECK (aKeyOff.isTriggerActive);   // Still active!

    // Lift Sustain Pedal (CC 64 = 0) -> now released!
    auto aPedalUp = engine.onControlChange (64, 0);
    CHECK (aPedalUp.releaseCount == 4);
    CHECK (aPedalUp.releaseNotes[0] == 48);
    CHECK (! aPedalUp.isTriggerActive);
    CHECK (engine.getActiveSlot() == -1);
}

void testModesAndReset()
{
    cs::TriggerEngine engine;
    cs::TriggerConfig cfg;
    cfg.mode = cs::TriggerMode::LoopOnly; // Disabled
    cfg.rootNote = 60;
    cfg.layout = cs::TriggerLayout::WhiteKeys;
    cfg.numSlots = 8;

    auto a1 = engine.onNoteOn (60, 100, cfg);
    CHECK (a1.startSlot == -1);
    CHECK (! a1.isTriggerActive);

    cfg.mode = cs::TriggerMode::Auto;
    engine.onNoteOn (60, 100, cfg);
    const int chord[] = { 60, 64, 67 };
    engine.setSoundingNotes (chord, 3);
    CHECK (engine.isTriggerActive());

    // Reset flushes sounding notes and held keys
    auto aReset = engine.reset();
    CHECK (aReset.releaseCount == 3);
    CHECK (! aReset.isTriggerActive);
    CHECK (engine.getNumHeldKeys() == 0);
}

int main()
{
    testMapping();
    testHeldNoteStackAndFallback();
    testSustainPedal();
    testModesAndReset();

    std::printf ("Trigger tests: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
