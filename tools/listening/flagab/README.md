# Flag A/B minimal pairs

Listening material for three engine flags, each compared OFF against ON on the same mood, tonic and seed. Built 18 November 2026. No flag is enabled in the repo: the headers in `core/include` are unchanged. Each ON build is a copy of the headers in a scratch directory with one line changed.

| Flag | Constant | ON value | Moods |
|---|---|---|---|
| mood | `kMoodDiatonicContext` (Generator.h) | `true` (`kMinorDiatonicMajorContext` untouched; per-mood strengths from the mood table: Epic 0.75, Melancholic 0.75, Hard 0.40) | Epic, Melancholic, Hard |
| tritone | `kTritoneWeight` (Metrics.h) | `0.05f` (off: `0.0f`) | Tense, Dark |
| softclip | `kBrightnessSoftClip` (Metrics.h) | `0.8f` (off: `0.0f`) | Dark, Tense, Dreamy |

## Files

| File | Role |
|---|---|
| `flagab_probe.cpp` | Roll probe. `roll MOOD TONIC SEED` gives the same output as `make_panel roll`. `batch` does the same for many rolls read from stdin. `types` prints the chord-type intervals for the chord-tone check. |
| `build_flagab.py` | Reproducible build: patches the header copies, compiles, rolls, selects pairs, writes MIDI, checks, writes the key. Standard library plus `write_pairs.py` (which uses the SMF writer of `r10/make_stimuli.py`). |
| `midi/flagabNN_{A,B}.mid` | 12 pairs, 24 files. Which of A or B is ON is decided per pair by a seeded coin flip (`--rng-seed 2026`). |
| `operator/key.json` | Operator key, not for listeners. Per pair: flag, mood, tonic, seed, which file is ON, chords of each file, per-slot Brightness, Tension and Energy from the roll, differing slots, selection record. Also the frequency tables for seeds 1..300 and 1..1000, and the check results. |

## Method

1. Builds. OFF is an unpatched copy of `core/include/chordslots`. Each ON is a copy with the one line above changed. The script asserts that the patch target occurs exactly once and that exactly one line differs from the repo file.
2. Rolls. For each mood used by a flag, tonics C (0), A (9), D (2), F# (6), seeds 1..1000. The probe runs `applyMood`, `layoutEven` (locked to one chord per bar), then `roll`, as in `make_panel roll`.
3. Minimal pair. A combination whose OFF and ON progressions differ in one or two of the four slots. A slot is compared on root, engine suffix and bass interval. Combinations with no difference are not used, and no pair has byte-identical MIDI files (checked by SHA-256).
4. Selection. Seeds 1..300 first. A flag with fewer than four minimal pairs would be searched over 1..1000, and then filled with three-difference pairs. **None of the three flags needed the extension**, so all twelve pairs come from seeds 1..300.
5. Spread and interest. Moods are cycled over the flag's moods. Within a mood, the candidate with an unused tonic is preferred, then the one whose differing slots show the flag's characteristic chords:
   - mood: a bVI or bVII chord (major third, root 8 or 10 above the tonic) opposite a v (minor, root 7), counted higher;
   - tritone: dim, dim7, m7b5 or a dominant (major third and minor seventh);
   - softclip: dim, m7b5 or aug.
6. MIDI. As in the blind panel: 4 bars, one chord per bar, GM program 0, 90 bpm, 4/4, velocity 80, the engine's voicing from the roll (repeated pitches written once) through `write_pairs.write_midi`.

## Frequency tables

Number of (tonic, seed) combinations, out of 1200 (4 tonics x 300 seeds), by the number of differing slots (0 to 4). Primary range, seeds 1..300. The selection used this range only.

**mood (kMoodDiatonicContext)**

| Mood | 0 | 1 | 2 | 3 | 4 | combos |
|---|---|---|---|---|---|---|
| Epic | 0 | 8 | 200 | 912 | 80 | 1200 |
| Melancholic | 0 | 28 | 360 | 696 | 116 | 1200 |
| Hard | 56 | 296 | 548 | 292 | 8 | 1200 |

**tritone (kTritoneWeight = 0.05)**

| Mood | 0 | 1 | 2 | 3 | 4 | combos |
|---|---|---|---|---|---|---|
| Tense | 12 | 117 | 328 | 477 | 266 | 1200 |
| Dark | 368 | 334 | 235 | 234 | 29 | 1200 |

**softclip (kBrightnessSoftClip = 0.8)**

| Mood | 0 | 1 | 2 | 3 | 4 | combos |
|---|---|---|---|---|---|---|
| Dark | 12 | 95 | 333 | 651 | 109 | 1200 |
| Tense | 0 | 21 | 122 | 464 | 593 | 1200 |
| Dreamy | 0 | 4 | 52 | 357 | 787 | 1200 |

Seeds 1..1000 (4000 combinations per mood) are in `operator/key.json`. The same pattern holds at the larger range, with small shifts: minimal combinations (1 or 2 differing) are 668 of 4000 for Epic, 2908 of 4000 for Hard, and 1904 of 4000 for Dark under tritone (which has 1247 at 0).

