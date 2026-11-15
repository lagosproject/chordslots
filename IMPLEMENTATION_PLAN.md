# ChordSlots v0.2: Prioritized Implementation Plan

> Source: the review of 2026-11-15 (corpus, metrics, voicing, inference, producer
> features). This plan replaces the remaining stages of Section 8 in
> `ROADMAP_AND_SPECIFICATION.md`. Section 8 items that are already built (UI split,
> picker overlay with audition, anti-oscillation, voice-leading pass, shape stamps,
> Analyze MIDI, preview sounds) are not repeated here.

## Ordering principles

1. **Measure before changing the music.** An evaluation harness comes first, so every
   later phase can prove that it made the output better and did not just change it.
2. **Fix what is wrong before adding what is new.** Correctness and honesty fixes are
   cheap and come before any feature.
3. **Build foundations before the features that depend on them.** The chord model
   comes before voicings and the real corpus. The inference engine comes before
   contextual tension and melody harmonization.
4. **Order the rest by what the user hears.** Voicing quality is ahead of corpus
   accuracy, and corpus accuracy is ahead of tension theory.

## Dependency graph

```
P0 Stabilize + eval harness
 └─ P1 Correctness fixes
     └─ P2 Chord model (interval sets, new chord types)
         ├─ P3 Voicing engine            (highest audible impact)
         └─ P4 Real corpus prior
             └─ P5 Exact inference + joint chord/bass
                 ├─ P6 Contextual tension
                 └─ P7 Producer features (melody harmonization, bass patterns, song mode)
P8 Formats & CI (CLAP, AU MIDI FX, Windows, pluginval): parallel track, any time after P0
```

Effort is in focused working days. Build times on this machine are slow, so add a buffer.

---

## P0 · Stabilize and measure (≈2 days) — prerequisite for everything

| # | Task | Done when |
|---|---|---|
| 0.1 | Commit the current WIP (UI split into `plugin/ui/`, comping, picker, preview sounds) as separate logical commits. Move or ignore the `ui_snapshot*.png` files in the root. | `git status` is clean; each commit builds and passes the tests. |
| 0.2 | **Honesty fix, today:** relabel the prior as "hand-authored style prior (provisional)" in `build_corpus.py`, the `Corpus.h` header, the README and the UI ("Pop/Rock (Billboard)" becomes "Pop/Rock"). | No text claims corpus-derived data until P4 lands. |
| 0.3 | Fix the documentation drift: README Limitations, README roadmap, the duplicated test section in the spec, the `Slot::start` comment ("sixteenth steps"). | The documents match the code. |
| 0.4 | **Musical evaluation harness**: `chordslots-cli eval [moods] [N]` rolls N=1000 per mood and reports: tonic present in the loop %, distinct progressions %, bigram distribution, slots without bass %, voicings with intervals below the low-interval limit %, mean Fit, A-B-A-B rate. It writes JSON. Store a **baseline** in `tests/eval_baseline.json`. Add a loose-threshold ctest. | The baseline is committed; CI fails on large regressions. |

## P1 · Correctness fixes (≈3 days)

Each fix is checked against the P0 baseline.

| # | Task | Notes |
|---|---|---|
| 1.1 | **Fix the PMI term.** Remove `+0.3·ln P(B)` (it rewards unseen destinations). Replace it with interpolated backoff: `P(B|A) = λ·P_ml(B|A) + (1-λ)·P(B)`, and leave a source row with no counts on the unigram instead of a uniform row. | Probe check: `Am→C#` must cost more than `Am→G` and `Am→F` in every cluster. |
| 1.2 | **Pick the style cluster by tonality, not by mood name.** Add a `style` field to `Mood` and remove the `strcmp` calls. Scales with a minor third use MinorLoops by default; the Style combo overrides it. | Fixes Uplifting, Epic and Hard. |
| 1.3 | **Make `bVII` depend on the scale** in `functionOf`: subdominant (0.4) in major/Mixolydian, dominant-like (0.7) in minor/Aeolian. | Small calibration change; re-run `cli table`. |
| 1.4 | **Always keep the bass.** Low energy means fewer doublings and lower velocity, never a missing bass note. Recalibrate the density term in `updateActuals`. | Harness: slots without bass = 0%. |
| 1.5 | **Targeted rolls use `slotCost`.** Roll Root, Roll Color and Roll Bass sample from the scored candidates, restricted to the one layer being rolled (temperature like `rerollSlots`). | No more random `aug` or `dim7` from Roll Color. |
| 1.6 | **Do phrase repeat (AA′) inside the sampler.** Tie each slot i in the second half to slot i−h as one variable whose cost sums both slots' targets. Sample the cadence slot freely, and score the seam. | Fit no longer drops when AA′ is on. |
| 1.7 | **Comping must sound live.** Pre-bake the comp hits into a fixed-size event array in `PlayData`, built on the message thread. Make `getCompHits` write into a fixed buffer (no `std::vector`). Live playback, the preview synth and export read the same baked events. Size `pendingNotes` for the worst case. | **Export parity test:** the live event stream equals the `.mid` export for every comp style × split × strum. |

