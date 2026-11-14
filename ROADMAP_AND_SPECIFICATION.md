# ChordSlots: Current State, Architecture & Implementation Roadmap

> **Purpose**: This document provides a complete, self-contained snapshot of the ChordSlots codebase, completed features, architectural invariants, and exact technical specifications for upcoming sections. Any fresh agent or developer session can pick up immediately from this document.

---

## 1. Project Overview & Current Git State
* **Repository**: `https://github.com/lagosproject/chordslots.git` (Branch: `main`)
* **License**: GNU Affero General Public License v3 or later (AGPLv3)
* **Framework**: JUCE 9 (shared code + VST3, LV2, Standalone targets) + headless STL-only C++17 core.
* **Latest Commits**:
  - `ceeb52d`: Initial target-driven engine replacement & CI integration.
  - `1db12a6`: Priority 1: Bass Inversions, Slash Chords & Bass Motion DP engine.
  - `fb512bc`: Priority 2: Performance & Groove Transform (Strum, Accent, Humanize, MIDI bake).
  - `d64b420`: UI: 3-panel workflow (`1. HARMONY GENERATOR`, `2. GROOVE & FEEL`, `OUTPUT & EXPORT`).
  - `aea007a`: docs: updated README with features and roadmap.
  - `a42ab25`: feat: implement Section 3 Incoming MIDI Trigger Mode (Trigger.h, held stack, CC64, glow).

---

## 2. Completed Sections & Architecture Summary

### A. Core Architecture (`core/include/chordslots/`)
* **Strict Independence**: Headless, standard library only (`<vector>`, `<cmath>`, `<random>`), zero JUCE dependencies, zero dynamic allocations on audio path.
* **Theory & Voicings (`Theory.h`)**:
  - 12 pitch classes, scale definitions (Major, Natural Minor, Harmonic Minor, Dorian, Phrygian, Lydian, Mixolydian, Melodic Minor).
  - Chord structure with `bassInt` (semitones above root for inversions/slash chords).
  - Figured-bass Roman numeral generation (`I6`, `V65`, `ii42`).
* **Target-Driven Generation (`Generator.h`, `Metrics.h`)**:
  - 3 perceptual target curves: **Brightness** (line-of-fifths register), **Tension** (roughness & dissonance), and **Energy** (MIDI velocity & density).
  - Circular Gibbs sampling with circular Viterbi dynamic programming pass for walking basslines (`generateBassLine()` governed by `bassMotion`).
* **Phase 2 Performance Engine (`Performance.h`)**:
  - Pure function `applyPerformance(in, params, slotLenMs, slotIndex, pass)` applied downstream of Phase 1 metrics.
  - Bidirectional strum ($-80\text{ms} \dots +80\text{ms}$), pinned bass on down-strum, top voice melody accent ($+18$ boost, $-10$ inner ducking), deterministic SplitMix64 humanize jitter.
* **Incoming MIDI Trigger Engine (`Trigger.h`)**:
  - Headless trigger state machine with held-key stack (up to 16 keys, last-note priority fallback).
  - Note-to-slot mapping (White Keys vs Chromatic relative to root note).
  - Exact sounding-note tracker so slot edits never leave stuck notes.
  - Release-then-start same-sample ordering.
  - Sustain pedal (CC 64) deferral.

### B. Plugin & Audio Processing (`plugin/`)
* **Live Ingestion & Voice Management (`PluginProcessor.h`, `PluginProcessor.cpp`)**:
  - Auto-mode priority: incoming trigger notes mute loop playback; releasing notes cleanly resumes loop.
  - Nominal 500ms duration for live strumming so fast slots don't truncate spread.
  - Fast-tap cancellation of pending strum notes.
  - Real-time velocity scaling ($V_{\text{final}} = V_{\text{slot}} \times \frac{V_{\text{in}}}{100}$).
  - Audio-thread 32-element queue `pendingNotes` manages strum offsets crossing host block boundaries without dynamic allocation.
  - Prepares live triggered slot glow for UI timeline with guaranteed minimum 120ms duration.
* **MIDI Drag-and-Drop Bake**:
  - `createMidiFile()` converts `offsetMs` to ticks at tempo ($480\text{ PPQ}$) so dragged clips match live audio.
* **3-Panel Workflow Layout (`PluginEditor.cpp`)**:
  - **Header**: Key, Scale, Bars, Slots, Cut Grid, Mood, Fit score, **`TRIGGER` Mode** ("Auto", "Trigger Only", "Loop Only"), **`MAP` Layout** ("White Keys (C3)", "Chromatic (C3)", "Pads (C1)").
  - **`1. HARMONY GENERATOR`**: Locks (`Rhythm`, `Roots`, `Colors`, `Voicing`), `ADVENTURE`, `SYNCOPATION`, `BASS MOTION`, `Undo`, `ROLL`.
  - **`2. GROOVE & FEEL`**: `STRUM`, `ACCENT`, `HUMANIZE`, `Re-Feel`.
  - **`OUTPUT & EXPORT`**: `GATE`, `SYNTH VOL`, `Play`, `Save .mid`, `MidiDragTile`.
  - **Timeline**: Animated energy glow when a slot is triggered live.

