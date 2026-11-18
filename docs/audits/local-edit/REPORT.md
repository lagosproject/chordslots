# ChordSlots Brightness · single-slot edit

Extension of the [curve audit](../REPORT.md), on `chordslots-source`, branch `main`, commit `7dc1ef0126d06c10c1616ff86d0acdb31cf30975`. The original CSVs were preserved. The helper lives in `audit-curves/local-edit` and reuses the [helper generator of fixed-center headers](../fixed-header.mjs). The bases were checked against `../brightness-current.csv`.

## Paired design

- C major, natural minor and Dorian. Seeds 101–120. Same settings as in the original audit, including four equidistant slots across four 4/4 bars, Free style, Piano voicing, Adventure 0.30, Variety 0.50, target Tension 0.40, target Energy 0.60, Voice Leading 0.50 and locked rhythm. Each scale/seed combination produces a single base with Roll at Brightness 0.5.
- For each trial the base is copied together with the full RNG state immediately after the Roll. To test 0.4, the slot's starting target is set to 0.4 in the copy, without regenerating the base. This way all trials for each seed and scale start from the same chords and voicings. The reference regenerates with the trial's starting target, so there are 0.4→0.4 and 0.5→0.5 controls. The increases are 0.4→0.6 and 0.5→0.7.
- `rerollSlots` is run with a single bit and its default temperature, 0.004. Slot 2 (interior, next 3) and slot 4 (last, next 1) are run separately. One variant is compiled with the current center (`57 + 14 × target`) and another with the center fixed at 64. In the base both use 64. In the engine, the edit only revoices the slot if its chord changes. The effective pitches are recorded, even if the target center has changed.
- The 240 slots of the 60 bases were verified in the four combinations of center and position and in all the copies, comparing chord, MIDI notes, velocity, Tension and harmonic and register measures. The initial chords and notes match the original CSV. The four full serializations of the RNG states also match. Each aggregate row of the table uses 60 paired trials, 20 per scale. There are 960 `rerollSlots` runs counting both centers, positions, starts and their controls.

## Measured results

`rawH` is the unclipped harmonic term, `0.55 × quality + 0.25 × tonal color`. An absolute variation less than or equal to 0.0000001 is treated as a tie. Register is the mean of the MIDI notes ≥48, in semitones. Δ is computed by subtracting the state indicated by the comparison. The median of magnitudes uses the absolute value per trial, to show movement even when the signed changes cancel out. ΔT refers to the Tension computed by the engine (0–1). The last column gives its median magnitude in the edited slot and in the next one.

| Slot | Center | Start | Comparison | rawH ↑ | rawH = | rawH ↓ | Median Δ register | Median register magnitude | Median ΔT magnitude slot / next |
|---:|---|---:|---|---:|---:|---:|---:|---:|---:|
| 2 | Current | 0.4 | Reference vs base | 6/60 (10.0%) | 8 | 46 | 0.00 | 2.25 | 0.049 / 0.006 |
| 2 | Current | 0.4 | Increase vs base | 44/60 (73.3%) | 11 | 5 | 0.00 | 1.77 | 0.050 / 0.007 |
| 2 | Current | 0.4 | Increase vs reference | 59/60 (98.3%) | 1 | 0 | 0.50 | 2.50 | 0.076 / 0.006 |
| 2 | Current | 0.5 | Reference vs base | 26/60 (43.3%) | 16 | 18 | 0.00 | 1.50 | 0.040 / 0.004 |
| 2 | Current | 0.5 | Increase vs base | 50/60 (83.3%) | 7 | 3 | 0.50 | 2.07 | 0.058 / 0.007 |
| 2 | Current | 0.5 | Increase vs reference | 41/60 (68.3%) | 19 | 0 | 0.00 | 1.05 | 0.041 / 0.005 |
| 2 | Fixed | 0.4 | Reference vs base | 6/60 (10.0%) | 8 | 46 | 0.00 | 2.25 | 0.051 / 0.006 |
| 2 | Fixed | 0.4 | Increase vs base | 44/60 (73.3%) | 11 | 5 | 0.00 | 2.02 | 0.053 / 0.007 |
| 2 | Fixed | 0.4 | Increase vs reference | 59/60 (98.3%) | 1 | 0 | -0.13 | 2.50 | 0.082 / 0.005 |
| 2 | Fixed | 0.5 | Reference vs base | 26/60 (43.3%) | 16 | 18 | 0.00 | 1.50 | 0.040 / 0.004 |
| 2 | Fixed | 0.5 | Increase vs base | 50/60 (83.3%) | 7 | 3 | 0.00 | 2.48 | 0.058 / 0.007 |
| 2 | Fixed | 0.5 | Increase vs reference | 41/60 (68.3%) | 19 | 0 | 0.00 | 1.75 | 0.040 / 0.004 |
| 4 | Current | 0.4 | Reference vs base | 3/60 (5.0%) | 14 | 43 | 0.00 | 1.65 | 0.063 / 0.006 |
| 4 | Current | 0.4 | Increase vs base | 37/60 (61.7%) | 17 | 6 | 0.00 | 1.00 | 0.049 / 0.007 |
| 4 | Current | 0.4 | Increase vs reference | 52/60 (86.7%) | 8 | 0 | 0.93 | 2.50 | 0.060 / 0.006 |
| 4 | Current | 0.5 | Reference vs base | 22/60 (36.7%) | 21 | 17 | 0.00 | 1.13 | 0.043 / 0.004 |
| 4 | Current | 0.5 | Increase vs base | 47/60 (78.3%) | 11 | 2 | 0.15 | 1.75 | 0.053 / 0.009 |
| 4 | Current | 0.5 | Increase vs reference | 43/60 (71.7%) | 17 | 0 | 0.00 | 1.75 | 0.051 / 0.006 |
| 4 | Fixed | 0.4 | Reference vs base | 3/60 (5.0%) | 14 | 43 | 0.00 | 1.65 | 0.063 / 0.006 |
| 4 | Fixed | 0.4 | Increase vs base | 37/60 (61.7%) | 17 | 6 | 0.00 | 1.27 | 0.053 / 0.007 |
| 4 | Fixed | 0.4 | Increase vs reference | 52/60 (86.7%) | 8 | 0 | 0.85 | 2.50 | 0.058 / 0.006 |
| 4 | Fixed | 0.5 | Reference vs base | 22/60 (36.7%) | 21 | 17 | 0.00 | 1.13 | 0.043 / 0.004 |
| 4 | Fixed | 0.5 | Increase vs base | 47/60 (78.3%) | 11 | 2 | 0.00 | 2.50 | 0.057 / 0.009 |
| 4 | Fixed | 0.5 | Increase vs reference | 43/60 (71.7%) | 17 | 0 | 0.00 | 2.90 | 0.066 / 0.006 |

