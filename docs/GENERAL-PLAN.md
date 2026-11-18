# ChordSlots general plan

Updated on 18 November 2026. This document gathers the state of the VST review, the order of work and the pending decisions. The linked reports keep the methods and results.

The [research base](RESEARCH-BASE.md) collects the evidence and references that guided the audits and design decisions.

This plan incorporates a [critical review of the music engine](#critical-review-of-the-music-engine) dated 18 November 2026. Its findings and tasks (R1–R9) are pending prioritisation and are checked against the literature, not only against the judgement of whoever develops the VST.

## Goal

Make every control of the VST have an understandable and predictable effect when editing a loop. We will use the engine and MIDI tests to measure what the product does, and external listening to decide which effects are perceived and which are worth controlling from the interface.

The scope of this plan is Harmony, its associated performance controls, the presets and the verification of the VST in a DAW.

## Current status

| Area | Done | Pending |
|---|---|---|
| **Tension** | Comparison with published listener responses. 11 progressions and 63 positions were used. The contextual model showed no consistent advantage over the static reference with voicing. | Test what context contributes when **generating and editing** chords. Circular closure has been measured as a diagnostic on existing sequences. [Report](audits/REPORT.md) |
| **Brightness** | Sweep across scales and single-slot editing. The harmonic score rose in 195 of 240 increases compared with an equivalent regeneration, also with a fixed centre. The actual register and the tension can vary when the chord changes. | Test **harmonic colour versus register** with listeners. Extend the editing check to configurations closer to everyday use when it is time to verify the VST. [Report](audits/local-edit/REPORT.md) |
| **Energy** | Audit with fixed chords in Piano, Close and Rootless. The velocity and density response was checked. The processor MIDI with simulated transport and the internal Play matched the SMF in 288 cases per route. | Confirm the integration in a DAW. The current sample gives no reason to recalibrate the bar. [Report](audits/energy/REPORT.md) |
| **Match and help** | Text diff reviewed. Match compares Brightness and Tension. The lane help describes the effect of each control. | See the tooltips in a real window. [README](../README.md) |
| **Moods** | Preset preview validated in the JUCE editor and in render. Auto, Free manual and Custom were verified. | The menu header now warns that a preset also runs a Roll (done, render checked). Check the menu in a DAW and measure whether the presets produce different results. [Check](audits/mood-check/check.cpp) |

The review recorded a Release build of the VST3 and five passing CTest tests. The interactive check in a host is still pending.

## Work order

Work on one main task at a time. If a task depends on external access, you can move on to the next one that is ready.

| Order | Task | It is considered done when… |
|---|---|---|
| **1** | **Close the Mood change. Done (18 November 2026): the header reads "... and generate a progression (also runs a Roll)"; the render fits and the mood check passes. The DAW check stays in task 4.** Add to the header that applying the preset replaces the curves and generates a progression. Review the text and the width of the menu in the render. | The warning is read before choosing and the Auto, manual and Custom checks still pass. Opening inside the DAW is recorded in task 4. |
| **2** | **Compare contextual and static Tension as generator controls. Measured 18 November 2026: [report](audits/task2-tension-generator.md). No measurable advantage either way (differences about 0.01 or less); recommendation: keep the constant and decide in R8 with listening.** Use the same baselines, seeds and targets for Roll and single-slot editing. Include ascending, descending and resolving curves, and examine separately the jump from the last slot to the first. | There is a reproducible report on target tracking, localisation of peaks and drops, and side effects. Both generators are compared with common scoring references. The report states whether to keep the context, investigate a specific failure or gather more evidence. |
| **3** | **Audit the differentiation of the moods. Measured 18 November 2026: [report](audits/task3-mood-differentiation.md). Chord-level only; Epic, Uplifting and Hard differ almost only by Brightness curve and Energy; the perceptual half is open.** Start with Dark / Tense and Epic / Euphoric / Uplifting. First equalise the performance and then, in separate tests, the scale and effective style or target curves. | We know what each group of settings contributes, which presets overlap under the conditions tested and which differences depend on the producer's perception. |
| **4** | **Verify the VST3 in a DAW.** Open the compiled version, review tooltips and menu, edit slots with varied curves, check locks and Undo, and capture MIDI on another track to compare it with Save and Drag Master under equivalent conditions. | The host, the version, the cases tested and the reproducible differences are recorded. It can be run as soon as there is access to a DAW. |
| **5** | **Carry out the Brightness listening.** Prepare pairs of harmonic change with matched register and of register change with the same chords. Collect perception and preference of use separately. | There are responses recorded per person and example, and a provisional decision on combined Brightness, Harmonic colour or separate controls. [Protocol and session sheet](CONTINUITY-PLAN.md#listening-protocol-with-producers) |

### Criterion for the next Tension audit

Separate the **model that chooses the chords** from the **model that scores the result**. Otherwise, each variant could come out favoured by its own metric. Evaluate both outputs with the same static calculation with voicing and the same contextual calculation, and report both results. Keep the current weights during the comparison. Treat the circular-closure discrepancy as an engine diagnostic, since the published responses do not cover that return of the loop.

## Critical review of the music engine

A reading of `Theory.h`, `Metrics.h`, `SpiralTension.h`, `Generator.h` and `Corpus.h` found points where the numerical output of the engine may differ from what a musician hears. The figures under "Evidence" come from a reproducible probe on `tensionOf` and `chordMetrics` (estimate per chord symbol, without voicing or context) and from `buildCandidates`. They do not replace an audit with the full generator. The sources are summarised in the [research base](RESEARCH-BASE.md#cross-check-added-by-the-critical-review).

### Findings

| Id | Finding | Evidence | Cross-check with the literature |
|---|---|---|---|
| **H1** | **Tension does not model tritone resolution or attraction between notes.** With functional chords the hierarchy is generally reasonable (18 of 19 conventional assertions pass in R1b). The failure is concentrated where the tritone should dominate. Tonal function is a per-degree table that ignores quality. | In C major, Gmaj7 (0.76) ranks above G7 (0.74), which is the only conventional assertion that fails. Cmaj7 and Cm score the same (0.41). ♭VII counts as a dominant. The [Tension audit](audits/REPORT.md) points in the same direction in two progressions, with very few positions: in P7 the human mean rises from 0.446 to 0.777 in D7/C→A7 and the engine goes down, and in P9 it rises from 0.602 to 0.818 in E→E7/D and the contextual one falls slightly. They are not enough to prove anything on their own. | Lerdahl and Krumhansl (2007) combine distance in tonal space, surface tension (dissonance) and **melodic attraction**. Here there are only the first two, in approximate form. Bigand, Parncutt and Lerdahl (1996) already separated dissonance, function and movement. |
| **H2** | **Partial overlap in dissonance.** Roughness (Sethares) and the TIV vector norm are added in equal parts and both depend on interval content. The R3 measurement shows them correlated but not redundant, so keeping both is reasonable. | `typeDissonance` = 0.5·roughness + 0.5·harmonic dissonance. Across the 32 types, Pearson 0.75 and Spearman 0.63. Dim7 is the largest disagreement (rank 29 in roughness and 5 in TIV). | Harrison and Pearce (2020) separate roughness, harmonicity and familiarity as distinct factors. It would be necessary to check whether here they contribute independent information. |
| **H3** | **The weights and calibrations are not fitted to data.** Brightness uses 0.55/0.25/0.20 and Tension 0.45/0.35/0.20. The `Calibration` constants were set by "inspecting the CLI table". | Five types saturate at 0 or 1 (R4, over 46,080 combinations): dim (85 %) and m7b5 (68 %) at 0, aug (85 %) and maj7#11 (15 %) at 1, and quartal (5 %) at 0. In addition, unrelated types tie at three decimals (sus4, madd9, m13, 7#9 and 9sus4 at 0.419). The [Brightness sweep](audits/REPORT.md) already observes the clipping: H=0 in 25 of 80 cases in major, 22 in Dorian and 21 in Lydian at the low end of Roll, and describes it as "metric clippings". | Eerola, Friberg and Bresin (2013) treat expression as a sum of cues with empirical weights. Farbood (2012) estimates from listener data how much each parameter contributes to tension. Temperley and Tan (2013) contradict a strictly increasing order along the line of fifths (Ionian above Lydian). |
| **H4** | **Brightness mixes three constructs and is circular with register.** It mixes chord quality, colour within the key and register. The Brightness target also sets the centre of the voicing (`57 + 14·targetB`), so register counts twice. | `centreForBrightness` in `Voicing.h` and `registerBrightness` in `Metrics.h`. The [audit](audits/REPORT.md) quantifies the effect: with the moving centre, the upper register rises by between 3.32 and 5.35 semitones on average between targets 0.1 and 0.9 in Roll, and with the fixed centre it changes between −0.70 and +1.35. | Smit et al. (2020) find effects of mode and mean pitch on the valence of cadences. This is the reason for task 5, which must also adjust the weights, not only decide the control. |
| **H5** | **The candidate pool and the prior contradict each other in minor.** With Adventure low, major V does not enter the Minor scale: the pool admits 0 out-of-scale notes below 0.2 and the Minor scale is natural Aeolian. The prior, on the other hand, scores V→i. In the minor cluster, V and V7 add up to about 6.5 % of the transitions, resolve to the tonic 65 % and 85 % of the time, and appear at least once in 94 of the 173 songs (54 %). | In A minor with Adventure 0.1: 66 candidates, with Em and without E or E7. At 0.3 both appear. R5 count over 17,589 transitions: V 3.67 %, V7 2.84 %, v 4.27 %, ♭VII 13.71 %. Per-song count from R5b: V in 58, V7 in 58, either of the two in 94, ♭VII in 114 and v in 64. The median of V or V7 is 10 % of the chords of those songs. | de Clercq and Temperley (2011), the basis of the RS200 corpus, treat rock harmony as a practice of its own and not as an extension of the classical one. A missing V can be consistent with the PopRock style and not with jazz, classical or a mood such as Epic. Vuvan and Hughes (2019) show that the expectation of V–I versus ♭VII–I depends on style. **Pending:** the R5 count is per transition, not per song. We still need to know how many minor songs use V or V7. |
| **H6** | **The transition prior is coarse and from a single genre.** It groups chords into 5 classes, so Imaj7→IV and I→IV share statistics. The "SoulRnb" cluster is defined by "≥30 % of extended chords". The data are pop and rock from 1958–1991 and the product targets producers (808, Hard, Dreamy). Minor has 173 annotations. The R6 measurement confirms that the grouping hides real differences (see results). | `QualityClass` (5 values) and `tools/build_corpus.py`. | Eitel et al. (2024) model sequences with PPM over richer symbols. de Clercq and Temperley (2011) limit the corpus to rock. Chordonomicon, already listed in the base, covers more genres. Review provenance and quality before using it. |
| **H7** | **The contextual model is active by default without evidence.** `kUseContextualTension = true` is used at 13 points of `Generator.h`: Epic and Hard curves, selection and alternative costs, `slot.tension`, fit score and refresh when splitting or removing slots. A block of `core_tests.cpp` is skipped if the constant is false. The code comment asks to change it only after blind listening, and the audit showed no advantage. | `Metrics.h` and `Generator.h`, definition of `moods()`. | This is the question of task 2. The constant and the curves should be decided together. |
| **H8** | **Implicit convention in the interval predicates, with no failures today.** Extensions above 12 are ignored (`hasMajorThird` looks for 4, not 16). With the current vocabulary there are no discrepancies in 32 types. Reduced modulo 12, quartal would change its quality class and maj7#11 would only change `hasDimFifth` (guard test added). Quartal is labelled as sus because it has a fourth and no third. Altered dominants are missing (7#5, 7b13, 7#11). | `Theory.h`. Checked with an independent verifier. A test is advisable before adding new types. |
| **H9** | **The generator almost never produces a dominant in minor keys, even when it is diatonic.** The corpus prior weighs between 0.005 and 0.016 per transition, whereas moving away from the Brightness and Tension targets costs on the order of 0.2. A major V is "bright" because of its quality, so it clashes with minor and dark moods. The user only gets V or V7 if they raise both Brightness and Tension of the last slot at once and Adventure ≥ 0.3. | Probe over 300 seeds and 4 slots in C minor. With the default targets, V/V7/V9 are 0 % of the chords at all Adventure values from 0.1 to 0.7. Mood Tense (harmonic minor, diatonic V): 0.1 %. Uplifting, Epic, Melancholic, Dark and Hard: 0 %. The corpus has V or V7 in 54 % of the minor songs and in about 6.5 % of the transitions. With the last slot at Brightness 0.7 and Tension 0.8, it appears in 13 % (Adventure 0.3) and 28 % (0.6), and ♭VII dominates. G has a unary cost of 0.204 versus 0.009 for the best chord. | This is an engineering matter, with no source. |

### Proposed tasks

They do not alter the order of tasks 1–5. R2 and R4 extend tasks 2 and 5 with the same reports and protocols. The suggested order is R1, R3, R5, R2/R4, R6, R7 and R8. R10 depends on listeners and is grouped with task 5.

| Id | Task | It is considered done when… |
|---|---|---|
| **R1** | **Music-theory conformance tests.** Turn facts from a standard harmony textbook into assertions: Roman numerals, tension hierarchy (a dominant with a seventh versus the same root with maj7), chord names by key, qualities. A person with music-theory training chooses the assertions. | There is a reviewed list of assertions, with the failing ones recorded and without yet requiring them to be fixed. |
| **R2** | **Function and attraction in Tension.** Test a tritone or leading-tone term and a function that depends on quality, within the comparison of task 2 and without touching the other weights. | The ablation is reported with the Navarro-Cáceres data and with the Barratt-Fitzgerald et al. set as a second external cross-check. |
| **R3** | **Collinearity in H2.** Compute the correlation between roughness and TIV dissonance across the 32 types and 12 roots, and compare the tension with only one of the terms. | There is a report that decides whether to keep both, merge them or drop one, with the correlation as justification. |
| **R4** | **Brightness weights and saturation.** Fit the weights with the pairwise responses of task 5 instead of setting them by hand. Review the saturation of Caug, Cdim and Cm7b5. | The weights have an uncertainty interval and there are no chord types saturated at the extremes without reason. |
| **R5** | **Dominant in minor (H5).** Count V and V7 in the minor songs of the corpus. Decide whether the candidate pool includes the harmonic minor notes for chords of dominant function, or whether it depends on style. | The decision is documented with that count, and the CLI candidate test reflects it. |
| **R6** | **Transition prior (H6).** Measure whether separating maj/maj7 and min/min7 changes the prior. Evaluate Chordonomicon for absent genres (only with the authors' permission). Evaluating Chordonomicon is parked (see Dependencies). Consider POP909 (MIT, 909 songs with qualities, order and key, 401 in minor) as a third source for the prior in minor; first the rights to the source songs must be settled. Rename the clusters if they are not styles. | The effect of each change is reported and the attribution remains in `DATA_LICENSES.md`. |
| **R7** | **Blind panel of judges.** Several people, including at least one with music-theory training, rate ChordSlots progressions, random progressions and real progressions from the corpus blind. | There is a table with inter-judge agreement and a conclusion about which conditions are distinguished. |
| **R8** | **Default value of the contextual model (H7).** Set `kUseContextualTension` and the Epic and Hard curves according to the result of task 2. | The constant and the curves are changed together in a single diff with the report linked. |
| **R9** | **Audit reports inside the repository.** Done in `e39b0ca`: the reports, scripts and tables are in [docs/audits](audits/README.md). The figures quoted in this plan (11 progressions, 63 positions, 195 of 240, 288 cases) were checked against the reports. The reports examined commit `7dc1ef0`. The core (`core/`) has not changed since then. | Done. The Windows scripts (`.cmd`, `.ps1`) have not been tested on Linux. |
| **R10** | **Pairwise test for the tritone term (needs listeners).** Isolated chords and short cadences with the same instrument, tempo and register: G7 versus Gmaj7, dim versus m7b5, V7 versus IV, V7 versus V. Questions: which sounds more tense, which asks to resolve, and no difference or I don't know. It is grouped with the session of task 5 to recruit only once, and admits listeners who are not producers. | There are responses per person and pair, and a `kTritoneWeight` weight chosen from them (probably between 0.04 and 0.06) or the decision to leave it at 0. The stimuli and the response sheet can be prepared before recruiting. |
| **R11** | **Dominant in minor (H9).** Test two options behind constants whose default value changes nothing: (a) Brightness that takes context into account for chords of dominant function in minor keys; (b) a cadence term that favours the dominant before the loop returns. Measure the proportion of V/V7 generated against the corpus and the sensitivity to the targets. | There is a table with the proportion of the V-family by mood and Adventure against the corpus 6.5 %, and the effect on the R1 and R1b assertions and on `eval`. Documented decision on whether the product should offer cadences by default or only on request. |

### Preliminary results of R1, R3, R4 and R5

Four read-only analyses were run on 18 November 2026 with scripts in the local workspace. None modified the repository. The R3 and R5 numbers were recomputed independently from their output files. The R4 ones match the initial probe.

| Id | Result | What is missing |
|---|---|---|
| **R1** | 44 assertions on spelling, Roman numerals with inversions, intervals of the chord types and tonal function: 44 pass and 3 debatable ones also pass. It is weak evidence: the expectations were written with the code in view and none covers Tension or Brightness, which is where the findings are. | Review by a person with music-theory training. Add tension-hierarchy assertions that would fail today (H1). Move vi as tonic and vii° as dominant to debatable. **Update:** 8 tension-hierarchy assertions and 2 debatable ones were added to `tests/core_tests.cpp` ([report](audits/r1-tension-assertions.md)); only TH1 (G7 vs Gmaj7, 0.743 vs 0.760) fails, which confirms H1, and the debatable Bdim > Bm7b5 does not hold (0.755 vs 0.787). The 44-assertion R1 list is not in the repository, so moving vi and vii° to debatable is still open. |
| **R3** | Roughness and TIV dissonance are correlated, not collinear (see H2). | Decide by listening whether it is worth adjusting the 50/50 split. |
| **R4** | Saturation limited to five chord types, plus ties between distinct types (see H3). | Adjust `brightLo` and `brightHi` or the weights in the full R4, with the responses of task 5. |
| **R5** | The minor cluster counts are in H5. They are transitions with repetitions collapsed, not weighted by duration, with the tonic inferred by heuristic. The raw files are in `chordslots-data` and allow the per-song count to be repeated. | Decide the candidate pool policy in minor and repeat the count at song level. |

### Preliminary results of R1b, R5b, R6 and the H7 and H8 audit

| Id | Result | What is missing |
|---|---|---|
| **R1b** | 19 conventional tension and brightness assertions: 18 pass and T1 fails (G7 below Gmaj7). Of three debatable ones, two fail: Caug at the Brightness ceiling and Cdim equal to Cm7b5 at the floor. The ordering of V7 versus IV, of Bdim versus Am and of E7 versus Am in A minor is correct. | Review by a person with music-theory training. The ♭VII versus V comparison was the agent's choice and does not count as evidence. |
| **R5b** | Reproduces the corpus (173 minor songs, 139 from Billboard and 34 from RS200, matching checksums). V or V7 appear in 94 songs. See H5. | Decide the candidate pool policy. |
| **R6** | Over 1,090 songs and 99,389 transitions, 34.6 % of the transitions involve a seventh or extended chord (39.6 % in Billboard and 12.1 % in RS200). When separating triad and seventh family, 9 of the 10 most frequent sources exceed 0.1 bits of Jensen-Shannon divergence (between 0.08 and 0.33) against a noise floor of 0.003 to 0.04. The highest are major V (0.325), major ♭III (0.275) and major ♭VII (0.235). | The noise floor assumes independent sampling between transitions. Songs cluster by style, so the real noise is larger. Repeat with per-song resampling before deciding. **Resolved later with a prediction test on unseen songs and POP909 as a third source (see "R6 progress"): separating does not improve across corpora, so the 5-class prior is kept.** |
| **H7** | List of the 13 points and of the effects on tests (see H7). The reading is static: the constant was not flipped and `eval` was not run, so it is unknown how much the baselines move. **Update:** measured, see the [report](audits/h7-contextual-flip.md). With the constant false, `core` passes (the contextual block is compiled out silently), N=1000 `eval --compare` has 0 failures, 68 of 128 metrics move (Epic s4 distinct -7.2 points, Uplifting -4.1, Dark +3.1), and the N=60 smoke test fails on Tense s8 and Dreamy s4 tonic presence. | Decide R8 with the result of task 2. |
| **H8** | No discrepancies in 32 types (see H8). | A test that fails if a type is added whose classification depends on the convention. |

### R2 progress: tritone term

The `r2-tritone-term` branch adds `kTritoneWeight` in `Metrics.h`, with value 0, and `hasChordTritone`, which marks chords with a tritone among their own notes (dominant sevenths, dim, m7b5, dim7; the ♯11 of maj7#11 does not count). With weight 0 the values do not change: the four core CTest tests, including `eval`, pass.

Sweep on a copy of the headers with the audit engine, reproduced on Linux with the same published tables (11 progressions, mean Pearson with the stable region):

| Weight | G7 versus Gmaj7 | Static with voicing | Contextual |
|---|---|---|---|
| 0.00 | 0.743 versus 0.760 | 0.686 | 0.665 |
| 0.04 | 0.783 versus 0.760 | 0.696 | 0.676 |
| 0.06 | 0.803 versus 0.760 | 0.699 | 0.681 |
| 0.10 | 0.843 versus 0.760 | 0.704 | 0.687 |

Pearson rises monotonically in the three models, but the increase is about 0.02 with only 11 progressions and one cohort. The data do not allow choosing the weight, and the uncapped increase up to 0.10 suggests that it is fitting these stimuli. For that reason the default value stays at 0. **Pending:** set the weight with a reasoned criterion (separate G7 from Gmaj7 by a difference comparable to the dissonance difference between the two, 0.04 to 0.06) and validate it with independent data before activating it. The Barratt-Fitzgerald et al. set was examined and is not suitable for this (see the [research base](RESEARCH-BASE.md)): its stimuli manipulate dissonance preparation and modulations, and there is no MIDI. The remaining options are a pairwise test with listeners on isolated chords and in cadences (G7 versus Gmaj7, dim versus m7b5) and review by a person with music-theory training. The change also moves Bdim, Cdim and the moods, so `eval` will have to be run again.

### R11 progress: Brightness context for the dominant in minor

The `r2-tritone-term` branch adds `kMinorDominantContext` in `Metrics.h`, with value 0, and `contextQualityBrightness`, which brings the quality term of a chord with a major third on degree 7 of a scale with a minor third closer to that of a minor triad. It is used in candidate selection and in the drawn Brightness. With 0 the values do not change and the four core CTest tests pass. With 1.0, 84 of 3,840 Brightness values change, all on root 7 with a major third, and none in Major, Lydian or Mixolydian. The Brightness of V drops from 0.80 to 0.44 and that of V7 from 0.62 to 0.39.

Proportion of V, V7 and V9 among the generated chords (300 seeds, 4 slots, C, each mood's own scale):

| Value | Tense (harmonic minor) | Epic | Uplifting | Melancholic | Dark | Hard |
|---|---|---|---|---|---|---|
| 0.00 | 0.1 % | 0.0 % | 0.0 % | 0.0 % | 0.0 % | 0.0 % |
| 0.50 | 1.7 % | 0.2 % | 0.3 % | 0.0 % | 0.0 % | 0.0 % |
| 0.75 | 5.2 % | 0.4 % | 0.1 % | 0.0 % | 0.0 % | 0.0 % |
| 1.00 | 9.2 % | 0.9 % | 0.0 % | 0.1 % | 0.0 % | 0.0 % |

With the default targets there is no change at any value. The corpus reference is about 6.5 % of the transitions. Tense at 0.75 reaches 5.2 % without moving ♭VII or the tonic by more than one point. **The change helps only Tense.** In the scales with a natural minor third (Epic, Uplifting, Melancholic, Hard) there is a second obstacle: the leading tone of V counts as an out-of-scale note and receives the Adventure penalty. In Epic, the last slot gives G a cost of 0.054 versus 0.005 for the best chord, and the out-of-scale-note term contributes about 0.034. Chord-by-chord comparisons with the same seed are not useful for measuring locality: the sampling is joint and a different cost changes the whole sequence.

**Option (c): leading tone as a scale note.** `kMinorLeadingToneInScale` in `Theory.h`, deactivated, makes the major third of a V-family chord not count as an out-of-scale note in scales with a minor third. With it activated, V and V7 enter the candidate pool at Adventure 0.1 and 0.15 (72 candidates instead of 66), which resolves the original inconsistency of H5. A test checks that it changes no other chord and no major scales.

Proportion of V, V7 and V9 with the leading tone activated (300 seeds, each mood's own scale; corpus ref.: about 6.5 %):

| Brightness context | Tense | Epic | Uplifting | Melancholic | Dark | Hard |
|---|---|---|---|---|---|---|
| 0.00 | 0.1 % | 0.8 % | 0.8 % | 0.0 % | 0.0 % | 0.2 % |
| 0.50 | 1.7 % | 3.2 % | 6.9 % | 0.0 % | 0.0 % | 6.2 % |
| 0.75 | 5.2 % | 5.0 % | 4.0 % | 0.0 % | 0.0 % | 16.4 % |
| 1.00 | 9.2 % | 9.2 % | 0.5 % | 0.8 % | 0.0 % | 18.7 % |

The tonic proportion stays within one point of the baseline in Tense, Epic and Uplifting at all values except one. No single setting serves all moods: **0.5** brings Uplifting, Epic and Hard closer to the corpus and leaves Tense low; **0.75** puts Tense, Epic and Uplifting in the band, but takes Hard to 16 %. Melancholic and Dark do not change. With the default targets there is no effect, and with the leading tone activated and the context at 1.0, the last slot with high targets produces V in more than 90 % of cases, which is excessive. **Status:** both constants remain deactivated. Activating them moves the `eval` baselines and the results of several moods, and the 6.5 % is a global corpus reference, not a per-mood target. Before activating (leading tone activated and context near 0.5) it is advisable that a person with music-theory training listen to progressions with and without the change, within the R10 session.

### R11 cross-check with a second corpus: POP909

To find out whether the 6.5 % of V/V7 in minor is an oddity of Billboard and RS200, the same thing was measured on **POP909** (909 pop songs with chords and key, 401 in minor; MIT licence in the repository). All figures are of the same kind: chords with consecutive repetitions collapsed, degree relative to the song's tonic, prior quality classes (major includes maj7 and maj6; minor includes min7; dom7 is 7; sus and aug stay apart). The key of each song is the one with the longest duration. It was repeated with the first key (407 songs in minor) and the figures change by less than 0.3 points. The POP909 figures were first computed by a subagent and then recomputed with a script of our own, with differences of one or two points that come from how the key is chosen. Billboard and RS200 use the same code as `tools/build_corpus.py`.

| Songs in minor | Billboard (139) | RS200 (34) | Billboard + RS200 (173) | POP909 (401) |
|---|---|---|---|---|
| Major V or V7, % of chords | 6.0 | 8.1 | **6.5** | **5.2** |
| Minor v, % of chords | 4.9 | 2.9 | 4.4 | 9.1 |
| Major ♭VII, % of chords | 13.2 | 14.9 | 13.6 | 15.5 |
| Among the degree-7 chords: major or dominant / minor | 55 / 45 % | 74 / 26 % | 60 / 40 % | 36 / 64 % |
| Songs with major V or V7 at least once | 53 % | 59 % | 54 % | 77 % |
| Songs with ♭VII | 65 % | 71 % | 66 % | 95 % |

Reference in major (V or V7): 16.2 % in Billboard and RS200, 19.4 % in POP909.

**What is concluded.** (1) The 6.5 % is not an oddity of one corpus: POP909 gives 5.2 %, so the reasonable reference in minor is **5 to 6.5 %** of the chords. (2) V remains a minority in minor and ♭VII is the most frequent high-degree chord in both (13 to 16 %). That agrees with de Clercq and Temperley's (2011) description of rock in minor and with the idea of Nobile (2016, cited in an automatic review; the source was not read) of a "VI-based" minor where ♭VII and IV assume dominant function, and it is what would be expected from a natural minor scale, not a classical one. (3) In POP909 degree 7 is minor (v) in almost two out of three cases, so the improvement does not consist of putting in more major V. (4) In the cross-check, **0.75 with the leading tone activated is too much**: Hard reaches 16.4 % of V-family, more than double that of any corpus. With 0.5, Uplifting (6.9 %), Epic (3.2 %) and Hard (6.2 %) end up in the band or below. This confirms the recommendation of 0.5 if it is activated, without changing the fact that the constants remain deactivated.

**Limits.** POP909 consists of piano arrangements of Chinese pop songs, not of producers' genres. Its keys come from audio and may confuse relative major and minor (148 songs change key). The definition of "song in minor" for Billboard and RS200 is that of `build_corpus.py` and for POP909 it is the annotated key, so comparisons of one point are not conclusive. **Pending:** measure the proportion of minor v and of ♭VII in the generator per mood, which was not measured; without that it is not known whether the generator gets the minor degree 7 or ♭VII right, only that it lacks major V. The scripts were left in the session scratchpad and are not in the repository.

### Generator measurement against the corpus in minor (major V, minor v, ♭VII)

Pending from the previous cross-check. 600 seeds per mood were generated (4 slots, tonic C, each mood's own scale, repetitions collapsed) and each chord was classified by its intervals (with a major third and minor seventh: dominant; with a major third without it: major; with a minor third: minor). The baseline (default constants) and the two variants with the leading tone activated were compared. Corpus reference in minor: major V or V7 5.2 to 6.5 %, minor v 4.4 to 9.1 %, major ♭VII 13.6 to 15.5 %, tonic about 26 %, and the whole degree 7 between 11 % (Billboard and RS200) and 14 % (POP909).

Moods with a minor-third scale, baseline:

| Mood | Scale | Major/dom V | Minor v | Major ♭VII | Tonic | Total degree 7 |
|---|---|---|---|---|---|---|
| Uplifting | Minor | 0.2 % | 0.0 % | 22.3 % | 26.2 % | 0.2 % |
| Epic | Minor | 0.0 % | 17.8 % | 20.9 % | 25.2 % | 17.8 % |
| Melancholic | Minor | 0.0 % | 17.7 % | 0.5 % | 26.9 % | 17.7 % |
| Hard | Minor | 0.0 % | 23.6 % | 5.1 % | 25.2 % | 23.6 % |
| Dark | Phrygian | 0.0 % | 5.2 % | 0.0 % | 25.7 % | 5.2 % |
| Tense | Harmonic minor | 8.5 % | 6.8 % | 0.0 % | 13.2 % | 15.3 % |

With the leading tone activated (major/dom V, minor v): context 0.5, Uplifting 6.4 and 0.0, Epic 3.8 and 16.0, Hard 7.7 and 22.7, Tense 11.1 and 6.4; context 0.75, Uplifting 3.9 and 0.0, Epic 6.4 and 13.8, Hard 17.3 and 22.0, Tense 18.0 and 5.0. The other moods barely change. Euphoric (Major) has 24.3 % of V, above the 16 to 19 % of the corpus in major.

**What is concluded.** (1) The problem is not that degree 7 is missing, but that **degree 7 is almost always minor**: Epic, Melancholic and Hard generate between 17.7 and 23.6 % of minor v, two to five times the corpus (4.4 to 9.1 %), with a total for degree 7 above the 11 to 14 % of the corpus. (2) The tonic proportion (25 to 27 %) matches the corpus. (3) ♭VII, the most characteristic chord of minor in the corpus, appears little in Melancholic (0.5 %), Hard (5.1 %), Dark and Tense (0 %), and appears somewhat more than normal in Uplifting and Epic (21 to 22 %). Part of this may be intentional, because a dark mood chooses minor chords, but it has not been checked with listeners. (4) Option (c) **adds V without removing v**: in Hard the total for degree 7 goes from 23.6 % to 30.4 % with 0.5 and to 39.3 % with 0.75, and what gives way is ♭VII, not v. In Uplifting, on the other hand, 0.5 yields 6.4 % of V and no v, which is the corpus band. So the recommendation becomes more nuanced: **activating the leading tone with context 0.5 clearly helps only Uplifting (and somewhat Epic)**; for Hard and Epic the defect is the excess of minor v, and this option does not resolve that.

**Correction to an earlier figure.** The R11 table says that Tense generates 0.1 % of V-family in the baseline. That count included only V, V7 and V9 without alterations. Tense generates mostly altered dominants (V7♯9 4.2 %, V7♭9 4.2 %, V7sus4 1.7 %), which add up to the 8.5 % of this count. In the corpus those variants are grouped in the dominant class, but their alterations cannot be checked with the 5-class prior. The figures in the earlier V-family tables are therefore "plain V", and this section uses "any major or dominant V".

**Pending.** Decide whether it is advisable to reduce minor v. The mechanism is in the next section. It is a product decision and must be heard before changing anything. The scripts were left in the session scratchpad.

### Mechanism behind the excess of minor v and the shortage of ♭VII

For each slot of Epic, Melancholic, Hard and Uplifting (C minor, without sampling), the unary cost of the key chords (`dB² + dT²` plus the Adventure penalty) and their position in the ranking were listed, and the slot in which each chord comes out was counted over 600 seeds. The earlier hypothesis (low target Brightness) was incomplete: the factor is the **combination of low Brightness and high Tension in the last slot**.

- **The last slot asks for "dark but tense".** The curves of Epic, Melancholic and Hard end with Brightness 0.28 to 0.45 and Tension 0.60 to 0.64. Gm (v) has Brightness 0.33 and Tension 0.65, almost exactly that: it is first in the ranking in those three moods (cost 0.005 to 0.015). The Tension of v is high because the attraction term treats the root of degree 7 as a dominant whatever its quality. G (V) has Brightness 0.80, and G7 has 0.62 with Tension 0.78, so both are far away (rank 17 to 144; cost 0.06 to 0.2).
- **Result in the sampling.** Minor v in the last slot: Epic 71 %, Melancholic 58 %, Hard 36 % (and another 51 % in slot 2 of Hard, again with Brightness 0.35 and Tension 0.54). Almost all the excess of minor degree 7 comes from those slots.
- **♭VII is penalised by its major quality.** B♭ major has Brightness 0.67 regardless of being diatonic in minor. With targets of 0.22 to 0.45 (Melancholic, Hard) it costs 0.05 to 0.3 and almost never wins (0.5 % and 10 % in the last slot). It only comes out when the target is high: Epic 61 % in slot 2 (target Brightness 0.66) and Uplifting 62 % in slot 3. The same happens with ♭VI and ♭III (Brightness 0.57 and 0.62).
- **It is the same shortcoming as H9, enlarged.** The Brightness of `qualityBrightness` is independent of the key: a major chord is always bright. `contextQualityBrightness` corrects only degree 7. The diatonic major chords of a minor key (♭III, ♭VI, ♭VII) have the same problem, and ♭VII is the most frequent high chord of the corpus in minor (13 to 16 %). That fits the discussion of Brightness as a context-independent measure (H3, H4) and is consistent with Temperley and Tan (2013), who show that perceived brightness depends on the mode of the scale (not only on the quality of each chord); their study is of modes, not of chords, so it is indirect support.

**Limits.** The costs above are unary and use the static Brightness and Tension, without the context or the joint sampling, so they give the order, not the exact probabilities; the pattern matches the real sampling. No correction was tested. **Options to decide** (none implemented): (a) extend `contextQualityBrightness` to ♭III, ♭VI and ♭VII in minor scales, which would lower their Brightness and let them compete with v; (b) lower the Tension of minor v when major V is not available; (c) change nothing and accept that dark moods resolve to v. It is a sound decision, so it goes with the R10 listener test and task 5 (Brightness).

### Prototype of option (a): Brightness with context for ♭III, ♭VI and ♭VII

`kMinorDiatonicMajorContext` in `Metrics.h`, with value 0 (deactivated), generalises `contextQualityBrightness`: a chord with a major third on ♭III, ♭VI or ♭VII, in a scale with a minor third and with its triad (root, major third and fifth) inside the scale, brings the quality term closer to that of a minor triad, just as is already done with V. The harmonic minor scale has no ♭VII, so ♭VII does not count in it, and scales with a major third do not change. With 0 the values do not change: the core tests pass (157,626 checks, 0 failures) and `eval` gives 128 metrics with no differences. The new tests check that the default value is the identity across the 32 types, 12 roots and all scales, and that with 1.0 only chords with a major third on degrees 3, 8 and 10 of scales with a minor third change, and only downwards.

**Measurement.** 600 seeds per mood, 4 slots, tonic C, each mood's own scale, repetitions collapsed, same script as the previous section (the baseline reproduces its figures). Corpus reference in minor: major V or dominant 5 to 6.5 %, minor v 4 to 9 %, ♭VII 13 to 16 %, tonic about 26 %. Each cell is V / v / ♭VII / tonic in %.

| Mood | Baseline | Context 0.5 | Context 0.75 | 0.5 + leading tone | 0.75 + leading tone |
|---|---|---|---|---|---|
| Uplifting | 0.2 / 0.0 / 22.3 / 26.2 | 0.2 / 0.0 / 21.0 / **34.0** | 1.1 / 0.2 / **7.5** / **39.9** | 5.6 / 0.0 / 17.0 / **34.8** | 16.9 / 0.2 / 2.9 / 40.8 |
| Epic | 0.0 / 17.8 / 20.9 / 25.2 | 1.1 / 9.5 / 19.8 / 25.8 | **3.5 / 4.5 / 15.5 / 26.4** | 10.9 / 5.9 / 17.5 / 25.2 | 18.8 / 1.7 / 17.7 / 25.8 |
| Melancholic | 0.0 / 17.7 / 0.5 / 26.9 | 0.0 / 13.4 / 3.6 / 27.0 | 0.0 / **8.0** / 8.5 / 29.7 | = (V does not enter) | = |
| Hard | 0.0 / 23.6 / 5.1 / 25.2 | 0.0 / 14.3 / 20.6 / 25.0 | 0.0 / 14.0 / 24.0 / 24.5 | 0.0 / 14.5 / 20.5 / 25.0 | 0.0 / 13.9 / 24.1 / 24.5 |
| Dark (Phrygian) | 0.0 / 5.2 / 0.0 / 25.7 | 0.0 / 3.9 / 0.0 / 25.1 | 0.0 / 3.4 / 0.0 / 23.3 | = | = |
| Tense (harmonic minor) | 8.5 / 6.8 / 0.0 / 13.2 | 7.9 / 6.4 / 0.0 / 12.1 | 7.8 / 6.7 / 0.0 / 11.7 | = | = |

(Intermediate values 0.25 and 1.0 are in the session outputs; with 1.0 Uplifting almost does not generate ♭VII, 0.2 %, and the tonic rises to 42.5 %.)

**What improves.** (1) **Epic** and **Melancholic**: with 0.75 the minor v drops from 17.8 and 17.7 % to 4.5 and 8.0 %, within the corpus band, and ♭VII rises to 15.5 % (Epic, in the band) and to 8.5 % (Melancholic, from 0.5 %). The tonic holds (26.4 %) in Epic and rises three points in Melancholic. This is the effect expected from the mechanism: the "dark and tense" last slot stops resolving to Gm because B♭ and A♭ now compete. (2) **Dark** lowers v somewhat (5.2 to 3.4 %) and still does not generate ♭VII (major B♭ is not diatonic in Phrygian, whose degree ♭7 chord is B♭m; Dark generates 29 % of degree ♭7 chords, almost all minor, see the [Dark report](audits/dark-bVII/README.md)). (3) **Hard** reduces v from 23.6 to 14 %, but ♭VII goes from 5.1 % to 20 to 24 %, above the corpus (13 to 16 %), and v remains high.

**What gets worse.** **Uplifting**: with 0.5 the tonic rises from 26 to 34 % and with 0.75 to almost 40 %, and ♭VII falls to 7.5 %. Uplifting asks for high Brightness in the last slots and ♭VII was its natural bright chord; when its Brightness is lowered, the generator goes back to the tonic. That is, the curves of the minor moods were written with Brightness without context and the change shifts them differently depending on the mood. **Tense** barely changes, although it loses one point of tonic.

**Combination with the leading tone** (`kMinorLeadingToneInScale`). They add up without interfering, because they affect different chords (V versus ♭III, ♭VI and ♭VII). With 0.5 and leading tone, Uplifting reaches V 5.6 % without v and ♭VII 17 % (the best of its results, although with the tonic at 34.8 %) and Epic reaches V 10.9 % and v 5.9 %, with V somewhat above the band. With 0.75 and leading tone, Epic reaches V 18.8 % and Uplifting 16.9 %, two to three times the corpus. With a crude measure (sum of points out of band in V, v, ♭VII and tonic, over the six minor moods) the best value without leading tone is 0.75 (95.3 versus 129.3 for the baseline) and with leading tone it is 0.5 (91.9 versus 124.1). That sum is dominated by Dark and Tense, which do not change, and mixes metrics from different scales, so it is only useful for ordering.

**Conclusion.** **There is no single global value that is good for all moods.** The context of 0.75 leaves Epic and Melancholic almost at the corpus and is a clear improvement, but it harms Uplifting, and Hard is left with the excess of ♭VII. If it is activated, the clean way out is for the value to depend on the mood (for example 0.75 in Epic and Melancholic, 0 in Uplifting, 0.5 in Hard) or for the Uplifting curves to be adjusted at the same time. That adds a parameter per mood, and it is a product decision that depends on how the moods sound, not on the corpus. **Needs listening:** (i) whether Epic and Melancholic with ♭VII and ♭VI in the last slot sound "epic" or "melancholic" rather than neutral; (ii) whether Hard with so much ♭VII still sounds hard. The constant stays at 0. **Cross-check with the bibliography:** de Clercq and Temperley (2011) describe ♭VII as the characteristic chord of rock in minor and Nobile (2016, not read) a "VI-based" minor where ♭VII and IV act as dominant. Temperley and Tan (2013) indirectly support that brightness depends on the mode and not only on the chord, but their study is of modes in melodies, not of chords. No study reviewed measures how much darker B♭ major sounds inside C minor than outside it, so the value of the context (0.5 or 0.75) comes from the corpus proportions and not from perceptual data. The scripts are in `/home/vant/chordslots-data/session-scripts/` (`probe2.cpp` and `run.sh` from this session were copied there).

### R6 progress: separating maj/maj7 and min/min7 with POP909 as a third source

Measurement only: POP909 is not used in the prior and is not distributed (the raw data are in `chordslots-data/raw/pop909`, outside the repository, and the rights to the source songs are still not settled). It was measured with the code of `tools/build_corpus.py` for the 1,090 songs of Billboard and RS200 (degree and quality tokens, repetitions collapsed, "extended" flag of that script) and with the same rule for the 909 of POP909 (chords from `chord_midi.txt` passed through `harte_chord`; key of longest duration). The question changed with respect to the earlier R6 measurement (Jensen-Shannon divergence against a noise floor): now the question is whether separating the **context** (the source chord) into plain versus seventh improves the prediction of the next chord in **unseen** songs. The model is that of the prior (transition matrix smoothed with the marginal, beta 200); the separated model is smoothed with the non-separated one as a fallback, and the error is the cross-entropy in bits per transition, with a per-song bootstrap interval.

Rate of "extended" chords among major and minor tokens: Billboard 8 % of the majors and 44 % of the minors; RS200 1.5 % and 9 %; POP909 5.3 % and 13 %. **The rate depends heavily on the source and on the annotation convention**, not only on the music.

| Test (gain in bits per transition; positive = separating improves) | Gain | 95 % CI |
|---|---|---|
| 10-fold validation, Billboard+RS200 | −0.008 | [−0.020, +0.002] |
| 10-fold validation, POP909 | +0.013 | [+0.010, +0.017] |
| Billboard+RS200, minor only / major only | −0.061 / +0.004 | [−0.110, −0.017] / [−0.005, +0.012] |
| POP909, minor only / major only | +0.007 / +0.014 | [−0.001, +0.014] / [+0.008, +0.020] |
| Train on Billboard+RS200, test on POP909 | **−0.104** | [−0.113, −0.094] |
| Train on POP909, test on Billboard+RS200 | **−0.049** | [−0.063, −0.038] |
| Train on Billboard, test on RS200 / the other way round | +0.005 / −0.164 | [−0.019, +0.027] / [−0.185, −0.145] |
| Cross with beta 800 (more smoothing) | −0.014 and −0.023 | [−0.021, −0.008] and [−0.037, −0.013] |

**What is concluded.** Separating improves somewhat within POP909 (+0.013 bits) and worsens within Billboard+RS200, and **across corpora it worsens in every case** (the only exception, from Billboard to RS200, is zero within the interval). With more smoothing the loss shrinks, but it never becomes a gain. It is consistent with the "triad or seventh" distinction reflecting mostly how each source annotates (Billboard often annotates sevenths, POP909 comes from automatic extraction on piano arrangements, RS200 uses Roman numeral notation) and not a transferable transition rule. The literature points in the same direction: Koops et al. (2019, *Annotator subjectivity in harmony annotations of popular music*, J. New Music Research; not read in full) find disagreements between annotators mostly in chord extensions, and Humphrey and Bello (2015) discuss the effect of the chord vocabulary on estimation systems. **Decision:** do not separate maj/maj7 or min/min7 in the prior; the 34.6 % of transitions with a seventh is kept as the warning of H6 (the prior mixes conventions), but the separation is not justified with these data. Whether the generator chooses maj7 versus major still depends on Brightness and Tension, not on the prior. **Limits:** the cross output is domain against domain (the cross errors are 3.8 to 4.0 bits versus 3.3 within the corpus), only the separation of the source chord was tested, not that of the target, and POP909 consists of piano arrangements of Chinese pop songs. The script is `r6_split.py` in `session-scripts`.

### Decision on the Minor presets and follow-up work (18 November 2026)

**Decision (the user delegated it, judged from the literature and common practice, not from new listening):** the Minor presets should differ in harmony *moderately*. Music-emotion research (Eerola, Friberg and Bresin 2013; Gabrielsson and Lindström 2010, cited from memory and not re-read this session) finds mode, tempo, loudness and register carry most of the perceived mood, so intensity and colour are legitimate differences. But producers also expect genre-typical vocabularies: epic loops lean on bVI and bVII, melancholic ones on iv and v, and the corpus shows bVII as the commonest upper-degree chord in minor. Today Epic, Melancholic and Hard differ little by chord (Melancholic and Hard 0.084 bits). The step taken is a per-mood strength for the diatonic Brightness context, `kMoodDiatonicContext` (off), see [report](audits/task-mood-context/README.md). Uplifting keeps 0 so it does not collapse to the tonic. Turning it on needs listening (R10 session).

**Tritone side effects** ([report](audits/task2/tritone-sidefx.md), Haiku probe, baseline cross-checked against the task 3 numbers): with weight 0.04 to 0.06 only Tense and Dark change visibly (Tense dim and dim7 12.5 to 14.9 %, dominant 17.8 to 20.4 %; Dark dim and dim7 3.5 to about 6 %). Note: `hasChordTritone` is also true for m6 and m6/9 (minor third plus major sixth), which the code comment does not say.

**R10 stimuli** are in `tools/listening/r10/` (8 MIDI files, session-sheet generator). Pair 2 (Bdim vs Bm7b5) does not isolate the tritone, since both contain it; it tests the added seventh, and the plan wording should be read that way.

### How to validate without depending on a single person

Whoever develops the VST is not a professional producer, and it is not necessary to be one for most of the tests. It is advisable to separate two questions:

- **Perception** (does it sound more tense or brighter?). Bigand and Poulin-Charronnat (2006) review that untrained listeners perceive tension, expectation and structure in a similar way to musicians. These tests admit non-producer listeners, which makes recruiting easier. Lerdahl and Krumhansl (2007) and Farbood (2012) collected their data from listeners with designs of that kind.
- **Use** (which control would I want to have when editing a loop? does this preset sound like its name?). Here production experience does matter. It is advisable to collect that information from several people and record it separately.
- **Quality of the outputs.** Collins et al. adapt the Consensual Assessment Technique (Amabile) so that independent, blind judges rate the output of music models. R7 applies that design.

Record in each test who took part, their musical training and their production experience, so as not to mix populations when interpreting the results.

## Dependencies and decisions

**Parked (18 November 2026): emails to Chordonomicon and Hooktheory.** They are not sent and nothing depends on them. Reason: ChordSlots is AGPL-3.0 and the songs made with it can be sold or published, and a commercial use of cc-by-nc-4.0 data (Chordonomicon, which was also collected from user content) or data subject to Hooktheory's terms is not covered without express permission. The prior continues with Billboard (CC0) and RS200 (CC BY); POP909 is only used for measuring. If it is resumed, the draft must say that users will publish commercial music made with the tool. Unsent draft in `chordslots-data/email-drafts.md` (outside the repository).

| Dependency | Status and effect |
|---|---|
| **Participants** | Pending recruitment. The sheets and stimuli can be prepared before the session. Perception tests can include listeners who are not producers. Use tests need producers. |
| **Reviewer with music-theory training** | Needed for R1 and R7. Without this person, those tasks remain limited to the judgement of whoever develops the VST. |
| **Interactive access to a DAW** | An attempt to open ChordSlots through Windows control was denied. The integration task remains pending until a testing route is available. |
| **Exact material from the Navarro study** | The correspondence of P3/e3 and parts of the original reconstruction remain open. Resolving them would improve the external replication. The generator audit can move forward with what is already available. |

The open decisions are the Tension weights, the separation of colour and register, the possible Closure option and which presets to keep or redesign. The location of Energy is also an interface decision. The current audit documents its MIDI operation for the cases tested.

## How to maintain the plan

1. Before each assignment, identify a row of this plan, the question it answers and its closing criterion.
2. When finished, link the diff or report and record what was verified. A pending test must state what it still needs in order to be run.
3. Review the result before opening the next assignment. Keep the steps of the experiments in their reports and the producer protocol in the continuity plan.

**Next assignment:** the listening and DAW sessions (tasks 4 and 5), which wait on a host and listeners; the open engine work is the product decision on the Minor presets (see the task 3 report) and R8. Tasks 1, 2 and 3, the H7 measurement and the R2 ablation (see the task 2 report) are done. Tests for H8 (interval-convention guard), the R1 debatable function assertions and the R5 minor candidate pool are in `tests/core_tests.cpp`.
