# Why Dark "never produces bVII" (R11 open measurement)

Date: 18 November 2026. Probes `mech.cpp` (unary costs per slot, Dark and Melancholic) and `root10.cpp` (all chords on degree b7 in Dark, 600 seeds, C), raw output `raw.txt`, `root10_raw.txt`.

**Result: it was a measurement artifact, and a statement in the plan was wrong.** The Phrygian scale is {0, 1, 3, 5, 7, 8, 10}. Major Bb (Bb, D, F) needs D, which is not in the scale, so major bVII is not diatonic in Phrygian; the diatonic chord on that degree is Bbm. The earlier tables counted only *major* bVII, so Dark showed 0.0 %. Counting every chord rooted on degree b7, Dark generates **29.0 %** (Bbm 11.2 %, Bbm7 9.9 %, Bbm11 2.0 %, sus chords about 2.7 %). That is two to three times the corpus figure for the whole degree 7 in minor (11 to 14 %).

Mechanism (unary costs, Dark slots 1 and 3, targets Brightness 0.15 / 0.10 and Tension 0.60 / 0.75): Bbm has Brightness 0.19 and Tension 0.63, almost exactly the target, so it ranks 1 to 3. Major Bb (Brightness 0.67) ranks 142 to 144 and would not be wanted by a Brightness 0.10 target anyway. Same "dark but tense" last-slot pattern as minor v in Epic, Melancholic and Hard.

Consequences: (1) no flag is needed for Dark; the per-mood diatonic context correctly does nothing there (`isMinorDiatonicMajorChord` requires the major triad inside the scale). (2) The line in the plan's diatonic-context section that B-flat major "is indeed diatonic in Phrygian" is corrected. (3) The open question for Dark is the opposite one: too much degree b7, not too little. Whether that sounds right for a Dark preset is a listening question; no change made.
