# Response format for an interactive listening test

If you build the listening session as an HTML page, make it **export the same CSV files that the paper sheets use**. Then `analyse_pairs.py` (blocks 1, 2, 4) and `blindpanel/analyse_panel.py` (block 3) work unchanged.

## Design calls

1. **Do not play MIDI in the browser.** Browsers have no built-in piano, so each listener would hear a different sound. Render every MIDI file once to audio (WAV or MP3) with one piano instrument, for example in Ableton with one piano on one track, export per file. The page then plays plain audio files. Same sound for everybody is a condition of the test.
2. **Reuse the generated sheets for order.** Do not randomise in the page. Run the generators (commands in `FACILITATOR.md`), and let the page load one sheet CSV per listener. That keeps the A/B balance and the repeat trials that the analysis expects. The page shows `first_file` then `second_file` and fills in the answer columns.
3. **Never show anything from the keys** (`pairs.json`, `pairs_matched.json`, `key.json`, `key.csv`) or the `mood_label` of pairs outside block 4. File names are neutral, but serve the audio under neutral names (for example `t01_first.mp3`, `t01_second.mp3`) so the name never leaks the pair or the order.
4. **Allow one replay per pair** and write `replayed` in `notes`. Always offer the four answers *First / Second / Same / Not sure*, so that "no difference" is easy to give.
5. **Save as you go** (download or local storage), so a closed tab does not lose a listener.

## Columns to export (one CSV per listener and block, same header as the generated sheet)

| Block | File name | Columns the page must fill |
|---|---|---|
| 1 Tritone (r10) | `P01.csv` in `r10/` | `more_tense`, `asks_to_resolve`, `notes` |
| 2 Brightness | `P01.csv` in `brightness/` | `brighter`, `lighter_open_use`, `notes` |
| 3 Progressions (R7) | `P01.csv` in `panel/` | `plausibility` (1-7), `would_use` (1-7), `notes` |
| 4 Mood pairs (flag A/B) | `P01.csv` in `flagab/` | `fits_mood`, `would_use`, `notes` |

- Answer values for blocks 1, 2 and 4 are exactly: `first`, `second`, `same`, `unsure`. Ratings in block 3 are whole numbers 1 to 7.
- Fill `training` (`music theory` or `none`) and `production_experience` for every row of the listener's sheet. Keep the other columns as generated.
- Keep the original header line and the row order of the sheet.
- Do not change `participant`, `pair_id`, `first_file`, `second_file` or `item`.

## Run the analysis

```
python3 tools/listening/analyse_pairs.py r10        <folder with the r10 CSVs>
python3 tools/listening/analyse_pairs.py brightness <folder>
python3 tools/listening/analyse_pairs.py flagab     <folder>
python3 tools/listening/blindpanel/analyse_panel.py <folder> --key tools/listening/blindpanel/operator/key.csv
python3 tools/listening/test_analyse_pairs.py       # self-test with simulated listeners
```

The decision rules (what share of listeners triggers a proposal) are written at the top of `analyse_pairs.py` and were fixed before any data. With 3 to 6 listeners this is a screen: only a large effect fires a rule. A small effect means "keep the default", not "no effect".
