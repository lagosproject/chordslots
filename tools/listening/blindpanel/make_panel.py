#!/usr/bin/env python3
"""R7 blind panel: build 24 items (8 ChordSlots, 8 random in-scale, 8 corpus) as MIDI.

Writes:
  midi/itemNN.mid        the stimuli for listeners (neutral names, shuffled order)
  operator/key.csv       item -> condition and source. Operator only, never sent with the listener sheets.

Conditions
  chordslots  rolled by the engine (make_panel roll): mood, tonic and seed are in the key.
  random      four diatonic triads or sevenths of one scale (major or natural minor), random degrees.
  corpus      four consecutive chords from a Billboard annotation (ChoCo v1.0.0, CC BY 4.0), plain triads and
              sevenths only, absolute roots kept. Source file and window start are in the key.

Every item: 4 bars, one chord per bar, General MIDI program 0, 90 bpm, 4/4, velocity 80, the engine's voicing
(the same voicing path for the random and corpus items, voiceAll with default slot targets).

Usage (from this directory, after building make_panel.cpp):
  python3 make_panel.py [--seed 2026] [--zip PATH] [--probe PATH] [--r10-dir PATH]
"""

import argparse
import csv
import os
import random
import re
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
BRIGHTNESS_DIR = os.path.join(HERE, "..", "brightness")
sys.path.insert(0, os.path.abspath(BRIGHTNESS_DIR))
import write_pairs  # noqa: E402  (MIDI writer and loader shared with task 5)

DEFAULT_ZIP = "/home/vant/chordslots-data/raw/choco-v1.0.0.zip"
LAB_PATTERN = re.compile(r"^.*/partitions/billboard/raw/mirex/\d+/full\.lab$")
PC = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NAMES = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
# ChoCo (Billboard) quality -> engine suffix. Only plain triads and sevenths are kept.
QMAP = {"maj": "", "min": "m", "7": "7", "min7": "m7", "maj7": "maj7", "dim": "dim", "hdim7": "m7b5",
        "dim7": "dim7", "sus4": "sus4", "sus2": "sus2", "aug": "aug", "maj6": "6", "min6": "m6", "minmaj7": "mMaj7"}
# Engine suffixes for the diatonic triads and sevenths, keyed by interval sets from the root.
TRIAD = {(4, 7): "", (3, 7): "m", (3, 6): "dim", (4, 8): "aug"}
SEVENTH = {(4, 7, 11): "maj7", (4, 7, 10): "7", (3, 7, 10): "m7", (3, 6, 10): "m7b5", (3, 6, 9): "dim7",
           (3, 7, 11): "mMaj7"}
SCALES = {"Major": [0, 2, 4, 5, 7, 9, 11], "Minor": [0, 2, 3, 5, 7, 8, 10]}
MOODS = ["Epic", "Melancholic", "Uplifting", "Dark"]  # two items each


def chord_name(root, suffix, bass=0):
    name = NAMES[root % 12] + suffix
    if bass:
        name += "/" + NAMES[(root + bass) % 12]
    return name


def parse_label(label):
    """ChoCo label 'G:min7/3' -> (root pitch class, engine suffix), or None if not a plain triad or seventh."""
    if ":" not in label:
        return None
    root_s, quality = label.split(":", 1)
    quality = quality.split("/")[0]
    if quality not in QMAP:
        return None
    m = re.match(r"^([A-G])(#*|b*)$", root_s)
    if not m:
        return None
    root = PC[m.group(1)] + m.group(2).count("#") - m.group(2).count("b")
    return root % 12, QMAP[quality]


def corpus_windows(zip_path):
    """Four-chord windows from the Billboard partition, one list per song. Repeated labels are merged."""
    z = zipfile.ZipFile(zip_path)
    songs = []
    for name in sorted(n for n in z.namelist() if LAB_PATTERN.match(n)):
        labels = []
        for line in z.read(name).decode("utf-8", "replace").splitlines():
            parts = line.split()
            if len(parts) >= 3 and (not labels or labels[-1] != parts[2]):
                labels.append(parts[2])
        chords = [parse_label(lab) for lab in labels]
        wins = []
        for i in range(len(chords) - 3):
            w = chords[i:i + 4]
            if any(c is None for c in w) or len(set(w)) == 1:
                continue
            wins.append((name, i, w))
        if wins:
            songs.append(wins)
    return songs


def diatonic_chord(scale_pcs, tonic, degree, seventh):
    pcs = [(tonic + s) % 12 for s in scale_pcs]
    root = pcs[degree]
    iv = lambda k: (pcs[(degree + k) % 7] - root) % 12  # noqa: E731
    if seventh:
        key = (iv(2), iv(4), iv(6))
        suffix = SEVENTH.get(key)
    else:
        key = (iv(2), iv(4))
        suffix = TRIAD.get(key)
    return (root, suffix) if suffix is not None else None


def run_roll(probe, mood, tonic, seed):
    out = subprocess.run([probe, "roll", mood, str(tonic), str(seed)], capture_output=True, text=True, check=True)
    slots = []
    scale = None
    for line in out.stdout.splitlines():
        parts = line.split()
        if parts[0] == "PROG":
            scale = int(parts[3].split("=")[1])
        elif parts[0] == "SLOT":
            suffix = "" if parts[3] == "-" else parts[3]
            slots.append({"root": int(parts[2]), "suffix": suffix, "bass": int(parts[4]),
                          "brightness": float(parts[5]), "tension": float(parts[6]),
                          "notes": sorted({int(x) for x in parts[8:]})})  # parts[7] is energy
    assert len(slots) == 4, "roll must return four slots"
    return scale, slots