## P2 · Chord model refactor (≈5 days) — foundation

| # | Task | Notes |
|---|---|---|
| 2.1 | Derive every chord property from the **interval set**: quality class, Roman suffix, figured bass, minorish, complexity. Remove the checks on raw type indices (`type == 2`, `type >= 7 && <= 12`, the `qualityClassOf` switch). | Saved states store type indices, so the table becomes **append-only** with a test that guards it. |
| 2.2 | Add chord types: `m11`, `maj7#11`, `13`, `m13`, `6/9`, `7b9`, `7#9`, `add11`, `9sus4`, quartal (`4ths`). | Needs omission rules (P3). Check `kMaxNotes = 8` with bass doublings. |
| 2.3 | Recalibrate `Calibration` constants with `cli table`, and update the tests. | Existing chords keep their metric values within ±0.03, or each change is documented. |

## P3 · Voicing engine (≈7–8 days) — highest audible impact

| # | Task | Notes |
|---|---|---|
| 3.1 | **Voicing style dictionary** (interval templates per style, after tonal.js `voicing-dictionary`): Close, Piano (left hand root-5/root-10 plus right-hand upper structure), Spread/Pad, Drop-2, Drop-3, Rootless, Stacked/EDM (doubled top), Guitar (later). | One `VOICING STYLE` parameter per variation. |
| 3.2 | **Omission rules:** drop the 5th first; drop the 11th on chords with a major 3rd; never drop the 3rd or 7th in jazz styles. | Required for 9/11/13 chords. |
| 3.3 | **Low-interval-limit table** as a hard constraint (e.g. no m2 below ~E3, no M3 below ~Bb2), plus a "lowest upper note" setting. | Harness: low clusters = 0%. |
| 3.4 | **Real assignment voice-leading distance:** brute-force or Hungarian matching for ≤8 voices, with dummy voices when the counts differ. Replaces the nearest-neighbour average. | Matches what the spec promises. |
| 3.5 | Retune the Fit non-regression guard; state format **v4** (voicing style + new fields), with migration from v3. | Old sessions load unchanged. |

## P4 · Real corpus prior (≈6 days incl. licence work)

| # | Task | Notes |
|---|---|---|
| 4.1 | **Licence spike (½–1 day):** read the ChoCo v1.0.0 `LICENSE.md` and the metadata page per partition; get Billboard straight from McGill (CC0); check RS200 terms. Write `DATA_LICENSES.md`. | **Rule: only CC0 / CC-BY / public domain go into tables we ship.** No NC or SA-NC sources. Chordonomicon (scraped user tabs, unclear provenance) is used offline for comparison only, unless the spike proves it clean. |
| 4.2 | Rewrite `build_corpus.py` as a real pipeline: download a pinned release with a SHA-256 check, parse Harte (`.lab`/JAMS) and RS200 Roman numerals, normalize to the key using the annotated tonic, collapse repeats, tokenize with the P2 quality classes. | The header records each source, song count, bigram count and licence. |
| 4.3 | **Clusters from metadata, not by hand:** split by song mode (major / minor-modal) and by genre tags where available (pop/rock, soul/R&B/jazz). Smooth with Kneser-Ney or interpolated backoff (the P1.1 form). | The cluster names in the UI match the real filters. |
| 4.4 | Harness: compare the bigram distribution of generated output with the corpus (KL divergence) per cluster at low Adventure. README attribution. Restore the "data-driven" claim **only now**. | KL at Adventure 0 lower than at baseline. |

