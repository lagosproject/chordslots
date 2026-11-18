# R10: pairwise tritone listening test

Material for the R10 row in `docs/GENERAL-PLAN.md`. It is run in the same session as task 5 (producer listening). Perception only: no production experience is needed, and non-producers are fine.

## Contents

| File | Role |
|---|---|
| `make_stimuli.py` | Writes the MIDI stimuli to `stimuli/` and the operator key `pairs.json`. Standard library only. |
| `make_session_sheet.py` | Writes one CSV response sheet per participant. Standard library only. |
| `stimuli/r10_pairNN_{A,B}.mid` | The eight stimulus files (type 0 SMF). Give these to listeners. |
| `pairs.json` | Operator only. Chord content of each file, tritone flags, and the contrast each pair tests. Do not give it to listeners. |
| `sheets_seed<S>/P*.csv` | Generated per session. Operator only. |

## Pairs

| Pair | A | B | Length |
|---|---|---|---|
| pair01 | G7 | Gmaj7 | 2 beats, isolated |
| pair02 | Bdim | Bm7b5 | 2 beats, isolated. Both contain B-F, so this pair does not vary the tritone itself. |
| pair03 | G7 then C | F then C | 2 bars (4 beats each), cadence |
| pair04 | C, G7, C | C, G, C | 3 bars, I-V-I. Only the middle bar differs. |

Render settings for all files: General MIDI program 0 (Acoustic Grand Piano), 80 bpm, 4/4, velocity 80 on every note. Scientific pitch, C3 = MIDI 48. Every note lies in C3 to C5. The root is in the bass, with the other chord tones in close position above it.

The files contain no silence after the last note. Leave a gap of about one bar between the first and second file in playback so the listener can hear the return.

## How to run

1. Regenerate if needed:
   ```
   python3 make_stimuli.py
   python3 make_session_sheet.py --participants N --seed S
   ```
   `N` is the number of participants. Use an even number so every pair is heard equally often in each order. `S` makes the session reproducible. Sheets go to `sheets_seed<S>/`, one `P01.csv`, `P02.csv`, and so on.
2. Before the session, fill in the participant's production_experience (for example "writes music in a DAW", "listens only").
3. Run a practice example before the test, using a pair from outside this set, so the listener understands the task.
4. Play each trial's `first_file`, then `second_file`, in the sheet's order. Use the same playback device and listening level for everyone.
5. After each trial, read the two questions below and record the answer in `more_tense` and `asks_to_resolve`. Record any spontaneous comment in `notes`.
6. The sheet has five trials per participant. One of them (position random, from trial 3 on) repeats an earlier pair in reversed order, marked "repeat check of trial k" in `notes`. Compare its answers with trial k. Report how often the answers agree as a consistency figure, and decide how to use it when the data are in.
7. Fill in `training` on each row if a training session is given. Leave it blank otherwise.

## Question wording

Read these exactly, after each trial. Keep the answer options as listed.

1. **"Which of the two sounds more tense? A, B, the same, or unsure."**
2. **"Which of the two makes you want to hear something else next, more than the other? A, B, the same, or unsure."**

Do not say which chord is expected, which should resolve, or what either file contains. Do not mention a tritone, a dominant, or any theory term. If the listener asks what is meant, repeat the question. If they still do not understand, record "unsure".

## Notes

- This is a perception test. The answers are not a judgment of musical quality.
- Non-producers are included. Record production experience so responses can be compared by group.
- The `kTritoneWeight` decision in R10 depends on the pooled responses, not on any single participant.
- `sheets_seed<S>/` and any recordings made in the session should not be committed. The stimuli and `pairs.json` are deterministic and can be regenerated.
