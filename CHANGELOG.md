# Changelog

## 0.4.2 Test Release

- Refined the Harmony, Feel, and Song workspaces and the chord editing workflow.
- Expanded automated coverage for editing, playback, and MIDI export.
- Added a producer manual and harmony walkthrough, and removed superseded implementation documents.


## 0.4.1 Test Release

- Harmony groups loop settings into a single summary and puts generation, preview, and MIDI export in a shared footer. Roll Settings offers quick access to generation controls.
- Feel pairs the piano roll and accent lane with a dedicated performance panel. Song uses section cards and a separate panel for editing the selected section.
- Improved slider tracks, Strum's zero marker, timing handles, chord selection, and the contrast of target and actual values.
- Refined button states, Match readouts, and the chord inspector for compact plugin windows.

## 0.4.0 Test Release

- Harmony groups Roll, Play, and Match and labels target and measured values on the slot-aligned curves.
- A docked chord inspector lets you audition and replace alternatives and edit voicing while keeping the selected chord and its curves visible.
- Groove & Feel shows chord names aligned with the piano roll.
- Song has a dedicated workspace for arranging, transposing, previewing, and exporting sections.
- Added an Acoustic Piano preview sound and improved chord voicing edits.

## 0.3.1 Test Release

- Added Redo (button, Ctrl/Cmd+Shift+Z, Ctrl/Cmd+Y; 64 steps per variation) and a percentage readout on the Brightness, Tension and Energy lanes (target and measured chord value, on hover and while dragging).
- Fixed: dragging Energy on one slot no longer re-voices the other slots.
- Engine experiments (tritone weight, minor-key and mood context, Brightness soft clip) are in the code but switched off. Chord choice is unchanged.
- Added a listening survey page to help decide which of those experiments should be switched on: https://lagosproject.github.io/chordslots/listening-test.html

## 0.3.0 Test Release

- Separated Harmony and Groove & Feel into tabs with a shared Undo button, synth preview, and MIDI output and export.
- Placed the target curves directly below the chord timeline and grouped curve editing and import controls below the lanes.
- Added a chord-alternative plot with audition and swap, plus expanded chord editing controls.

## 0.2.0 Draft

This release takes ChordSlots from a chord sampler to a MIDI production tool, with more reliable harmony, editable performance parts, and additional plugin formats.

### P0–P1 Stability and musical correctness

- Added a repeatable musical evaluation CLI and baseline guard for tonic presence, progression variety, bass coverage, voicing intervals, Fit, and repeated patterns.
- Replaced the transition score with interpolated unigram backoff. Style priors now follow scale tonality, bVII behavior is scale-aware, and low Energy retains bass.
- Made layer-specific rolls sample candidates against their slot cost. Tied repeated A/A′ phrases in the sampler. Aligned live comping, preview, and MIDI export through shared baked events.
- Added bounded event handling and moved host tempo publication onto the message thread.

### P2–P3 Chord model and voicings

- Derived chord properties from interval sets and added 11 chord types without dropping chord tones.
- Added Close, Piano, Spread, Drop 2, Drop 3, Rootless, and Stacked voicing styles. Added style-specific omission rules, low-register and gap limits, semitone avoidance, and voice-leading assignment.
- Added per-variation voicing selection. New progressions default to Piano, and saved notes remain intact when loading older projects.

### P4–P6 Corpus prior, inference, and tension

- Built the Pop/Rock prior from McGill Billboard annotations and RS200 analyses, with smoothed transition counts and major, minor, and extended-harmony groups. See [DATA_LICENSES.md](DATA_LICENSES.md) for sources, checksums, exclusions, and attribution.
- Jointly sampled chord and bass inversion, added per-variation Variety, and established a new evaluation baseline.
- Replaced static tension scoring with contextual tension that accounts for tonal and harmonic context.

### P7 Producer workflow

- Added melody-aware chord selection with a per-variation Melody Fit control.
- Added bass patterns for sustain, octave pump, offbeat eighths, 808 legato, and walking lines. Split MIDI output sends bass on channel 2 and upper voicings on channel 1.
- Added four saved variations with quantized switching, follow-playback behavior, independent undo, and copy between variations.
- Added a 16-entry Song chain with variation selection and semitone transpose, plus Drag Song MIDI export.
- Added chord-name markers to master, chord-only, bass-only, and Song MIDI exports.
- Added style-specific Energy density calibration, measured knob readouts, and layout refinements for the 1040 × 620 editor.

### P8 Formats and release preparation

- Added a cross-platform build and test matrix with strictness-5 pluginval checks for VST3 and the macOS AU instrument. GitHub workflow execution awaits approval before enabling.
- Added an optional CLAP target using `clap-juce-extensions`. Windows `clap-validator` completed with 32 passed, 0 failed, 1 `state-invalid-random` warning, and 11 skipped. Direct CLAP MIDI note input through generated chord note-ons and note-offs passed integrated CTest. DAW routing smoke testing remains.
- Added a separate macOS AU MIDI FX build for Logic, plus the `midi_fx` processor test. The Logic smoke check remains pending.
- Updated the project version to 0.2.0 and refreshed build, usage, corpus attribution, and verification documentation.
