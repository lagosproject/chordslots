#!/usr/bin/env python3
"""Minimal listening pairs for three engine flags: flag OFF against flag ON, same seed and tonic and mood.

Flags (each patched on a COPY of core/include in a scratch directory; the repo headers are never edited):
  mood      kMoodDiatonicContext (Generator.h) = true        moods Epic, Melancholic, Hard
  tritone   kTritoneWeight (Metrics.h) = 0.05f                moods Tense, Dark
  softclip  kBrightnessSoftClip (Metrics.h) = 0.8f            moods Dark, Tense, Dreamy

Method
  Tonics 0, 9, 2, 6 (C, A, D, F#). Seeds 1..300 first; seeds 1..1000 only if a flag gives fewer than four
  minimal pairs. A minimal pair is one where the OFF and ON progressions differ in one or two of the four
  slots (chord identity = root, engine suffix, bass interval). Zero-difference combinations are not used.
  Four pairs per flag, spread over its moods and tonics; among candidates, prefer those whose differing
  slots contain the flag's characteristic chords. If a flag still has fewer than four minimal pairs in 1000
  seeds, the remainder are filled with three-difference pairs and this is recorded in the key.

Outputs (this directory):
  midi/flagabNN_{A,B}.mid        12 pairs, 24 files. Order of OFF and ON is shuffled per pair (fixed seed).
  operator/key.json              operator key: flag, mood, tonic, seed, which file is ON, chords, per-slot
                                 Brightness and Tension, differing slots, frequency tables, check results.

Standard library only, plus write_pairs.py (which imports the SMF writer of tools/listening/r10/make_stimuli.py).
MIDI conventions as in the blind panel: 4 bars, one chord per bar, GM program 0, 90 bpm, 4/4, velocity 80,
the engine's voicing from the roll output (repeated pitches written once).

Usage (from anywhere):
  python3 tools/listening/flagab/build_flagab.py [--work DIR] [--rng-seed 2026]
  --work defaults to a fresh temporary directory (kept at the end, its path is printed).
"""

import argparse
import hashlib
import json
import os
import random
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SRC_INC = os.path.join(ROOT, "core", "include", "chordslots")
PROBE_SRC = os.path.join(HERE, "flagab_probe.cpp")
BRIGHTNESS_DIR = os.path.join(HERE, "..", "brightness")
R10_DIR = os.path.join(HERE, "..", "r10")
sys.path.insert(0, os.path.abspath(BRIGHTNESS_DIR))
import write_pairs  # noqa: E402  (MIDI writer and loader shared with the blind panel)

TONICS = [0, 9, 2, 6]
NAMES = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
PRIMARY_SEEDS = 300
EXTENDED_SEEDS = 1000
PAIRS_PER_FLAG = 4

FLAGS = [
    {"id": "mood", "const": "kMoodDiatonicContext", "file": "Generator.h",
     "old": "inline constexpr bool kMoodDiatonicContext = false;",
     "new": "inline constexpr bool kMoodDiatonicContext = true;",
     "on_value": "true", "moods": ["Epic", "Melancholic", "Hard"]},
    {"id": "tritone", "const": "kTritoneWeight", "file": "Metrics.h",
     "old": "inline constexpr float kTritoneWeight = 0.0f;",
     "new": "inline constexpr float kTritoneWeight = 0.05f;",
     "on_value": "0.05", "moods": ["Tense", "Dark"]},
    {"id": "softclip", "const": "kBrightnessSoftClip", "file": "Metrics.h",
     "old": "inline constexpr float kBrightnessSoftClip = 0.0f;",
     "new": "inline constexpr float kBrightnessSoftClip = 0.8f;",
     "on_value": "0.8", "moods": ["Dark", "Tense", "Dreamy"]},
]
ALL_MOODS = ["Epic", "Melancholic", "Uplifting", "Hard", "Dark", "Tense", "Dreamy", "Euphoric"]
MINOR3 = 3
MAJOR3 = 4
DIM_LIKE = {"dim", "dim7", "m7b5"}
AUG_LIKE = {"aug"}


