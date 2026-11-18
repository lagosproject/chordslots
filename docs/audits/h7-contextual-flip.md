# H7: effect of flipping `kUseContextualTension`

Measurement only. The constant stays `true` (`core/include/.../Metrics.h:305`). Core-only Release build, no plugin. Date: 18 November 2026.

## CTest

| Check | `true` | `false` |
|---|---|---|
| `core` | pass (157,626 checks, 0 failures) | pass (157,286 checks, 0 failures) |
| `eval` smoke (N=60, 128 metrics vs `tests/eval_smoke_baseline.json`) | pass, 0 failures | **fail, 2 guard failures** |

The 340 fewer checks with `false` come from the block at `core_tests.cpp:1767`, which is wrapped in `if constexpr (kUseContextualTension)` and is compiled out silently, with no skip message.

Smoke guard failures with `false` (guard: drop > 5 points):

- `Tense.s8.tonicPresentPct`: baseline 75.00, now 56.67 (-18.33)
- `Dreamy.s4.tonicPresentPct`: baseline 95.00, now 88.33 (-6.67)

At N=1000, `--compare` against `tests/eval_baseline.json` reports 0 failures in both configurations. The smoke failures are therefore at N=60, where sampling noise is larger. The N=1000 tonic change for Tense is -2.0 points at s8, so the N=60 drop probably mixes real change with noise; this was not separated.

## N=1000, per mood (false minus true)

68 of 128 metrics differ.

| Mood | Metrics changed (of 16) | Max abs delta | Largest deltas (false minus true) |
|---|---:|---:|---|
| Euphoric | 8 | 2.4000 | `s4.distinctPct` +2.4000, `s8.distinctPct` -0.7000, `s4.tonicPresentPct` -0.1000 |
| Uplifting | 9 | 4.1000 | `s4.distinctPct` -4.1000, `s4.tonicPresentPct` -0.9000, `s8.tonicPresentPct` -0.5000 |
| Epic | 8 | 7.2000 | `s4.distinctPct` -7.2000, `s8.distinctPct` -1.0000, `s4.tonicPresentPct` +0.1000 |
| Melancholic | 9 | 2.7000 | `s4.distinctPct` -2.7000, `s4.tonicPresentPct` -1.1000, `s8.tonicPresentPct` +0.1000 |
| Dark | 9 | 3.1000 | `s4.distinctPct` +3.1000, `s4.tonicPresentPct` -2.0000, `s8.tonicPresentPct` -0.5000 |
| Tense | 9 | 2.0000 | `s4.tonicPresentPct` +2.0000, `s8.tonicPresentPct` -2.0000, `s4.distinctPct` -0.3000 |
| Dreamy | 8 | 0.5000 | `s4.tonicPresentPct` -0.5000, `s4.distinctPct` -0.5000, `s8.meanOutsideTones` +0.0252 |
| Hard | 8 | 1.5000 | `s4.distinctPct` +1.5000, `s4.tonicPresentPct` -0.2000, `s8.distinctPct` +0.2000 |

## Full table

