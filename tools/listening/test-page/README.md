# Interactive listening test page

One file, `index.html`. Open it in Chrome or Edge (double-click, no server). Nothing is uploaded anywhere.

## Use

1. Make the listener sheets with the generators (commands in `../FACILITATOR.md`), one CSV per listener and block.
2. Open `index.html`. Pick the listener's **sheet CSV**, then the **folder with the sound files** (the `.mid` files, or rendered `.mp3`/`.wav` with the same base names). The page matches files by name, searches sub-folders, and tells you if any are missing. Pick the repo's `tools/listening` folder to cover all blocks at once.
3. Set training and production experience, then **Start**. Optional: **Practice trial** (a C chord then C7, not recorded).
4. The listener presses Play, hears first, 3 s of silence, second. One replay is allowed (logged as `replayed` in `notes`). The questions appear after the first full playback. **Next** unlocks only when both are answered.
5. At the end, **Download answers (CSV)**. The file keeps the sheet's columns and order and is named `<block>_<participant>.csv`, so `analyse_pairs.py` and `analyse_panel.py` read it unchanged (`python3 tools/listening/analyse_pairs.py r10 file1.csv file2.csv ...`).

Progress is saved in the browser after every answer. Reloading the page with the same sheet resumes where the listener stopped.

## Blocks it handles
r10 (tritone pairs), brightness pairs, flag A/B pairs (shows the mood label), and the R7 panel (two 1-7 ratings per item). The block is detected from the CSV columns.

## Producer mode
Tick it in setup. After both answers of a pair are given, two draggable chips appear ("Loop 1", "Loop 2") that drag the `.mid` file out under a neutral name (`loop_07_1.mid`), plus download links as a fallback. A drag is logged as `dragged` in `notes`. Dragging works through Chrome's `DownloadURL` drag type; whether Ableton accepts a drop straight from the browser window is not verified, so try one drag first (dropping onto Explorer, then into Ableton, also works).

## Sound
If a rendered audio file exists it is used. Otherwise the `.mid` is played with a small built-in additive synth: identical for every listener, but not a real piano. For the real session, render all files once with one piano and put the audio next to the MIDI files.

## Tested
- `node test_page_logic.js`: CSV parsing and export round-trip on all four generated sheet types, file matching, and the MIDI parser against an independent note count on 100 stimulus files.
- Headless Chrome with a scripted listener: setup, play, replay limit, answers, producer-mode drag box, finish, CSV output.
- Not tested: real audio output and its quality, the drag into Ableton, Edge/Firefox.