### Reading the comparison

- **Local harmonic control.** Against its paired regeneration, the higher target produced higher `rawH` in 195/240 trials per center condition. The other 45 tied. Both conditions chose the same chords in the 480 result pairs, counting references and increases. The moderate harmonic effect persists with the fixed center.
- **The chord that was already sounding is a different reference.** A regeneration at the same target changed the chord in 186/240 trials with the current behavior. The increases improved `rawH` against the base in 178/240 trials, tied in 46 and lowered it in 16. The control raises the probability of finding a harmonically brighter result, with limited guarantees for a chord that is already present. At 0.5→0.7, C minor was especially prone to tying against its reference (9/20 go up in slot 2 and 8/20 in slot 4). In major, 20/20 went up in both positions. The breakdowns for the three scales are in [summary.csv](summary.csv).
- **Register and Tension.** Against the reference, the signed median of upper pitch ranges from 0 to +0.93 semitones with the moving center and from −0.13 to +0.85 with the fixed center. The median magnitudes reach 2.90 semitones even with the fixed center. Tension can move even though its target stays at 0.40. Against the base, the largest absolute change observed was 0.313 in the edited slot and 0.030 in the next one, considering controls and increases at both centers. In the 480 current trials the Tension of the next slot changed in 380, including the 4→1 cases. In the 960 trials, no slot outside the mask changed chord or notes, and the Tension of slots other than the edited one and its successor remained the same within the stated tolerance.

### Reproducible examples

| Scale, seed, slot and increase | Reference and result |
|---|---|
| C major, 101, slot 2, 0.4→0.6 | The base and the reference keep Am, `rawH=0.35000`. With the increase both centers choose Cadd11, `rawH=0.45065`. Upper pitch +0.95 semitones and Tension −0.05251 in slot 2 and −0.01299 in slot 3 relative to the base. |
| C Dorian, 107, slot 4, 0.5→0.7 | The base has E♭add9, `rawH=0.45844`. Both the reference and the increase choose E♭, `rawH=0.45000`, at both centers. The upper pitch rises +2.50 semitones in both trials. Tension drops 0.12591 in slot 4 and 0.00225 in slot 1. The increase ties with its reference and falls back against the base. |

The paired contrast supports keeping Brightness as a local harmonic and register edit. The gesture acts as a preference over regeneration and can give the same harmonic result as regenerating at the starting target. The scope of this test is flat background curves, a single isolated edit and the cited settings. Changes in the internal metrics need a listening test to relate them to perceived brightness.

## Repetition and files

To repeat the trials, follow the [reproduction instructions](../README.md).

- [run.cmd](run.cmd), [local-edit.cpp](local-edit.cpp), [summarize.mjs](summarize.mjs).
- Per-slot CSVs with before/after of all chords, notes and measures. `slots-2-current.csv`, `slots-2-fixed.csv`, `slots-4-current.csv`, `slots-4-fixed.csv`.
- `edits.csv` contains the differences of each run relative to its base, including the checks on other slots. `paired.csv` contains the increase-minus-regeneration difference for the same start, center, slot, seed and scale. [summary.csv](summary.csv) breaks down medians, maxima and counts by scale.
- `rng-2-current.txt`, `rng-2-fixed.txt`, `rng-4-current.txt` and `rng-4-fixed.txt`. `original-checksums.json`.