| Metric | Baseline (eval_baseline.json) | N=1000 true | N=1000 false | false minus true |
|---|---:|---:|---:|---:|
| `Euphoric.s4.tonicPresentPct` | 99.7000 | 99.7000 | 99.6000 | -0.1000 |
| `Euphoric.s4.distinctPct` | 35.6000 | 35.6000 | 38.0000 | +2.4000 |
| `Euphoric.s4.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Euphoric.s4.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Euphoric.s4.meanFit` | 0.8632 | 0.8632 | 0.8594 | -0.0038 |
| `Euphoric.s4.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Euphoric.s4.meanUniqueChords` | 4.0000 | 4.0000 | 4.0000 | +0.0000 |
| `Euphoric.s4.meanOutsideTones` | 0.0118 | 0.0118 | 0.0275 | +0.0157 |
| `Euphoric.s8.tonicPresentPct` | 100.0000 | 100.0000 | 100.0000 | +0.0000 |
| `Euphoric.s8.distinctPct` | 99.3000 | 99.3000 | 98.6000 | -0.7000 |
| `Euphoric.s8.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Euphoric.s8.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Euphoric.s8.meanFit` | 0.8368 | 0.8368 | 0.8469 | +0.0101 |
| `Euphoric.s8.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Euphoric.s8.meanUniqueChords` | 7.2680 | 7.2680 | 7.2860 | +0.0180 |
| `Euphoric.s8.meanOutsideTones` | 0.0226 | 0.0226 | 0.0348 | +0.0122 |
| `Uplifting.s4.tonicPresentPct` | 99.0000 | 99.0000 | 98.1000 | -0.9000 |
| `Uplifting.s4.distinctPct` | 37.0000 | 37.0000 | 32.9000 | -4.1000 |
| `Uplifting.s4.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Uplifting.s4.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Uplifting.s4.meanFit` | 0.8759 | 0.8759 | 0.8805 | +0.0046 |
| `Uplifting.s4.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Uplifting.s4.meanUniqueChords` | 4.0000 | 4.0000 | 4.0000 | +0.0000 |
| `Uplifting.s4.meanOutsideTones` | 0.0187 | 0.0187 | 0.0175 | -0.0012 |
| `Uplifting.s8.tonicPresentPct` | 99.5000 | 99.5000 | 99.0000 | -0.5000 |
| `Uplifting.s8.distinctPct` | 99.4000 | 99.4000 | 99.3000 | -0.1000 |
| `Uplifting.s8.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Uplifting.s8.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Uplifting.s8.meanFit` | 0.8496 | 0.8496 | 0.8516 | +0.0020 |
| `Uplifting.s8.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Uplifting.s8.meanUniqueChords` | 7.4300 | 7.4300 | 7.5860 | +0.1560 |
| `Uplifting.s8.meanOutsideTones` | 0.0253 | 0.0253 | 0.0278 | +0.0025 |
| `Epic.s4.tonicPresentPct` | 99.4000 | 99.4000 | 99.5000 | +0.1000 |
| `Epic.s4.distinctPct` | 35.8000 | 35.8000 | 28.6000 | -7.2000 |
| `Epic.s4.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Epic.s4.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Epic.s4.meanFit` | 0.8995 | 0.8995 | 0.9108 | +0.0113 |
| `Epic.s4.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Epic.s4.meanUniqueChords` | 4.0000 | 4.0000 | 4.0000 | +0.0000 |
| `Epic.s4.meanOutsideTones` | 0.0135 | 0.0135 | 0.0120 | -0.0015 |
| `Epic.s8.tonicPresentPct` | 99.8000 | 99.8000 | 99.8000 | +0.0000 |
| `Epic.s8.distinctPct` | 99.8000 | 99.8000 | 98.8000 | -1.0000 |
| `Epic.s8.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Epic.s8.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Epic.s8.meanFit` | 0.8498 | 0.8498 | 0.8462 | -0.0036 |
| `Epic.s8.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Epic.s8.meanUniqueChords` | 7.8430 | 7.8430 | 7.9350 | +0.0920 |
| `Epic.s8.meanOutsideTones` | 0.0286 | 0.0286 | 0.0307 | +0.0021 |
| `Melancholic.s4.tonicPresentPct` | 98.6000 | 98.6000 | 97.5000 | -1.1000 |
| `Melancholic.s4.distinctPct` | 48.9000 | 48.9000 | 46.2000 | -2.7000 |
| `Melancholic.s4.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Melancholic.s4.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Melancholic.s4.meanFit` | 0.8937 | 0.8937 | 0.8940 | +0.0003 |
| `Melancholic.s4.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Melancholic.s4.meanUniqueChords` | 4.0000 | 4.0000 | 4.0000 | +0.0000 |
| `Melancholic.s4.meanOutsideTones` | 0.0192 | 0.0192 | 0.0262 | +0.0070 |
| `Melancholic.s8.tonicPresentPct` | 99.8000 | 99.8000 | 99.9000 | +0.1000 |
| `Melancholic.s8.distinctPct` | 99.8000 | 99.8000 | 99.9000 | +0.1000 |
| `Melancholic.s8.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Melancholic.s8.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Melancholic.s8.meanFit` | 0.8545 | 0.8545 | 0.8559 | +0.0014 |
| `Melancholic.s8.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Melancholic.s8.meanUniqueChords` | 7.4440 | 7.4440 | 7.5350 | +0.0910 |
| `Melancholic.s8.meanOutsideTones` | 0.0256 | 0.0256 | 0.0325 | +0.0069 |
| `Dark.s4.tonicPresentPct` | 98.6000 | 98.6000 | 96.6000 | -2.0000 |
| `Dark.s4.distinctPct` | 73.4000 | 73.4000 | 76.5000 | +3.1000 |
| `Dark.s4.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dark.s4.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dark.s4.meanFit` | 0.8427 | 0.8427 | 0.8542 | +0.0115 |
| `Dark.s4.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dark.s4.meanUniqueChords` | 4.0000 | 4.0000 | 4.0000 | +0.0000 |
| `Dark.s4.meanOutsideTones` | 0.1005 | 0.1005 | 0.0985 | -0.0020 |
| `Dark.s8.tonicPresentPct` | 98.7000 | 98.7000 | 98.2000 | -0.5000 |
| `Dark.s8.distinctPct` | 100.0000 | 100.0000 | 100.0000 | +0.0000 |
| `Dark.s8.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dark.s8.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dark.s8.meanFit` | 0.8141 | 0.8141 | 0.8250 | +0.0109 |
| `Dark.s8.ababPct` | 0.1000 | 0.1000 | 0.0000 | -0.1000 |
| `Dark.s8.meanUniqueChords` | 7.5830 | 7.5830 | 7.5920 | +0.0090 |
| `Dark.s8.meanOutsideTones` | 0.1535 | 0.1535 | 0.1472 | -0.0063 |
| `Tense.s4.tonicPresentPct` | 52.8000 | 52.8000 | 54.8000 | +2.0000 |
| `Tense.s4.distinctPct` | 99.1000 | 99.1000 | 98.8000 | -0.3000 |
| `Tense.s4.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Tense.s4.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Tense.s4.meanFit` | 0.8420 | 0.8420 | 0.8543 | +0.0123 |
| `Tense.s4.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Tense.s4.meanUniqueChords` | 4.0000 | 4.0000 | 4.0000 | +0.0000 |
| `Tense.s4.meanOutsideTones` | 0.8253 | 0.8253 | 0.7255 | -0.0998 |
| `Tense.s8.tonicPresentPct` | 66.5000 | 66.5000 | 64.5000 | -2.0000 |
| `Tense.s8.distinctPct` | 100.0000 | 100.0000 | 100.0000 | +0.0000 |
| `Tense.s8.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Tense.s8.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Tense.s8.meanFit` | 0.8411 | 0.8411 | 0.8492 | +0.0081 |
| `Tense.s8.ababPct` | 0.0000 | 0.0000 | 0.1000 | +0.1000 |
| `Tense.s8.meanUniqueChords` | 7.9370 | 7.9370 | 7.9230 | -0.0140 |
| `Tense.s8.meanOutsideTones` | 0.8157 | 0.8157 | 0.8094 | -0.0063 |
| `Dreamy.s4.tonicPresentPct` | 97.6000 | 97.6000 | 97.1000 | -0.5000 |
| `Dreamy.s4.distinctPct` | 97.9000 | 97.9000 | 97.4000 | -0.5000 |
| `Dreamy.s4.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dreamy.s4.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dreamy.s4.meanFit` | 0.8722 | 0.8722 | 0.8805 | +0.0083 |
| `Dreamy.s4.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dreamy.s4.meanUniqueChords` | 3.9980 | 3.9980 | 4.0000 | +0.0020 |
| `Dreamy.s4.meanOutsideTones` | 0.2208 | 0.2208 | 0.2447 | +0.0239 |
| `Dreamy.s8.tonicPresentPct` | 98.6000 | 98.6000 | 98.6000 | +0.0000 |
| `Dreamy.s8.distinctPct` | 100.0000 | 100.0000 | 100.0000 | +0.0000 |
| `Dreamy.s8.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dreamy.s8.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dreamy.s8.meanFit` | 0.8683 | 0.8683 | 0.8671 | -0.0012 |
| `Dreamy.s8.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Dreamy.s8.meanUniqueChords` | 7.5480 | 7.5480 | 7.5440 | -0.0040 |
| `Dreamy.s8.meanOutsideTones` | 0.2824 | 0.2824 | 0.3076 | +0.0252 |
| `Hard.s4.tonicPresentPct` | 99.7000 | 99.7000 | 99.5000 | -0.2000 |
| `Hard.s4.distinctPct` | 18.5000 | 18.5000 | 20.0000 | +1.5000 |
| `Hard.s4.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Hard.s4.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Hard.s4.meanFit` | 0.8492 | 0.8492 | 0.8665 | +0.0173 |
| `Hard.s4.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Hard.s4.meanUniqueChords` | 3.9980 | 3.9980 | 3.9990 | +0.0010 |
| `Hard.s4.meanOutsideTones` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Hard.s8.tonicPresentPct` | 99.9000 | 99.9000 | 99.8000 | -0.1000 |
| `Hard.s8.distinctPct` | 98.8000 | 98.8000 | 99.0000 | +0.2000 |
| `Hard.s8.noBassSlotPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Hard.s8.lowIntervalPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Hard.s8.meanFit` | 0.8185 | 0.8185 | 0.8145 | -0.0040 |
| `Hard.s8.ababPct` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
| `Hard.s8.meanUniqueChords` | 7.2230 | 7.2230 | 7.3450 | +0.1220 |
| `Hard.s8.meanOutsideTones` | 0.0000 | 0.0000 | 0.0000 | +0.0000 |
