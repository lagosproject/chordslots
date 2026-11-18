# R3: do both dissonance terms belong in Tension?

Measurement only. No file under `core/` or `tests/` was changed. Three copies of the headers were made with the dissonance split set to 0.5/0.5 (a, the default), 1/0 (b, roughness only) and 0/1 (c, harmonic only). Each copy was compiled with the audit engine (`docs/audits/engine.cpp`, tension mode) and scored with `docs/audits/analyze.mjs` against the study materials at the pinned revision `a5ca4a2` (the commit used by `docs/audits/REPORT.md`). Variant a reproduces the published values (static with voicing 0.686, contextual 0.665), so the pipeline is valid for the comparison.

**Caveat.** 11 progressions cannot separate small differences. The chord-level and progression-level numbers below describe how the model behaves, not which split listeners prefer.

## Patch

The 0.5/0.5 split is written in two places in `core/include/chordslots/Metrics.h`: `typeDissonance` (symbolic tension, `tensionOf`) and `voicedTension` (roughness from the sounding voicing). Patching only `typeDissonance` would leave the scored static-with-voicing and contextual values unchanged, because `voicedTension` has its own copy of the formula. Both sites were therefore patched with the same weights. `patch_weights.py` checks that each pattern matches exactly once, and `raw/patch_a.diff` is empty (the default copy is identical to the original). The diffs for b and c are in `raw/patch_b.diff` and `raw/patch_c.diff`.

## 1. Mean Pearson against the listener tensions (11 progressions, P3 excluded)

| Variant | Static with voicing (stable) | Contextual (stable) | Static symbolic (stable) | Static with voicing (late) | Contextual (late) |
|---|---:|---:|---:|---:|---:|
| a, 0.5/0.5 (default) | **0.686** (ref 0.686) | **0.665** (ref 0.665) | 0.677 | 0.851 | 0.850 |
| b, roughness 1/0 | 0.688 | 0.666 | 0.673 | 0.847 | 0.845 |
| c, harmonic 0/1 | 0.678 | 0.658 | 0.678 | 0.848 | 0.849 |

The per-progression values are in `raw/summary.md`. Their largest swing is P4 with static with voicing (0.645 in a, 0.552 in b, 0.734 in c).

## 2. Tension of the named chords, C major

| Chord | a, 0.5/0.5 | b, 1/0 | c, 0/1 |
|---|---:|---:|---:|
| Gmaj7 | 0.760 | 0.759 | 0.762 |
| G7 | 0.743 | 0.791 | 0.694 |
| Cmaj7 | 0.406 | 0.405 | 0.408 |
| Cm | 0.413 | 0.442 | 0.384 |
| Bdim | 0.755 | 0.843 | 0.667 |
| Bm7b5 | 0.787 | 0.844 | 0.729 |
| Caug | 0.467 | 0.480 | 0.453 |
| Cdim7 | 0.612 | 0.723 | 0.502 |

These are symbolic values (`tensionOf`, no voicing). The values for variant a match the R2 table in `docs/GENERAL-PLAN.md` (G7 0.743, Gmaj7 0.760).

## 3. Chord types whose Tension changes by more than 0.05

Counted over the 32 chord types on root C in C major, with `tensionOf`.

- a versus b: **5 of 32** (dim, m7b5, dim7, m6, 7b9).
- a versus c: **5 of 32** (dim, m7b5, dim7, m6, 7b9).

G7 (|Δ| = 0.048 in b, 0.049 in c) sits just under the threshold. The full table is in `raw/summary.md`.

## Reading

- Neither term can be dropped on the progression data. Mean Pearson moves by at most 0.008 across the three splits for both scored models, which is below what 11 progressions can resolve.
- The split does change chord tension where it matters most for the R2 question. Roughness only makes G7 sit above Gmaj7 (0.791 versus 0.759), harmonic only makes it sit below (0.694 versus 0.762), and the default is in between (0.743 versus 0.760). The sign of the G7/Gmaj7 gap therefore depends on the split, not only on the tritone weight.
- Dim, m7b5 and dim7 are the chords most affected: their tension falls by 0.06 to 0.11 in c and rises by 0.06 to 0.11 in b. The tritone term in R2 targets the same chords, so the two effects should be tested together.
- Given the data, the 50/50 default is kept. The question of whether both terms are needed is still open: the data do not support one term over the other.

Reproduce with `docs/audits/r3-dissonance/run.sh`. Study materials are cloned to the scratch directory (not committed).
