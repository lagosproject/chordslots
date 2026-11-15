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
- [x] **Section 6: Meter & Triplet Grid (24-Step Timebase, 4/4, 3/4, 6/8, Triplet Grids)**
- [x] **Section 7: Hand-Authored Style Prior (Provisional)**
- [ ] **Section 8: Interactive Targeted Randomization & Relative Chord Metric Substitutions (Future Task - Superseded by IMPLEMENTATION_PLAN.md)**

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

### Section 7: Hand-Authored Style Prior (Provisional) (Completed)
* **Goal**: Incorporate style prior transition probability matrices reflecting contemporary songwriting chord movements. Hand-authored style prior (provisional): transition weights written by hand from common pop/rock, minor-loop and soul/R&B practice. Not derived from a corpus yet; replacement with licence-clean corpus data is planned (IMPLEMENTATION_PLAN.md, P4).
* **Architecture & Invariants**:
  1. **Reproducible Pipeline (`tools/build_corpus.py`)**:
     - Encodes hand-authored transition counts across common pop/rock, minor-loop, and soul/R&B practice.
     - Collapses frame-based repeated chords to reflect actual chord changes.
     - Emits `core/include/chordslots/Corpus.h` with verifiable script SHA-256 hash and provenance comments.
  2. **Harmonic Tokenization & Style Clusters**:
     - 60 tokens: 12 chromatic scale degrees $\times$ 5 quality classes (`Major`, `Minor`, `Dominant`, `Diminished`, `SusOrAug`). Deterministic table-driven mapping for all 21 chord types.
     - 3 Style Clusters: `PopRock` ($I, IV, V, vi, bVII, bVI, bIII, ii$), `MinorLoops` ($i, bVI, bVII, iv, v/V, bIII, ii^\circ$), and `SoulRnb` ($ii \to V, iii \to vi, IV, I$, secondary dominants).
     - Selecting a Mood defaults to the appropriate style cluster (e.g. Euphoric $\to$ PopRock, Dark $\to$ MinorLoops, Dreamy $\to$ SoulRnb).
  3. **PMI Scoring & Add-$k$ Smoothing**:
     - Smooths zero-count cells via add-$k$ smoothing ($k = 0.5$).
     - Balances negative log-likelihood with Pointwise Mutual Information (PMI) discounting to prevent destination chords from dominating purely by raw unigram popularity.
  4. **Circular Loop-Closure Resolution**:
     - Scores the loop wrap-around transition ($C_{N-1} \to C_0$) with the prior matrix, guaranteeing circular loop progressions resolve naturally without dead ends.
  5. **Modal Degree-Relative Scoring + Diatonic Bonus**:
     - Evaluates chords relative to chromatic degree from tonic, providing diatonic mode rewards for modal scale tones (e.g. $bVII$ in Mixolydian, $IV$ in Dorian).
  6. **Removal of Classical Retrogression Penalty**:
     - Eliminates the classical $V \to IV$ penalty, naturally embracing $V \to IV \to I$ blues/rock/pop movements.
  7. **Adventure Modulation**:
     - Low adventure strictly guides progressions toward common songwriting transitions. High adventure relaxes the prior to give drawn target curves full discovery freedom.
  8. **Zero Allocation & Cache Friendly**:
     - Quantized `constexpr uint8_t` matrices ($< 11\text{ KB}$ total), $O(1)$ lookup, zero audio/message thread dynamic allocation.

---

### Section 8: Interactive Targeted Randomization, Advanced Substitutions & Producer Workflow (Specification & Implementation Plan)

> **Note**: The remaining stages of Section 8 are superseded by [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).

* **Goal**: Deliver a comprehensive producer workflow upgrade: surgical parameter control via rich Timeline Cards and a dynamic Context Bar, in-context chord substitutions, phrase structure & anti-oscillation harmonic intelligence, a mathematically robust voice-leading engine that preserves Fit score, patterned 24-step comping rhythms, MIDI-to-curve extraction, keyswitches, and a modularized, HiDPI-ready UI layout.
* **Core Philosophy**: Protect and showcase ChordSlots' true differentiators—**target curves and measured perceptual metrics**—while delivering table-stakes workflow tools (voice leading, chord suggestions, audition in context) with zero audio-thread allocations and strict determinism.

