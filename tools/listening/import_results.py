#!/usr/bin/env python3
"""Imports listening-test results into tools/listening/data/<kind>/<kind>_<id>.csv after validating them.

Accepts: the one-file results (.json) the page produces, a text file holding that JSON (pasted from an email),
or the four plain CSVs (r10_P03.csv, ...). Input is untrusted: ids are reduced to letters and digits, answers must
be in the allowed vocabulary, files are never executed. An id that is already imported is refused unless --force
(a returning participant who resumed sends a fuller file: use --force to replace the old one).

Usage: python3 tools/listening/import_results.py [--data DIR] [--force] FILE...
"""
import argparse, csv, io, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAIR = {"first", "second", "same", "unsure"}
KINDS = {
    "r10": (["participant", "training", "production_experience", "trial", "pair_id", "mood_label", "first_file", "second_file", "more_tense", "asks_to_resolve", "notes"], PAIR),
    "brightness": (["participant", "training", "production_experience", "trial", "pair_id", "mood_label", "first_file", "second_file", "brighter", "lighter_open_use", "notes"], PAIR),
    "flagab": (["participant", "training", "production_experience", "trial", "pair_id", "mood_label", "first_file", "second_file", "fits_mood", "would_use", "notes"], PAIR),
    "panel": (["participant", "training", "item", "plausibility", "would_use", "notes"], {str(i) for i in range(1, 8)}),
}
ANSWER_COLS = {"r10": 2, "brightness": 2, "flagab": 2, "panel": 2}


def clean_id(s):
    s = re.sub(r"[^A-Za-z0-9]", "", str(s)).upper()
    return s[:13] if s.startswith("P") else ("P" + s)[:13]


def validate(kind, text):
    """Return (rows, problems). rows are dicts; every problem is a string."""
    header, vocab = KINDS[kind]
    rd = csv.DictReader(io.StringIO(text, newline=""))
    if rd.fieldnames != header:
        return None, [f"{kind}: columns are {rd.fieldnames}, expected {header}"]
    rows, problems, ids = [], [], set()
    for i, r in enumerate(rd, start=2):
        ids.add(r["participant"])
        for c in header[-3:-1]:
            if r[c] != "" and r[c] not in vocab:
                problems.append(f"{kind} line {i}: {c}={r[c]!r} not allowed")
        rows.append(r)
    if len(ids) != 1:
        problems.append(f"{kind}: expected one participant, found {sorted(ids)}")
    if not rows:
        problems.append(f"{kind}: no rows")
    return rows, problems


def answered(kind, rows):
    cols = KINDS[kind][0][-3:-1]
    return sum(1 for r in rows if all(r[c] for c in cols))


def load(path):
    """Yield (kind, csv_text) for one input file."""
    raw = open(path, encoding="utf-8-sig", newline="").read()
    if raw.lstrip().startswith("{"):
        doc = json.loads(raw)
        if doc.get("format") != "cs-listening-1" or not isinstance(doc.get("blocks"), dict):
            raise ValueError("not a cs-listening-1 results file")
        for kind, text in doc["blocks"].items():
            if kind in KINDS and isinstance(text, str):
                yield kind, text
        return
    m = re.match(r"(r10|brightness|flagab|panel)_.*\.csv$", os.path.basename(path))
    if not m:
        raise ValueError("unrecognised file (expected .json results or kind_ID.csv)")
    yield m.group(1), raw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--data", default=os.path.join(HERE, "data"))
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args()
    bad = 0
    for path in a.files:
        try:
            parts = list(load(path))
        except Exception as e:
            print(f"SKIP {path}: {e}"); bad += 1; continue
        for kind, text in parts:
            rows, problems = validate(kind, text)
            if problems:
                print(f"SKIP {path} [{kind}]: " + "; ".join(problems[:3])); bad += 1; continue
            pid = clean_id(rows[0]["participant"])
            dest = os.path.join(a.data, kind, f"{kind}_{pid}.csv")
            if os.path.exists(dest) and not a.force:
                print(f"SKIP {path} [{kind}]: {pid} already imported (use --force to replace)"); bad += 1; continue
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "w", encoding="utf-8", newline="") as fh:
                fh.write(text)
            print(f"OK   {pid} {kind}: {answered(kind, rows)}/{len(rows)} answered -> {os.path.relpath(dest)}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
