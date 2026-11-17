# ChordSlots 0.2.0 Draft Release Notes

**Prepared for review. Not published.**

ChordSlots 0.2 makes the target-curve generator easier to use in a production session. Draw brightness, tension, and energy over a loop, then shape chords, bass, timing, and MIDI output in the editor.

## Highlights

- Four variations and an editable Song chain with up to 16 variation and transpose entries.
- Melody-aware harmony, five bass patterns, split-channel output, chord-name MIDI markers, and Drag Song export.
- Seven voicing styles, contextual harmonic tension, corpus-informed transitions, and per-style Energy calibration.
- An optional CLAP target and a separate macOS AU MIDI FX target for Logic.
- An updated editor, build guide, and corpus attribution.

## Formats and compatibility

The standard build produces VST3 and Standalone, plus AU on macOS or LV2 on non-Apple platforms. CLAP is optional with `-DCHORDSLOTS_BUILD_CLAP=ON`. macOS also provides `ChordSlotsMidiFX_AU`. Use the normal **ChordSlots** instrument when you need its preview synth.

## Verification status

The Windows VST3 passed pluginval strictness 5 in the local validation run. The CI workflow defines Linux, macOS, and Windows builds, headless tests, artifact uploads, and strictness-5 pluginval checks. It awaits approval before being enabled on GitHub, so there are no remote matrix results yet.

On Windows, the full `clap-validator` run completed with 32 passed, 0 failed, 1 warning, and 11 skipped. The `state-invalid-random` warning says the wrapper reported a successful nonempty random-state load while the plugin remained stable. Integrated CTest passed direct CLAP MIDI note input through to generated chord note-ons and note-offs. A DAW smoke test remains to confirm host-level MIDI routing.

The macOS AU MIDI FX has processor-level coverage through the `midi_fx` test. Run `auval -v aumi ChsM Chsl` and complete the Logic software-instrument routing smoke check on a Mac with Logic. That physical check remains outstanding.

Use the evaluation suite as a regression guard against the Linux baseline. Listening across hosts and projects remains useful for judging musical preference.

## Build

See [README.md](README.md) for per-format build and install notes, [optional CLAP instructions](README.md#optional-clap), the offline dependency override, and AU MIDI FX instructions. Corpus sources and attribution are in [DATA_LICENSES.md](DATA_LICENSES.md).