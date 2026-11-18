# Task 3: how much do the mood presets differ, and what makes them differ

Measurement only. No constant or preset changed. Date: 18 November 2026. Engine at fdc1fd3 (`kUseContextualTension = true`). Probe `task3/t3.cpp`, driver `task3/t3run.sh`, raw output `task3/t3_400.txt`.

## Method

400 seeds per configuration, tonic C, 4 slots, Piano voicing, automatic style, each configuration generated with `roll`. A mood is the bundle: scale, Adventure, syncopation, corpus style (only Dreamy has a default style; the others follow the scale), and the Brightness, Tension and Energy target curves.

- **Difference between two moods** = Jensen-Shannon divergence (bits, 0 to 1) between their histograms of chord tokens (degree relative to the tonic × the five prior quality classes). Noise floor from splitting the seeds of one mood in halves: 0.003 to 0.011, and 0.027 for Tense (the widest distribution).
- **Component swap.** Start from mood X, replace one component with mood Y's, regenerate, and report the share of the X-to-Y gap that closes (`closed`; 1 = indistinguishable from Y, negative = moved away). Components: scale, Adventure, syncopation, style, Brightness curve, Tension curve, Energy curve, all three curves, and scale+Adventure+syncopation together.
- **Limit of the metric:** it sees only which chords are chosen. Energy changes velocity and voicing density, not the chords, so its share is 0 by construction. The mean actual Brightness, Tension and Energy of the output are reported next to each swap for that reason.

## Overlap between presets (baseline)

| Pair | Divergence | Reading |
|---|---|---|
| Melancholic, Hard | 0.084 | Chord content nearly the same (both Minor, 68 and 74 % minor chords) |
| Uplifting, Epic | 0.143 | Close |
| Epic, Hard | 0.164 | Close |
| Epic, Melancholic | 0.181 | Close |
| Euphoric, Dreamy | 0.198 | Close (Major against Lydian, both about 82 % mean Brightness) |
| Melancholic, Dark | 0.316 | Moderate |
| Dark, Hard | 0.354 | Moderate |
| Uplifting, Melancholic | 0.359 | Moderate |
| Dark, Tense | 0.411 | Moderate |

Euphoric differs from the minor moods by 0.9 to 1.0 (almost no shared tokens). All remaining pairs are above 0.4.

Mean actual Brightness / Tension / Energy per mood (the measured values of the output, not the targets): Euphoric 0.83 / 0.46 / 0.83; Uplifting 0.61 / 0.43 / 0.82; Epic 0.52 / 0.48 / 0.81; Melancholic 0.36 / 0.44 / 0.46; Dark 0.27 / 0.51 / 0.70; Tense 0.34 / 0.65 / 0.65; Dreamy 0.82 / 0.47 / 0.40; Hard 0.38 / 0.52 / 0.94. Measured Tension spans only 0.43 to 0.65 across the eight moods, and the chord tokens of Tense have the tonic in only 53 % of progressions (98 % to 100 % for the others).

## Where the difference comes from (share of gap closed)

| From to | Scale | Adventure | Brightness curve | Tension curve | All three curves | Scale+Adventure+syncopation |
|---|---|---|---|---|---|---|
| Dark to Tense | 0.31 | 0.17 | 0.24 | 0.08 | 0.21 | 0.48 |
| Tense to Dark | 0.38 | -0.23 | 0.21 | 0.28 | 0.44 | 0.54 |
| Epic to Euphoric | 0.52 | -0.01 | 0.35 | 0.09 | 0.30 | 0.53 |
| Euphoric to Epic | 0.62 | 0.01 | 0.04 | 0.00 | 0.05 | 0.60 |
| Epic to Uplifting | 0.00 | -0.08 | 0.86 | 0.02 | 0.97 | -0.09 |
| Uplifting to Epic | 0.00 | 0.02 | 0.67 | 0.09 | 0.96 | 0.03 |
| Euphoric to Uplifting | 0.80 | 0.00 | 0.01 | -0.00 | -0.00 | 0.80 |
| Uplifting to Euphoric | 0.69 | 0.00 | 0.12 | -0.07 | 0.18 | 0.68 |
| Melancholic to Dark | 0.52 | 0.08 | 0.34 | 0.02 | 0.44 | 0.55 |
| Hard to Epic | 0.00 | -0.06 | 0.78 | 0.25 | 0.87 | -0.05 |

Syncopation and style close 0 to 5 % in every case (they do not affect which chords are chosen here; style is only set by Dreamy). Energy closes 0 % by construction.

## What this says

1. **Euphoric against Epic and Uplifting is a scale effect.** Swapping the scale alone closes 52 to 80 % of the gap; the Brightness curves barely matter once the scale is Major. Uplifting and Epic are both Minor, so the scale does nothing between them.
2. **Epic and Uplifting differ only by the Brightness curve.** Swapping the Brightness curve closes 67 to 86 % and the three curves 96 to 97 %. Their Tension, Energy, Adventure and syncopation have no effect on the chords. Hard against Epic behaves the same (Brightness curve 78 %, all curves 87 %). Those three presets are one chord generator with different Brightness curves; what separates Hard is Energy (mean 0.94 against 0.81), which the chord metric does not see.
3. **Dark and Tense are separated mostly by scale (Phrygian against harmonic minor), then by the curves.** Scale, Adventure and syncopation together close 48 to 54 %, the three curves 21 to 44 %, together not all of the gap. Tense's tonic is present in only 53 % of progressions, against 98 % for Dark, which is probably the largest audible difference; the cause is the harmonic-minor out-of-scale chords with Adventure 0.65 (mean 0.83 outside tones per chord), not the curves.
4. **The Tension curve has the smallest, most inconsistent effect** (0 to 0.28, and slightly negative in two pairs), consistent with the task 2 finding that Tension is tracked to about 0.09 and that the achievable range across presets is narrow (0.43 to 0.65 in measured means).
5. **The Adventure effect is small and can have the wrong sign** (Tense to Dark, -0.23): it moves the chord vocabulary but not toward the other mood.

## What it does not tell

- Whether any pair that overlaps on chords is distinguishable by ear. Melancholic and Hard share chord content but differ by 0.47 in mean Energy (0.46 against 0.94), and Epic and Uplifting differ in Brightness by 0.09 in the means (0.52 against 0.61); whether that reads as a different mood is a perception question for listeners.
- Voicing and rhythm, which differ per mood (Energy, syncopation, comping) but are not in the chord metric.
- The effect of the progression's length (4 slots only), the tonic (C only), the voicing style (Piano only) and the style choice (Auto only).
- The first requirement of the task ("first equalise the performance") is met only for Energy by omission: the chord metric ignores Energy. A performance audit (velocity, density, rhythm) per mood is a separate measurement.

## Decision for the plan

The audit answers which presets overlap and what each setting contributes, so task 3 is closed as a measurement. **Open product question (needs the user):** the presets Uplifting, Epic, Hard and Melancholic form a cluster of Minor presets that differ little in harmony (divergence 0.08 to 0.36), and Epic and Uplifting differ only by the Brightness curve. If the presets are meant to sound different in harmony, their Tension curves, Adventure or scales would have to be redesigned; if they are meant to differ in intensity and colour only, the current state is consistent. I did not change any preset.
