#!/usr/bin/env python3
"""Create per-participant randomised response sheets for the R10 listening test.

Reads pairs.json (written by make_stimuli.py) and writes one CSV per participant.

Design
- Each pair appears once per participant, plus one deliberate repeat trial per
  participant: a pair they already heard, presented again in reversed order.
- Presentation order (A first or B first) is counterbalanced across participants:
  for every pair, with an even number of participants, exactly half hear A first.
  Odd N: the extra participant gets a random orientation.
- Trial order within a participant is shuffled. The repeat trial is inserted at a
  random position from trial 3 onward.
- Same --participants and --seed always give the same sheets.

Usage: python3 make_session_sheet.py --participants N --seed S [--pairs-json PATH] [--out-dir DIR]
"""

import argparse
import csv
import json
import os
import random

COLUMNS = [
    "participant", "training", "production_experience", "trial", "pair_id",
    "first_file", "second_file", "more_tense", "asks_to_resolve", "notes",
]


def orientations(pair_ids, n, rng):
    """For each pair, a list of n orientations ('AB' or 'BA') that is balanced."""
    table = {}
    for pid in pair_ids:
        orient = ["AB"] * (n // 2) + ["BA"] * (n // 2)
        if n % 2:
            orient.append(rng.choice(["AB", "BA"]))
        rng.shuffle(orient)
        table[pid] = orient
    return table


def file_name(pair_id, letter):
    return f"r10_{pair_id}_{letter}.mid"


def files_for(pair_id, orient):
    first, second = (orient[0], orient[1])
    return file_name(pair_id, first), file_name(pair_id, second)


def build_trials(pair_ids, participant_index, table, rng):
    """Return a list of dicts: pair_id, orient, notes for one participant."""
    trials = [{"pair_id": p, "orient": table[p][participant_index], "notes": ""}
              for p in pair_ids]
    rng.shuffle(trials)

    # The original must leave room for a non-adjacent repeat after it.
    original = rng.choice(trials[:len(trials) - 2])
    reversed_orient = "BA" if original["orient"] == "AB" else "AB"
    repeat = {"pair_id": original["pair_id"], "orient": reversed_orient, "notes": ""}
    # Insert from trial 3 onward so the repeat is never adjacent to its original.
    orig_idx = trials.index(original)
    low = max(orig_idx + 2, 2)
    pos = rng.randint(low, len(trials))
    trials.insert(pos, repeat)
    orig_pos = trials.index(original) + 1
    repeat["notes"] = f"repeat check of trial {orig_pos}"
    return trials


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--participants", type=int, required=True)
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--pairs-json", default=os.path.join(here, "pairs.json"))
    ap.add_argument("--out-dir", default=None,
                    help="default: sheets_seed<seed> next to this script")
    args = ap.parse_args()

    if args.participants < 1:
        ap.error("--participants must be at least 1")

    with open(args.pairs_json, encoding="utf-8") as fh:
        pair_ids = [p["pair_id"] for p in json.load(fh)["pairs"]]

    out_dir = args.out_dir or os.path.join(here, f"sheets_seed{args.seed}")
    os.makedirs(out_dir, exist_ok=True)

    master = random.Random(args.seed)
    table = orientations(pair_ids, args.participants, master)

    for i in range(args.participants):
        pid_label = f"P{i + 1:02d}"
        rng = random.Random(f"{args.seed}:{pid_label}")
        trials = build_trials(pair_ids, i, table, rng)
        path = os.path.join(out_dir, f"{pid_label}.csv")
        with open(path, "w", newline="", encoding="utf-8") as fh:
            writer = csv.writer(fh)
            writer.writerow(COLUMNS)
            for t, trial in enumerate(trials, start=1):
                first, second = files_for(trial["pair_id"], trial["orient"])
                writer.writerow([
                    pid_label, "", "", t, trial["pair_id"], first, second,
                    "", "", trial["notes"],
                ])
        print(f"wrote {path} ({len(trials)} trials)")


if __name__ == "__main__":
    main()
