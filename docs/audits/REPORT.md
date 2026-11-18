# ChordSlots Tension and Brightness audit

November 18, 2026. Engine analyzed in `B:\Proyectos\Programacion\chordslots\chordslots-source`, branch **main**, commit **`7dc1ef0126d06c10c1616ff86d0acdb31cf30975`**. The checkout was clean at the start and at the end. The helper compiles the engine headers directly. The whole audit lives in `audit-curves`, next to the checkout.

## Executive summary

- **Tension.** Ten sufficiently long profiles were recovered per stimulus. The comparison uses eleven progressions and 63 chord positions with compatible audio and MIDI. The contextual model beats the static-with-voicing one in 3 of 11 progressions under the stability criterion and in 4 of 11 when averaging the second half of each window. The mean Pearson per progression is 0.665 versus 0.686 with stability and 0.850 versus 0.851 with the second half. The provisional conclusion is to keep the curve and its weights. The recovered data provide evidence of contour tracking, with an inconsistent contextual advantage.
- **Brightness.** In the full sweep, each of the 480 paired slots changes chord between the 0.1 and 0.9 targets, also with the center fixed. The mean of the harmonic component increases in all six scales. With the moving center, the upper register rises between 3.32 and 5.35 semitones on average depending on the scale in Roll. With the center fixed, the mean change in upper pitch still ranges between −0.70 and +1.35. Keep the curve provisionally as a combined control of harmony and register. The sweep measures the engine's response. Perceived brightness is left for a phase with listeners.

## 1. Tension against listeners

### Open materials and traceability