# ----------------------------------------------------------------------------------------------- builds
def build_variant(name, patch, work):
    """Copy core/include/chordslots, apply the one-line patch if any, compile the probe against the copy."""
    inc = os.path.join(work, f"inc_{name}")
    shutil.copytree(SRC_INC, os.path.join(inc, "chordslots"))
    changed = 0
    if patch:
        path = os.path.join(inc, "chordslots", patch["file"])
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        assert text.count(patch["old"]) == 1, f"{patch['file']}: patch target not found exactly once"
        text = text.replace(patch["old"], patch["new"])
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(text)
        with open(os.path.join(SRC_INC, patch["file"]), encoding="utf-8") as fh:
            orig = fh.read().splitlines()
        changed = sum(1 for a, b in zip(orig, text.splitlines()) if a != b)
        assert changed == 1, f"expected one changed line in {patch['file']}, got {changed}"
    binary = os.path.join(work, f"probe_{name}")
    subprocess.run(["g++", "-std=c++17", "-O2", "-I", inc, PROBE_SRC, "-o", binary], check=True)
    return binary, changed


def run_types(binary):
    out = subprocess.run([binary, "types"], capture_output=True, text=True, check=True).stdout
    types = {}
    for line in out.splitlines():
        p = line.split()
        if p and p[0] == "TYPE":
            suffix = "" if p[2] == "-" else p[2]
            types[suffix] = [int(x) for x in p[3:]]
    assert len(types) == len([l for l in out.splitlines() if l.startswith("TYPE")]), "duplicate suffix"
    return types


def run_batch(binary, combos):
    """combos: list of (mood, tonic, seed). Returns one parsed roll block per combo, in order."""
    text = "\n".join(f"{m} {t} {s}" for m, t, s in combos) + "\n"
    out = subprocess.run([binary, "batch"], input=text, capture_output=True, text=True, check=True).stdout
    blocks, cur = [], None
    for line in out.splitlines():
        p = line.split()
        if not p:
            continue
        if p[0] == "PROG":
            cur = {"mood": p[1].split("=")[1], "tonic": int(p[2].split("=")[1]),
                   "scale": int(p[3].split("=")[1]), "slots": []}
        elif p[0] == "SLOT":
            cur["slots"].append({
                "slot": int(p[1]), "root": int(p[2]), "suffix": "" if p[3] == "-" else p[3],
                "bass": int(p[4]), "brightness": float(p[5]), "tension": float(p[6]), "energy": float(p[7]),
                "notes": sorted({int(x) for x in p[8:]})})
        elif p[0] == "END":
            blocks.append(cur)
            cur = None
    assert len(blocks) == len(combos), f"batch returned {len(blocks)} blocks for {len(combos)} rolls"
    for (m, t, _s), b in zip(combos, blocks):
        assert b["mood"] == m and b["tonic"] == t % 12 and len(b["slots"]) == 4
    return blocks


def chord_of(slot):
    return (slot["root"], slot["suffix"], slot["bass"])


def chord_name(root, suffix, bass):
    name = NAMES[root % 12] + suffix
    if bass:
        name += "/" + NAMES[(root + bass) % 12]
    return name


def differing(off_block, on_block):
    return [i for i in range(4) if chord_of(off_block["slots"][i]) != chord_of(on_block["slots"][i])]


# ----------------------------------------------------------------------------------------- statistics
def frequency_table(flag, rolls_off, rolls_on, types_, seeds):
    """Per mood: count of (tonic, seed) combinations with 0..4 differing slots."""
    table = {}
    for mood in flag["moods"]:
        counts = [0] * 5
        for t in TONICS:
            for s in range(1, seeds + 1):
                k = len(differing(rolls_off[(mood, t, s)], rolls_on[(mood, t, s)]))
                counts[k] += 1
        table[mood] = {"seeds": seeds, "tonics": TONICS, "combinations": 4 * seeds, "counts": counts}
    return table


