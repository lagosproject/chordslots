#!/usr/bin/env python3
"""Analyse filled two-file listening sheets: R10 tritone, Brightness pairs, flag A/B pairs. Standard library only.

Input: the CSV sheets made by r10/make_session_sheet.py and make_pair_sheets.py (or exported by an interactive test
with the same columns, see tools/listening/RESPONSE-FORMAT.md). Keys are read from the operator files here, never by
the sheet generators.

  python3 analyse_pairs.py r10        SHEET_DIR_OR_FILES...
  python3 analyse_pairs.py brightness SHEET_DIR_OR_FILES...
  python3 analyse_pairs.py flagab     SHEET_DIR_OR_FILES...

Answers are first / second / same / unsure. They are mapped back to file A or B with first_file and second_file.
Rows with a blank answer are skipped. The first answer of a repeated pair is analysed; the repeat gives a
consistency figure.

DECISION RULES (fixed before any data, so they cannot be bent afterwards). With 3 to 6 listeners this is a screen,
not a trial: a rule fires only when the effect is large. "Decisive" means an answer other than same / unsure.
  R10 tritone: propose kTritoneWeight 0.05 only if, on the isolated pair G7/Gmaj7 and the two cadence pairs pooled,
      the tritone file is chosen "more tense" in >= 75 % of at least 10 decisive answers AND "same" is under 50 %
      of all answers. Otherwise keep 0. Pair 2 (Bdim/Bm7b5) is reported but never used (does not isolate the tritone).
  Brightness: register effect = share choosing the higher register as brighter on register pairs; colour effect =
      share choosing the brighter-colour file on colour_small pairs (same engine gap as register pairs). On conflict
      pairs, the register-win share decides which cue dominates: >= 65 % register dominates, <= 35 % colour
      dominates, otherwise neither (keep the weights). Needs >= 8 decisive answers per group.
  Flag A/B: propose a flag only if the flag-on file is chosen in >= 70 % of >= 8 decisive answers for BOTH
      questions (fits the mood, would use), for that flag's pairs pooled. A flag below 40 % is proposed to stay off
      for good. Everything between: keep off and note it. The user approves each flag in any case.
"""

import argparse
import csv
import glob
import json
import math
import os
import re
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))

ANSWER_ALIASES = {
    "first": "first", "1": "first", "f": "first",
    "second": "second", "2": "second", "s": "second",
    "same": "same", "equal": "same", "=": "same", "no difference": "same",
    "unsure": "unsure", "?": "unsure", "dont know": "unsure", "don't know": "unsure", "dk": "unsure",
}

KINDS = {
    "r10": {"questions": ("more_tense", "asks_to_resolve"),
            "keys": [os.path.join(HERE, "r10", "pairs.json")]},
    "brightness": {"questions": ("brighter", "lighter_open_use"),
                   "keys": [os.path.join(HERE, "brightness", "pairs.json"),
                            os.path.join(HERE, "brightness", "pairs_matched.json")]},
    "flagab": {"questions": ("fits_mood", "would_use"),
               "keys": [os.path.join(HERE, "flagab", "operator", "key.json")]},
}


# --------------------------------------------------------------------------- statistics

def sign_test_p(k, n):
    """Exact two-sided binomial p-value for k successes of n at p = 0.5."""
    if n == 0:
        return float("nan")
    k = max(k, n - k)
    tail = sum(math.comb(n, i) for i in range(k, n + 1)) / 2 ** n
    return min(1.0, 2 * tail)


def wilson(k, n, z=1.96):
    if n == 0:
        return (float("nan"), float("nan"))
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return (max(0.0, c - h), min(1.0, c + h))


def fmt_share(k, n):
    if n == 0:
        return "n/a (0 decisive)"
    lo, hi = wilson(k, n)
    return f"{k}/{n} = {100 * k / n:.0f}% (95% CI {100 * lo:.0f}-{100 * hi:.0f}%), sign-test p = {sign_test_p(k, n):.3f}"