def run_voice(probe, chord_lists, seed):
    lines = []
    for chords in chord_lists:
        lines.append(" ".join(f"{r}:{s if s else '-'}" for r, s in chords))
    out = subprocess.run([probe, "voice", str(seed)], input="\n".join(lines) + "\n", capture_output=True,
                         text=True, check=True)
    result = []
    for line in out.stdout.splitlines():
        groups = [sorted({int(x) for x in g.split()}) for g in line.split("|")]
        result.append(groups)
    assert len(result) == len(chord_lists), "voice returned a different number of progressions"
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--seed", type=int, default=2026)
    ap.add_argument("--zip", default=DEFAULT_ZIP)
    ap.add_argument("--probe", default=os.path.join(HERE, "make_panel"))
    ap.add_argument("--r10-dir", default=os.path.join(HERE, "..", "r10"))
    ap.add_argument("--midi-dir", default=os.path.join(HERE, "midi"))
    ap.add_argument("--key", default=os.path.join(HERE, "operator", "key.csv"))
    args = ap.parse_args()

    write_pairs.load_r10(args.r10_dir)
    rng = random.Random(args.seed)
    items = []  # dicts with condition, chords, notes (one list per bar), and source fields

    # 1. ChordSlots: rolled by the engine, moods Epic, Melancholic, Uplifting, Dark, two each.
    for i in range(8):
        mood = MOODS[i % 4]
        tonic = rng.randrange(12)
        engine_seed = args.seed * 100 + i + 1
        scale, slots = run_roll(args.probe, mood, tonic, engine_seed)
        items.append({"condition": "chordslots", "mood": mood, "tonic": tonic, "scale": scale,
                      "engine_seed": engine_seed, "source": "",
                      "chords": [(s["root"], s["suffix"], s["bass"]) for s in slots],
                      "notes": [s["notes"] for s in slots],
                      "brightness": [round(s["brightness"], 6) for s in slots],
                      "tension": [round(s["tension"], 6) for s in slots]})

    # 2. Random: four diatonic chords from one scale (major or natural minor).
    voice_jobs = []  # (item, chords) for voicing with the engine
    for _ in range(8):
        scale_name = rng.choice(list(SCALES))
        tonic = rng.randrange(12)
        while True:
            chords = []
            for _ in range(4):
                while True:
                    c = diatonic_chord(SCALES[scale_name], tonic, rng.randrange(7), rng.random() < 0.5)
                    if c is not None:
                        chords.append(c)
                        break
            if len(set(chords)) > 1:
                break
        item = {"condition": "random", "mood": "", "tonic": tonic, "scale": scale_name, "engine_seed": "",
                "source": "", "chords": [(r, s, 0) for r, s in chords]}
        items.append(item)
        voice_jobs.append(item)

    # 3. Corpus: one window from each of eight different songs (Billboard, ChoCo v1.0.0).
    if os.path.exists(args.zip):
        songs = corpus_windows(args.zip)
        rng.shuffle(songs)
        for wins in songs[:8]:
            name, start, chords = rng.choice(wins)
            item = {"condition": "corpus", "mood": "", "tonic": "", "scale": "", "engine_seed": "",
                    "source": f"{os.path.basename(args.zip)}::{name.split('/partitions/')[1]}@{start}",
                    "chords": [(r, s, 0) for r, s in chords]}
            items.append(item)
            voice_jobs.append(item)
        print(f"corpus: {len(songs)} songs with plain-chord windows; 8 windows drawn")
    else:
        print(f"corpus source missing: {args.zip}")

    # 4. Voice the random and corpus items with the engine.
    voiced = run_voice(args.probe, [[(r, s) for r, s, _b in it["chords"]] for it in voice_jobs], args.seed)
    for item, groups in zip(voice_jobs, voiced):
        item["notes"] = groups
        item["brightness"] = []
        item["tension"] = []

    # 5. Shuffle and number the items. The numbering is the only thing the listener sees.
    rng.shuffle(items)
    os.makedirs(args.midi_dir, exist_ok=True)
    os.makedirs(os.path.dirname(args.key), exist_ok=True)

    rows = []
    for n, item in enumerate(items, start=1):
        item_id = f"item{n:02d}"
        fname = f"{item_id}.mid"
        bars = [sorted(set(notes)) for notes in item["notes"]]
        write_pairs.write_midi(os.path.join(args.midi_dir, fname), bars, item_id)
        chord_names = [chord_name(r, s, b) for r, s, b in item["chords"]]
        rows.append({"item": item_id, "file": f"midi/{fname}", "condition": item["condition"],
                     "mood": item["mood"], "tonic": item["tonic"], "scale": item["scale"],
                     "engine_seed": item["engine_seed"], "source": item["source"],
                     "chords": " ".join(chord_names),
                     "slot_brightness": " ".join(map(str, item["brightness"])),
                     "slot_tension": " ".join(map(str, item["tension"]))})
        print(f"{item_id}.mid  {item['condition']:<10} {' '.join(chord_names)}")

    with open(args.key, "w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {args.key} ({len(rows)} items)")
    counts = {c: sum(1 for r in rows if r["condition"] == c) for c in ("chordslots", "random", "corpus")}
    print("counts:", counts)
    return 0


if __name__ == "__main__":
    sys.exit(main())