def interest(flag_id, tonic, off_block, on_block, diff, types_):
    """Score how much the differing slots show the flag's characteristic chords (higher is more interesting)."""
    score = 0
    for i in diff:
        a, b = off_block["slots"][i], on_block["slots"][i]
        if flag_id == "mood":
            def rel(s):
                return (s["root"] - tonic) % 12

            def bvi_vii(s):
                return rel(s) in (8, 10) and MAJOR3 in types_[s["suffix"]]

            def v_minor(s):
                return rel(s) == 7 and MINOR3 in types_[s["suffix"]]
            if (bvi_vii(a) and v_minor(b)) or (v_minor(a) and bvi_vii(b)):
                score += 2
            elif bvi_vii(a) or bvi_vii(b):
                score += 1
        elif flag_id == "tritone":
            def dominant(s):
                iv = types_[s["suffix"]]
                return MAJOR3 in iv and 10 in iv
            if any(s["suffix"] in DIM_LIKE or dominant(s) for s in (a, b)):
                score += 1
        elif flag_id == "softclip":
            if any(s["suffix"] in DIM_LIKE | AUG_LIKE for s in (a, b)):
                score += 1
    return score


def choose(flag, rolls_off, rolls_on, types_, seeds, ks, n, taken):
    """Greedy pick of n pairs from (tonic, seed) combinations with differing count in ks.
    Moods are cycled; within a mood, prefer interesting pairs, then a tonic not yet used for this flag."""
    cands = []
    for mood in flag["moods"]:
        for t in TONICS:
            for s in range(1, seeds + 1):
                off, on = rolls_off[(mood, t, s)], rolls_on[(mood, t, s)]
                diff = differing(off, on)
                if len(diff) in ks and (mood, t, s) not in taken:
                    cands.append({"mood": mood, "tonic": t, "seed": s, "diff": diff,
                                  "score": interest(flag["id"], t, off, on, diff, types_)})
    picks = []
    tonic_use = {}
    for i in range(n):
        mood = flag["moods"][i % len(flag["moods"])]
        pool = [c for c in cands if c["mood"] == mood and (c["mood"], c["tonic"], c["seed"]) not in taken]
        if not any(c["score"] > 0 for c in pool):
            pool = [c for c in cands if (c["mood"], c["tonic"], c["seed"]) not in taken]
        else:
            pool = [c for c in pool if c["score"] > 0]
        if not pool:
            break
        pool.sort(key=lambda c: (tonic_use.get(c["tonic"], 0), -c["score"], len(c["diff"]), c["seed"]))
        best = pool[0]
        picks.append(best)
        taken.add((best["mood"], best["tonic"], best["seed"]))
        tonic_use[best["tonic"]] = tonic_use.get(best["tonic"], 0) + 1
    return picks


def select_pairs(flag, rolls_off, rolls_on, types_):
    """Minimal pairs in seeds 1..300; if fewer than four, redo the pick over seeds 1..1000; if still fewer,
    fill the rest with three-difference pairs from seeds 1..1000. Returns (picks, stage record)."""
    picks = choose(flag, rolls_off, rolls_on, types_, PRIMARY_SEEDS, {1, 2}, PAIRS_PER_FLAG, set())
    stage = {"primary_minimal_found": len(picks), "seed_range": PRIMARY_SEEDS}
    if len(picks) < PAIRS_PER_FLAG:
        picks = choose(flag, rolls_off, rolls_on, types_, EXTENDED_SEEDS, {1, 2}, PAIRS_PER_FLAG, set())
        stage.update({"extended_minimal_found": len(picks), "seed_range": EXTENDED_SEEDS})
    if len(picks) < PAIRS_PER_FLAG:
        taken = {(c["mood"], c["tonic"], c["seed"]) for c in picks}
        extra = choose(flag, rolls_off, rolls_on, types_, EXTENDED_SEEDS, {3},
                       PAIRS_PER_FLAG - len(picks), taken)
        picks = picks + extra
        stage["three_difference_fill"] = len(extra)
    return picks, stage