# --------------------------------------------------------------------------- reading

def letter_of(name):
    m = re.search(r"_([AB])\.mid$", name.strip())
    if not m:
        raise SystemExit(f"cannot read the A/B letter from file name {name!r}")
    return m.group(1)


def collect(paths):
    files = []
    for p in paths:
        files.extend(sorted(glob.glob(os.path.join(p, "*.csv"))) if os.path.isdir(p) else [p])
    if not files:
        raise SystemExit("no sheet files found")
    return files


def read_trials(files, questions):
    """Yield dicts: participant, training, pair_id, first, second, repeat (bool), answers {question: first/second/same/unsure}."""
    rows = []
    for path in files:
        with open(path, newline="", encoding="utf-8") as fh:
            for r in csv.DictReader(fh):
                ans = {}
                for q in questions:
                    raw = (r.get(q) or "").strip().lower()
                    if raw == "":
                        continue
                    if raw not in ANSWER_ALIASES:
                        raise SystemExit(f"{path}: unknown answer {raw!r} for {q} (use first/second/same/unsure)")
                    ans[q] = ANSWER_ALIASES[raw]
                if not ans:
                    continue
                rows.append({
                    "participant": r["participant"],
                    "training": (r.get("training") or "").strip().lower() or "unknown",
                    "pair_id": r["pair_id"],
                    "first": letter_of(r["first_file"]),
                    "repeat": "repeat" in (r.get("notes") or "").lower(),
                    "answers": ans,
                })
    return rows


def to_letter(answer, first_letter):
    """first/second -> 'A'/'B'; same/unsure kept."""
    if answer == "first":
        return first_letter
    if answer == "second":
        return "B" if first_letter == "A" else "A"
    return answer


def split_original_repeat(rows):
    seen, originals, repeats = set(), [], []
    for r in rows:  # file order = trial order within a sheet
        key = (r["participant"], r["pair_id"])
        if r["repeat"] and key in seen:
            repeats.append(r)
        elif key not in seen:
            seen.add(key)
            originals.append(r)
        # a second non-repeat row for the same pair is ignored (should not exist)
    return originals, repeats


def consistency(originals, repeats, question):
    first = {(r["participant"], r["pair_id"]): to_letter(r["answers"][question], r["first"])
             for r in originals if question in r["answers"]}
    agree = total = 0
    for r in repeats:
        k = (r["participant"], r["pair_id"])
        if k in first and question in r["answers"]:
            total += 1
            agree += first[k] == to_letter(r["answers"][question], r["first"])
    return agree, total


def tally(items):
    """items: list of letters/same/unsure already expressed as 'target'/'other'/'same'/'unsure'."""
    c = defaultdict(int)
    for i in items:
        c[i] += 1
    return c


def by_training(rows_scored):
    out = defaultdict(lambda: defaultdict(int))
    for training, outcome in rows_scored:
        out[training][outcome] += 1
    return out


def report_group(title, scored, lines):
    """scored: list of (training, outcome) with outcome in target/other/same/unsure. Returns (k_target, n_decisive, n_all, n_same)."""
    c = tally(o for _, o in scored)
    n_dec = c["target"] + c["other"]
    n_all = sum(c.values())
    lines.append(f"- {title}: {fmt_share(c['target'], n_dec)}; same {c['same']}, unsure {c['unsure']} of {n_all}")
    for tr, cc in sorted(by_training(scored).items()):
        d = cc["target"] + cc["other"]
        lines.append(f"    - training '{tr}': target {cc['target']}/{d} decisive, same {cc['same']}, unsure {cc['unsure']}")
    return c["target"], n_dec, n_all, c["same"]


def score(rows, qkey, target_letter_of):
    """Score each original row for question qkey. target_letter_of(pair_id) -> letter whose choice counts as 'target'."""
    scored = []
    for r in rows:
        if qkey not in r["answers"]:
            continue
        t = target_letter_of(r["pair_id"])
        if t is None:
            continue
        a = to_letter(r["answers"][qkey], r["first"])
        outcome = "target" if a == t else ("other" if a in ("A", "B") else a)
        scored.append((r["training"], outcome))
    return scored


