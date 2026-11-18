#!/usr/bin/env python3
"""Summarize the R3 variants: progression Pearson means, named-chord tensions and type counts.

usage: summarize.py RAW_DIR
Reads tension-comparison_{a,b,c}.csv (from analyze.mjs), probe_{a,b,c}.csv (from r3probe.cpp).
"""
import csv
import sys
from pathlib import Path

VARIANTS = [("a", "default 0.5/0.5"), ("b", "roughness only 1/0"), ("c", "harmonic only 0/1")]
COLUMNS = {
    "static symbolic": "staticSymbol_humanStable",
    "static with voicing": "staticVoiced_humanStable",
    "contextual": "contextLinear_humanStable",
}
REFERENCE = {"static with voicing": 0.686, "contextual": 0.665}


def load_comparison(raw: Path, name: str):
    with open(raw / f"tension-comparison_{name}.csv", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    # P3 has no chords compared (its MIDI is a stimulus mismatch), so it is excluded from the 11-progression mean.
    return [r for r in rows if r["matchedChords"] not in ("", "0")]


def mean(xs):
    return sum(xs) / len(xs)


def load_probe(raw: Path, name: str):
    named, types = [], {}
    with open(raw / f"probe_{name}.csv", encoding="utf-8") as f:
        for line in f:
            parts = line.strip().split(",")
            if parts[0] == "NAMED":
                named.append((parts[2], parts[3], float(parts[4])))
            elif parts[0] == "TYPE":
                types[int(parts[2])] = (parts[3], float(parts[4]))
    return named, types


def main() -> int:
    raw = Path(sys.argv[1])
    out = []
    out.append("## Progression Pearson (11 progressions, P3 excluded)\n")
    out.append("| Variant | Model | Mean r stable | Mean r late | Reference | Match |")
    out.append("|---|---|---:|---:|---:|---|")
    ok_a = True
    for name, label in VARIANTS:
        rows = load_comparison(raw, name)
        for model, col in COLUMNS.items():
            stable = [float(r[col]) for r in rows]
            late = [float(r[col.replace("Stable", "Late")]) for r in rows]
            m_stable, m_late = mean(stable), mean(late)
            ref = REFERENCE.get(model)
            match = ""
            if ref is not None and name == "a":
                match = "yes" if round(m_stable, 3) == ref else f"NO (expected {ref})"
                ok_a = ok_a and round(m_stable, 3) == ref
            out.append(f"| {name} ({label}) | {model} | {m_stable:.3f} | {m_late:.3f} | {ref if ref is not None else ''} | {match} |")
    out.append("")
    out.append("Per-progression r (stable region):\n")
    out.append("| P | " + " | ".join(f"{n} static voiced" for n, _ in VARIANTS) + " | " + " | ".join(f"{n} contextual" for n, _ in VARIANTS) + " |")
    out.append("|---:|" + "---:|" * (2 * len(VARIANTS)))
    per = {n: load_comparison(raw, n) for n, _ in VARIANTS}
    for i in range(len(per["a"])):
        pid = per["a"][i]["progression"]
        vals = [per[n][i]["staticVoiced_humanStable"] for n, _ in VARIANTS] + [per[n][i]["contextLinear_humanStable"] for n, _ in VARIANTS]
        out.append(f"| {pid} | " + " | ".join(f"{float(v):.3f}" for v in vals) + " |")
    out.append("")

    probes = {n: load_probe(raw, n) for n, _ in VARIANTS}
    out.append("## Tension of named chords, C major (symbolic tensionOf)\n")
    out.append("| Chord | a (0.5/0.5) | b (1/0) | c (0/1) |")
    out.append("|---|---:|---:|---:|")
    for k, (name_a, _, _) in enumerate(probes["a"][0]):
        vals = [probes[n][0][k][2] for n, _ in VARIANTS]
        out.append(f"| {probes['a'][0][k][0]} | " + " | ".join(f"{v:.3f}" for v in vals) + " |")
    out.append("")

    out.append("## Chord types whose Tension changes by more than 0.05 (32 types, root C, C major)\n")
    ta, tb, tc = probes["a"][1], probes["b"][1], probes["c"][1]
    changed_ab = [ta[i][0] for i in sorted(ta) if abs(ta[i][1] - tb[i][1]) > 0.05]
    changed_ac = [ta[i][0] for i in sorted(ta) if abs(ta[i][1] - tc[i][1]) > 0.05]
    out.append(f"- a vs b: {len(changed_ab)} of {len(ta)} ({', '.join(changed_ab) or 'none'})")
    out.append(f"- a vs c: {len(changed_ac)} of {len(ta)} ({', '.join(changed_ac) or 'none'})")
    out.append("")
    out.append("| Index | Type | a | b | c |")
    out.append("|---:|---|---:|---:|---:|")
    for i in sorted(ta):
        out.append(f"| {i} | {ta[i][0]} | {ta[i][1]:.3f} | {tb[i][1]:.3f} | {tc[i][1]:.3f} |")
    out.append("")
    out.append("MATCH_A_OK" if ok_a else "MATCH_A_FAILED")
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