# ----------------------------------------------------------------------------------------------- MIDI
def write_pair_files(pair_id, letter_blocks, out_dir):
    """letter_blocks: {"A": block, "B": block}. Writes midi/flagabNN_{A,B}.mid and returns the file records."""
    records = {}
    for letter in ("A", "B"):
        block = letter_blocks[letter]
        bars = [sorted(set(s["notes"])) for s in block["slots"]]
        assert all(bars), "empty bar"
        fname = f"{pair_id}_{letter}.mid"
        write_pairs.write_midi(os.path.join(out_dir, fname), bars, f"{pair_id}{letter}")
        records[letter] = fname
    return records


def unsuffix(s):
    """Key files write the major triad as '-'; the engine's suffix is the empty string."""
    return "" if s == "-" else s


def chord_pcs(root, suffix, bass, types_):
    pcs = {(root + iv) % 12 for iv in types_[suffix]}
    if bass:
        pcs.add((root + bass) % 12)
    return pcs


def check_files(pairs, out_dir, types_):
    """Re-read every MIDI file, compare with the roll (exact notes) and with the chord list (pitch classes)."""
    bar_ticks = write_pairs.BAR_BEATS * write_pairs.r10.TPQ
    rows, all_ok = [], True
    for pair in pairs:
        digest = {}
        for letter in ("A", "B"):
            fname = pair["files"][letter]["file"]
            path = os.path.join(out_dir, os.path.basename(fname))
            with open(path, "rb") as fh:
                digest[letter] = hashlib.sha256(fh.read()).hexdigest()
            smf = write_pairs.read_smf(path)
            block = pair["files"][letter]["slots"]
            bars = write_pairs.bar_pitches(smf["notes"], bar_ticks, 4)
            meta_ok = (smf["format"] == 0 and smf["bpm"] is not None and abs(smf["bpm"] - 90) < 0.01
                       and smf["program"] == 0 and smf["velocities"] == [80] and len(bars) == 4
                       and sum(len(b) for b in bars) == len(smf["notes"]))
            notes_ok = all(bars[i] == block[i]["notes"] for i in range(4))
            pcs_ok = all({p % 12 for p in bars[i]} == chord_pcs(block[i]["root"], unsuffix(block[i]["suffix"]),
                                                              block[i]["bass"], types_) for i in range(4))
            bass_ok = all(min(bars[i]) % 12 == (block[i]["root"] + block[i]["bass"]) % 12 for i in range(4))
            ok = meta_ok and notes_ok and pcs_ok
            all_ok = all_ok and ok
            rows.append((pair["pair_id"], letter, fname, smf["format"], smf["bpm"], smf["program"],
                         ",".join(map(str, smf["velocities"])), len(bars), notes_ok, pcs_ok, bass_ok, ok))
        pair["files_identical"] = digest["A"] == digest["B"]
        all_ok = all_ok and not pair["files_identical"]
    print("Check table: MIDI files re-read with write_pairs.read_smf")
    print(f"{'pair':<10}{'file':<4}{'fname':<22}{'fmt':>4}{'bpm':>7}{'prog':>5}{'vel':>5}{'bars':>5}"
          f"{'notes=roll':>12}{'pcs=chord':>11}{'low=bass':>10}{'ok':>5}")
    for r in rows:
        print(f"{r[0]:<10}{r[1]:<4}{r[2]:<22}{r[3]:>4}{r[4]:>7}{r[5]:>5}{r[6]:>5}{r[7]:>5}"
              f"{('yes' if r[8] else 'NO'):>12}{('yes' if r[9] else 'NO'):>11}{('yes' if r[10] else 'no'):>10}"
              f"{('yes' if r[11] else 'NO'):>5}")
    print("files identical within any pair:", any(p["files_identical"] for p in pairs))
    return all_ok, rows


