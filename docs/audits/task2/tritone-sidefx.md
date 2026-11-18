# Tritone term: side effects on chord choice per mood

Measurement only. No file under `core/` or `tests/` was changed. Builds used header copies with `kTritoneWeight` patched by `sed`.

## Setup

- Files: `tritone_sidefx.cpp`, `tritone_sidefx.sh` (driver), raw output `tritone_sidefx_raw.txt`.
- 8 moods from `moods()`, 600 seeds each (seed 1..600), 4 slots, tonic C.
- Each progression uses the mood's own scale, adventure, syncopation and B/T/E curves (`applyMood`), then `roll()`.
- Consecutive identical chords are collapsed to one, so a chord repeated across slots counts once.
- Weights: `kTritoneWeight` = 0, 0.04, 0.06 (three builds). Contextual tension is on (the default).
- Denominator: collapsed chords per mood and weight (about 2400 per cell).

## Classification

Based on `chordTypes()` in `Theory.h`, by suffix name or intervals:

- **Dominant**: intervals contain a major third (4) and a minor seventh (10) mod 12. This gives `7, 9, 13, 7b9, 7#9`. `7sus4` and `9sus4` have no major third, so they are excluded.
- **Dim + dim7**: suffix `dim` or `dim7`.
- **m7b5**: suffix `m7b5`.
- **Any tritone**: `hasChordTritone(type)` from `Metrics.h`.
- **Tonic**: chord root equals the tonic pitch class, any quality.

## Results (% of collapsed chords)

| Mood | w | chords | Dominant | dim + dim7 | m7b5 | any tritone | tonic |
|---|---|---|---|---|---|---|---|
| Euphoric | 0.00 | 2400 | 0.0 | 0.0 | 0.0 | 0.0 | 29.1 |
| Euphoric | 0.04 | 2400 | 0.0 | 0.0 | 0.0 | 0.0 | 29.1 |
| Euphoric | 0.06 | 2400 | 0.0 | 0.0 | 0.0 | 0.0 | 29.1 |
| Uplifting | 0.00 | 2400 | 0.2 | 0.0 | 0.0 | 0.2 | 26.2 |
| Uplifting | 0.04 | 2400 | 0.1 | 0.0 | 0.0 | 0.2 | 26.1 |
| Uplifting | 0.06 | 2400 | 0.0 | 0.0 | 0.0 | 0.1 | 26.1 |
| Epic | 0.00 | 2400 | 0.5 | 0.0 | 0.0 | 1.0 | 25.2 |
| Epic | 0.04 | 2400 | 0.4 | 0.0 | 0.0 | 0.9 | 25.1 |
| Epic | 0.06 | 2400 | 0.2 | 0.0 | 0.0 | 0.8 | 25.2 |
| Melancholic | 0.00 | 2400 | 0.2 | 0.0 | 0.0 | 0.3 | 26.9 |
| Melancholic | 0.04 | 2400 | 0.1 | 0.0 | 0.0 | 0.1 | 26.8 |
| Melancholic | 0.06 | 2400 | 0.1 | 0.0 | 0.0 | 0.2 | 26.8 |
| Dark | 0.00 | 2400 | 1.0 | 3.5 | 1.1 | 5.8 | 25.7 |
| Dark | 0.04 | 2400 | 1.0 | 6.0 | 1.4 | 8.5 | 25.7 |
| Dark | 0.06 | 2400 | 1.2 | 5.8 | 1.5 | 8.5 | 25.8 |
| Tense | 0.00 | 2400 | 17.8 | 12.5 | 0.5 | 37.3 | 13.2 |
| Tense | 0.04 | 2400 | 19.5 | 14.3 | 0.5 | 40.2 | 13.0 |
| Tense | 0.06 | 2400 | 20.4 | 14.9 | 0.5 | 41.0 | 13.5 |
| Dreamy | 0.00 | 2400 | 5.2 | 0.0 | 0.0 | 8.4 | 38.7 |
| Dreamy | 0.04 | 2400 | 4.2 | 0.0 | 0.0 | 6.9 | 38.6 |
| Dreamy | 0.06 | 2400 | 3.4 | 0.0 | 0.0 | 6.4 | 38.5 |
| Hard | 0.00 | 2398 | 1.8 | 0.0 | 0.0 | 2.0 | 25.2 |
| Hard | 0.04 | 2398 | 1.5 | 0.0 | 0.0 | 1.6 | 25.1 |
| Hard | 0.06 | 2398 | 1.3 | 0.0 | 0.0 | 1.3 | 25.1 |

Sampling noise: each share is over about 2400 chords from 600 progressions, so differences of roughly 1-2 points are within noise. Chords within a progression are correlated, so the real uncertainty is larger than a binomial estimate.

## Notes

- **The "any tritone" column includes m6 and m6/9.** In `hasChordTritone`, the minor third (3) and major sixth (9) form a tritone, so `m6` and `m6/9` count. The Tense row at weight 0 has 121 m6 and 36 m6/9 chords. These account for the gap between the tritone column and the dominant + dim + dim7 + m7b5 sum (for example Tense: 426 + 10 + 291 + 11 = 738 of 895). The comment on `hasChordTritone` in `Metrics.h` lists only dominant, dim, m7b5 and dim7, so it is incomplete.
- dim and dim7 chords appear only in Tense and Dark. Dominant chords also appear in Dreamy and Hard, at a few percent. This holds at every weight.
- Dreamy's dominant share falls as the weight rises (5.2 to 3.4). This is the opposite of what the term would seem to predict; I measured it but did not investigate the cause.

## Plain-language summary

Turning the tritone term on mostly changes Tense and Dark. In Tense, dominant-family chords rise from 17.8% to 20.4% and dim/dim7 from 12.5% to 14.9% as the weight goes from 0 to 0.06. In Dark, dim/dim7 rises from 3.5% to about 6%. The other moods hardly use these chords, so the term has almost no effect there. Dreamy goes the other way, with dominant chords falling from 5.2% to 3.4%. Most of the changes are a few points and sit near the noise level of this sample, so they should be read as direction, not precise effect sizes.
