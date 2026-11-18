#!/usr/bin/env python3
"""Listener rating sheets for the R7 blind panel. Standard library only.

One CSV per participant (P01.csv, P02.csv, ...). Each row is one item. Columns:
  participant, training, item, plausibility, would_use, notes

Item IDs are read from the midi folder only. The condition key (key.csv) is never read here and is not
referenced from the sheets. Each participant gets their own shuffled item order, from a seed of
"<seed>:<participant>", so the same --seed always gives the same sheets.

Usage:
  python3 make_rating_sheets.py --participants 6 --seed 7 --theory P01,P04 [--out-dir DIR]
"""

import argparse
import csv
import glob
import os
import random

COLUMNS = ["participant", "training", "item", "plausibility", "would_use", "notes"]
HERE = os.path.dirname(os.path.abspath(__file__))


def item_ids(midi_dir):
    files = sorted(glob.glob(os.path.join(midi_dir, "item*.mid")))
    ids = [os.path.splitext(os.path.basename(f))[0] for f in files]
    if not ids:
        raise SystemExit(f"no item*.mid files in {midi_dir}; run make_panel.py first")
    return ids


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--participants", type=int, required=True)
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--theory", default="", help="comma-separated participant IDs with music-theory training")
    ap.add_argument("--midi-dir", default=os.path.join(HERE, "midi"))
    ap.add_argument("--out-dir", default=None, help="default: sheets_seed<seed> next to this script")
    args = ap.parse_args()

    if args.participants < 1:
        ap.error("--participants must be at least 1")
    ids = item_ids(args.midi_dir)
    theory = {p.strip() for p in args.theory.split(",") if p.strip()}
    out_dir = args.out_dir or os.path.join(HERE, f"sheets_seed{args.seed}")
    os.makedirs(out_dir, exist_ok=True)

    for i in range(args.participants):
        pid = f"P{i + 1:02d}"
        training = "music theory" if pid in theory else "none"
        order = list(ids)
        random.Random(f"{args.seed}:{pid}").shuffle(order)
        path = os.path.join(out_dir, f"{pid}.csv")
        with open(path, "w", newline="", encoding="utf-8") as fh:
            writer = csv.writer(fh)
            writer.writerow(COLUMNS)
            for item in order:
                writer.writerow([pid, training, item, "", "", ""])
        print(f"wrote {path} ({len(order)} items, training: {training})")
    unknown = theory - {f"P{i + 1:02d}" for i in range(args.participants)}
    if unknown:
        print(f"warning: --theory names not in this session: {', '.join(sorted(unknown))}")


if __name__ == "__main__":
    main()
