#!/usr/bin/env python3
"""Per-participant response sheets for the two-file listening tests: Brightness pairs and flag A/B pairs.

(The R10 tritone test has its own generator in r10/make_session_sheet.py, and the R7 panel in
blindpanel/make_rating_sheets.py.)

Design
- Every pair appears once per participant. Which file plays first is balanced across participants: with an even
  number of participants, each pair is heard A-first by exactly half of them.
- Trial order is shuffled per participant. For Brightness the two pair sets (original and matched) are mixed.
- Each participant gets one repeat trial (a pair heard earlier, reversed order, from trial 4 on) as a consistency
  check. The first answer is the one analysed for the pair; the repeat gives the agreement figure.
- Answers are recorded as first / second / same / unsure, as heard. Map them back to A/B with first_file and
  second_file at analysis time.
- Same arguments and seed give the same sheets.

Usage
  python3 make_pair_sheets.py brightness --participants N --seed S --out-dir DIR
  python3 make_pair_sheets.py flagab      --participants N --seed S --out-dir DIR

Sheets are operator-side paper: keep them, and the keys, out of the repository (the generator is deterministic).
"""

import argparse
import csv
import json
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))

KINDS = {
    "brightness": {
        "keys": [os.path.join(HERE, "brightness", "pairs.json"), os.path.join(HERE, "brightness", "pairs_matched.json")],
        "dir": "brightness/pairs",
        "answers": ["brighter", "lighter_open_use"],
    },
    "flagab": {
        "keys": [os.path.join(HERE, "flagab", "operator", "key.json")],
        "dir": "flagab/midi",
        "answers": ["fits_mood", "would_use"],
    },
}


def load_pairs(kind):
    """List of (pair_id, mood or '') in key order. Only fields a listener sheet may carry are returned."""
    pairs = []
    for path in KINDS[kind]["keys"]:
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
        rows = doc["pairs"] if isinstance(doc, dict) else doc
        for p in rows:
            pairs.append((p["pair_id"], p.get("mood", "")))
    return pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("kind", choices=sorted(KINDS))
    ap.add_argument("--participants", type=int, required=True)
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()
    if args.participants < 1:
        ap.error("--participants must be at least 1")

    cfg = KINDS[args.kind]
    pairs = load_pairs(args.kind)
    ids = [p for p, _ in pairs]
    mood_of = dict(pairs)
    os.makedirs(args.out_dir, exist_ok=True)

    master = random.Random(args.seed)
    n = args.participants
    orient = {}
    for pid in ids:
        o = ["AB"] * (n // 2) + ["BA"] * (n // 2)
        if n % 2:
            o.append(master.choice(["AB", "BA"]))
        master.shuffle(o)
        orient[pid] = o

    columns = ["participant", "training", "production_experience", "trial", "pair_id", "mood_label",
               "first_file", "second_file"] + cfg["answers"] + ["notes"]
    for i in range(n):
        label = f"P{i + 1:02d}"
        rng = random.Random(f"{args.seed}:{args.kind}:{label}")
        trials = [{"pair_id": p, "orient": orient[p][i], "notes": ""} for p in ids]
        rng.shuffle(trials)
        original = rng.choice(trials[:-3])
        reversed_orient = "BA" if original["orient"] == "AB" else "AB"
        repeat = {"pair_id": original["pair_id"], "orient": reversed_orient, "notes": ""}
        pos = rng.randint(max(trials.index(original) + 3, 3), len(trials))
        trials.insert(pos, repeat)
        repeat["notes"] = f"repeat check of trial {trials.index(original) + 1}"
        path = os.path.join(args.out_dir, f"{label}.csv")
        with open(path, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(columns)
            for t, tr in enumerate(trials, start=1):
                first, second = tr["orient"][0], tr["orient"][1]
                w.writerow([label, "", "", t, tr["pair_id"], mood_of[tr["pair_id"]],
                            f"{cfg['dir']}/{tr['pair_id']}_{first}.mid", f"{cfg['dir']}/{tr['pair_id']}_{second}.mid"]
                           + [""] * len(cfg["answers"]) + [tr["notes"]])
        print(f"wrote {path} ({len(trials)} trials)")


if __name__ == "__main__":
    main()
