# R4 engine half: soft end for the Brightness dial

Prototype and measurement, 18 November 2026. `kBrightnessSoftClip` (Metrics.h) is **0 = off**: the hard clamp is kept and a test checks `softClip01(x, 0) == clamp01(x)` (core tests: 158,387 checks, 0 failures). With k > 0 the clamp becomes a bounded tanh curve with the same ends (`y = 0.5 + 0.5 tanh(k(2x-1))/tanh(2k)`); k about 0.8 has slope 0.87 at the middle.

`probe.cpp` (type x root x scale, 3,840 combinations, neutral register) reproduces the saturation: dim 85 % at 0, aug 85 % at 1, m7b5 68 % at 0, maj7#11 15 % at 1; overall 4.9 % at 0 and 3.1 % at 1, 244 distinct 3-decimal values. The raw (unclamped) range is only [-0.19, 1.14], so a mild curve is enough: with k from 0.4 to 1.2 nothing saturates and the number of distinct values stays 229 to 265 (it does not by itself break ties between unrelated types).

`moods.cpp` (600 seeds per mood, 4 slots, tonic C), k = 0 against k = 0.8:

| Mood | Share of dim, m7b5, aug, maj7#11 | Tonic | Mean abs error vs Brightness target |
|---|---|---|---|
| Euphoric | 0.0 to 0.2 % | 29.1 to 31.8 % | 0.059 to 0.046 |
| Uplifting | 0.0 to 0.0 % | 26.2 to 25.8 % | 0.062 to 0.049 |
| Epic | 0.0 to 0.0 % | 25.2 to 25.1 % | 0.054 to 0.049 |
| Melancholic | 0.0 to 0.0 % | 26.9 to 26.0 % | 0.051 to 0.040 |
| Dark | **4.3 to 17.1 %** | 25.7 to 25.5 % | 0.086 to 0.085 |
| Tense | **0.9 to 9.0 %** | 13.2 to 13.3 % | 0.060 to 0.058 |
| Dreamy | **2.8 to 10.9 %** | 38.7 to 40.9 % | 0.055 to 0.053 |
| Hard | 0.0 to 0.4 % | 25.2 to 25.1 % | 0.071 to 0.088 |

**Reading.** The prototype does what it was meant to: the four types no longer sit pinned at the ends. But because the old clamp made dim, m7b5 and aug look extreme, the generator avoided them; once they are given a middle-ish value, Dark, Tense and Dreamy pick them three to four times more often. That is a large change in the character of those moods, so the clamp is not just a numerical nuisance but part of what those presets sound like today. Brightness target tracking improves in six moods and worsens in Hard (0.071 to 0.088). I did not run `eval` with the flag on. **Not to be enabled** without listening to Dark, Tense and Dreamy, and the weights themselves still wait for the task 5 pairwise data. Limits: neutral register, tonic C, 4 slots only.