Study repository on branch `master`, commit `a5ca4a211e4ab34dcc9b7ba4f69cb863d801494f`. The numbers P1–P12 follow the article. [Materials at the audited revision](https://github.com/merismeris/tonal-tension-TIS/tree/a5ca4a211e4ab34dcc9b7ba4f69cb863d801494f)

| Material | Content and link |
|---|---|
| `ChordProgressionsMIDI/Progression1.mid` … `Progression12.mid` | The twelve SMFs were opened and the attacks and metadata were read. They have 5–10 four-note positions. P3 and P9 are byte-for-byte identical in MIDI. The MIDI files declare a C major key signature and attacks every 3,840 ticks with PPQ 960 and tempo 600,000 µs per quarter note, equivalent to 2.4 s. To align the responses, the experiment's audio clock was used. |
| `Experiment1/Interface/TensionWin/AUDIO/e0.wav` … `e12.wav` and `Tension.pd` | The 13 Windows WAVs and the capture patch were opened. Each trial is saved under its own WAV name. The e1→P1 mapping, etc., was checked with spectra of interior windows against the MIDI pitches. For eleven stimuli the correspondence is compatible. In e3 the attacks contain pitches systematically incompatible with P3, whereas e9 agrees with the MIDI shared by P3 and P9. The e3 and e9 WAVs have different audio. P3 is excluded from the agreement and its human profile from e3 and its MIDI diagnostic are kept separately through `stimulusMatch=0`. e0 is left outside P1–P12. |
| `Experiment1/Results/p1.txt` … `p15.txt` | The 15 individual continuous-response files were parsed, with lists of slider positions under `AUDIO/eN.wav` labels. p1–p10 cover the analysis windows of all stimuli. p11–p13 have approximately 7–13 samples per stimulus. p14 and p15 lack samples for e1–e12. The article describes 15 listeners with at least six years of musical training. The local analysis is based on the ten long files, without assuming the reason for the incomplete recordings. |
| Mean profiles and aggregates of experiment 1 | Figure 8 contains means with standard error per chord and Table 3 contains correlations per progression. The inspected repository provides individual responses, code and model outputs. The delivered mean was reconstructed from the recovered responses. No figures were digitized. |
| `Experiment2/ResulstListeningTest.xlsx` | The container and the sheet `Sheet0` were opened. It contains rows with a date and categorical choices 1–6, with explicit columns P1–P12. 91 rows with a numeric date were counted, 71 with all twelve valid codes, and 74–77 codes per column including partial rows. Those filters on the workbook differ from the 73 listeners who completed the experiment according to the article, which reports 51 with training and 22 without. The workbook allows each choice to be linked to a P. The article's exact analytical row selection remains pending. Tables 4–7 publish percentages, agreement between listeners and comparisons of global categories. This workbook is used only for the inventory. |
| `code/data.m`, `code/main_analysis.m`, `code/final_measures/TIS_select_candidates.m` | They were opened as text. `data.m` contains two series of 12 output blocks, with indicators `vs` (TIS), `vl` (Lerdahl) and totals. The second series has weighted totals different from the initial sums. Its `vs_total` vector is recovered by order and stimulus length as an archived reference. The source code shows that the TIS stage can choose inversions for voice leading. Still unverified are the exact provenance of the coefficients in that transcription and its identity with the final version of Figure 8, as well as the correspondence with the calculation's internal voicings. |

**Licenses.** The article declares CC BY 4.0. No root license or specific grant appears for MIDI, WAV, TXT, XLSX or the study's own TIS code at the examined revision. The bundled Pure Data comes with an improved BSD license and its Tcl/Tk components have their own notices. The MATLAB MIDI reader contains copyright notices from Ken Schutte. Those licenses and notices belong to specific components. The reproduction of this audit downloads the originals from their repository. [Article with license notice](https://pmc.ncbi.nlm.nih.gov/articles/PMC7712964/)

### Alignment and scoring

1. The article (§4.2.4) describes min–max per profile, ten samples per chord and the mean of a stable contiguous region with a difference between samples of up to ±0.005 after normalizing. In the published patch, `readsf~` starts the WAV and `delay 2060` starts `metro 200`. The observed counts are consistent with that start and the duration of each WAV. Therefore sample 0 is taken as ≈2.06 s and the first ten samples as chord 2. The tail from the nominal end of the last chord is ignored. The first chord is left without a usable observation in all the compared profiles.
2. The samples that fall in those windows are min–max normalized. In each window, the longest contiguous stretch with differences ≤0.005 is taken, with ties resolved in favor of the later one and a minimum of one sample. The article does not specify tie-breaking, handling of one-sample stretches or alignment with the patch's delay. This result is an explicit reconstruction of the method, with a sensitivity of the mean of the final five samples. Each listener weighs equally in the per-chord mean. [Published method, sections 4.1–4.2](https://www.mdpi.com/1099-4300/22/11/1291)
3. Each MIDI note, its octave, doubling and temporal order are preserved. The root is fixed according to the harmonic reading of each chord, the available quality with the smallest symmetric difference of pitch classes is searched, and ties are broken by catalog order. The bass note is used as the inversion if it exists in the chosen type. The CSV counts the discrepant classes per slot. For example, P7.2 uses symbol D7/C with the fifth absent in the stimulus, and P5.2 is approximated with D7♯9 by catalog distance. The roughness analysis and the spiral center receive the four original pitches even in these cases. The symbolic reference and the symbol-dependent parts of the contextual model do absorb the representation error.
4. C major is used except for the MIDI pair P3/P9, where A minor is inferred from the initial Am and the Dm–Gm–E–E7 stretch. The article describes tonalities of C major or minor and the MIDI key signatures are uniform, so that assignment carries explicit uncertainty. The P9 sensitivity CSV also tests harmonic A minor, C major and C minor. In A minor, the static-with-voicing model beats the contextual one (0.839 to 0.774). In C major the order is reversed (0.320 to 0.494). The original pitch is kept in both tests.
5. The reference for the context-free branch is `tensionOf` (static symbolic). `voicedTension` is added to isolate the value of context given the same voicing. The active model combines **0.86 × static with voicing + 0.09 × distance to key + 0.05 × displacement from the previous chord**. It normalizes and clips the last two terms with ceilings 0.33 and 0.36. It follows the weights of the commit without recalibration. For the first linear chord, displacement is set to zero and the key is kept. `measuredContextualTension` is used directly for the rest and in the circular diagnostic. [Contextual implementation](../../core/include/chordslots/Metrics.h) · [Per-slot computation](../../core/include/chordslots/Generator.h)

### Measured results

Pearson between the available positions of each progression and the human mean reconstructed with the stable region. It measures contour agreement. The sample size per correlation is the number of paired chords (4–9), with ten profiles per mean. The correlations are descriptive and conditioned by each row representing few chords and by the same cohort evaluating all progressions.

| P | MIDI chords | Complete profiles | Chords compared | r static symbolic | r static with voicing | r contextual | r archived TIS | Δ first chord in loop |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 6 | 10 | 5 | 0.704 | 0.771 | 0.746 | 0.816 | 0.000 |
| 2 | 10 | 10 | 9 | 0.695 | 0.748 | 0.728 | 0.804 | 0.014 |
| 3 | 6 | 10 | 0 | — | — | — | — | 0.028 |
| 4 | 6 | 10 | 5 | 0.629 | 0.645 | 0.515 | 0.626 | 0.013 |
| 5 | 7 | 10 | 6 | 0.951 | 0.941 | 0.943 | 0.815 | 0.014 |
| 6 | 5 | 10 | 4 | 0.565 | 0.548 | 0.488 | 0.861 | 0.014 |
| 7 | 6 | 10 | 5 | 0.408 | 0.464 | 0.539 | 0.717 | 0.000 |
| 8 | 10 | 10 | 9 | 0.499 | 0.528 | 0.519 | 0.848 | 0.000 |
| 9 | 6 | 10 | 5 | 0.875 | 0.839 | 0.774 | 0.805 | 0.028 |
| 10 | 6 | 10 | 5 | 0.965 | 0.945 | 0.933 | 0.952 | 0.013 |
| 11 | 6 | 10 | 5 | 0.672 | 0.684 | 0.680 | -0.212 | 0.023 |
| 12 | 6 | 10 | 5 | 0.481 | 0.433 | 0.450 | -0.880 | 0.000 |

The TIS column compares with the archived output of the second series of `data.m`. It is a historical reference of a different scope from the contrast between two ChordSlots formulas with the same voicing. The article reports a global Pearson of 0.750 for TIS with that study's fit. The published figure uses a different set of positions and profiles than the local reconstruction. [Fit and comparison of the study, §4.2.5–4.2.6](https://www.mdpi.com/1099-4300/22/11/1291)

- **Rise and final fall, P5.** From slot 5 to 6, the human mean rises from 0.606 to 0.725 and the contextual model from 0.640 to 0.659. When resolving G7→C in slot 7, they fall to 0.403 and 0.223. The static reference also reproduces both directions.
- **Peak in the wrong place, P7.** The mean rises from 0.446 to 0.777 at D7/C→A7. The contextual model falls from 0.537 to 0.495 and the static with voicing from 0.574 to 0.485. The contextual model improves the global Pearson from 0.464 to 0.539 and keeps the final G→C fall, with the engine's maximum displaced to G.
- **Final ascent, P9.** The mean rises 0.602→0.818 with E→E7/D. The static with voicing rises 0.769→0.824, whereas the contextual model falls slightly, 0.759→0.753. It is an example of how displacement and distance to key can reduce agreement.
- **Inversion and resolution, P12.** C/E receives 0.535 and the following C 0.336 in the human mean. The contextual model detects the drop (0.196→0.190) with a small change. At the end, F/C→C falls 0.549→0.171 in the listeners and 0.475→0.192 in the contextual model. The archived TIS rises at that close. That discrepancy deserves verifying which version produced the transcription.
- **Circular closure separately.** The last column is the difference in the first chord's score when adding last→first relative to the linear input. In these stimuli it reaches +0.028, with a maximum in P3/P9 because of their identical MIDI and the same inferred tonal context. This is an engine diagnostic without human ratings for that transition.

**Inference and open items.** Most of the contour tracking is already in the static formula. The two alignment methods change the absolute level of correlation considerably, especially in P4, P6 and P8. The mean contextual advantage remains near zero or below the reference with voicing. Recovering the exact MIDI of e3, the extraction of Figure 8 or the original synchronization pipeline, and resolving the tonality of P9 would reduce the uncertainty. The phrase-start and circular-closure behavior would also need to be validated with listeners in a later phase.

## 2. Brightness with fixed center

### Flow and experimental design

- The gesture writes `Slot::targetB`. On release, `LanesView::mouseUp` calls `rerollSlots` for the edited mask. The Roll button uses `roll`. Both reach `generateChords`. The edit revoices the slots whose chord has changed via `setChord`; Roll revoices and can optimize global voice leading. [UI lanes](../../plugin/ui/LanesView.cpp) · [Engine operations](../../core/include/chordslots/Generator.h)
- The per-candidate cost includes `(cand.brightness - targetB)²`. The candidate brightness uses neutral register 0.5. We define **H** as that calibrated value. Before calibration, the strictly harmonic term `rawH` is 0.55 × chord quality + 0.25 × color relative to the tonic, both derived from the line of fifths. The calibration subtracts 0.24, divides by 0.50 and clips to [0,1]. The cost also incorporates tension and other generation conditions. [Metrics](../../core/include/chordslots/Metrics.h) · [Selection](../../core/include/chordslots/Generator.h)
- The target center is `57 + 14 × targetB` (58.4 / 64 / 69.6 MIDI). `makeVoicing` penalizes the distance of the mean of the upper register from the center. The circular optimization uses the center when building/ordering voicing candidates and can revert if the fit falls by more than 0.02. The fixed condition replaces only that formula with 64 in an auxiliary copy of the headers. The fit criterion keeps the original targets. [Voicing selection](../../core/include/chordslots/Voicing.h) · [Optimization and measurement](../../core/include/chordslots/Generator.h)
- The result drawn by the Brightness lane is `Slot::brightness`: the calibrated formula with 20% actual register, computed as `clamp((upper MIDI mean − 48)/36)` over MIDI notes ≥48. The audit records that mean and a second mean over all notes stored in the voicing, including the bass. The results precede groove and performance processing and reflect the voicing's effective MIDI numbers. [Register metric](../../core/include/chordslots/Metrics.h) · [Drawn value](../../plugin/ui/LanesView.cpp)

**Controls.** C in six scales. Each level uses seeds 101–120, in pairs by seed and slot, with four equidistant slots across four 4/4 bars and locked rhythm. Free harmonic style, Piano voicing, Adventure 0.30, Variety 0.50, target Tension 0.40 and target Energy 0.60. Voice Leading 0.50, Bass Motion 0, Sustain bass, no melody, with phrase repeat disabled and no other locks. The first sweep starts from a new progression per level. The second simulates applying an edit to the four slots, starting from the same mid-target Roll progression per seed; it uses the default `rerollSlots` temperature (0.004). Mouse coordinates are not simulated. There are 1,440 rows per operation and condition, 80 observations per scale and level. The selected chords (root, type and symbolic bass) coincided between fixed and moving center in all paired rows of the same level and operation.

### Measured results by scale

L/M/H are the targets 0.1/0.5/0.9. "Change" compares root, quality and inversion between L and H. The arrows count changes of H for each paired interval. "H at maximum" counts H equal to 1 among 80 slots of the high level. Each Δ is high minus low. It is given in semitones for MIDI pitch and on the 0–1 scale for the UI lane.

### Roll

| Scale | H low / mid / high | Change L→H | ↑ / = / ↓ L→M | ↑ / = / ↓ M→H | H at maximum | Upper MIDI Δ fixed / current | Total MIDI Δ fixed / current | UI Δ fixed / current |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Major | 0.183 / 0.495 / 0.821 | 80 | 79/1/0 | 80/0/0 | 0 | 0.16 / 4.28 | 0.37 / 3.68 | 0.64 / 0.68 |
| Minor | 0.206 / 0.519 / 0.755 | 80 | 80/0/0 | 78/2/0 | 0 | -0.51 / 3.74 | -0.26 / 3.59 | 0.54 / 0.59 |
| Dorian | 0.151 / 0.512 / 0.750 | 80 | 80/0/0 | 78/0/2 | 0 | -0.70 / 3.32 | -0.37 / 3.11 | 0.59 / 0.63 |
| Phrygian | 0.216 / 0.519 / 0.725 | 80 | 80/0/0 | 79/1/0 | 0 | -0.31 / 3.36 | -0.02 / 3.36 | 0.51 / 0.55 |
| Lydian | 0.160 / 0.503 / 0.838 | 80 | 79/1/0 | 80/0/0 | 1 | 1.11 / 4.50 | 0.60 / 3.43 | 0.69 / 0.73 |
| Harmonic Minor | 0.190 / 0.495 / 0.812 | 80 | 80/0/0 | 80/0/0 | 13 | 1.35 / 5.35 | 1.27 / 4.54 | 0.63 / 0.68 |

### Edit

| Scale | H low / mid / high | Change L→H | ↑ / = / ↓ L→M | ↑ / = / ↓ M→H | H at maximum | Upper MIDI Δ fixed / current | Total MIDI Δ fixed / current | UI Δ fixed / current |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Major | 0.189 / 0.464 / 0.837 | 80 | 80/0/0 | 80/0/0 | 0 | -0.57 / 3.12 | -0.36 / 2.77 | 0.64 / 0.68 |
| Minor | 0.185 / 0.531 / 0.749 | 80 | 80/0/0 | 80/0/0 | 0 | 0.04 / 2.42 | 0.10 / 2.21 | 0.56 / 0.59 |
| Dorian | 0.118 / 0.514 / 0.760 | 80 | 80/0/0 | 80/0/0 | 0 | -1.12 / 1.95 | -0.43 / 2.20 | 0.63 / 0.66 |
| Phrygian | 0.188 / 0.534 / 0.719 | 80 | 80/0/0 | 78/2/0 | 0 | 0.49 / 3.07 | 0.54 / 2.77 | 0.54 / 0.56 |
| Lydian | 0.131 / 0.492 / 0.842 | 80 | 80/0/0 | 80/0/0 | 0 | 0.02 / 2.65 | -0.22 / 2.07 | 0.71 / 0.74 |
| Harmonic Minor | 0.187 / 0.510 / 0.829 | 80 | 80/0/0 | 80/0/0 | 19 | 1.83 / 4.01 | 1.31 / 3.14 | 0.66 / 0.68 |

**Headroom and saturation.** All candidate pools in this setup reach H=0 and H=1, but that interval alone says little about the outcome achievable in a progression. Lydian has the largest mean range in Roll (H +0.678) and Phrygian the smallest (+0.509), with a high mean of 0.725. Harmonic Minor shows the most frequent local ceiling (13/80 in Roll and 19/80 in edit). At the low end of Roll, H=0 appears in 25/80 in major, 5/80 in minor, 22/80 in Dorian, 1/80 in Phrygian, 21/80 in Lydian and 15/80 in harmonic minor. They are clippings of the metric for those chords. The data do not establish a plateau across the whole scale from a particular target. For that, closer levels and the effect of other Adventure settings, styles and non-flat curves remain to be measured.

**Reproducible examples.**

| Operation and parameters | Result |
|---|---|
| Roll, C harmonic minor, seed 101, slot 1 | Cm → A♭ → C. H 0.284 → 0.575 → 0.756. Between low and high, the fixed center leaves +0.50 semitones in the upper voices and +0.40 counting the bass. In the current operation, +4.25 and +3.40 are obtained, respectively. |
| Roll, C Dorian, seed 108, slot 3, mid → high | E♭6 → B♭add9, ΔH −0.0052. The unclipped harmonic range also drops (−0.0026). |
| Roll, C Dorian, seed 115, slot 2, mid → high | E♭add9 → E♭, ΔH −0.0169 and Δ of the unclipped harmonic term −0.0084. |

**Inference.** The generator makes substantial use of the harmonic lever in these settings. The two Dorian exceptions show that the sequence optimum has room to sacrifice the local Brightness fit even when the target rises. The larger range of the lane measure with a moving center coincides with a rise in actual pitches. Taking a difference between the two conditions as the effect of the center is valid for this paired intervention. It is not equivalent to a perceptual decomposition of the chord.

## Reproduction

To repeat the trials, follow the [reproduction instructions](README.md).

- C++ helper [engine.cpp](engine.cpp), build [run-engine.cmd](run-engine.cmd) and the [fixed-center](fixed-header.mjs) variant.
- Tension. [MIDI and response reader](stimuli.mjs), [spectral audio diagnostic](check-audio.mjs), `source-inventory.txt`, `audio-check.txt`, `exp2-inventory.txt` and its [script](inspect-exp2.ps1). The spectral diagnostic orders magnitudes at MIDI frequencies 32–83 in seven interior windows per chord; each line gives expected MIDI notes, six peaks of the chosen central window and the worst position of an expected note in each window.
- Tension. `stimuli.csv`, `tension-coverage.csv`, `tension-profiles.csv`, [comparisons and temporal sensitivity](tension-comparison.csv) and [P9 tonality sensitivity](tension-key-sensitivity.csv).
- Brightness. Per-slot data from `brightness-current.csv`, `brightness-fixed.csv`, `brightness-edit-current.csv` and `brightness-edit-fixed.csv`. They include all MIDI notes, the nominal center, the upper and total pitch, H, `rawH` and the value measured for the UI.
- Brightness. `brightness-pairs.csv`, `brightness-edit-pairs.csv`, [Roll summary](brightness-summary.csv) and [edit summary](brightness-edit-summary.csv). [analyze.mjs](analyze.mjs) generates the summaries, compares the tension models and writes the `tables.md`.

## Provisional recommendation

- **Tension.** Keep the name, the model and the weights during this phase. To defend the weight of context or change it, look for a reproducible improvement over `voicedTension` on independent progressions, that preserves the direction of rises and resolutions when the alignment varies and that makes verifiable predictions for last→first. The small observed differential does not justify recalibrating on these eleven progressions.
- **Brightness.** Keep the combined curve provisionally. A listening test with matched register and another with register-shifted voicings over the same chords could change the recommendation. If listeners stably distinguish one of the two effects and attribute the other to pitch or harmonic color, there would be a basis to revisit the name or expose two controls. If both contribute to the judgment in the ranges used, the combined control gains perceptual support.