def load_pairs(kind):
    pairs = {}
    for path in KINDS[kind]["keys"]:
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
        for p in doc["pairs"]:
            pairs[p["pair_id"]] = p
    return pairs


# --------------------------------------------------------------------------- analyses

def analyse_r10(rows, lines):
    pairs = load_pairs("r10")
    originals, repeats = split_original_repeat(rows)
    q_tense, q_res = KINDS["r10"]["questions"]
    tritone_letter = {}
    for pid, p in pairs.items():
        t = [k for k, v in p["files"].items() if v["contains_tritone"]]
        tritone_letter[pid] = t[0] if len(t) == 1 else None  # pair 2: both contain it
    lines.append("## R10: tritone pairs\n")
    pooled = []
    for pid, p in pairs.items():
        lines.append(f"### {pid}: {p['label']} ({p['type']})")
        if tritone_letter[pid] is None:
            lines.append("Both files contain a tritone: this pair tests the added seventh, not the tritone. Reported, never used in the rule.")
            tl = lambda _p: "A"  # arbitrary: A is the dim triad
            lines.append("(Counts below are for file A = " + p["files"]["A"]["name"] + ".)")
        else:
            tl = lambda _p, pid=pid: tritone_letter[pid]
        for q, qname in ((q_tense, "more tense"), (q_res, "asks to resolve")):
            sc = score([r for r in originals if r["pair_id"] == pid], q, tl)
            report_group(f"{qname}", sc, lines)
            if q == q_tense and tritone_letter[pid] is not None:
                pooled.extend(sc)
        lines.append("")
    c = tally(o for _, o in pooled)
    dec = c["target"] + c["other"]
    n_all = sum(c.values())
    lines.append("### Pooled rule (pairs 1, 3, 4; 'more tense')")
    lines.append(f"- tritone file chosen more tense: {fmt_share(c['target'], dec)}; 'same' {c['same']} of {n_all}")
    same_share = c["same"] / n_all if n_all else 0.0
    if dec >= 10 and c["target"] / dec >= 0.75 and same_share < 0.5:
        verdict = "PROPOSE kTritoneWeight = 0.05 (rule met). The user approves."
    else:
        why = []
        if dec < 10:
            why.append(f"only {dec} decisive answers (need 10)")
        elif c["target"] / dec < 0.75:
            why.append(f"share {100 * c['target'] / dec:.0f}% below 75%")
        if same_share >= 0.5:
            why.append(f"'same' is {100 * same_share:.0f}% of answers")
        verdict = "KEEP kTritoneWeight = 0 (" + "; ".join(why) + ")."
    lines.append(f"- **Verdict: {verdict}**\n")
    for q in (q_tense, q_res):
        a, t = consistency(originals, repeats, q)
        lines.append(f"Repeat consistency for {q}: {a}/{t}" if t else f"Repeat consistency for {q}: no repeats answered")