# ----------------------------------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--work", default=None, help="scratch directory for header copies and binaries")
    ap.add_argument("--rng-seed", type=int, default=2026, help="seed for the per-pair A/B order")
    ap.add_argument("--midi-dir", default=os.path.join(HERE, "midi"))
    ap.add_argument("--key", default=os.path.join(HERE, "operator", "key.json"))
    args = ap.parse_args()

    work = args.work or tempfile.mkdtemp(prefix="flagab_")
    os.makedirs(work, exist_ok=True)
    print(f"scratch directory: {work}")
    write_pairs.load_r10(R10_DIR)

    # 1. Builds: OFF is an unpatched copy; each ON is a copy with one line changed.
    off_bin, _ = build_variant("off", None, work)
    on_bins = {}
    for flag in FLAGS:
        on_bins[flag["id"]], changed = build_variant(flag["id"], flag, work)
        print(f"built {flag['id']}: {flag['const']} changed on {changed} line of {flag['file']}")
    types_ = run_types(off_bin)

    # 2. Rolls. OFF for every mood used by any flag, ON for the moods of each flag.
    moods_all = sorted({m for f in FLAGS for m in f["moods"]})
    combos_all = [(m, t, s) for m in moods_all for t in TONICS for s in range(1, EXTENDED_SEEDS + 1)]
    rolls_off = dict(zip(combos_all, run_batch(off_bin, combos_all)))
    rolls_on = {}
    for flag in FLAGS:
        combos = [(m, t, s) for m in flag["moods"] for t in TONICS for s in range(1, EXTENDED_SEEDS + 1)]
        rolls_on[flag["id"]] = dict(zip(combos, run_batch(on_bins[flag["id"]], combos)))

    # 3. Frequency tables (over the primary range, and over the extended range).
    tables = {}
    for flag in FLAGS:
        ro, rn = rolls_off, rolls_on[flag["id"]]
        tables[flag["id"]] = {
            "seeds_1_300": frequency_table(flag, ro, rn, types_, PRIMARY_SEEDS),
            "seeds_1_1000": frequency_table(flag, ro, rn, types_, EXTENDED_SEEDS),
        }
        print(f"\nFrequency table, flag {flag['id']} ({flag['const']}), seeds 1..{PRIMARY_SEEDS}, tonics {TONICS}")
        print(f"{'mood':<13}{'0':>7}{'1':>7}{'2':>7}{'3':>7}{'4':>7}{'combos':>9}")
        for mood, row in tables[flag["id"]]["seeds_1_300"].items():
            print(f"{mood:<13}" + "".join(f"{c:>7}" for c in row["counts"]) + f"{row['combinations']:>9}")

    # 4. Pair selection.
    rng = random.Random(args.rng_seed)
    pair_no = 0
    pairs = []
    os.makedirs(args.midi_dir, exist_ok=True)
    for flag in FLAGS:
        picks, stage = select_pairs(flag, rolls_off, rolls_on[flag["id"]], types_)
        if len(picks) < PAIRS_PER_FLAG:
            raise SystemExit(f"flag {flag['id']}: only {len(picks)} pairs found")
        print(f"\nflag {flag['id']}: selection {stage}")
        for c in picks:
            pair_no += 1
            pid = f"flagab{pair_no:02d}"
            off = rolls_off[(c["mood"], c["tonic"], c["seed"])]
            on = rolls_on[flag["id"]][(c["mood"], c["tonic"], c["seed"])]
            on_is_a = rng.random() < 0.5
            blocks = {"A": on if on_is_a else off, "B": off if on_is_a else on}
            roles = {"A": "on" if on_is_a else "off", "B": "off" if on_is_a else "on"}
            names = write_pair_files(pid, blocks, args.midi_dir)
            entry = {"pair_id": pid, "flag_id": flag["id"], "flag": flag["const"], "on_value": flag["on_value"],
                     "mood": c["mood"], "tonic": c["tonic"], "tonic_name": NAMES[c["tonic"]],
                     "seed": c["seed"], "seed_range_searched": stage["seed_range"],
                     "selection": stage,
                     "differing_slots": c["diff"], "differing_count": len(c["diff"]),
                     "interest_score": c["score"], "on_file": "A" if on_is_a else "B",
                     "scale": blocks["A"]["scale"], "files": {}}
            for letter in ("A", "B"):
                blk = blocks[letter]
                entry["files"][letter] = {
                    "file": f"midi/{names[letter]}", "role": roles[letter],
                    "chords": [chord_name(*chord_of(s)) for s in blk["slots"]],
                    "slots": [{"slot": s["slot"], "chord": chord_name(*chord_of(s)), "root": s["root"],
                               "suffix": s["suffix"] or "-", "bass": s["bass"], "brightness": s["brightness"],
                               "tension": s["tension"], "energy": s["energy"], "notes": sorted(set(s["notes"]))}
                              for s in blk["slots"]],
                }
            pairs.append(entry)
            off_letter = "A" if roles["A"] == "off" else "B"
            on_letter = "B" if off_letter == "A" else "A"
            print(f"{pid}  {flag['id']:<9} {c['mood']:<12} tonic {NAMES[c['tonic']]:<3} seed {c['seed']:<5} "
                  f"diff {c['diff']}  off {' '.join(entry['files'][off_letter]['chords'])}  "
                  f"on {' '.join(entry['files'][on_letter]['chords'])}")

    # 5. Check the MIDI files.
    print()
    midi_ok, _rows = check_files(pairs, args.midi_dir, types_)

    # 6. Reproducibility: OFF rolls with the unpatched repo headers, ON rolls with the patched binaries.
    # The OFF roll of every pair, compiled against the repo headers directly (no copy, nothing patched).
    repo_bin = os.path.join(work, "probe_repo")
    subprocess.run(["g++", "-std=c++17", "-O2", "-I", os.path.join(ROOT, "core", "include"), PROBE_SRC,
                    "-o", repo_bin], check=True)
    combos_p = [(p["mood"], p["tonic"], p["seed"]) for p in pairs]
    off_letters = [("A" if p["files"]["A"]["role"] == "off" else "B") for p in pairs]
    repo_rolls = run_batch(repo_bin, combos_p)
    repro_off = True
    for p, combo, letter, blk in zip(pairs, combos_p, off_letters, repo_rolls):
        stored = [(s["root"], s["suffix"], s["bass"], s["brightness"], s["tension"], s["energy"], s["notes"])
                  for s in rolls_off[combo]["slots"]]
        fresh = [(s["root"], s["suffix"], s["bass"], s["brightness"], s["tension"], s["energy"], s["notes"])
                 for s in blk["slots"]]
        repro_off = repro_off and stored == fresh
    # The ON roll of every pair, rerun through its patched binary.
    repro_on = True
    for p, combo in zip(pairs, combos_p):
        again = run_batch(on_bins[p["flag_id"]], [combo])[0]
        on_letter = "A" if p["files"]["A"]["role"] == "on" else "B"
        repro_on = repro_on and [s["notes"] for s in again["slots"]] == [s["notes"] for s in
                                                                           rolls_on[p["flag_id"]][combo]["slots"]]
        repro_on = repro_on and on_letter in ("A", "B")
    print(f"\nreproducible with unpatched repo headers (12 OFF rolls): {'yes' if repro_off else 'NO'}")
    print(f"ON rolls reproduced by a second run of the patched build (12 ON rolls): {'yes' if repro_on else 'NO'}")

    # 7. Key.
    doc = {
        "description": "Operator key for the flag A/B pairs. Do not give this file to listeners.",
        "method": {
            "tonics": TONICS, "primary_seeds": PRIMARY_SEEDS, "extended_seeds": EXTENDED_SEEDS,
            "pairs_per_flag": PAIRS_PER_FLAG, "minimal": "1 or 2 of the 4 slots differ (root, suffix, bass)",
            "rng_seed_for_ab_order": args.rng_seed, "tempo_bpm": 90, "meter": "4/4", "program": 0,
            "velocity": 80, "bars_per_file": 4,
        },
        "frequency_tables": tables,
        "checks": {"midi_all_ok": midi_ok, "reproducible_off_unpatched_repo": repro_off,
                   "reproducible_on_patched": repro_on},
        "pairs": pairs,
    }
    os.makedirs(os.path.dirname(args.key), exist_ok=True)
    with open(args.key, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=2, ensure_ascii=False)
        fh.write("\n")
    print(f"wrote {args.key}")
    print("all checks passed" if (midi_ok and repro_off and repro_on) else "SOME CHECKS FAILED")
    return 0 if (midi_ok and repro_off and repro_on) else 1


if __name__ == "__main__":
    sys.exit(main())
