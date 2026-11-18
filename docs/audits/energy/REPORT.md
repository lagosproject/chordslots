# Energy with fixed chords in ChordSlots

November 18, 2026. Checkout `chordslots-source`, branch `main` tracking `origin/main`, commit `7dc1ef0126d06c10c1616ff86d0acdb31cf30975`. The initial tree was clean. The product diff modifies three files with English text.

## What the help describes

- **Match.** For each slot, `dB = brightness − targetB` and `dT = tension − targetT` are computed from the values measured on the current progression. Its score is `clamp(1 − 2 × sqrt((dB² + dT²) / 2), 0, 1)`. The slots are averaged with equal weight, regardless of their duration, and the editor rounds the percentage to an integer. Energy has no term of its own in the calculation. It can move Match indirectly if it changes the notes used to measure Brightness or Tension. [`Generator.h`](../../../core/include/chordslots/Generator.h#L1158) · [`PluginEditor.cpp`](../../../plugin/PluginEditor.cpp#L1180)
- **Brightness and Tension.** On release, `rerollSlots` is called with the mask of edited slots. The targets guide the regeneration. Brightness combines quality, key-relative color and upper register. Tension combines the voicing's local tension, the distance from its harmonic center to the key's center and the displacement of the center from the previous chord, including the last-to-first link. [`LanesView.cpp`](../../../plugin/ui/LanesView.cpp#L151) · [`Generator.h`](../../../core/include/chordslots/Generator.h#L953) · [`Metrics.h`](../../../core/include/chordslots/Metrics.h#L265)
- **Energy.** Dragging writes `targetE` and marks the mood as custom. On release, `applyEnergy` walks all unlocked slots with a voicing, even if the drag touched only one. It preserves the chord's identity and uses its previous voicing as a reference. Roll enters from `rollButton.onClick` through `processor.edit` and `cs::roll`, which prepares cuts, selects chords and assigns voicings, with voice-leading optimization when enabled. With the Voicing layer locked, an intact chord in Roll only updates its base velocity. [`LanesView.cpp`](../../../plugin/ui/LanesView.cpp#L105) · [`Generator.h`](../../../core/include/chordslots/Generator.h#L1007) · [`Generator.h`](../../../core/include/chordslots/Generator.h#L1361) · [`Generator.h`](../../../core/include/chordslots/Generator.h#L1702)

## Test design

Piano, Close and Rootless, each with seeds 101–108. For each style-seed pair, a progression of four equidistant slots across two 4/4 bars was generated with Roll, tonic C, minor for odd seeds and major for even ones, Free transitions, locked rhythm and Bass Motion at 0. The B targets are 35/45/55/65%, T 30/40/50/60% and E 50% in all slots. The progression and the RNG after that Roll were saved. Each level starts from a fresh copy of both and changes only the E of slot 2 in the edit. The same engine entry point that the editor uses on release was invoked. The gesture and the repaint were followed in code.

The levels tested were 10, 25, 50, 59, 60, 61, 75, 84, 85, 86, 95 and 100%. The 50% control goes through the same operation. The sweep was then repeated with Roll and the Roots and Colors locks enabled to freeze the whole chord. Root, type, symbolic bass interval, cuts and B/T targets were checked in all slots and levels. The 24 bases had a symbolic bass in root position. The additional inversion test takes seed 101 of each style, prepares A♭/C in slot 2 and clones the new base and its RNG before editing to 10, 50 or 95%.

## Energy edit

Each density cell is a range of note counts across the style's eight seeds. Doublings are additional notes of a pitch class already present, also counting the bass.

| Style | 10–60% | 61–85% | 86–100% | Additions between 50 and 95% |
| --- | ---: | ---: | ---: | --- |
| Piano | 5–6 | 6–7 | 7–8 | 8/8 added a chord tonic above and a lower octave of the bass |
| Close | 3–6 | 4–7 | 5–8 | 8/8 added the high note and the lower octave |
| Rootless | 2–5 | 2–5 | 3–6 | 7/8 added the lower octave |

| Energy (%) | 10 | 25 | 50 | 59 | 60 | 61 | 75 | 84 | 85 | 86 | 95 | 100 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| MIDI base velocity | 53 | 64 | 83 | 89 | 90 | 91 | 101 | 108 | 109 | 110 | 116 | 120 |

In all three styles `assembleVoicing` computes `round(45 + 75 × E)` and clamps to 1–127. Above 0.60, Piano and Close can add the root above the highest voice. The available register, the interval constraints and the maximum note count can prevent it. Rootless omits that rule. Above 0.85, the engine can add the bass one octave lower if the new note lands at MIDI 24 or above and there is capacity. Rootless-101 keeps four notes even though its velocity rises from 53 to 116 between 10 and 95%, because its main bass MIDI 32 would produce MIDI 20. [`Voicing.h`](../../../core/include/chordslots/Voicing.h#L1119)

In the 288 edit cases, the neighbors kept their notes and velocities relative to the base. Going from 50 to 95%, Piano, Close and Rootless kept all the previous notes of the edited slot. The main bass kept its pitch in all 24 pairs. With a low octave, the slot's lowest note dropped 12 semitones. At 95%, the maximum absolute leap between consecutive main basses in each style was 10 semitones, counting the loop closure. Following the lowest note of each slot, the maximum was 21 semitones. In the A♭/C test the symbolic interval of 4 and the main bass MIDI 36 were preserved in all three styles. At 95%, MIDI 24 also appeared.

The computed Tension of slot 3 changed between 50 and 95% in 8/8 Piano, 8/8 Close and 7/8 Rootless, even with its MIDI intact. Slot 3 uses the previous voicing to compute the movement term. In Piano-101 it went from 0.519752 to 0.520260. The `mouseUp` comment promising that everything else stays exactly the same is too broad. The contextual recomputation explains this case, and the Energy code does not limit the revoicing to the mask either. In the sample, an alteration of neighboring notes through the Energy edit was not reproduced.

## Roll with fixed chords

The chords, symbolic basses, cuts and B/T targets were preserved in the 288 paired Rolls. The neighboring voicings did change even though their E targets stayed at 50%. The count is the number of neighboring slots with different notes or velocity relative to the control Roll, out of 24 per style and level.

| Style | Mean notes in slot 2 at 10 / 50 / 95% | Different neighbors at 61 / 95% | Main bass of slot 2 different at 95% |
| --- | --- | --- | ---: |
| Piano | 5.25 / 5.25 / 7 | 11 / 10 | 1 of 8 |
| Close | 4 / 4 / 6 | 13 / 13 | 0 of 8 |
| Rootless | 3.625 / 3.625 / 4.375 | 0 / 2 | 1 of 8 |

The control Roll also changed one neighbor with Close and another with Rootless relative to their respective bases. Comparing levels against their control Roll separates those changes from the ones associated with the change in E. For example, Piano-101 in Roll changes slot 3 from `44 51 60 63 70 75` to `44 51 60 63 75 82` between 50 and 95%. In the local edit, that neighbor keeps `44 51 60 63 70 75`. Roll generates with cross-slot references and can optimize the voice leading of the whole cycle. Its result is not equivalent to editing a curve over the existing voicing.

## MIDI output and preview

The JUCE processor was run with simulated host transport and with the internal transport that Play activates. The `processBlock` events were captured and finite synthesis audio with a peak greater than zero was confirmed. The synthesizer receives that same MIDI buffer at the end of `processBlock`. For export, `createMidiFile`, SMF writing and reading of the serialized file were run. Save uses the same exporter and the same SMF format according to the number of tracks. The Master drag toward `writeTempMidiFile` was also followed in code. [`PluginProcessor.cpp`](../../../plugin/PluginProcessor.cpp#L1746) · [`PluginEditor.cpp`](../../../plugin/PluginEditor.cpp#L1321)

For each engine mode, style and seed, 10, 50 and 95% were captured, with Split off and on. Common conditions were 120 BPM, 48 kHz, 256-sample blocks, gate 80%, Sustain on bass and comping, and Strum, Accent and Humanize at zero. Note-on and note-off were paired by channel and note, samples were converted to ticks at 480 PPQ, and pitch, channel, velocity, start and duration were compared.

| Comparison with the SMF of the same mode | Cases | Note/channel or velocity differences | Maximum start and duration difference |
| --- | ---: | --- | --- |
| Simulated host transport | 288 | 0 | 0 ticks in both |
| Internal Play with the host stopped | 288 | 0 | 0 ticks in both |

With Split off, the note-ons of slot 2 at 95% totaled 58 in Piano, 48 in Close and 36 in Rootless across the eight edit seeds. With Split on they were 50, 40 and 29. The Sustain bass on Ch 2 is monophonic and emits `primaryBass`. The low doubling is lost, consistently in live, Play and export. All the emitted notes of slot 2 in that condition kept velocity 116. The plugin's bass pattern, comping and dynamics controls can separately transform the voicing and the base velocity before emitting MIDI. [`Generator.h`](../../../core/include/chordslots/Generator.h#L1814) · [`PluginProcessor.cpp`](../../../plugin/PluginProcessor.cpp#L1782)

Reproducible example, Piano-101, A♭ major in slot 2. The engine gives `44 51 60 63 72 75` at 50%, adds MIDI 80 at 61% and adds MIDI 32 at 86%. At 95%, the combined MIDI emits the eight notes on Ch 1. Split emits `51 60 63 72 75 80` on Ch 1 and `44` on Ch 2. In both files the notes start at tick 960 and last 768 ticks (0.8 s), with velocity 116.

The check that was run reaches the processor and the SMF. Still pending are loading the compiled VST3 in a DAW, hovering over Match and the three lanes to verify the visible tooltip, capturing the MIDI on a second track and repeating Save/Drag Master in the host. The Play test checks the events the synthesizer receives, without attributing a perceived energy to them. The auditions of the chord selector and of the patterns are outside that loop comparison.

## Commands and files

To repeat the trials, follow the [reproduction instructions](../README.md).

- [C++ capture](energy.cpp), [run script](run.ps1) and [Node.js summary](summarize.mjs). The script takes flags and libraries from the plugin test's Release project and uses its simulated host.
- `slots.csv` and `slash-bass.csv`. They include the engine's base MIDI, metrics, targets, time limits and symbolic bass. [Summary by style, mode and level](summary.csv) and `slot-comparisons.csv`, with doublings, main bass leaps and lowest-note leaps.
- `midi-events.csv`, [per-case parity](midi-parity.csv) and example SMFs `piano-101-combined.mid` and `piano-101-split.mid`.
- `build-shared.log`, `build-vst3.log`, `build-plugin-tests.log`, `ctest.log` and `reproducibility.log`.

## Product decision

The help can describe Energy as a velocity and density control that depends on the voicing. The flat density stretches observed keep a velocity response. The bass-octave difference between Combined and Split follows from Split's monophonic nature. If Split is meant to keep the double octaves, that would change its contract and deserves a separate decision. The sample does not yield a reproducible calibration failure. The metrics document MIDI notes and dynamics. Assessing perceived energy would require listening.