def analyse_brightness(rows, lines):
    pairs = load_pairs("brightness")
    originals, repeats = split_original_repeat(rows)
    q_b, q_use = KINDS["brightness"]["questions"]
    lines.append("## Brightness pairs\n")

    def bright_target(pid):  # engine-brighter file
        return pairs[pid]["higher_brightness_file"]

    def register_target(pid):
        return pairs[pid]["higher_register_file"]

    groups = {
        "register": ("higher register chosen as brighter", register_target),
        "colour": ("brighter-colour file chosen as brighter (large engine gap)", bright_target),
        "colour_small": ("brighter-colour file chosen as brighter (same gap as register pairs)", bright_target),
        "conflict": ("HIGHER REGISTER chosen as brighter (register wins)", register_target),
    }
    verdict_data = {}
    for gname, (desc, tl) in groups.items():
        ids = [pid for pid, p in pairs.items() if p["type"] == gname]
        for q, qn in ((q_b, "brighter"), (q_use, "lighter/open use")):
            sc = score([r for r in originals if r["pair_id"] in ids], q, tl)
            lines.append(f"### {gname} pairs ({len(ids)}), {qn}: {desc}")
            k, n, _, _ = report_group("pooled", sc, lines)
            if q == q_b:
                verdict_data[gname] = (k, n)
            lines.append("")
    lines.append("### Rule")
    for g in ("register", "colour_small"):
        k, n = verdict_data.get(g, (0, 0))
        lines.append(f"- {g} effect: {fmt_share(k, n)}")
    k, n = verdict_data.get("conflict", (0, 0))
    if n < 8:
        verdict = f"NO DECISION: only {n} decisive conflict answers (need 8). Keep the Brightness weights."
    elif k / n >= 0.65:
        verdict = "REGISTER dominates colour: do not lower the register term; consider raising it. The user decides the weights."
    elif k / n <= 0.35:
        verdict = "COLOUR dominates register: consider lowering the register term (and the 57 + 14*B voicing centre coupling). The user decides."
    else:
        verdict = "NEITHER cue clearly dominates: keep the Brightness weights."
    lines.append(f"- conflict pairs, register wins: {fmt_share(k, n)}")
    lines.append(f"- **Verdict: {verdict}**\n")
    for q in (q_b, q_use):
        a, t = consistency(originals, repeats, q)
        lines.append(f"Repeat consistency for {q}: {a}/{t}" if t else f"Repeat consistency for {q}: no repeats answered")


def analyse_flagab(rows, lines):
    pairs = load_pairs("flagab")
    originals, repeats = split_original_repeat(rows)
    q_fit, q_use = KINDS["flagab"]["questions"]
    lines.append("## Flag A/B pairs (target = flag ON file)\n")
    flags = defaultdict(list)
    for pid, p in pairs.items():
        flags[p["flag"]].append(pid)
    on_letter = lambda pid: pairs[pid]["on_file"]
    for flag, ids in flags.items():
        lines.append(f"### {flag} ({len(ids)} pairs)")
        res = {}
        for q, qn in ((q_fit, "fits the mood"), (q_use, "would use")):
            for pid in ids:
                sc = score([r for r in originals if r["pair_id"] == pid], q, on_letter)
                report_group(f"{pid} ({pairs[pid]['mood']}), {qn}", sc, lines)
            sc = score([r for r in originals if r["pair_id"] in ids], q, on_letter)
            res[q] = report_group(f"POOLED {qn}", sc, lines)
        ok_on = all(res[q][1] >= 8 and res[q][0] / res[q][1] >= 0.70 for q in res)
        ok_off = all(res[q][1] >= 8 and res[q][0] / res[q][1] < 0.40 for q in res)
        if ok_on:
            verdict = "PROPOSE turning the flag on (rule met). The user approves."
        elif ok_off:
            verdict = "KEEP OFF for good (listeners prefer the flag-off file)."
        else:
            verdict = "KEEP OFF (rule not met); note the shares above."
        lines.append(f"- **Verdict: {verdict}**\n")
    for q in (q_fit, q_use):
        a, t = consistency(originals, repeats, q)
        lines.append(f"Repeat consistency for {q}: {a}/{t}" if t else f"Repeat consistency for {q}: no repeats answered")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("kind", choices=sorted(KINDS))
    ap.add_argument("sheets", nargs="+")
    args = ap.parse_args()
    rows = read_trials(collect(args.sheets), KINDS[args.kind]["questions"])
    people = sorted({r["participant"] for r in rows})
    lines = [f"# {args.kind}: {len(people)} participants ({', '.join(people)}), {len(rows)} answered trials\n"]
    {"r10": analyse_r10, "brightness": analyse_brightness, "flagab": analyse_flagab}[args.kind](rows, lines)
    print("\n".join(lines))


if __name__ == "__main__":
    main()
