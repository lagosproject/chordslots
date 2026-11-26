# Roadmap pilot

## Demo files

Run the built `chordslots_plugin_tests` executable with `--roadmap-demo <output-directory>` to generate the MIDI and matched Felt Piano recordings. All takes use 120 BPM. Rendered audio comes from the plug-in's progression audition path. Use a fresh output directory for each run.

- **Chord-first.** The four-bar A minor starting point is Down the Stairwell. Compare `curve-flat.wav` and `curve-rise.wav`. Both rerolls keep the first chord and the slot cuts fixed. The rising target changes only bars 3–4. `chord-first-song.mid` chains the two takes and transposes the second by +2 semitones starting at bar 5.
- **Controlled follow-up.** `fixed-opening-flat.wav` and `fixed-opening-rise.wav` also lock the second slot. The rendered first four seconds match sample for sample. Use the same two questions, with the order hidden, and report these responses separately from the original pair.
- **Feel.** Backbeat Answer accents beats 2 and 4 with a light pickup. Long-Short uses a long opening followed by two spaced hits. Broken Sixteenths leaves gaps between short accented bursts. Each Feel recording uses the same starting progression.
- **Melody-first.** `syncopated-lead.mid` has an eighth-note pickup, an E held across the bar-2 cut, and a rest of one beat between the held E and the next onset. Compare `melody-sustain.wav` with `melody-follow.wav`. `melody-variation.wav` swaps the chord under the held E. `melody-first-song.mid` exports the updated take with its source melody and a second section transposed by +2 semitones.
- **Arp and import.** `chord-arp.mid` contains the upward arp at the default eighth-note rate. `chord-arp.wav` auditions it with the bass. `block-chord-import.mid` proposes C/E, F6 (with Dm7/F as an alternative), and G7 at steps 0, 36, and 72.

## Host checks

Use Windows VST3 in Ableton Live first. Use Logic's AU MIDI FX path separately with the plug-in ahead of a destination instrument. Record the exact build and host version. Time installation and routing separately from musical edits.

1. Open Progression Library, preview Down the Stairwell, load it, lock slot 1 and the cuts, and draw a rise in Tension across bars 3–4. Roll and check the first chord and voicing. Compare the three new Feel patterns with the same sound and progression.
2. In A–D, keep the first take, audition two credible alternatives, and return to the keeper. Log actions, bank slots used solely for audition, and any mismatch in recovered chord, voicing, or feel.
3. Import `syncopated-lead.mid`. In Original Timing with Follow Melody Rhythm, expect chord starts at ticks 240, 720, 1560, 2760, 3180, 4320, 4980, 6060, and 6900 at 480 PPQ. Compare Sustain by ear. Set the loop to eight bars and check that Fit to Loop doubles the supplied start ticks. Swap the chord under the held E and check that source pitches and hit positions survive.
4. Select an arp in Feel and drag the arp into the host. Check the imported clip against its preview for direction, rhythm, gate, and first downbeat. Build a four-bar verse and chorus in Song with the second entry transposed by +2. Try both Song drag and MIDI file import. Check start ticks 0 and 7680, track or channel separation, pitch offsets, note endings, and retained track names and section markers. Repeat with source melody included. Note which metadata the host retains for each route.
5. Import a simple block-chord MIDI clip through **Import Chords**. Compare the proposed slots to the clip. Try an F6/Dm7 pitch set and verify that the bass-root suggestion and alternate name are visible before confirmation. Adjust a cut, apply, lock the opening slot, roll, then undo through the import.

The chord importer accepts one pitched track and channel, simultaneous note starts and ends within each chord, supported pitch-class sets, and up to eight bars or 16 chords in 4/4, 3/4 or 6/8. Each slot is voiced by ChordSlots. When gaps or rounded cuts change the timing, the preview flags the adjustment before applying.

## Producer decisions

Use the [existing listening survey workflow](../tools/listening/test-page/README.md) for evidence about perceived tension, brightness, and mood. The two demo comparisons are diagnostic fixtures. Keep their observations separate from survey results and consult the survey before requesting any additional listening rounds.

For the workflow pilot, compare the same chord-first tasks in Scaler 3 and Chordjam, the melody task in ANTHEM and Hookpad 3, and rapid audition in SPAWN. Include the producer's usual DAW workflow as a baseline. Alternate product order and record raw counts alongside medians.

Record time to a take worth keeping, clicks, wrong recoveries, routing or timing fixes, and whether the user could tell when to enable Follow Melody Rhythm. Zero unplanned note offsets, failed lock recoveries, or section alignment errors are required on these fixtures. Use repeated recovery friction to decide whether to prototype temporary A/B. Use repeated chord-choice difficulty or section-transition edits to decide which Phase 5 feature to build.
