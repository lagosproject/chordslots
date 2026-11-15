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
- [x] **Section 4: Split-MIDI Output / Dual-Channel Voicing (Channel 1 Chords, Channel 2 Monophonic Bass)**
- [x] **Section 5: 4-Variation Bank (A/B/C/D), Follow Playback, and Independent Undo History**
- [ ] **Section 6: Meter & Triplet Grid (In Progress)**
- [ ] **Section 7: Modern Transition Prior / Billboard Corpus Integration**
- [ ] **Section 8: Interactive Targeted Randomization & Relative Chord Metric Substitutions (Future Task)**
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

### Section 4: Split-MIDI Output / Dual-Channel Voicing (Completed)
* **Goal**: Enable dual-synth layering from a single instance of ChordSlots (e.g., sending the walking bassline to a Monosynth/Sub on MIDI Channel 2, and the chord pad to a Polyphonic Synth on MIDI Channel 1).
* **Architecture & Invariants**:
  1. **Single Source of Truth (`bassCount`)**: `Voicing.bassCount` and `PlaySlot.bassCount` strictly define voice boundaries. Legacy states automatically migrate via `computeBassCount()`.
  2. **Monophonic & 808 Safe**: Channel 2 emits only the primary bass note (`primaryBass`). Octave doublings are suppressed on Channel 2 in split mode to eliminate monosynth/sub bass retrigger glitches.
  3. **Continuous Bass Track**: Low-energy slots automatically synthesize a continuous primary bass note ($pc + 12k \approx 40$) so Channel 2 never drops out or has silent holes. Non-split output remains untouched.
  4. **Grid-Locked Bass with Strummed Chords**: Strumming offsets apply exclusively to upper voices on Channel 1; bass notes on Channel 2 remain locked to the sample grid.
  5. **Channel-Aware Voice Tracking**: `held` and `pendingNotes` track `{note, channel}` pairs. Note-offs strictly match emitted channels. CC 123 (All Notes Off) backstop sent on both channels on transport stop, split toggles, and mode changes.
  6. **DAW Drag-and-Drop**: Master drag exports a Format-1 two-track file (Track 0: "Chords" on Ch 1, Track 1: "Bass" on Ch 2) with matching end ticks. Dedicated single-track sub-tiles (**Drag Chords** and **Drag Bass**) work across any DAW.

---

### Section 5: 4-Variation Bank (A/B/C/D), Follow Playback & Independent Undo (Completed)
* **Goal**: Provide 4 song sections (Verse, Chorus, Bridge, Outro) with DAW automation stability, decoupled editing, and isolated undo history.
* **Architecture & Invariants**:
  1. **4-Choice Bank from Day One**: `Bank.h` defines `kNumVariations = 4`, `SwitchQuantize` (`Immediate`, `NextBar`, `NextSlot`), and `PlayBank` (`PlayData variations[4]`). This keeps normalized DAW automation values stable.
  2. **Decoupled Playback vs. Edit Target**:
     - `variation` (APVTS parameter: "A", "B", "C", "D") controls what **plays**.
     - `editVariation` (processor state: 0..3) controls what the editor displays and modifies.
     - `followPlayback` toggle: when ON, edit target automatically tracks playback variation; when OFF, user can edit Variation B while Variation A plays in loop.
  3. **Audio Thread Safety**: Zero mutex locks, zero memory allocations on the audio thread. All 4 variations are baked into `PlayBank` at publish time on the message thread; the audio thread reads the atomic variation index lock-free.
  4. **Quantized Variation Switching**: `quantizeSwitch` parameter supports `Immediate`, `NextBar` (default for automation), and `NextSlot`. Releases held voices cleanly on both MIDI channels and clears pending strum notes on variation flip.
  5. **Independent Undo Stack per Variation**: Each variation owns its own 64-step history stack (`histories[4]`). Edits, rolls, and cuts in Variation A do not affect Variation B's undo history.
  6. **Reversible Copy**: `copyVariation(src, dst)` pushes `dst`'s prior progression to `dst`'s history stack before copying, allowing immediate undo if accidental.
  7. **V2 XML State Persistence & V1 Fallback**: Version 2 state file stores `<ProgressionA>`, `<ProgressionB>`, `<ProgressionC>`, `<ProgressionD>`, `editVariation`, and `followPlayback`. Legacy v1 states seamlessly populate all 4 variations without crashing.

