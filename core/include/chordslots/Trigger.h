// ChordSlots - headless trigger state machine for live MIDI playing.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Theory.h"
#include <algorithm>
#include <cstdint>

namespace cs
{

enum class TriggerMode : int
{
    Auto = 0,        // Incoming trigger keys take priority over loop
    TriggerOnly = 1, // Loop is silent; only triggered chords play
    LoopOnly = 2     // Incoming keys are ignored; only loop plays
};

enum class TriggerLayout : int
{
    WhiteKeys = 0,   // C3, D3, E3, F3... (white keys relative to trigger root)
    Chromatic = 1    // C3, C#3, D3, D#3...
};

struct TriggerConfig
{
    TriggerMode mode = TriggerMode::Auto;
    int rootNote = 60; // Default C3 (MIDI 60)
    TriggerLayout layout = TriggerLayout::WhiteKeys;
    int numSlots = 8;
};

class TriggerEngine
{
public:
    static constexpr int kMaxHeldKeys = 16;
    static constexpr int kMaxSounding = 16;

    struct SoundingVoice
    {
        int note = 0;
        int channel = 1;
    };

    struct TriggerAction
    {
        int releaseNotes[kMaxSounding] = {};
        int releaseChannels[kMaxSounding] = {};
        int releaseCount = 0;
        int startSlot = -1;       // >= 0 to trigger this slot
        int startVelocity = 0;    // incoming velocity for scaling
        bool isTriggerActive = false; // true if a chord is sounding or sustained
    };

    /** Maps an incoming MIDI note to a slot index [0..numSlots-1], or -1 if unmapped. */
    static int noteToSlot (int midiNote, int rootNote, TriggerLayout layout, int numSlots) noexcept
    {
        const int diff = midiNote - rootNote;
        if (diff < 0)
            return -1;

        int slot = -1;
        if (layout == TriggerLayout::Chromatic)
        {
            slot = diff;
        }
        else
        {
            const int octave = diff / 12;
            const int semitone = diff % 12;
            int whiteKey = -1;
            switch (semitone)
            {
                case 0:  whiteKey = 0; break; // C
                case 2:  whiteKey = 1; break; // D
                case 4:  whiteKey = 2; break; // E
                case 5:  whiteKey = 3; break; // F
                case 7:  whiteKey = 4; break; // G
                case 9:  whiteKey = 5; break; // A
                case 11: whiteKey = 6; break; // B
                default: whiteKey = -1; break; // Black keys
            }
            if (whiteKey >= 0)
                slot = octave * 7 + whiteKey;
        }

        return (slot >= 0 && slot < numSlots) ? slot : -1;
    }

    int noteToSlot (int midiNote, const TriggerConfig& cfg) const noexcept
    {
        return noteToSlot (midiNote, cfg.rootNote, cfg.layout, cfg.numSlots);
    }

    /** Call when incoming Note-On arrives. */
    TriggerAction onNoteOn (int midiNote, int velocity, const TriggerConfig& cfg) noexcept
    {
        TriggerAction act;
        if (cfg.mode == TriggerMode::LoopOnly)
            return act;

        const int targetSlot = noteToSlot (midiNote, cfg);
        if (targetSlot < 0)
            return act; // Unmapped note ignored

        // Push to held keys stack with last-note priority
        removeHeldKey (midiNote);
        if (numHeld < kMaxHeldKeys)
        {
            heldKeys[numHeld] = midiNote;
            heldVels[numHeld] = velocity;
            ++numHeld;
        }

        // New chord should start
        prepareSwitchChord (targetSlot, velocity, act);
        act.isTriggerActive = true;
        return act;
    }

