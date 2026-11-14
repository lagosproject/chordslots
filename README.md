# ChordSlots

An open-source chord progression generator plugin (VST3 / LV2 / AU / Standalone)
where you do not pick chords from a list of tagged presets. You draw **how
bright, how tense and how loud** each part of the loop should be, and ChordSlots
finds chords whose *measured* properties follow those curves.

- **Slots with free cuts** – the loop is divided by cut points you can drag,
  add and remove. Chords do not have to last a bar, a half or a quarter.
- **Target lanes** – per-slot Brightness, Tension and Energy targets. The white
  mark in each column is what the current chord actually measures, so you can
  see the fit at a glance. The overall match is the **Fit** score.
- **Locks** – freeze any layer (Rhythm, Roots, Colors, Voicing) or any single
  slot, then **Roll** the rest.
- **Moods** – named sets of target curves (Euphoric, Uplifting, Epic,
  Melancholic, Dark, Tense, Dreamy, Hard). A mood is a starting shape for the
  lanes, not a database of canned progressions.
- **Gets out of the way** – built-in preview synth, MIDI output to drive any
  instrument, drag-and-drop a MIDI clip into the DAW, undo for every roll.

Status: **v0.1, early.** It builds, loads and generates; see
[Limitations](#limitations).

## How the metrics work

Everything is computed from the notes. Nothing is hand-tagged.

| Metric | What it is | Model |
|---|---|---|
| **Brightness** | dark ↔ bright | Mean position of the chord tones on the *line of fifths*, both relative to the chord root (quality: maj7 > maj > sus > m > dim) and relative to the tonic (colour inside the key: a borrowed iv or bVI is darker, a II or #IV is brighter), plus the register of the voicing. After Temperley (2001). |
| **Tension** | relaxed ↔ tense | Sensory roughness of the chord (Plomp–Levelt curves in Sethares' parameterisation, 6 harmonics per note) + harmonic consonance and distance from the tonic in the *Tonal Interval Space* (Bernardes et al., 2016) + a small functional term for dominants. |
| **Energy** | soft ↔ loud | A performance property, not a harmonic one: MIDI velocity and voicing density (bass, sub and top doublings). |

"Amplitude" cannot be a property of a chord symbol, so it is treated honestly as
*Energy* and realised through velocity and how many notes are stacked.

The generator builds every chord available in the key at the current
**Adventure** level (0 = diatonic triads, 1 = borrowed chords and extensions),
scores each against the slot's targets plus a light prior on chord-to-chord
motion, and Gibbs-samples the loop so that each chord fits its neighbours on
both sides (the last chord leads back into the first). Voicings are then chosen
for smooth voice leading.

The calibration constants live in `core/include/chordslots/Metrics.h`. They were
set by inspection and are the most obvious thing to improve with listening
tests. `chordslots-cli table` prints the numbers for every chord.

## Using it

1. Load ChordSlots on an instrument track. It makes sound by itself (turn
   **Synth Vol** down to silence it) and sends MIDI on channel 1.
2. Pick **Key**, **Scale** and a **Mood**, press **ROLL**.
3. Drag the bars in the lanes. Releasing the mouse re-picks only the slots you
   touched.
4. Drag slot edges to move cuts, double-click a slot to split it, double-click a
   cut to remove it. Right-click a slot for the best-fitting alternatives.
5. Lock what you like (a slot's padlock, or a whole layer) and roll again.
6. Press Play in the DAW (it follows host tempo and position) or the plugin's
   own **Play** button, then drag the **Drag MIDI** tile onto a track.

Routing the MIDI output to another instrument depends on the host: in Bitwig,
REAPER and Ardour pick ChordSlots as a MIDI source/route; in Ableton Live set
another track's *MIDI From* to the ChordSlots track; in FL Studio set a MIDI
output port. Where that is awkward, drag the MIDI clip instead.

## Building

Requirements: CMake ≥ 3.22 and a C++17 compiler. JUCE 9 is fetched
automatically (or used from `third_party/JUCE` if present).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build          # core unit tests
```

Artefacts land in `build/ChordSlots_artefacts/Release/`. Options:

- `-DCHORDSLOTS_COPY_AFTER_BUILD=ON` installs into your user plugin folders
  (`~/.vst3`, `~/.lv2`).
- `-DCHORDSLOTS_LTO=ON` enables link-time optimisation for release binaries
  (much slower to link).

`ctest` runs two suites: `core` (theory, metrics, generator, locks) and `plugin`
(drives the real audio processor with a fake host transport and checks note
timing, stuck notes, state save/restore and the MIDI export).
`build/chordslots_plugin_tests --snapshot ui.png` renders the editor to a PNG
without a host. Before a release, also run
[pluginval](https://github.com/Tracktion/pluginval) against the VST3.

Linux packages (Debian/Ubuntu):

```sh
sudo apt install build-essential cmake libasound2-dev libx11-dev libxext-dev \
     libxrandr-dev libxinerama-dev libxcursor-dev libxrender-dev \
     libxcomposite-dev libxi-dev libfreetype-dev libfontconfig1-dev
```

No root? `./scripts/fetch-linux-headers.sh` unpacks the missing headers into
`third_party/sysroot`, which the build picks up on its own.

Only the musical core, without JUCE:

```sh
cmake -S . -B build-core -DCHORDSLOTS_BUILD_PLUGIN=OFF && cmake --build build-core
./build-core/chordslots-cli roll all 5      # five progressions per mood
./build-core/chordslots-cli table 1         # metrics of every chord in C minor
```

## Layout

```
core/include/chordslots/   header-only, STL-only: theory, metrics, generator
tests/                     core unit tests (no framework)
tools/cli.cpp              terminal front-end to the core
plugin/                    JUCE processor, editor and preview synth
```

The split is deliberate: all musical logic is testable and reusable without a
plugin host, and could be wrapped for CLAP, a web UI or a hardware sequencer.

## Limitations

- 4/4 only, sixteenth-note cut resolution, up to 8 bars and 16 slots.
- One chord per slot, sustained (with a Gate control); no strum or arp patterns
  inside a slot yet.
- Incoming MIDI is ignored; you cannot yet trigger slots from a keyboard.
- The metric calibration has not been validated with listening tests.
- No transition statistics learned from real music yet. Open corpora such as
  the McGill Billboard annotations or Chordonomicon could replace the hand-written
  motion prior, subject to their licences.
- Tested on Linux only so far. macOS and Windows should build through JUCE but
  are unverified.

## References

- D. Temperley, *The Cognition of Basic Musical Structures*, MIT Press, 2001 (line of fifths).
- R. Plomp, W. Levelt, "Tonal consonance and critical bandwidth", JASA 38, 1965.
- W. Sethares, *Tuning, Timbre, Spectrum, Scale*, Springer, 1998.
- G. Bernardes et al., "A multi-level tonal interval space for modelling pitch
  relatedness and musical consonance", Journal of New Music Research 45(4), 2016.

## Licence

ChordSlots is free software under the **GNU Affero General Public License v3 or
later** (see `LICENSE`). It is built on [JUCE](https://juce.com) (AGPLv3) and
the Steinberg VST3 SDK. VST is a registered trademark of Steinberg Media
Technologies GmbH.