---

### Section 6: Meter & Triplet Grid (Completed)
* **Goal**: Support 4/4, 3/4, 6/8 meters, 24-step/quarter timebase, and triplet cut grids (1/4T, 1/8T, 1/16T).
* **Architecture & Invariants**:
  1. **Timebase Upgrade ($kStepsPerQuarter = 24$)**: Exact integer math across all straight and triplet divisions ($1/4=24, 1/4\text{T}=16, 1/8=12, 1/8\text{T}=8, 1/16=6, 1/16\text{T}=4, 1/32=3$). MIDI ticks per step = $480/24=20$.
  2. **Compound & Simple Meter Groupings (`Meter`)**: 4/4 = `{24,24,24,24}` (96 steps/bar), 3/4 = `{24,24,24}` (72 steps/bar), 6/8 = `{36,36}` (72 steps/bar compound duple). Strong beat positions bias rhythm generation and roll cuts.
  3. **Per-Bar Cut Remapping**: `setMeter` preserves each cut's beat offset within its bar, clamping and deduplicating rather than scaling proportionally across the loop length, keeping beat alignment natural. Pushes old state to undo history.
  4. **Header Controls**: `METER` combo (`4/4`, `3/4`, `6/8`, `Host`) and `CUT GRID` combo (`1/4`, `1/4T`, `1/8`, `1/8T`, `1/16`, `1/16T`, `1/32`).
  5. **Audio Thread Safety & Host Tracking**: Lock-free atomic host time-signature reporting in `processBlock`. When "Host" is selected, message thread applies meter change on signature change via undoable `setMeter`. Bar start alignment using `ppqPositionOfLastBarStart`.
  6. **MIDI Export**: Writes `timeSignatureMetaEvent (p.timeSigNum, p.timeSigDen)` in all Format-0 and Format-1 export branches.
  7. **State Migration**: Seamless v1/v2 $\to$ v3 state migration with $6\times$ step scaling, snap mapping, and fallback defaults.

---

### Section 7: Modern Transition Prior / Billboard Corpus Integration
* **Goal**: Incorporate data-driven transition probability matrices trained on contemporary songwriting chord movements.

---

### Section 8: Interactive Targeted Randomization & Relative Chord Metric Substitutions (Future Task)
* **Goal**: Provide surgical, single-parameter randomization and intuitive perceptual feedback when substituting chords.
* **Requirements**:
  1. **Targeted Single-Parameter Randomization on Selection**:
     - Selecting a specific layer or parameter for a slot (e.g. left-selecting a slot's bass note / inversion, root pitch class, voicing extension, or cut position) enables rolling/randomizing *only that isolated parameter* without regenerating or altering any other part of the slot or progression.
     - For example: if a user likes a chord's harmony but wants to explore basslines, selecting the bass note allows re-rolling only the bass motion/inversion for that slot.
  2. **Contextual Chord Substitution with $\Delta$-Metrics Comparison**:
     - When clicking a slot to explore substitute chords (alternatives popup or browser), each candidate chord displays the direct relative delta comparing its measured properties against the currently loaded chord:
       - $\Delta \text{Energy}$: e.g. `+14% Energy` or `-10% Energy`
       - $\Delta \text{Brightness}$: e.g. `+8% Brightness` or `-15% Brightness`
       - $\Delta \text{Tension}$: e.g. `+20% Tension` (more dissonance/friction) or `-12% Tension` (more consonant)
     - Empowers musicians to immediately predict how a substitution changes the emotional arc of the progression before auditioning.

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
