# SPDX-License-Identifier: AGPL-3.0-or-later

import argparse
import csv
import math
import sys

EXPECTED_COUNT = 20


def read_csv(path, expected_headers):
    try:
        with open(path, "r", newline="", encoding="utf-8-sig") as f:
            reader = csv.DictReader(f)
            if reader.fieldnames != expected_headers:
                raise ValueError(
                    f"{path}: expected columns {','.join(expected_headers)}; "
                    f"found {','.join(reader.fieldnames or [])}"
                )
            rows = list(reader)
            for line, row in enumerate(rows, start=2):
                if None in row or any(row[field] is None for field in expected_headers):
                    raise ValueError(f"{path}:{line}: incorrect number of columns")
    except OSError as exc:
        raise ValueError(f"cannot read {path}: {exc}") from exc
    return rows


def read_ratings(path):
    rows = read_csv(path, ["id", "rating"])
    values = {}
    for line, row in enumerate(rows, start=2):
        ident = (row["id"] or "").strip()
        raw = (row["rating"] or "").strip()
        if not ident:
            raise ValueError(f"{path}:{line}: id must not be empty")
        if ident in values:
            raise ValueError(f"{path}:{line}: duplicate id {ident!r}")
        try:
            rating = int(raw)
        except ValueError as exc:
            raise ValueError(f"{path}:{line}: rating must be an integer from 1 to 5") from exc
        if str(rating) != raw or not 1 <= rating <= 5:
            raise ValueError(f"{path}:{line}: rating must be an integer from 1 to 5")
        values[ident] = rating
    if len(values) != EXPECTED_COUNT:
        raise ValueError(f"{path}: expected exactly {EXPECTED_COUNT} unique ids, found {len(values)}")
    return values


def read_key(path):
    rows = read_csv(path, ["id", "static", "contextual"])
    values = {}
    for line, row in enumerate(rows, start=2):
        ident = (row["id"] or "").strip()
        if not ident:
            raise ValueError(f"{path}:{line}: id must not be empty")
        if ident in values:
            raise ValueError(f"{path}:{line}: duplicate id {ident!r}")
        predictions = []
        for column in ("static", "contextual"):
            raw = (row[column] or "").strip()
            try:
                number = float(raw)
            except ValueError as exc:
                raise ValueError(f"{path}:{line}: {column} must be finite numeric data") from exc
            if not math.isfinite(number):
                raise ValueError(f"{path}:{line}: {column} must be finite numeric data")
            predictions.append(number)
        values[ident] = tuple(predictions)
    if len(values) != EXPECTED_COUNT:
        raise ValueError(f"{path}: expected exactly {EXPECTED_COUNT} unique ids, found {len(values)}")
    return values


def average_ranks(values):
    ordered = sorted(enumerate(values), key=lambda pair: pair[1])
    ranks = [0.0] * len(values)
    i = 0
    while i < len(ordered):
        j = i + 1
        while j < len(ordered) and ordered[j][1] == ordered[i][1]:
            j += 1
        average = ((i + 1) + j) / 2.0
        for k in range(i, j):
            ranks[ordered[k][0]] = average
        i = j
    return ranks


def pearson(x, y):
    mean_x = sum(x) / len(x)
    mean_y = sum(y) / len(y)
    dx = [v - mean_x for v in x]
    dy = [v - mean_y for v in y]
    denom = math.sqrt(sum(v * v for v in dx) * sum(v * v for v in dy))
    if denom == 0:
        return None
    return sum(a * b for a, b in zip(dx, dy)) / denom


def spearman(x, y):
    return pearson(average_ranks(x), average_ranks(y))


def fmt(value):
    return "undefined (constant input)" if value is None else f"{value:.6f}"


def main():
    parser = argparse.ArgumentParser(
        description="Compare 20 blind-listening ratings with static and contextual predictions using Spearman correlation."
    )
    parser.add_argument("ratings_csv", help="CSV with columns id,rating")
    parser.add_argument("answer_key_csv", help="CSV with columns id,static,contextual")
    args = parser.parse_args()
    try:
        ratings = read_ratings(args.ratings_csv)
        key = read_key(args.answer_key_csv)
        if ratings.keys() != key.keys():
            missing = sorted(ratings.keys() - key.keys())
            extra = sorted(key.keys() - ratings.keys())
            details = []
            if missing:
                details.append("missing from answer key: " + ", ".join(missing))
            if extra:
                details.append("extra in answer key: " + ", ".join(extra))
            raise ValueError("ID sets do not match (" + "; ".join(details) + ")")
        ids = list(ratings)
        observed = [ratings[i] for i in ids]
        static = [key[i][0] for i in ids]
        contextual = [key[i][1] for i in ids]
        static_corr = spearman(observed, static)
        contextual_corr = spearman(observed, contextual)
        print(f"Static Spearman correlation: {fmt(static_corr)}")
        print(f"Contextual Spearman correlation: {fmt(contextual_corr)}")
        if static_corr is not None and contextual_corr is not None:
            if static_corr > contextual_corr:
                print("Better correlate: static")
            elif contextual_corr > static_corr:
                print("Better correlate: contextual")
            else:
                print("Better correlate: tie")
        return 0
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
