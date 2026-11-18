# R1 tension-hierarchy assertions

Assertions added to `testTensionHierarchy()` in `tests/core_tests.cpp` for task R1 of the [general plan](../GENERAL-PLAN.md) (finding H1). They use the static metric `tensionOf` (`kTritoneWeight = 0`), with C major as the key unless stated. Measured on 18 November 2026.

Textbook assertions that fail today are listed as **known failing**. They are printed and do not fail the build. A known-failing assertion that starts to pass is also printed, so the list can be trimmed. Debatable assertions are printed with their values and never fail.

## Textbook assertions

| Id | Claim | Tension (lhs vs rhs) | Result |
|---|---|---|---|
| TH1 | G7 > Gmaj7 (dominant seventh vs major seventh, same root) | 0.743 vs 0.760 | **known failing** (H1) |
| TH2 | G7 > G (V7 vs V triad) | 0.743 vs 0.639 | pass |
| TH3 | G7 > F (V7 vs IV) | 0.743 vs 0.529 | pass |
| TH4 | G7 > Dm7 (V7 vs ii7) | 0.743 vs 0.590 | pass |
| TH5 | Bdim7 > Bm7b5 (fully vs half diminished, same root) | 0.844 vs 0.787 | pass |
| TH6 | Bdim7 > Bdim (seventh added to diminished triad) | 0.844 vs 0.755 | pass |
| TH7 | G7 > C (V7 vs tonic triad) | 0.743 vs 0.207 | pass |
| TH8 | E7 > Am in A minor (V7 vs i) | 0.781 vs 0.211 | pass |

## Debatable assertions (never fail)

| Id | Claim | Tension (lhs vs rhs) | Result |
|---|---|---|---|
| DB1 | Bdim > Bm7b5 (diminished triad vs half diminished) | 0.755 vs 0.787 | does not hold |
| DB2 | Cmaj7 > C (I7 vs I) | 0.406 vs 0.207 | holds |

DB1 is the dim-versus-m7b5 pair that the task named. Whether a diminished triad should be tenser than a half-diminished seventh chord is a judgement for the reviewer with music-theory training. The 0.755 vs 0.787 margin is small.

## Known failing today

- **TH1** (G7 vs Gmaj7): 0.743 vs 0.760. This is the H1 finding, which the R1b run already reported (T1).

## Not done

- **Moving "vi as tonic" and "vii° as dominant" to debatable.** These assertions are not in the repository. The R1 list (44 assertions plus 3 debatable) and the R1b list (19 assertions) are only described in `GENERAL-PLAN.md`. Neither `tests/`, `docs/audits/` nor the CMake files contain them, and a search for `Gmaj7`, `Cdim` and `Caug` finds only the plan. Their source must be located before they can be moved.
- The existing check that G (V triad) is less tense than G7 is already in `testMetricOrdering` and is not duplicated here, although TH2 covers the same pair.
