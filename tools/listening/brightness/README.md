# Task 5: Brightness listening pairs

Material for task 5 in `docs/GENERAL-PLAN.md` (carry out the Brightness listening) and the protocol in `docs/CONTINUITY-PLAN.md` ("Listening protocol with producers"). Twelve pairs of four-chord loops: six in C major and six in A minor. Each pair is two MIDI files. A second, matched set (pairs 13-22, see "Matched set" below) was added so that register and colour effects can be compared at equal size.

## Contents

| File | Role |
|---|---|
| `make_pairs.cpp` | Engine probe. Builds the loops with the engine's voicing (`voiceAll`) and measures Brightness with `updateActuals`. Prints JSON on stdout. Also `make_pairs check`, which returns the engine Brightness of given voicings. |
| `write_pairs.py` | Writes `pairs/pairNN_{A,B}.mid`, writes `pairs.json`, re-reads every MIDI file and prints the check tables. Reuses the SMF writer of `tools/listening/r10/make_stimuli.py`. |
| `pairs/pairNN_{A,B}.mid` | The 24 stimulus files (format 0). Give these to listeners. |
| `pairs.json` | Operator key: chords, pitch classes, MIDI notes, register and Brightness values for each file and pair. Do not give it to listeners. |

## Build and regenerate

```
g++ -std=c++17 -O2 -I core/include tools/listening/brightness/make_pairs.cpp -o tools/listening/brightness/make_pairs
tools/listening/brightness/make_pairs > pairs_raw.json
python3 tools/listening/brightness/write_pairs.py pairs_raw.json --r10-dir tools/listening/r10
```

`--r10-dir` points to the folder that holds `make_stimuli.py`. On the `main` branch that is `tools/listening/r10/`. On a branch without it, extract the file from `main` into a scratch folder and pass that folder.

The binary `make_pairs` is not committed. The output is deterministic: the same engine gives the same files.

## Stimulus conventions

- General MIDI program 0 (Acoustic Grand Piano), channel 0.
- 90 bpm, 4/4, velocity 80 on every note, 480 ticks per beat.
- One chord per bar, four bars per file (16 beats). Each chord lasts the whole bar.
- Root in the bass, upper voices above. Voicings come from the engine, not hand-written.
- A repeated pitch in a voicing is written once.

## The pairs

Each pair has the same length and tempo, and the files are named neutrally. The chords and key of each file are in `pairs.json`.

- **Register pairs (6 pairs, 3 in each key).** The two files have the same chord sequence. In one of them the top voices of each chord are an octave lower, so the mean upper register is about 7 to 9 semitones lower. The bass and the chord content do not change.
- **Colour pairs (6 pairs, 3 in each key).** The chord qualities change on the same roots (at least three of four chords change). The voicing is chosen so that the mean upper register is within 0.1 semitones of the other file, so the register is matched.

Within each pair the order of A and B alternates across the pairs, so that the base voicing is not always the first file.

## Listener part (can be shared)

Play each pair as two files, A then B, in the order the session sheet gives. Use the same device and listening level for everyone. Leave about one bar of silence between the two files.

Ask after each pair, reading the questions exactly:

1. "Which sounds brighter? A, B, the same, or unsure."
2. "Which would you use for a lighter, more open feel? A, B, the same, or unsure."

Do not explain what differs between the files. If the listener asks what is meant, repeat the question. If they still do not understand, record "unsure". Record any spontaneous comment.

## Operator notes (do not share)

- **Brightness gap.** The engine's Brightness mixes chord quality, colour within the key and register (audit H4). So the two pair types are checked separately. Colour pairs differ by 0.25 to 0.33 in engine Brightness. Register pairs differ by 0.08 to 0.10 in Brightness, which is the register term alone. The register pairs are therefore a test of the register part of the measure, not of a Brightness gap of 0.25.
- **Clipping.** Diminished, augmented and half-diminished chords sit at the clipping limits (0 or 1) in the engine (audit H3). They were excluded from the colour alternatives, and no colour candidate has a slot at 0 or 1.
- **Tolerance.** The 0.25 threshold and the 1-semitone register tolerance are the engine's numbers. They are not perceptual thresholds. The listening result decides whether they matter.
- **Search cap.** The colour search stops after 400 voicings per pair (`prescreened 14` in the probe's stderr is that cap: 14 candidates times 30 voicings). The probe picks the first candidate that meets both conditions within the cap. It is not the best candidate in the search space.
- **Chord vocabulary.** Colour changes draw from major, minor, sus2, sus4, maj7, m7, dominant 7, 6 and m6 chords on the same root.

## Check results

`write_pairs.py` re-reads each MIDI file and compares it with the probe: format, tempo, program, velocity, bar count and pitch classes of each bar. It recomputes Brightness from the MIDI notes with `make_pairs check`, and compares the mean upper register and the loop Brightness with the key. The last run passed all checks (see the output printed by `write_pairs.py`).

## Matched set (pairs 13 to 22)

The first twelve pairs cannot compare register and colour at equal size: register pairs differ by 0.08 to 0.10 engine Brightness and colour pairs by 0.25 to 0.33. The matched set fixes that. It is built by `make_pairs matched` and written with the same `write_pairs.py`:

```
make_pairs matched > matched_raw.json
python3 tools/listening/brightness/write_pairs.py matched_raw.json --json tools/listening/brightness/pairs_matched.json
```

(`pairs/pair13_A.mid` ... `pair22_B.mid`; operator key in `pairs_matched.json`. Regenerating the first set with the same code gives byte-identical files and an identical `pairs.json`.)

| Pairs | Type | What differs | Engine gap |
|---|---|---|---|
| 13-18 | `colour_small` | Chord qualities change on 2 to 4 of the 4 chords, upper register matched within 0.06 semitone. Alternates whether file A or B is the brighter one. | 0.075 to 0.105 (checked 0.081 to 0.104), the same size as a register pair |
| 19-22 | `conflict` | Register and colour point in opposite directions. One file has the brighter chord qualities about 7 to 8 semitones lower. The other has darker qualities at the original register. | Net gap within 0.03 (checked 0.002 to 0.005). The colour gap alone is 0.075 to 0.105 and the register gap about 0.09. |

How to read them. In `colour_small` and in the register pairs of the first set, the "brighter" answer should follow the engine if the engine's Brightness tracks what is heard, and the two types can be compared directly because the engine gap is the same. In `conflict` pairs the engine sees a tie, so the listener's choice shows which cue they weigh more: the higher register (register dominates) or the brighter chords (colour dominates). `pairs_matched.json` gives `lower_register_file` and `brighter_colour_file` for each conflict pair (by construction they are the same file).

Caveats.

- The colour changes are chosen by search, not by hand. Several of them also change the mode of a chord, for example Amaj7 against Am (pair 16), A against Am and Cm6 on the C degree (pair 17), or Cm and A in C major (pair 19). So "colour" here includes mode mixture and suspensions, and a result says "chord quality including mode changes", not a pure colour effect. A hand-checked set of subtler changes (maj7 and 6 added to the same triads) would isolate colour better; it was not built.
- Some loops are odd as harmony (for example Fm6 G C7 Asus4 in pair 20). Listeners may react to that, not to brightness. Record spontaneous comments.
- Same engine voicing, 90 bpm, piano, as the first set. The numbers are engine numbers, not perceptual thresholds.
- Ten more pairs add about 7 minutes to a session.
