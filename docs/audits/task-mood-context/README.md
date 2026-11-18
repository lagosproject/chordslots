# Per-mood strength of the minor-key diatonic Brightness context

Measurement and prototype, 18 November 2026. `kMoodDiatonicContext` (Generator.h) is **false**: with it off, every value is identical to before (core tests: 158,265 checks, 0 failures, `eval` baselines unchanged).

With the flag on, each preset uses `Mood::diatonicContext` instead of the global `kMinorDiatonicMajorContext`: Epic 0.75, Melancholic 0.75, Hard 0.40, all others 0. Only chords with a major third on bIII, bVI or bVII in a minor-third scale are affected, and only downwards (see the plan).

`probe.cpp` + `run.sh <true|false> <hard value> [seeds]` reproduce it (600 seeds, 4 slots, tonic C, each mood's own scale). Cells: major V or dominant / minor v / major bVII / tonic, % of chords. Corpus in minor: V 5-6.5, v 4-9, bVII 13-16, tonic about 26.

| Mood | Flag off | Flag on |
|---|---|---|
| Uplifting | 0.2 / 0.0 / 22.3 / 26.2 | unchanged |
| Epic | 0.0 / 17.8 / 20.9 / 25.2 | 3.5 / 4.5 / 15.5 / 26.4 |
| Melancholic | 0.0 / 17.7 / 0.5 / 26.9 | 0.0 / 8.0 / 8.5 / 29.7 |
| Hard (0.25 / 0.40) | 0.0 / 23.6 / 5.1 / 25.2 | 0.0 / 20.0 / 13.9 / 25.0 and 0.0 / 15.8 / 18.8 / 25.1 |
| Dark, Tense, Euphoric, Dreamy | | unchanged |

Hard at 0.40 is a compromise, not a fit: minor v stays above the corpus band either way and bVII overshoots slightly. It is a placeholder to listen to. This removes the Uplifting regression of the global value (tonic 26 to 40 %) because Uplifting keeps 0. Whether the result sounds Epic, Melancholic or Hard is a listening question; the flag stays off.
