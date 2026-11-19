# DAW checklist, session 2 (remaining steps)

Continues [session 1](DAW-CHECKLIST.md#session-1-results-19-november-2026-windows-11-ableton-live-12-1-vst3-tester-not-a-professional). Same setup: Ableton Live 12.1, 120 BPM, one MIDI track with ChordSlots, Split off, Strum, Accent and Humanize at zero. Write what you saw, short is fine. "Not sure" is a valid answer.

## 0. Build and install (Windows)

1. `git pull` on the Windows machine (main).
2. Build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then `cmake --build build --config Release --parallel 2`.
3. Copy the new `ChordSlots.vst3` over the old one in your VST3 folder, rescan in Ableton.
4. Write down the commit: `git rev-parse --short HEAD` → ______

## 1. Check the bug fix (most important)

| # | Action | Expected | Result |
|---|---|---|---|
| N1 | Make a 4-slot progression (any mood), press ROLL. Write the chords down or take a screenshot. Press Play and listen to the loop once. | Baseline. | |
| N2 | Drag **Energy** on **slot 2 only** to a very low value, release. Play the loop. | Slot 2 sounds softer and thinner. Slots 1, 3 and 4 sound exactly as in N1. | |
| N3 | Drag Energy on slot 2 to a very high value, release. | Slot 2 louder and fuller. Slots 1, 3 and 4 still exactly as in N1. | |
| N4 | Repeat N2 three times, dragging different slots each time. | Only the dragged slot changes every time. | |

## 2. Steps not run in session 1

| # | Action | Expected | Result |
|---|---|---|---|
| D2 | Drag **Tension** up on one slot, release, Play. | That slot sounds tenser (more dissonant chord). Nothing to measure, just say if you hear it. | |
| D6 | Set one slot's Energy to 59%, 60%, 61%, releasing each time and listening. | Write "no jump" or "jump" between 59 and 61. | |
| D7 | Write down the **Match %** after N1, N2 and D2. | An integer %. A pure Energy edit may leave it unchanged. | |
| D8 | Add a cut, then drag it. | Slot boundaries move and chords stay with their slots. | |
| E2 | Turn on **Roots** and **Colors**. Press ROLL five times. | Chords do not change at all. | |
| E4 | Set a slot's Energy low, turn on **Voicing** lock only, press ROLL. | Chords that stay the same keep the same notes. (Velocity is not expected to change.) | |
| F5 | Create variations A and B with different chords. Press Play, then click B. Try each switch timing (now, next slot, next bar). | The switch happens at the chosen boundary. | |

## 3. Save and reload

| # | Action | Expected | Result |
|---|---|---|---|
| I1 | Make a progression, lock one slot, change a curve by hand (mood becomes Custom), turn Split on. Write down the **Match %** and the chords. Press **Save MIDI** and keep the file. | Values noted. | |
| I2 | Save the Ableton project, close Ableton, reopen. | Progression, locks, mood (Custom), variations and Split are the same. | |
| I3 | Compare Match %, and press **Save MIDI** again. | Same Match %. Same notes in the new file. | |
| I5 | Check that Split is still on. | Split on. | |

## 4. Optional

- **G6**: stop the host, press Play in ChordSlots, record the notes on another track. Compare with a host-driven take.
- **U1**: while pressing Undo repeatedly, count how many steps it goes back. Write the number.

## Log

| Field | Value |
|---|---|
| Date | |
| Commit | |
| Bug fix confirmed (N2 to N4) | yes / no / not sure |
| Anything odd | |

## Results (19 November 2026, Windows 11, Ableton Live 12.1)

| Step | Result |
|---|---|
| N1 to N4 | **Pass.** The Energy fix works in the host: only the dragged slot changes. |
| D2 | Pass. |
| D6 | **Not testable.** The UI shows no Energy value and cannot set an exact percentage. Skipped. The engine audit already covers 59, 60 and 61 %. |
| D7 | Largest Match change from an Energy edit: 3 %. |
| D8 | A cut cannot be dragged past its neighbouring cuts. **By design** (`moveCut` keeps each cut between its neighbours, at least one step from each; a locked cut does not move). |
| E2 | Pass. |
| F5 | Not understood, skipped. |
| I1 to I3, I5 | Pass. |
| U1 | Undo went back 69 steps; Redo does not exist. The code keeps 64 steps per variation, so 69 suggests the count mixed several variations or some steps were counted twice. Not explained yet. |

**Open items:** no Redo (decide whether to add it); no Energy value display (a number readout while dragging would make D6 possible); Undo depth undocumented.

## Session 3: Redo and lane readouts (new build needed)

| # | Action | Expected | Result |
|---|---|---|---|
| R1 | Make 3 edits (for example three Rolls). Press Undo three times, then **Redo** three times (button, then Ctrl+Shift+Z, then Ctrl+Y). | Each Redo brings back one edit, in order. The Redo button is greyed out when there is nothing to redo. | |
| R2 | Undo twice, then make a new edit. | Redo is greyed out (a new edit ends the redo branch). | |
| R3 | Undo on variation A, switch to B, switch back to A. | Redo still available on A. B has its own Undo and Redo. | |
| R4 | Hover over a slot in each lane (Brightness, Tension, Energy), without clicking. | A small label shows the target % and "(chord N%)", the measured value of the chord. | |
| R5 | Drag in each lane. | The label follows the slot and shows the value live. It disappears on release. | |
| R6 | Check the label does not hide something important and fits at the lane edges (first and last slot). | Readable, not cut off. | |
| R7 | Count Undo steps again from a fresh project (just Rolls on one variation). | Max 64. Write the number you get. | |
