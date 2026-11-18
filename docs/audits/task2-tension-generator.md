# Task 2: contextual vs static Tension as generator controls

Measurement only. No default constant changed. Date: 18 November 2026. Engine: `core/` at 786c507 (`kUseContextualTension = true` in the repo; the probe builds a header copy with the constant `true` and another with `false`).

## Method

Probe `task2/t2.cpp`, driver `task2/t2run.sh`, aggregation `task2/agg.py`, raw output `task2/t2_1000.txt`. 1000 seeds per scenario, tonic C, Adventure 0.3, Brightness target 0.5, Energy target 0.6, automatic style.

- **Scales:** Major and Minor. **Slots:** 4 and 8.
- **Tension target curves:** ascending (0.2 to 0.8), descending (0.8 to 0.2), resolving (rises to 0.8 at 75 % of the loop, then falls to 0.25), flat low (0.25), flat high (0.75).
- **Roll:** target curves written into the slots, then `roll`.
- **Single-slot edit:** baseline roll with flat target 0.45, then one slot's Tension target moved by +0.35 or -0.30, then `rerollSlots` of that slot only. Slot 1 (interior) and the last slot were edited separately.
- **Scoring:** both outputs are scored in both builds with the same two references: the static calculation with voicing (`voicedTension`) and the contextual one (`measuredContextualTension`). Neither generator is judged only by its own metric, but the contextual score is still the build's own objective when the constant is true, so that column is partly circular.
- Chosen chord and targets are the only differences between the two builds. The 13 uses of the constant also change the Epic and Hard curves, so the measurement uses the same flat or synthetic targets for both and does not use the mood presets (see the H7 report for those).

## Results

Roll, mean over the scale and slot-count scenarios (contextual on, then off):

| Curve | Target error, static score | Target error, contextual score | Correlation with target, static | Correlation with target, contextual | Peak in the right slot, static | Peak in the right slot, contextual |
|---|---|---|---|---|---|---|
| Ascending | 0.080, 0.078 | 0.083, 0.087 | 0.916, 0.929 | 0.920, 0.921 | 0.81, 0.80 | 0.81, 0.79 |
| Descending | 0.082, 0.084 | 0.083, 0.092 | 0.906, 0.916 | 0.912, 0.909 | 0.78, 0.75 | 0.79, 0.75 |
| Resolving | 0.083, 0.084 | 0.086, 0.091 | 0.886, 0.895 | 0.891, 0.885 | 0.84, 0.85 | 0.84, 0.85 |
| Flat low | 0.100, 0.098 | 0.102, 0.105 | n/a | n/a | n/a | n/a |
| Flat high | 0.105, 0.107 | 0.133, 0.146 | n/a | n/a | n/a | n/a |
| All | 0.090, 0.090 | 0.097, 0.104 | 0.903, 0.913 (directional curves) | 0.908, 0.905 | 0.81, 0.80 | 0.81, 0.79 |

Side effects (all scenarios, on then off): Brightness error against target 0.087 and 0.089; fraction of distinct chords 0.944 and 0.943; tonic present 72.1 % and 69.9 % (descending curve 81 % and 77 %); mean fit score 0.797 and 0.806 (flat high 0.766 and 0.794).

Last slot to first slot (target minus score, mean): the ascending curve asks for -0.60 and the generator gives -0.47 static, -0.43 contextual with the constant on, and -0.47, -0.41 with it off. The resolving curve asks for 0 and gives -0.04 in both builds. The flat curves give within 0.04 of 0 in both builds. The generator under-delivers the extreme jump by about a quarter in both builds, and the contextual build does not close it.

Single-slot edit, mean over scenarios (on, then off):

| Edit | Chord changed | Right direction (static score) | Right direction (contextual score) | Distance to the new target, static | Distance to the new target, contextual | Other slots' chords changed |
|---|---|---|---|---|---|---|
| Interior, +0.35 | 99.8 %, 99.8 % | 99.0 %, 99.2 % | 99.0 %, 99.2 % | 0.111, 0.121 | 0.136, 0.158 | 0 %, 0 % |
| Interior, -0.30 | 98.5 %, 97.2 % | 98.5 %, 96.8 % | 98.5 %, 96.5 % | 0.039, 0.043 | 0.052, 0.058 | 0 %, 0 % |
| Last, +0.35 | 99.5 %, 99.8 % | 98.5 %, 99.0 % | 98.8 %, 99.0 % | 0.112, 0.123 | 0.138, 0.159 | 0 %, 0 % |
| Last, -0.30 | 98.5 %, 97.0 % | 98.5 %, 96.8 % | 98.5 %, 96.8 % | 0.039, 0.042 | 0.054, 0.058 | 0 %, 0 % |

