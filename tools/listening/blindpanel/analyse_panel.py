#!/usr/bin/env python3
"""Analyse filled R7 rating sheets against the operator key. Standard library only.

Reports
  1. Per condition (cs_full, cs_plain, random, corpus): n, mean and SD of plausibility and would_use.
  2. Per training group (music theory, none) and condition: mean plausibility and would_use.
  3. Inter-judge agreement for each rating, over the items every judge rated:
     - Kendall's W (with the tie correction), judges as raters and items as objects;
     - mean pairwise Spearman correlation between judges (with min and max).

Inputs: sheet files or a folder of P*.csv sheets, and key.csv (from make_panel.py). The key is read only
here, never by the sheet generator.

Usage:
  python3 analyse_panel.py SHEET_DIR_OR_FILES... --key operator/key.csv
"""

import argparse
import csv
import glob
import math
import os
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
RATINGS = ("plausibility", "would_use")


def collect_sheets(paths):
    files = []
    for p in paths:
        if os.path.isdir(p):
            files.extend(sorted(glob.glob(os.path.join(p, "*.csv"))))
        else:
            files.append(p)
    if not files:
        raise SystemExit("no sheet files found")
    return files


def read_rows(files):
    rows, skipped = [], 0
    for path in files:
        with open(path, newline="", encoding="utf-8") as fh:
            for r in csv.DictReader(fh):
                vals = {}
                ok = True
                for key in RATINGS:
                    raw = (r.get(key) or "").strip()
                    if raw == "":
                        ok = False
                        break
                    v = int(raw)
                    if not 1 <= v <= 7:
                        raise SystemExit(f"{path}: {key}={v} outside 1-7 for item {r.get('item')}")
                    vals[key] = v
                if not ok:
                    skipped += 1
                    continue
                rows.append({"participant": r["participant"], "training": r.get("training", "") or "unknown",
                             "item": r["item"], **vals})
    return rows, skipped


def read_key(path):
    with open(path, newline="", encoding="utf-8") as fh:
        return {r["item"]: r["condition"] for r in csv.DictReader(fh)}


def mean(xs):
    return sum(xs) / len(xs) if xs else float("nan")


def sd(xs):
    if len(xs) < 2:
        return float("nan")
    m = mean(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def average_ranks(values):
    """Ranks 1..n with ties given their average rank; also returns the tie terms sum(t^3 - t)."""
    order = sorted(range(len(values)), key=lambda i: values[i])
    ranks = [0.0] * len(values)
    tie_term = 0.0
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and values[order[j + 1]] == values[order[i]]:
            j += 1
        avg = (i + j) / 2 + 1
        for k in range(i, j + 1):
            ranks[order[k]] = avg
        t = j - i + 1
        tie_term += t ** 3 - t
        i = j + 1
    return ranks, tie_term


def pearson(x, y):
    mx, my = mean(x), mean(y)
    sxy = sum((a - mx) * (b - my) for a, b in zip(x, y))
    sxx = sum((a - mx) ** 2 for a in x)
    syy = sum((b - my) ** 2 for b in y)
    if sxx == 0 or syy == 0:
        return float("nan")
    return sxy / math.sqrt(sxx * syy)


def agreement(rows, rating):
    """Kendall's W and mean pairwise Spearman across judges, over the items every judge rated."""
    by_judge = defaultdict(dict)
    for r in rows:
        by_judge[r["participant"]][r["item"]] = r[rating]
    judges = sorted(by_judge)
    if len(judges) < 2:
        return None
    common = set.intersection(*(set(by_judge[j]) for j in judges))
    items = sorted(common)
    m, n = len(judges), len(items)
    if n < 3:
        return None
    ranks, tie_sum = [], 0.0
    for j in judges:
        rk, t = average_ranks([by_judge[j][i] for i in items])
        ranks.append(rk)
        tie_sum += t
    rank_sums = [sum(ranks[j][k] for j in range(m)) for k in range(n)]
    mean_rs = m * (n + 1) / 2
    s = sum((rs - mean_rs) ** 2 for rs in rank_sums)
    denom = m * m * (n ** 3 - n) - m * tie_sum
    w = 12 * s / denom if denom else float("nan")
    rhos = [pearson(ranks[a], ranks[b]) for a in range(m) for b in range(a + 1, m)]
    rhos = [r for r in rhos if not math.isnan(r)]
    return {"judges": m, "items": n, "W": w,
            "rho_mean": mean(rhos) if rhos else float("nan"),
            "rho_min": min(rhos) if rhos else float("nan"),
            "rho_max": max(rhos) if rhos else float("nan")}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("sheets", nargs="+", help="sheet CSV files or a folder of them")
    ap.add_argument("--key", default=os.path.join(HERE, "operator", "key.csv"))
    args = ap.parse_args()

    files = collect_sheets(args.sheets)
    rows, skipped = read_rows(files)
    condition = read_key(args.key)
    unknown = sorted({r["item"] for r in rows if r["item"] not in condition})
    if unknown:
        raise SystemExit(f"items not in key: {', '.join(unknown)}")
    for r in rows:
        r["condition"] = condition[r["item"]]

    judges = sorted({r["participant"] for r in rows})
    print(f"sheets: {len(files)} files, {len(judges)} judges, {len(rows)} rated items, {skipped} unrated rows skipped")
    print()

    print("Table 1. Ratings by condition (mean, SD, n)")
    print(f"{'condition':<12}{'n':>5}{'plaus mean':>12}{'plaus SD':>10}{'use mean':>10}{'use SD':>9}")
    by_cond = defaultdict(list)
    for r in rows:
        by_cond[r["condition"]].append(r)
    for cond in sorted(by_cond):
        rs = by_cond[cond]
        p = [r["plausibility"] for r in rs]
        u = [r["would_use"] for r in rs]
        print(f"{cond:<12}{len(rs):>5}{mean(p):>12.2f}{sd(p):>10.2f}{mean(u):>10.2f}{sd(u):>9.2f}")
    print()

    print("Table 2. Ratings by training group and condition (mean plausibility / mean would_use, n)")
    groups = defaultdict(list)
    for r in rows:
        groups[(r["training"], r["condition"])].append(r)
    print(f"{'training':<14}{'condition':<12}{'n':>5}{'plaus mean':>12}{'use mean':>10}")
    for key in sorted(groups):
        rs = groups[key]
        print(f"{key[0]:<14}{key[1]:<12}{len(rs):>5}{mean([r['plausibility'] for r in rs]):>12.2f}"
              f"{mean([r['would_use'] for r in rs]):>10.2f}")
    print()

    print("Table 3. Inter-judge agreement (items rated by every judge)")
    print(f"{'rating':<14}{'judges':>7}{'items':>7}{'Kendall W':>11}{'mean rho':>10}{'min rho':>9}{'max rho':>9}")
    for rating in RATINGS:
        res = agreement(rows, rating)
        if res is None:
            print(f"{rating:<14}  not computable (need 2 or more judges and 3 or more shared items)")
            continue
        print(f"{rating:<14}{res['judges']:>7}{res['items']:>7}{res['W']:>11.3f}{res['rho_mean']:>10.3f}"
              f"{res['rho_min']:>9.3f}{res['rho_max']:>9.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