Reading the table: the share of minimal pairs (1 or 2 differing slots) varies by mood. It is about 17 % for Epic (mood flag, 208 of 1200), about 70 % for Hard (844 of 1200), and about 47 % for Dark under the tritone flag (569 of 1200). Where the 3 and 4 difference counts dominate (Tense and Dreamy under softclip, Epic and Melancholic under mood), the flag changes most of the progression, and minimal pairs are scarce. The 0 column means the flag did not change that progression at all: it is most common for Hard under mood (56 of 1200) and Dark under tritone (368 of 1200), where the affected chords are rarer.

## Chosen pairs

Pair order: flag by flag. "off" and "on" are the roll chords (root, suffix, bass). Each pair differs in exactly two slots (0-based slot indexes in brackets). Which MIDI file is ON is in `operator/key.json` (`on_file`).

| Pair | Flag | Mood | Tonic | Seed | Differing slots | OFF chords | ON chords |
|---|---|---|---|---|---|---|---|
| flagab01 | mood | Epic | C | 1 | 1, 3 | Cm Ab Bb Gm | Cm Ebsus2 Bb Bbadd11 |
| flagab02 | mood | Melancholic | A | 41 | 2, 3 | Am Dm F Em | Am Dm E5 F6/9 |
| flagab03 | mood | Hard | D | 50 | 1, 3 | Dm Am Gm C | Dm Bb Gm Am |
| flagab04 | mood | Epic | F# | 1 | 1, 3 | F#m D E Dbm | F#m Asus2 E Eadd11 |
| flagab05 | tritone | Tense | C | 17 | 2, 3 | Eb5 Abadd11 Bdim7 Bb7b9 | Eb5 Abadd11 Ddim7 Bmadd9 |
| flagab06 | tritone | Dark | A | 117 | 1, 2 | Am Dmadd9 Cm6 Equartal | Am Bdim F7 Equartal |
| flagab07 | tritone | Tense | D | 17 | 2, 3 | F5 Bbadd11 Dbdim7 C7b9 | F5 Bbadd11 Edim7 Dbmadd9 |
| flagab08 | tritone | Dark | F# | 117 | 1, 2 | F#m Bmadd9 Am6 Dbquartal | F#m Abdim D7 Dbquartal |
| flagab09 | softclip | Dark | C | 53 | 1, 3 | Cm Bbquartal Fm Bbm7 | Cm Dm7b5 Fm Gdim |
| flagab10 | softclip | Tense | A | 273 | 1, 2 | Dm Bdim7 Abm E7#9 | Dm Fadd11 Abdim7 E7#9 |
| flagab11 | softclip | Dreamy | D | 231 | 2, 3 | D6 A Dmaj7 G6 | D6 A Daug Db5 |
| flagab12 | softclip | Dark | F# | 53 | 1, 3 | F#m Equartal Bm Em7 | F#m Abm7b5 Bm Dbdim |

Notes on the picks. The mood pairs show bVII (Bbadd11, Bb) and bVI (Ab, Bb, F) in place of v or V. The tritone pairs show dim7, dim and dominant chords appearing or disappearing (Bdim7 to Ddim7; Am to Bdim and F7). The softclip pairs show dim, m7b5 and aug (Dm7b5, Gdim, Daug, Abm7b5, Dbdim). Every pair has two differing slots.

## Checks

- **MIDI re-read** (`write_pairs.read_smf`, all 24 files): format 0, 90 bpm, program 0, velocity 80 only, four bars, the note set of each bar equal to the roll's notes, and the pitch classes of each bar equal to the chord's pitch classes (root, the engine's interval list for the suffix, and the bass). The lowest note is the bass in every bar. All 24 pass.
- **Within a pair**: the A and B files are different (SHA-256), in all 12 pairs.
- **Reproducibility**: the OFF rolls of the 12 pairs, recompiled against the unpatched repo headers, match the stored rolls exactly (slots, Brightness, Tension, Energy and notes). The ON rolls match a second run of their patched build.
- **Header patches**: each ON copy differs from the repo file by exactly one line.

## Regenerate

From the repo root:

```
python3 tools/listening/flagab/build_flagab.py [--work DIR] [--rng-seed 2026]
```

It compiles five probes in the work directory (default: a fresh temporary directory, printed at the start), so it takes a few minutes. It rewrites `midi/` and `operator/key.json`. The repo's headers are not edited.

## Caveats

- Tonics are four (C, A, D, F#) and seeds 1..300 for every chosen pair. The selection is not a sample of the mood's typical progressions and should not be read as one.
- Mood Hard for the mood flag had the highest share of 0 difference (56 of 1200), and Dark for the tritone flag had the highest share of 0 (368 of 1200). Dark's 0 share was high for tritone and softclip alike.
- Chords are the engine's voicing from the roll. Notes are written with velocity 80 and no expression, so timbre and voicing differences are the only audible cues. Nothing about the listening result is measured here.
- The ON moods are mood-table dependent: `kMoodDiatonicContext` uses each preset's own strength (Epic 0.75, Melancholic 0.75, Hard 0.40) from `Generator.h`. Changing those numbers changes these pairs.
- Each flag's ON value is the proposed value from its audit note. The softclip audit calls 0.8 a middle-ground k, not a validated one. The tritone weight 0.05 is between the measured 0.04 and 0.06.
- The check table header in the script output has a cosmetic misalignment (the "file" and "fname" columns run together). Only the printed header is affected; the data and checks are correct.
- Not verified: how any of these sound. Listening is the next step, and the flags remain off.