## P5 · Exact inference + joint chord/bass (≈6 days)

| # | Task | Notes |
|---|---|---|
| 5.1 | Replace the 4-sweep Gibbs sampler with **ring forward-filtering / backward-sampling**: sample slot 0 from its exact marginal on the ring, then run FFBS conditioned on it. | About K²·N ≈ 1.5M operations per roll, fine on the message thread. |
| 5.2 | **Joint state (chord, bass interval)** with beam pruning (top 64 per slot by unary cost). Bass motion becomes a pairwise term in the same DP, so the separate `generateBassLine` pass goes away. | Locks just restrict the state set. |
| 5.3 | Handle the A-B-A term (second order) with a Metropolis–Hastings accept/reject step after sampling. | Keeps the DP first-order. |
| 5.4 | **k-best distinct** alternatives for the picker; a `VARIETY` knob (temperature). | Determinism test: same seed gives the same result. |

## P6 · Contextual tension (≈6 days, research-heavy)

| # | Task | Notes |
|---|---|---|
| 6.1 | Static part from the **actual voicing**: roughness of the real notes in `updateActuals`. Keep the type-level estimate only for preselection. | Brightness and tension then agree on whether the voicing matters. |
| 6.2 | Context part: distance from the previous chord and strain against the key (Spiral Array tensile strain / cloud momentum after Herremans & Chew; check against midi-miner and partitura). It goes into the P5 pairwise term. | This is why P6 comes after P5. |
| 6.3 | Recalibrate and retune the mood curves. **Listening protocol (you):** 20 progressions, rate tension 1–5 blind, report the correlation with the model. | Ship only if the correlation beats the static model. |

## P7 · Producer features (≈3–7 days each, in this order)

| # | Feature | Notes |
|---|---|---|
| 7.1 | **Harmonize my melody.** Drop in a MIDI melody; weight pitch classes per slot by duration and metric position; add a cost term in `slotCost` (chord tone 0, valid tension low, avoid note high, e.g. natural 4 over a major 3rd, b9 over the root). Optionally make the voicing's top note follow the melody. | Reuses the Analyze MIDI file loading. |
| 7.2 | **Bass patterns** on Ch 2: sustain, octave pump, offbeat, 808 legato/glide flag, walking. Baked into `PlayData` like comping. | Export parity test covers it. |
| 7.3 | **Song mode:** a chain of up to 16 variation entries (e.g. A A B C), whole-song export, transposition per variation (modulation). | State v5 or folded into v4. |
| 7.4 | **Chord-name markers in exported MIDI**: check what already exists in `PluginProcessor.cpp` and finish it for every export branch. | Small. |
| 7.5 | **UI polish:** Fit becomes a smaller "Match"; units on the knobs (ms, %); no truncated labels at 1040×620; Voicing Style and Variety controls. | Snapshot test. |

## P8 · Formats & CI (parallel track, ≈5 days)

| # | Task | Notes |
|---|---|---|
| 8.1 | CI: macOS and Windows builds; **pluginval in CI** (it is blocked locally). | GitHub runners. |
| 8.2 | **CLAP** target (JUCE 9 native support if present, otherwise clap-juce-extensions). | |
| 8.3 | **AU MIDI FX** second target (`IS_MIDI_EFFECT TRUE`, `kAudioUnitType_MIDIProcessor`) so Logic can drive any instrument. `auval` runs in macOS CI. | **Needs a person with a Mac and Logic for the final check.** |
| 8.4 | Release v0.2: README rewrite, `DATA_LICENSES.md`, changelog. | |

## Backlog (not scheduled)

Keyswitches for variation switching, a roll-history popover, host-automatable Roll,
freehand lane painting improvements, more preview synth models.

## Deliberately not doing

More mood presets, AI/LLM chord suggestions, a bigger built-in synth. They dilute the
target-curve idea, and producers route the MIDI to their own sounds anyway.

## Rough total

About 45–55 focused days of work. The phases that change what you hear are P1, P3 and P7.1.
After P1 + P3 (≈2.5 weeks) the plugin already sounds clearly better, which makes that a
good point for a v0.1.1 release.
