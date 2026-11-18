# Listening session: facilitator script

For whoever runs the session (it does not need to be a musician). Four blocks, in this order. Read the wording exactly; do not explain what differs between files, and never use theory words (tritone, dominant, resolve, minor, major) unless the listener says them first.

## Before the session

- **One sound for everyone.** Play all MIDI files through the same piano instrument (General MIDI program 0), the same device, the same headphones and a fixed volume that is comfortable. Best: render every file once to audio with that instrument and play the audio. Do not let a DAW or player pick a different default sound between listeners.
- **Sheets.** One per listener and block, generated beforehand (commands below). Fill in `training` (`music theory` or `none`) and `production_experience` on each sheet. Keep the keys (`pairs.json`, `pairs_matched.json`, `r10/pairs.json`, `blindpanel/operator/key.csv`, `flagab/operator/key.json`) out of sight.
- **Even number of listeners** if possible, so every pair is heard first-A and first-B equally often.
- **Time (estimate, not measured).** Block 1 about 5 min, block 2 about 15 to 20, block 3 about 12 to 15, block 4 about 8 to 10, plus practice and a break: 45 to 60 min. If that is too long, run block 1 and 2 in one sitting and 3 and 4 in another on another day. Pilot with one person first and write down the real time.
- **Pilot person** also tells you if the wording is understood. Do not use their data.

```
python3 tools/listening/r10/make_session_sheet.py --participants N --seed S --out-dir <outside repo>/r10
python3 tools/listening/make_pair_sheets.py brightness --participants N --seed S --out-dir <outside repo>/brightness
python3 tools/listening/blindpanel/make_rating_sheets.py --participants N --seed S --theory P01,P04 --out-dir <outside repo>/panel
python3 tools/listening/make_pair_sheets.py flagab --participants N --seed S --out-dir <outside repo>/flagab
```

## Say at the start

"You will hear short chord loops on a piano. There are no right or wrong answers, and I cannot tell you what to listen for. I am testing the music, not you. Answer from your first impression. If you do not hear a difference, say so, that is a useful answer. You can ask me to play a pair again once."

Pairs: play the first file, leave about one bar (3 seconds) of silence, play the second file, then ask. One replay per pair at most; write "replayed" in `notes`.

## Practice (do not record)

Use two short piano chords from outside the test, for example C then C7 played slowly, and ask the Block 1 question 1. If the listener hesitates, say: "Choose A, B, the same, or unsure, whichever fits." Then start.

## Block 1: tritone pairs (r10 sheet)

5 trials (one is a repeat). After each, ask both questions, exactly:

1. "Which of the two sounds more tense? First, second, the same, or unsure."
2. "Which of the two makes you want to hear something else next, more than the other? First, second, the same, or unsure."

## Block 2: brightness pairs (brightness sheet, 22 trials plus a repeat)

After each pair ask, exactly:

1. "Which sounds brighter? First, second, the same, or unsure."
2. "Which would you use for a lighter, more open feel? First, second, the same, or unsure."

If asked what "brighter" means: "However you hear it. Brighter against darker." Do not say pitch, chord or key. If still unclear, record `unsure`. Write down any spontaneous comment ("this one is higher", "this one sounds happier") in `notes`; these are valuable.

Short break here (2 minutes, water).

## Block 3: progression ratings (R7 sheet, 24 items)

Play each item once (they may ask for one replay). After each:

- "How plausible is this as a chord progression for a song? 1 is not at all, 7 is very."
- "Would you use it as a starting point for your own music? 1 is never, 7 is certainly."

Say nothing about who or what made the items. If asked: "A mix of sources, I cannot say which." The theory-trained listener also gets the extra task below.

## Block 4: mood pairs (flagab sheet, 12 trials plus a repeat)

Before each pair, read the label printed on the sheet (`mood_label`): "These two loops are meant to sound [label]." Then play the pair and ask:

1. "Which sounds more [label]? First, second, the same, or unsure."
2. "Which would you rather use for a track that should feel [label]? First, second, the same, or unsure."

## Extra for the theory-trained listener (about 10 minutes, after the blocks)

Give them the assertions listed in `docs/audits/r1-tension-assertions.md` (TH1 to TH8 and the debatable ones) and ask them to mark each as "agree / disagree / depends", and to say if the Roman-numeral, tonic and dominant conventions in `tests/core_tests.cpp` look right. This is review, not listening. Also ask: "Which of the 24 progressions in block 3 sounded like a real song to you, and why?" Write the answers verbatim.

## After the session

- Make sure each sheet has the participant label, training, production experience and the real time taken.
- Do not tell listeners which file was which until everyone has finished.
- Send the filled sheets as they are, with your notes on anything odd (a loud noise, someone who did not understand a question, a pair played twice). Missing answers stay blank.