---

## 3. Detailed Specifications for Upcoming Sections

- [x] **Section 1: Bass Inversions, Slash Chords & Bass Motion DP**
- [x] **Section 2: Phase 2 Performance & Groove Engine (Strum, Accent, Humanize, Re-Feel)**
- [x] **Section 3: Incoming MIDI Trigger Mode (Held-stack, Last-note priority, CC64, UI glow)**
- [ ] **Section 4: Split-MIDI Output / Dual-Channel Voicing (Next Priority)**
- [ ] **Section 5: A/B Comparison Snapshots & Session History**
- [ ] **Section 6: Triplet & Polyrhythmic Grid**
- [ ] **Section 7: Modern Transition Prior / Billboard Corpus Integration**
* **Goal**: Enable musicians to perform chord slots live using a MIDI keyboard or pad controller (e.g. Push, Launchpad, MPC, KeyStep).
* **Requirements & Invariants**:
  1. **Mapping Modes**:
     * **White Keys / Keyboard Mode**: Starting at $C_3$ (MIDI Note 60), white keys map sequentially: $C_3 \to \text{Slot } 0$, $D_3 \to \text{Slot } 1$, $E_3 \to \text{Slot } 2$, etc.
     * **Chromatic / Pad Mode**: Starting at $C_1$ (MIDI Note 36) or $C_3$ (60): each consecutive semitone maps to Slot $0 \dots N-1$.
  2. **Real-time Velocity & Gate**:
     * Incoming Note-On velocity scales the chord's base velocity proportionally: $V_{\text{final}} = \text{clamp}(1, 127, V_{\text{slot}} \times \frac{V_{\text{in}}}{100})$.
     * Key release (Note-Off) sends immediate Note-Off to all sounding notes of that slot.
     * Legato / overlap handling: Pressing a new slot key cleanly releases the previously held slot's notes.
  3. **Phase 2 Integration**:
     * Triggered slots pass through `applyPerformance()` so live play has the exact same strum stagger, melody accent, and microtiming feel as loop playback.
  4. **UI Visual Feedback**:
     * When a slot is triggered via MIDI, `currentSlot` updates and the timeline slot block highlights with a live glow.

---

### Section 4: Split-MIDI Output / Dual-Channel Voicing
* **Goal**: Enable dual-synth layering from a single instance of ChordSlots (e.g., sending the walking bassline to a Monosynth/Sub on MIDI Channel 2, and the chord pad to a Polyphonic Synth on MIDI Channel 1).
* **Requirements**:
  1. **APVTS Parameter**: `splitMidi` (Bool: Off = All on Ch 1; On = Upper structure on Ch 1, Bass on Ch 2).
  2. **Channel Assignment**:
     * Bass notes identified by `bassCount` from `makeVoicing()` / `PlaySlot` assigned to `kMidiChannelBass = 2`.
     * Upper structure notes assigned to `kMidiChannelChords = 1`.
  3. **MIDI Export & Preview**:
     * Exported `.mid` file contains either dual tracks or dual-channel messages.
     * Preview synth renders both channels cleanly.

---

### Section 5: A/B Comparison Snapshots & Session History
* **Goal**: Allow producers to quickly compare two harmonic variations of a progression without losing either.
* **Requirements**:
  1. **State Snapshots**: Two full `cs::Progression` instances stored in `ChordSlotsProcessor` (`progressionA`, `progressionB`), toggled by an `A/B` button in the header.
  2. **Copy A $\to$ B**: Quick duplicate button to branch an idea.
  3. **Non-destructive Switching**: Audio engine smoothly transitions without audio pops or stuck notes when switching A/B during live playback.

---

### Section 6: Triplet & Polyrhythmic Grid
* **Goal**: Support 3/4, 6/8, and triplet cut grids.
* **Requirements**:
  1. Time signature awareness in header ($4/4$, $3/4$, $6/8$).
  2. Grid snap options: $1/4$, $1/8$, $1/16$, $1/8\text{T}$, $1/16\text{T}$.
  3. Grid drawing and cut clamping adapted to fractional step lengths.

---

## 4. Verification & Testing Commands

To verify the entire project at any time:

```bash
# Build all targets (CLI, Core Tests, Performance Tests, Plugin Tests, VST3, LV2, Standalone)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Run the 3 test suites:
./build/chordslots_core_tests          # 62,682 checks (Theory, Metrics, Gibbs, Inversions)
./build/chordslots_performance_tests   # 11 checks (Phase 2 Strum, Accent, Humanize)
./build/chordslots_plugin_tests        # 3,357 checks (Audio queue, Relocation, MIDI export)

# Take a headless UI snapshot:
./build/chordslots_plugin_tests --snapshot ui_snapshot.png
```