---

#### 1. Grounded UI Audit & Vertical Space Rebalance (Measured from Actual Snapshot at 1040×620)

* **Current Bottlenecks & Measurement Corrections**:
  - **Header Truncation at Default Size**: `BARS`, `METER`, and `SLOTS` show only "…" ellipsis. `MAP` shows "Wh…" and `MOOD` label is truncated. Moving `TRIGGER` and `MAP` into an Options/Input popover frees ~190px of header width.
  - **Footer Squeeze**: Footer controls currently get only ~9% of window height (~58px), compressing knobs to 32px with zero space for readouts, and cutting off labels ("Rhyt…", "Voici…", "SYNCOPAT…", "BASS MOT…", "SYNTH V…", "Save…").
  - **Real Height Distribution**: Lanes occupy ~36% (~235px), Timeline cards occupy ~22% (~145px), Header is ~68px, and Footer is ~58px.
  - **Timeline Cards Under-utilized**: Cards are ~145px tall and mostly empty space (large chord name + tiny numeral). Space already exists on the cards for chord tones, bass note/inversion, a lock icon, and an options button ("⋯").
  - **Lanes Editability**: Keep 3 distinct lanes (do not collapse into a single overlay lane, which breaks per-slot bar dragging), but compress each lane to ~55px (~165px total).

* **Proposed Vertical Budget (at default ~650px height)**:
  | Area | Current Height | Proposed Height | Musical / UX Role |
  |---|---|---|---|
  | **Header** | ~68px | ~68px | Key, Scale, Bars, Meter, Slots, Snap, Mood/Style, Variations (enlarged to 28px) |
  | **Timeline Cards** | ~145px | ~135px | Rich cards: Chord, Numeral, Tones (e.g. "B3 D#4 F#4 B4"), Bass, Lock glyph, "⋯" button |
  | **Lane Toolbar** | 0px | ~22px | Mood ▾ · Style ▾ · Shape ▾ · Reset · Freehand paint tools |
  | **Lanes** | ~235px | ~165px (3×55px) | Brightness, Tension, Energy target bars (direct per-slot drag + freehand) |
  | **Footer Panels** | ~58px | ~100px | Balanced 3 panels, larger 42px knobs with permanent numeric readouts |
  | **Context / Hint Bar** | ~25px | ~30px | Selected slot inspector (chord, bass, safe melody notes, targeted rolls) or global hints |

---

#### 2. Section 7 Harmonic Enhancements (Corpus, Structure & Anti-Oscillation)

Analysis of sample progressions (e.g., $vi5 - I - IV - V - IV - V - Imaj7 - V7$) reveals crucial improvements to the generative prior:
1. **A-B-A Anti-Oscillation Penalty**:
   - Pure bigram transition matrices evaluate each step independently, allowing unnatural alternating ping-pongs across multiple bars ($IV \to V \to IV \to V$).
   - Add an $A \to B \to A$ penalty in `transitionCost()` that adds a cost penalty when returning to the chord 2 slots back, scaled inversely with `ADVENTURE`.
