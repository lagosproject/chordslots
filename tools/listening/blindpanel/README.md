# R7: blind panel of judges

Material for R7 in `docs/GENERAL-PLAN.md`: several people, including at least one with music-theory training, rate ChordSlots progressions, random progressions and corpus progressions blind. The table of inter-judge agreement and the condition comparison come from `analyse_panel.py`.

## Contents

| File | Role |
|---|---|
| `make_panel.cpp` | Engine bridge. `roll` rolls a ChordSlots progression (applyMood, roll). `voice` voices the random and corpus chords with the engine's voicing. |
| `make_panel.py` | Builds the 24 items: 8 ChordSlots, 8 random, 8 corpus. Shuffles them with a seed, writes `midi/itemNN.mid` and `operator/key.csv`. |
| `make_rating_sheets.py` | One listener sheet per participant (`P01.csv` ...), with a per-participant shuffled order. Reads item IDs only. |
| `analyse_panel.py` | Reads filled sheets and `operator/key.csv`. Prints means per condition, per training group, and inter-judge agreement (Kendall's W and mean pairwise Spearman). |
| `midi/item01.mid` ... `item24.mid` | The stimuli. Give these to listeners. |
| `operator/key.csv` | Condition key. Operator only. Not referenced by the listener sheets. |

The MIDI writer and loader are shared with task 5 (`tools/listening/brightness/write_pairs.py`), which reuses the SMF writer of `tools/listening/r10/make_stimuli.py`.

## Build and run

```
g++ -std=c++17 -O2 -I core/include tools/listening/blindpanel/make_panel.cpp -o tools/listening/blindpanel/make_panel
python3 tools/listening/blindpanel/make_panel.py --seed 2026 --r10-dir tools/listening/r10
python3 tools/listening/blindpanel/make_rating_sheets.py --participants 6 --seed 7 --theory P01,P04 --out-dir <folder outside the repo>
python3 tools/listening/blindpanel/analyse_panel.py <folder with the filled P*.csv> --key tools/listening/blindpanel/operator/key.csv
```

`--r10-dir` is the folder that holds `make_stimuli.py` (on `main`, `tools/listening/r10/`). `--theory` lists the participants with music-theory training; the others are marked `none`.

## Conditions

- **chordslots (8).** Rolled by the engine with `roll` and one of four moods (Epic, Melancholic, Uplifting, Dark), two items each. The tonic is random for each item, and the seed is in the key.
- **random (8).** Four chords drawn at random from the diatonic triads and sevenths of one scale (major or natural minor). The four chords are not all the same.
- **corpus (8).** Four consecutive chords from a Billboard annotation in the ChoCo v1.0.0 corpus (`/home/vant/chordslots-data/raw/choco-v1.0.0.zip`, partition `billboard`, CC BY 4.0). Repeated labels are merged. Only plain triads and sevenths are kept (no parenthesised extensions, no N or X). Absolute roots are kept, so the key is not normalised. One window from each of eight songs, drawn with the seed. The source file and window start are in the key.

Each item has four bars, one chord per bar, General MIDI program 0 (Acoustic Grand Piano), 90 bpm, 4/4, velocity 80 on every note. The voicing is the engine's own. Repeated pitches are written once.

## Confounds to read before running the panel

- **Vocabulary.** The ChordSlots items use the engine's full chord set (for example add9, 6/9, 5 and add11, as in `item11`, `item13` and `item16`). The random and corpus items use only triads and sevenths. A difference between the conditions can come from this vocabulary, not from the progression structure.
- **Voicing targets.** ChordSlots items are voiced with mood targets for Brightness and Energy. The random and corpus items use the default slot targets, so their register is not shaped by a mood.
- **Rhythm.** All items are locked to one chord per bar. The engine's syncopated cuts are not in the panel.
- **Key sets.** The random items are in major or natural minor, drawn at random. The moods in the ChordSlots items set their scale (Dark uses Phrygian).

These are the conditions as built. They are not balanced. The analysis reports them as they are.

## Listener part (can be shared)

Play each item once, or as often as the listener wants. Use the same device and listening level for everyone. Rate each item on two scales, 1 to 7:

- **plausibility** (1 to 7): "How plausible is this as a chord progression for a song? 1 = not at all, 7 = very."
- **would_use** (1 to 7): "Would you use this progression as a starting point for your own music? 1 = never, 7 = certainly."

Put anything you notice in `notes`. There are no right answers.

## Operator notes (do not share)

- **The sheets are blind.** They contain item IDs and training only. The condition key is in `operator/key.csv`. Keep it out of the session folder.
- **Training.** Record `music theory` or `none` for each participant (`--theory`). The panel needs at least one participant with music-theory training (R7 requirement). This is not yet met: no participant has been run.
- **Analysis.** `analyse_panel.py` reports per-condition means and SDs, the same by training group, and Kendall's W (tie-corrected) and mean pairwise Spearman over the items every judge rated. With fewer than two judges or fewer than three shared items, the agreement is not computed.
- **Corpus licence.** The corpus windows are derived from ChoCo (CC BY 4.0; Billboard partition). Attribution is needed for any release of the corpus items.