The neighbour's contextual Tension moves by about 0.01 after an edit in both builds, which is the momentum term. Neighbour static Tension does not move.

## What the data say

1. **Target tracking is the same.** The static target error is 0.090 in both builds. Differences between builds are at most 0.013 (flat high, contextual score) and most are below 0.005, on a 0 to 1 scale.
2. **The contextual build is slightly better in its own metric and slightly worse in the static one.** The contextual score error drops from 0.104 to 0.097 and the static correlation falls from 0.913 to 0.903. That is the circularity the plan warns about: each build is a little better at what it optimises. Neither is better on both references.
3. **Peak localisation barely changes** (81 % against 79 to 80 %). Both builds put the peak in the wrong slot in about a fifth of runs (not split by slot count here; the 20-seed smoke run suggested more misses with 8 slots).
4. **Single-slot editing is equally reliable.** More than 96 % of edits move Tension in the requested direction in both builds, and the contextual build lands slightly closer to the new target (0.111 against 0.121 for +0.35). Nothing else changes in the loop.
5. **Side effects are small and mixed.** The contextual build keeps the tonic a little more often (72 % against 70 %), has a lower fit score (0.797 against 0.806) and, for flat high targets, a notably lower fit (0.766 against 0.794).
6. **The last-to-first jump is the same in both builds.** The closure diagnostic does not distinguish them.

Statistical note: 1000 seeds give a standard error of about 0.001 to 0.002 on each mean error per scenario, so differences of 0.007 or more are probably not noise, but they are far below anything a listener could use. I did not compute confidence intervals per metric.

## Decision for the plan

Fits the second branch of the plan's criterion (investigate or gather more evidence), not the first (keep the context because it is better). The contextual model has no measurable advantage in generation, a small cost in the static reference, and 13 code paths. It also has no demonstrated disadvantage. **Recommendation: leave `kUseContextualTension = true` for now and do not spend work refining it.** The evidence that could change this is perceptual (tasks 5 and R10 with listeners), and flipping the constant also moves 68 of 128 `eval` baseline metrics (see [H7 report](h7-contextual-flip.md)). R8 stays open until a listening result exists, and the Epic and Hard curves should be decided in the same diff.

## Limits

- Synthetic flat and linear curves, not the mood presets; the presets also change the Epic and Hard curves when the constant changes.
- Two scales, tonic C, Adventure 0.3, Piano voicing only. No melody, no locks.
- The score references are the engine's own models. Whether any of them matches what a listener hears is the question of tasks 5 and R10.
- Only the Tension target is varied; Brightness and Energy are flat.

## R2 ablation: tritone weight inside the same probe

`task2/t2r2.sh`, raw output `task2/r2_500.txt`, 500 seeds, contextual constant true, `kTritoneWeight` 0, 0.04 and 0.06 (the reasoned range in the plan). Means over all scenarios:

| Weight | Roll target error, static | Roll target error, contextual | Roll fit score | Edit error to the new target, static | Edit error, contextual | Edit right direction |
|---|---|---|---|---|---|---|
| 0.00 | 0.090 | 0.097 | 0.797 | 0.075 | 0.096 | 98.7 % |
| 0.04 | 0.085 | 0.093 | 0.802 | 0.061 | 0.087 | 99.0 % |
| 0.06 | 0.083 | 0.090 | 0.805 | 0.052 | 0.081 | 99.0 % |

Brightness error, distinct-chord rate and tonic presence do not change (0.086 to 0.087, 0.945 to 0.946, 72 % to 72 %).

**Caveat that limits the reading.** The scorer is built from the same headers, so the tritone term is in both the generator's objective and the score. Part of the improvement is the scorer and the generator agreeing, not tracking getting better in an absolute sense. A fair test scores every run with a fixed weight (for example 0.05) or with the external listener data, which this probe does not do. The earlier sweep against the Navarro-Cáceres progressions (see the plan) is the independent evidence, and it moved Pearson by about 0.02 on 11 progressions.

**Conclusion.** Within the range 0.04 to 0.06 the term does no harm to tracking, Brightness, variety or tonic presence, and the edit control gets closer to its target. It does not tell which weight is right. The default stays 0. I did not measure how often V7, dim and m7b5 chords are chosen with the term on, which is the side effect a listener would notice first, nor the effect on `eval`.
