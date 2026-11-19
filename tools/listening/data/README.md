# Received listening results

Imported answers, one CSV per participant and block (`<kind>/<kind>_<ID>.csv`). Anonymous: a random ID, training, production experience and answers.

```
python3 tools/listening/import_results.py FILE...     # .json from the page, text pasted into a file, or plain CSVs
python3 tools/listening/recompute.py                  # recompute everything, with the minimum-N warning
```

`EXCLUDE.txt` (optional, one id per line, `#` for comments) drops participants from `recompute.py` without deleting them.
Fewer than 5 participants: trend only, no flag and no weight is proposed.
