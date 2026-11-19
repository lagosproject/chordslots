#!/usr/bin/env python3
"""Recomputes every listening analysis from tools/listening/data/ and says how much to trust it.

Usage: python3 tools/listening/recompute.py [--data DIR]

Rules fixed BEFORE any data (so that checking again after every new file does not change what counts):
  - MIN_PARTICIPANTS = 5 independent participants with at least half their trials answered. Below that, every result is
    a TREND ONLY and no flag or weight is proposed, whatever the percentages look like.
  - At least 1 participant with music-theory training is wanted for the tritone and brightness conclusions; the report
    warns when there is none.
  - data/EXCLUDE.txt (one id per line, '#' comments) removes participants, e.g. the pilot. Decide that rule once, not
    after seeing which way the pilot points.
  - A decision means the preset thresholds in analyse_pairs.py (tritone 75%, brightness 65/35%, flags 70% on both
    questions). They are evidence for YOU to approve each flag, not an automatic switch.
"""
import argparse, csv, os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
MIN_PARTICIPANTS = 5


def read_exclude(data):
    p = os.path.join(data, "EXCLUDE.txt")
    if not os.path.exists(p):
        return set()
    return {l.split("#")[0].strip().upper() for l in open(p, encoding="utf-8") if l.split("#")[0].strip()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default=os.path.join(HERE, "data"))
    a = ap.parse_args()
    excl = read_exclude(a.data)
    tmp = tempfile.mkdtemp(prefix="cs-recompute-")
    summary, theory = {}, set()
    try:
        for kind in ("r10", "brightness", "flagab", "panel"):
            d = os.path.join(tmp, kind); os.makedirs(d)
            people = set()
            src = os.path.join(a.data, kind)
            for f in sorted(os.listdir(src)) if os.path.isdir(src) else []:
                if not f.endswith(".csv"):
                    continue
                rows = list(csv.DictReader(open(os.path.join(src, f), encoding="utf-8", newline="")))
                if not rows or rows[0]["participant"].upper() in excl:
                    continue
                cols = list(rows[0].keys())[-3:-1]
                done = sum(1 for r in rows if all(r[c] for c in cols))
                if done * 2 < len(rows):
                    continue
                shutil.copy(os.path.join(src, f), d)
                people.add(rows[0]["participant"])
                if rows[0].get("training") == "music theory":
                    theory.add(rows[0]["participant"])
            summary[kind] = people
        print("PARTICIPANTS USED (at least half of the trials answered; excluded: %s)" % (", ".join(sorted(excl)) or "none"))
        for k, v in summary.items():
            print(f"  {k:11s} {len(v)}  ({', '.join(sorted(v)) or '-'})")
        print(f"  with music-theory training: {len(theory)}")
        print(f"  minimum for any decision: {MIN_PARTICIPANTS}\n")
        for kind in ("r10", "brightness", "flagab"):
            n = len(summary[kind])
            print("=" * 70)
            if n < MIN_PARTICIPANTS:
                print(f"{kind}: {n} participants (< {MIN_PARTICIPANTS}): TREND ONLY, no proposal, whatever the verdict line says.")
            elif not theory and kind in ("r10", "brightness"):
                print(f"{kind}: {n} participants but none with music-theory training: read the verdict with that caveat.")
            if n:
                r = subprocess.run([sys.executable, "-I", os.path.join(HERE, "analyse_pairs.py"), kind, os.path.join(tmp, kind)], capture_output=True, text=True)
                print(r.stdout + r.stderr)
        print("=" * 70)
        n = len(summary["panel"])
        if n < MIN_PARTICIPANTS:
            print(f"panel: {n} judges (< {MIN_PARTICIPANTS}): TREND ONLY.")
        if n:
            key = os.path.join(HERE, "blindpanel", "operator", "key.csv")
            r = subprocess.run([sys.executable, "-I", os.path.join(HERE, "blindpanel", "analyse_panel.py"), os.path.join(tmp, "panel"), "--key", key], capture_output=True, text=True)
            print(r.stdout + r.stderr)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
