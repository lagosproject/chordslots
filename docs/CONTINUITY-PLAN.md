# Brightness listening protocol

See the [general plan](GENERAL-PLAN.md) for the status and the order of work.

## Listening protocol with producers

**Goal.** Find out whether producers perceive the change in harmony, the register movement, or both as "brighter". Then check which change they find most useful when editing a loop.

### Before the session

1. Prepare a first exploratory round with **6–10 participants** and **8–12 pairs** per person, adjustable after checking the duration. Include people who use MIDI in their DAW and note their usual genre and production experience. The proposed size is enough to detect patterns and recurring comments.
2. Choose short loops and edit a single slot per example. Include **major and minor**, and a Dorian example if there is room. Cover an interior slot and the last one. Start from moderate increases and avoid selecting only the most favorable ones. If a case with a lot of Tension variation is used, mark it in advance as an additional contrast.
3. Prepare the following two contrasts and review the actual exported notes. If relevant differences in register or Tension remain in a harmony contrast, record them in the sheet and consider another example.

| Contrast | What changes | What is kept equal |
|---|---|---|
| **Harmony** | The chord of the edited slot, with two different values of the harmonic component. | Actual MIDI pitch of the upper voices, voice layout as far as possible, calculated Tension, instrument, rhythm, dynamics and listening level. The calculated Tension is used to select stimuli and can be compared afterwards with what listeners hear. |
| **Register** | The pitch height of the voicing of the same chord. | Pitch classes, rhythm, MIDI velocity and timbre. Match the listening level and note any change of inversion, bass or voice spacing. |

Render with the same instrument and tempo. Allow time to hear the return from the last chord to the first. Alternate the A/B order, mix the pair types and keep the variant names hidden. Run a practice example to check that the task is understood.

### During the session

After each pair:

1. **Which one sounds brighter to you?** A, B, no clear difference or don't know.
2. **What did you hear change?** Free answer in one sentence.
3. **Which one sounds more tense?** A, B, no clear difference or don't know. This question helps detect when the harmonic change carries along another sensation.

At the end, explain the two variants and ask what they would prefer to control from a bar when editing their own loop: **chord color**, **color and register together**, or **both separately**. If possible, let them try a short edit in the VST. The usage preference and the brightness response are recorded separately.

### After the session

- Summarize responses by contrast, person and example. Include ties, discrepancies by scale and comments that explain what people heard. Several responses from the same person still belong to the same listener.
- If the chord change is recognized with register matched and the producers want to control that part on its own, prototype **Harmonic color** with register as a voicing adjustment.
- If both changes are perceived as brightness and most of the usage comments favor the combined gesture, keep **Brightness** as an explicit macro.
- If the responses depend on the example, review the actual register, tension and voice movement in those cases. Choose a few additional contrasts before deciding on an interface change.

## Log to complete

| Field | Pending |
|---|---|
| Person in charge and session date | |
| VST version and commit | |
| Participants and experience | |
| Pair identifiers and order | |
| Instrument and render settings | |
| Location of audio, sheets and responses | |
| Perception and usage findings | |
| Decision and reason | |