    /** Call when incoming Note-Off arrives. */
    TriggerAction onNoteOff (int midiNote, const TriggerConfig& cfg) noexcept
    {
        TriggerAction act;
        if (cfg.mode == TriggerMode::LoopOnly)
            return act;

        if (! removeHeldKey (midiNote))
            return act; // Note was not held as a trigger

        if (numHeld > 0)
        {
            // Fallback to previous held key in stack (last-note priority)
            const int topNote = heldKeys[numHeld - 1];
            const int topVel  = heldVels[numHeld - 1];
            const int prevSlot = noteToSlot (topNote, cfg);
            if (prevSlot >= 0 && prevSlot != activeSlot)
            {
                prepareSwitchChord (prevSlot, topVel, act);
                act.isTriggerActive = true;
                return act;
            }
            // If it mapped to the same slot, keep sounding
            act.isTriggerActive = true;
            return act;
        }

        // No more keys held
        if (sustainPedal)
        {
            // Defer release until sustain pedal lifts
            isSustaining = true;
            act.isTriggerActive = true;
            return act;
        }

        // Release sounding notes
        emitRelease (act);
        act.isTriggerActive = false;
        return act;
    }

    /** Call on CC 64 (Sustain Pedal). */
    TriggerAction onControlChange (int cc, int value) noexcept
    {
        TriggerAction act;
        if (cc != 64)
            return act;

        const bool down = value >= 64;
        sustainPedal = down;

        if (! down && isSustaining)
        {
            // Pedal released and no keys are held -> release now
            isSustaining = false;
            if (numHeld == 0)
            {
                emitRelease (act);
                act.isTriggerActive = false;
            }
            else
            {
                act.isTriggerActive = true;
            }
        }
        else
        {
            act.isTriggerActive = (numHeld > 0 || isSustaining);
        }
        return act;
    }

    /** Remembers the exact notes that were sounded for the active chord. */
    void setSoundingNotes (const int* notes, int count, int defaultChannel = 1) noexcept
    {
        soundingCount = 0;
        for (int i = 0; i < count && i < kMaxSounding; ++i)
            soundingVoices[soundingCount++] = { notes[i], defaultChannel };
    }

    void setSoundingVoices (const SoundingVoice* voices, int count) noexcept
    {
        soundingCount = 0;
        for (int i = 0; i < count && i < kMaxSounding; ++i)
            soundingVoices[soundingCount++] = voices[i];
    }

    /** Full reset: releases everything and flushes stack. */
    TriggerAction reset() noexcept
    {
        TriggerAction act;
        emitRelease (act);
        numHeld = 0;
        sustainPedal = false;
        isSustaining = false;
        act.isTriggerActive = false;
        return act;
    }

    bool isTriggerActive() const noexcept { return numHeld > 0 || isSustaining; }
    int getActiveSlot() const noexcept { return activeSlot; }
    int getNumHeldKeys() const noexcept { return numHeld; }

private:
    bool removeHeldKey (int note) noexcept
    {
        int idx = -1;
        for (int i = 0; i < numHeld; ++i)
            if (heldKeys[i] == note) { idx = i; break; }
        if (idx < 0)
            return false;
        for (int i = idx; i < numHeld - 1; ++i)
        {
            heldKeys[i] = heldKeys[i + 1];
            heldVels[i] = heldVels[i + 1];
        }
        --numHeld;
        return true;
    }

    void emitRelease (TriggerAction& act) noexcept
    {
        act.releaseCount = soundingCount;
        for (int i = 0; i < soundingCount; ++i)
        {
            act.releaseNotes[i] = soundingVoices[i].note;
            act.releaseChannels[i] = soundingVoices[i].channel;
        }
        soundingCount = 0;
        activeSlot = -1;
    }

    void prepareSwitchChord (int targetSlot, int velocity, TriggerAction& act) noexcept
    {
        emitRelease (act); // Always emit releases first
        act.startSlot = targetSlot;
        act.startVelocity = velocity;
        activeSlot = targetSlot;
        isSustaining = false;
    }

    int heldKeys[kMaxHeldKeys] = {};
    int heldVels[kMaxHeldKeys] = {};
    int numHeld = 0;

    SoundingVoice soundingVoices[kMaxSounding] = {};
    int soundingCount = 0;
    int activeSlot = -1;

    bool sustainPedal = false;
    bool isSustaining = false;
};

} // namespace cs
