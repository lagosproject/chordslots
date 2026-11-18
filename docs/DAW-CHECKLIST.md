# DAW verification checklist

See the [general plan](GENERAL-PLAN.md#work-order) for task 4, which this sheet covers. Task 1 (Mood menu header) is closed in the repository; its DAW check is recorded here.

Use this sheet with the compiled VST3 in a real host. Work through the steps in order, because later steps reuse the progression built earlier. Mark each step **Pass**, **Fail** or leave it open with a note. Write the observed value, not only "ok", when a number or a text is given.

Sources used for the expected results are cited in brackets. Where the repository does not state a behaviour, the expectation is marked **confirm**: record what happens and do not treat it as a failure until the expectation is agreed.

## Before the session

1. Build the Release VST3 from the commit under test (see the [README](../README.md#build)).

   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release --parallel
   ```

2. Copy `build/ChordSlots_artefacts/Release/VST3/ChordSlots.vst3` to the host's plugin folder, or to `~/.vst3` on Linux [README, Build table].
3. Record the commit with `git rev-parse --short HEAD` in the log at the end of this sheet.
4. Start a new empty project at a tempo of **120 BPM**, 4/4, 48 kHz. These are the conditions used in the MIDI parity audit [energy/REPORT.md, "MIDI output and preview"].
5. Create two MIDI tracks: **Track 1** with ChordSlots as the instrument, and **Track 2** empty with another instrument (for capture).
6. Set the ChordSlots plugin options to the audit defaults where the host allows it: Split off, Strum, Accent and Humanize at zero, Sustain bass [energy/REPORT.md, "MIDI output and preview"].

## Session steps

### A. Host and plugin identification

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| A1 | Fill in the host fields in the log below: host name, host version, OS and version, plugin format (VST3), plugin commit. | Fields are complete. | |
| A2 | Scan for plugins in the host and confirm **ChordSlots** appears once, as an instrument. | Single entry, instrument type. Plugin scan result [README, Build]. **confirm** the category shown by the host. | |
| A3 | Insert ChordSlots on Track 1 and open its editor. | The editor opens at 1040 × 650 px [mood-check/check.cpp, "Default editor and Mood bounds"]. Two tabs are visible: **Harmony** and **Groove & Feel** [README, "What it does"]. | |

### B. Tooltips and lane help

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| B1 | Hover over **Match**. Read the tooltip in full. | The text describes the score: per slot, the distance of measured Brightness and Tension from their targets, averaged with equal weight across slots; Energy has no term of its own and can move Match only if it changes the notes measured [energy/REPORT.md, "What the help describes"]. **confirm** the wording against the help. | |
| B2 | Hover over the **Brightness** lane (anywhere on it), then move to the value marker. | Text: guides chord choice and voicing register when regenerating a slot, followed by "The white marker shows the current value." [energy/REPORT.md; plugin/ui/LanesView.cpp `getTooltip`]. | |
| B3 | Hover over the **Tension** lane. | Text: guides slot regeneration toward a target level of harmonic tension in context [energy/REPORT.md]. | |
| B4 | Hover over the **Energy** lane. | Text: adjusts MIDI velocity and voicing density while keeping the chords; doublings depend on the voicing style and notes [energy/REPORT.md]. | |
| B5 | Hover over the **ROLL** button. | Text: "Generate new chords for all unlocked slots" [plugin/PluginEditor.cpp, `rollButton`]. **confirm** that the text matches the docs' description of Roll (README, "Shape harmony"). | |
| B6 | Hover over the **Undo** button. | Text reads "Undo (Variation X)", where X is the selected variation A–D [plugin/PluginEditor.cpp, `undoButton`]. The button is disabled when there is nothing to undo [same file]. | |
| B7 | Hover over the **LOCKS** caption. | Text: "Locked layers keep their values when you roll." [plugin/PluginEditor.cpp, `lockCaption`]. | |
| B8 | Hover over each lock button: **Rhythm**, **Roots**, **Colors**, **Voicing**. | Each describes what it freezes during Roll: rhythm and cut positions, chord roots, chord types and qualities, inversions and voice registers [plugin/PluginEditor.cpp, `LockDef`]. README says only that a lock keeps a slot or layer before rolling again [README, "Edit and keep good ideas"]. **confirm** the four texts. | |
| B9 | Hover over the **Play** and **Save MIDI** buttons and the **Drag MIDI**, **Chords**, **Bass** and **Drag Song** tiles. | Tooltips match the source: "Drag the current progression onto a MIDI track in your DAW." and the matching text for each tile [plugin/PluginEditor.cpp, lines around 745–748]. **confirm** these against what the host shows. | |
| B10 | Hover over an empty area of the timeline and over a separator, then over a chord. | Separator help: drag to move, double-click to lock, right-click for more options [plugin/ui/TimelineView.cpp]. Chord help: open slot actions, and toggle the slot lock to keep the chord on Roll [same file]. **confirm** wording. | |

### C. Mood menu

The menu header replaced the old text in task 1 [GENERAL-PLAN.md, work order row 1].

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| C1 | Open the **Mood** combo box. Read the section header at the top of the menu. | Header reads: "All presets replace Brightness, Tension and Energy target curves and generate a progression (also runs a Roll)" [plugin/PluginEditor.cpp, `moodBox` section header]. The text is the same as the task 1 wording "... and generate a progression (also runs a Roll)". | |
| C2 | Check the header is fully visible. Note the width of the menu and whether any item is cut off. | The menu fits the editor (the mood box is 100 px wide; the popup must fit within 1040 × 650) [mood-check/check.cpp, lines 61 and 92]. Record the observed width in pixels, or write "not measured". **confirm** that the header is not clipped in the host window. | |
| C3 | Hover over **Dreamy** (or any preset) in the list. | The preview line shows key, Adventure, Syncopation and the Auto scale. Example expected text for Dreamy: "Lydian  |  Adventure 60%  |  Syncopation 15%  |  Auto uses Extended harmony" [mood-check/check.cpp, line 97]. | |
| C4 | Set the transition option to **Auto**. Select **Euphoric**. | The progression is replaced by a new one (a Roll runs). Scale becomes major, per the fallback "Auto uses Major" [mood-check/check.cpp, line 97]. | |
| C5 | Select **Uplifting** in Auto. | The scale falls back to minor: "Auto uses Minor" [mood-check/check.cpp, line 98]. | |
| C6 | Change one target curve by hand, then open the Mood menu again. | The mood changes to **Custom** and has no preset ID [energy/REPORT.md, "What the help describes"; mood-check/check.cpp, lines 94 and 137]. **confirm** that editing Brightness or Tension also resets the mood to Custom. | |
| C7 | Select a preset, then choose the manual transition style and select the same preset again. | The preset replaces the curves and generates a new progression. Manual style is accepted [mood-check/check.cpp, lines 120 and 124; GENERAL-PLAN.md, Current status, "Moods" row]. **confirm** what the manual style changes in the host. | |
| C8 | Count how many presets produce different results when applied to the same empty progression. Note the count. | The docs do not give an expected count. The Moods row of the status table leaves the question open [GENERAL-PLAN.md, Current status]. Record the observation only. | |

### D. Editing slots with varied curves

Create a progression first: 4 to 8 slots over 4 bars, using a preset in Auto, then Roll once. Common conditions are in [energy/REPORT.md, "MIDI output and preview"]. Record the initial Match value.

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| D1 | Drag the **Brightness** lane on one interior slot upward, then release. | The edited slot regenerates; the mask passed to the engine contains only the edited slot [energy/REPORT.md, "What the help describes", Brightness and Tension bullet]. Other slots should keep their chords. **confirm** that unedited slots keep their chords. | |
| D2 | Drag the **Tension** lane on the same slot, then on the last slot. | The regenerated chord reflects the new target. Tension is measured including the link from the last slot to the first [README, "How the musical model works"; energy/REPORT.md]. | |
| D3 | Draw an **ascending** Brightness curve across the loop, then a **descending** one, then a **resolving** curve (high at the start, low at the end, then back). Release after each. | Each release updates only the edited slots. Match changes with the targets. Note the Match value after each curve. | |
| D4 | Move the **Energy** lane on one slot to a low value (10%), then to a high value (95%), with Piano voicing. | On release, all unlocked slots with a voicing are updated, not only the dragged one. Chords are kept; velocity and density change. Doublings may appear or disappear depending on voicing style and notes [energy/REPORT.md, "Energy" bullet and "Energy edit"]. The mood changes to Custom [same file]. | |
| D5 | Repeat D4 with the **Split** option on. | Upper chords are on MIDI channel 1 and the bass on channel 2 [README, "Give the bass its own part"]. At 95% the bass may lose its low doubling in Split mode [energy/REPORT.md, "Energy edit"]. Record whether the double octave is present. | |
| D6 | Drag an edited slot's Energy to 50%, then 60% and 61%, and watch the output. | Energy at 50% is the same operation as the other levels [energy/REPORT.md, "Test design"]. The audit covered 10, 25, 50, 59, 60, 61, 75, 84, 85, 86, 95 and 100%. Record whether the jump at 60–61% is audible. **confirm** whether any step at those values is audible. | |
| D7 | Check the **Match** percentage after D1–D6. | The Match value is an integer percentage [energy/REPORT.md, "What the help describes", Match bullet]. A pure Energy edit may leave Match unchanged, or move it only if the measured notes change. Record each value. | |
| D8 | Move a slot's cut (add a cut, then drag it). | The slot boundaries change. Confirm that the chords stay with their slots. **confirm** the behaviour against the docs, which describe cuts only in the Roll and the Rhythm lock [energy/REPORT.md, "Test design"]. | |

### E. Locks

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| E1 | Lock one slot (click the slot lock or press L). Press ROLL. | The locked slot keeps its chord; unlocked slots change [README, "Edit and keep good ideas"; plugin/ui/TimelineView.cpp, "Lock slot (L)"]. | |
| E2 | Turn on **Roots** and **Colors**. Press ROLL several times. | Root, chord type, symbolic bass interval, cuts and the Brightness and Tension targets stay the same in every slot. The chord is frozen [energy/REPORT.md, "Test design", locks paragraph]. | |
| E3 | With **Roots** and **Colors** on, drag Energy on one slot. | Chords stay the same; only velocity and density change [energy/REPORT.md, "Test design"]. | |
| E4 | Turn on **Voicing** (with Roots and Colors off), then press ROLL. | An intact chord keeps its voicing and only its base velocity is updated [energy/REPORT.md, "What the help describes", Energy bullet]. | |
| E5 | Turn on **Rhythm**, then press ROLL. | Slot cut positions stay where they are [plugin/PluginEditor.cpp, `LockDef` for Rhythm]. | |
| E6 | Turn all four locks off. Press ROLL. | All unlocked slots change. | |

### F. Undo, Redo and variations

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| F1 | Note the progression. Make an edit (D1). Press **Undo**. | The edit is reverted. The Undo button is disabled when there is nothing left to undo [plugin/PluginEditor.cpp, `undoButton`]. | |
| F2 | Press **Cmd/Ctrl+Z** after an edit. | Same result as the Undo button [plugin/PluginEditor.cpp, key handler for Undo]. | |
| F3 | Look for a **Redo** control and try Cmd/Ctrl+Shift+Z or Cmd/Ctrl+Y. | The docs mention only Undo [README, "Edit and keep good ideas"]. The source contains no Redo control in the editor. **confirm** that Redo is absent; record what happens. | |
| F4 | Select variation **B**, make an edit, then switch back to **A**. | Each variation keeps its own progression. The Undo tooltip changes to "Undo (Variation B)" [plugin/PluginEditor.cpp, `undoButton`]. Switching timing options are: immediately, on the next slot, or on the next bar [README, "Edit and keep good ideas"]. Record which option the host session uses and whether the switch follows it. | |
| F5 | Change the variation while playing. | The switch follows the chosen option (immediately, next slot or next bar). **confirm** that the switch happens at that boundary. | |

### G. MIDI capture on another track

The Drag tile labelled **Drag MIDI** is the Master export [plugin/PluginEditor.cpp, `dragTile`]. The plan calls it "Drag Master". The **Save MIDI** button writes an SMF file [plugin/PluginEditor.cpp, `saveButton`]. Use the same progression for each capture.

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| G1 | Set the host to record ChordSlots output on Track 2 (a record-arm or MIDI monitor route, or bounce to a MIDI clip). Play the loop from the host for one full cycle with the progression from D. Stop. | Track 2 contains the chords and bass as notes. Record the number of note-ons and the note range. **confirm** the host's capture method. | |
| G2 | Press **Save MIDI**. Save the file. Import it to Track 3 and compare it with Track 2. | The file has the same notes, start times, durations, velocities and channels as the captured track. The audit compared the same export path and found 0 differences in 288 cases with simulated transport [energy/REPORT.md, "MIDI output and preview" table]. **confirm** on the host. | |
| G3 | Drag the **Drag MIDI** tile onto Track 4 (empty). Compare with Track 2 and Track 3. | The dragged clip matches the saved SMF and the captured track, under the same conditions as G1 [energy/REPORT.md, "MIDI output and preview" table, "Internal Play with the host stopped" row]. | |
| G4 | Switch Split **on**, then repeat G1–G3. | Channel 1 carries the chords and channel 2 the bass [README, "Give the bass its own part"]. Compare the channel numbers in the capture with the file. The Chords and Bass tiles should appear [plugin/PluginEditor.cpp, lines 1290–1291]. | |
| G5 | Use **Drag Song** after building a Song chain. Compare with the Song export. | The clip includes chord and section markers [README, "Arrange a song"]. **confirm** the markers in the host. | |
| G6 | Repeat G1 with the host **stopped** and press **Play** in ChordSlots. Capture the output. | The internal Play produces the same notes as the host-driven capture under the same conditions. The audit matched both routes to the SMF [energy/REPORT.md, "MIDI output and preview" table, "Internal Play with the host stopped" row]. **confirm**. | |

### H. Transport start and stop

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| H1 | Start host playback from the beginning. | The plugin's notes start at the bar line, following the host position [README, "Perform and export": host playback follows tempo and position]. | |
| H2 | Stop the host, then start it again from a different bar. | Notes stop with the transport. When the transport restarts from a different bar, the notes follow the new position. **confirm**. | |
| H3 | Change the host tempo to 140 BPM while playing. | Timing follows the new tempo [README, "Perform and export"]. | |
| H4 | Press **Play** in ChordSlots with the host stopped. Press **Stop** (the label changes). | Notes play with the host stopped. The label reads "Stop" while playing and "Play" otherwise [plugin/PluginEditor.cpp, Play button]. The audit verified the events the synthesizer receives, not the audible result [energy/REPORT.md]. **confirm** that the synth is silent after Stop. | |
| H5 | Start the host while the internal Play is running. | **confirm** the expected behaviour. The docs do not state it. Record what happens, with any hanging notes. | |
| H6 | Check for stuck notes after stopping during a held chord. | No notes remain sounding after stop. **confirm**. | |

### I. Project save and reload

| # | Action | Expected result | Pass / Fail / Notes |
|---|---|---|---|
| I1 | Save the project with the progression from D, the locks from E, and a Custom mood. Note the Match value and the Save MIDI output. | Note the values. | |
| I2 | Close the project and reopen it. | The progression, slot targets, locks, variations and mood are restored. The plugin's own test suite checks save and restore [README, "Tests and useful tools"]. **confirm** each item in the host. | |
| I3 | After reload, compare the Match value and the Save MIDI output with I1. | Match and the MIDI output match I1. Record any difference. **confirm** whether the restored state is byte-identical or only musically identical. | |
| I4 | Reload the project in a second host session, or on another instance of the plugin if the host allows it. | Same result as I2. | |
| I5 | Save the project with ChordSlots on a track where the Split option is on, then reload. | The Split setting is restored [plugin/PluginProcessor.cpp, state keys]. **confirm**. | |

## Log to complete

| Field | Value |
|---|---|
| Person in charge and session date | |
| Host name and version | |
| OS and version | |
| Plugin format and build path | |
| Plugin commit (`git rev-parse --short HEAD`) | |
| Sample rate and tempo used | |
| Steps run and steps skipped | |
| Match values at D1, D3, D4 and after reload (I3) | |
| Tooltip texts that differ from the expected text | |
| Differences found between Save MIDI, Drag MIDI and the captured track | |
| Redo present (F3) | |
| Decision and reason | |