2. **Phrase-Structure Generation ($A A'$ Option)**:
   - Provide an optional 4-slot or 2-bar phrase repetition mode where slots 0–3 define phrase $A$, and slots 4–7 repeat phrase $A$ with variation on the cadence slot ($A A'$). This directly mirrors authentic pop, rock, and EDM songwriting.
3. **Power Chord Roman Numeral & Quality Mapping**:
   - Diatonic fifth chords (e.g. $G\#5$ in $B$ Major) must reflect scale context: use lowercase roman numerals ($vi5$) when the scale's third is minor, and infer quality class from scale membership for corpus lookups.
4. **Style Selector**:
   - Expose explicit style priors (`PopRock`, `MinorLoops`, `SoulRnb`, `Free`) in the lane toolbar next to Mood.

---

#### 3. Revised 9-Stage Implementation Sequence

##### Stage 1: P0 UI Fixes, Vertical Rebalance & Mechanical File Split
* Split monolithic `PluginEditor.cpp` (1,355 lines) into `plugin/ui/` (`LookAndFeel`, `HeaderBar`, `TimelineView`, `LanesView`, `ControlPanels`, `ContextBar`).
* Widen BARS, METER, and SLOTS; move TRIGGER and MAP into an Input/Options popover.
* Widen footer to ~100px; add permanent numeric readouts under all 8 rotary sliders.
* Enlarge Variation buttons A–D to 28px; make Follow a clear toggle button.
* Guard continuously with `./build/chordslots_plugin_tests --snapshot`.

##### Stage 2: Per-Slot Pin, Rich Timeline Cards & Context Bar
* **Rich Cards**: Render chord tones, bass note, lock icon, and "⋯" options button directly on timeline cards. Add tooltips/legend explaining Brightness card fill and Tension bottom red strip.
* **Per-Slot Pin**: Make slot locking a first-class control (click lock glyph or press `L`). Gibbs sampling respects locked slots as clamped variables.
* **Context Bar**: Displays selected slot details (chord tones, safe melody notes, targeted reroll buttons: *Roll Root*, *Roll Color*, *Roll Bass*). Falls back to hints when no slot is selected.

##### Stage 3: `ChordPickerOverlay` with In-Context Audition
* Custom in-editor overlay replacing native popup menu.
* In-context cadence audition ($C_{\text{prev}} \to C_{\text{candidate}} \to C_{\text{next}}$) running through a dedicated lock-free preview queue in `PluginProcessor` (isolated from live MIDI trigger voices and host export).
* Categorized function tags (*Relative*, *Tritone*, *Modal Interchange*, *Secondary Dominant*) and $\Delta\text{Fit}$ badges.

##### Stage 4: Harmonic Structure (Anti-Oscillation & Phrase Repeat)
* Implement A-B-A anti-oscillation penalty in `Generator.h`.
* Implement optional $A A'$ phrase-structure generation.
* Fix Roman numeral casing for power chords based on scale degree.

##### Stage 5: Upper-Voice Voice-Leading Optimization Engine (in `Generator.h`)
* Implement optimal bipartite matching across varying voice counts ($N \le \text{kMaxNotes} = 8$), separating bass motion on Channel 2 from upper voices on Channel 1.
* Soft cost for soprano displacement ($\le 2$ semitones preferred).
* Metric-bounded re-voicing to guarantee that Fit score does not regress. Governed by a `VOICE LEADING` knob (`Smooth` $\leftrightarrow$ `Free`).

##### Stage 6: Patterned Comping Rhythm Engine (24-Step Grid)
* Grid-aligned step masks with per-hit velocity and gate defined per meter (4/4, 3/4, 6/8).
* Pre-bake comping note events into `PlayData` at publish time; expand `pendingNotes` ring buffer to 64–128 elements to prevent dropouts on dense stabs with strum/humanize.
* Full parity across preview synth, live MIDI output, drag-and-drop `.mid`, and split-channel bass.

##### Stage 7: Lane Toolbar, Freehand Painting & Harmonic Shape Stamps
* Lane toolbar: **Mood ▾ · Style ▾ · Shape ▾ · Reset**.
* Freehand horizontal drag across lane bars to paint continuous target curves.
* Shape stamps (*Rise*, *Drop*, *Arch*, *Cadence*) that automatically adapt to bar count and meter.

##### Stage 8: MIDI-to-Curves ("Analyze This"), Keyswitches & Markers
* Drop `.mid` file onto ChordSlots; slice notes by slot/bar and compute Brightness, Tension, and Energy using existing functions in `Metrics.h`.
* Map low-octave keyswitches ($C_0$–$D\#_0$) for real-time variation switching.
* 16-entry Roll History popover (storing seed + params) to favorite and recall rolls.
* Standard MIDI Marker / Text meta events with chord names in exported MIDI.

##### Stage 9: Preview Sound Models & Best-Effort Shortcuts
* Add lightweight, sample-free Rhodes / Warm EP (2-op FM), Felt Piano, and Analog Pad algorithms in `PreviewSynth.h`.
* Best-effort keyboard shortcuts when editor has focus (`Cmd/Ctrl+Z`, `1`–`4`, `L`, `Space`).
* Expose Roll and Variation Select as host-automatable momentary parameters.

---

#### 4. Answers to Open Technical Checks

1. **Is the editor resizable and HiDPI-aware?**
   - Yes: `setResizable (true, true); setResizeLimits (900, 540, 1800, 1100); setSize (1040, 620);`. JUCE handles HiDPI font and graphics scaling, but current fixed pixel bounds cause label truncation even at the default 1040px width. Moving to proportional flex layout in Stage 1 resolves this permanently.
2. **What do card fill color and red mini-bar encode?**
   - **Card Fill Color**: Brightness ($B \in [0, 1]$), mapped via `colourForBrightness()` from dark blue (dark/minor) through purple to warm gold (bright/major).
   - **Red Mini-Bar at Bottom**: Tension ($T \in [0, 1]$), drawn as `fillRect (r.getX(), r.getBottom() - 4.0f, r.getWidth() * s.tension, 4.0f)`.
   - Both will receive clear tooltips and visual legend labels in Stage 2.
3. **Does dragging across lane bars paint multiple slots?**
   - Yes: `LanesView::applyDrag()` tracks `editedMask |= 1u << slot;` during `mouseDrag()`, allowing horizontal painting across consecutive slots within that lane.
4. **Was the corpus prior active for the roll in the screenshot?**
   - Yes: `Generator.h` lines 327–375 invoke the hand-authored style prior via `styleForProgression()`. The alternating $IV \leftrightarrow V \leftrightarrow IV \leftrightarrow V$ pattern demonstrates that bigram models without an A-B-A penalty can get trapped in high-probability two-chord cycles, validating Stage 4's anti-oscillation penalty.

---

#### 5. Test Suite Additions & Verification Standards

1. **Fit-Score Non-Regression Test**: Run 1,000 randomized rolls through the Voice-Leading DP pass across all scales and meters; verify $\text{Fit}_{\text{after}} \ge \text{Fit}_{\text{before}} - 0.02$.
2. **Comping Note-Event Capacity Test**: Stress-test worst-case comping (1/16T stabs + maximum strum + maximum humanize on 8-note chords) verifying zero dropped notes and zero queue overflow.
3. **Determinism & Pinning Test**: Verify that a progression with Slot $k$ pinned produces identical chords and voicings across repeated rolls with the same seed.
4. **Export Parity Test**: Verify bit-level equivalence between live audio-thread MIDI event queues and baked drag-and-drop `.mid` files for comping, split-MIDI, and strum offsets.
5. **Anti-Oscillation & Structure Test**: Verify that progressions generated with anti-oscillation enabled exhibit zero 3-chord back-and-forth oscillations ($A-B-A-B$).

---

## 4. Verification & Testing Commands

To verify the entire project at any time:

```bash
# Build all targets (CLI, Core Tests, Performance Tests, Plugin Tests, VST3, LV2, Standalone)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Run the test suites:
./build/chordslots_core_tests          # 67,092 checks (Theory, Metrics, Gibbs, Inversions, Harmonics)
./build/chordslots_performance_tests   # 11 checks (Phase 2 Strum, Accent, Humanize)
./build/chordslots_trigger_tests       # 55 checks (Held-stack, Last-note priority, CC64, White Keys/Chromatic)
./build/chordslots_plugin_tests        # 4,385 checks (Audio queue, Relocation, MIDI export, Comping)
./build/chordslots-cli eval 60 --compare tests/eval_baseline.json  # 128 metric checks (Harness non-regression)

# Take a headless UI snapshot:
./build/chordslots_plugin_tests --snapshot ui_snapshot.png
```
