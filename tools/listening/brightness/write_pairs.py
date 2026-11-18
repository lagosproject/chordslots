#!/usr/bin/env python3
"""Write the Task 5 Brightness listening pairs as MIDI and check them by re-reading the files.

Input: the JSON printed by make_pairs (C++ probe, same directory).
Output: pairs/pairNN_{A,B}.mid and pairs.json (operator key), then a check table printed to stdout.

The SMF writer is the one from tools/listening/r10/make_stimuli.py. Its tempo and note range come from
module globals, which this script sets to 90 bpm and the full MIDI range. Velocity is 80 on every note,
General MIDI program 0 (Acoustic Grand Piano), 4/4, one chord per bar, four bars.

Usage (from this directory):
  ./make_pairs > pairs_raw.json
  python3 write_pairs.py pairs_raw.json
"""

import argparse
import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_R10 = os.path.join(HERE, "..", "r10")

BPM = 90
VELOCITY = 80
PROGRAM = 0
BAR_BEATS = 4
UPPER_FLOOR = 48  # same convention as kUpperFloor in the engine

r10 = None  # the R10 module, loaded by load_r10()


def load_r10(r10_dir):
    """Imports make_stimuli.py from the R10 folder (its SMF writer and VLQ encoder) and sets the tempo,
    velocity and note range that writer reads from module globals."""
    global r10
    sys.path.insert(0, os.path.abspath(r10_dir))
    import make_stimuli
    r10 = make_stimuli
    r10.BPM = BPM
    r10.VELOCITY = VELOCITY
    r10.PROGRAM = PROGRAM
    r10.LOW, r10.HIGH = 0, 127


def write_midi(path, bars, tag):
    """bars: one sorted MIDI note list per bar (4 beats each). Returns the end tick."""
    segments = []
    for i, notes in enumerate(bars):
        name = f"{tag}_bar{i}"
        r10.CHORDS[name] = list(notes)
        segments.append((name, BAR_BEATS))
    return r10.write_smf(path, segments)


def read_smf(path):
    """Minimal format-0 reader. Returns format, tempo (bpm), program, velocities and note-ons (tick, pitch, vel)."""
    with open(path, "rb") as fh:
        data = fh.read()
    assert data[:4] == b"MThd", "not an SMF file"
    fmt, _ntrk, division = struct.unpack(">HHH", data[8:14])
    assert data[14:18] == b"MTrk", "missing track chunk"
    tlen = struct.unpack(">I", data[18:22])[0]
    trk = data[22:22 + tlen]

    def vlq(buf, i):
        val = 0
        while True:
            b = buf[i]
            i += 1
            val = (val << 7) | (b & 0x7F)
            if not b & 0x80:
                return val, i

    tick, i, status = 0, 0, 0
    tempo_us, program, notes, velocities = None, None, [], set()
    while i < len(trk):
        delta, i = vlq(trk, i)
        tick += delta
        b = trk[i]
        if b == 0xFF:  # meta event
            mtype = trk[i + 1]
            length, j = vlq(trk, i + 2)
            if mtype == 0x51:
                tempo_us = int.from_bytes(trk[j:j + length], "big")
            i = j + length
            if mtype == 0x2F:
                break
            continue
        if b in (0xF0, 0xF7):  # sysex
            length, j = vlq(trk, i + 1)
            i = j + length
            continue
        if b & 0x80:
            status = b
            i += 1
        kind = status & 0xF0
        if kind in (0xC0, 0xD0):  # one data byte
            if kind == 0xC0:
                program = trk[i]
            i += 1
            continue
        pitch, vel = trk[i], trk[i + 1]
        i += 2
        if kind == 0x90 and vel > 0:
            notes.append((tick, pitch, vel))
            velocities.add(vel)
    return {"format": fmt, "division": division,
            "bpm": round(60_000_000 / tempo_us, 3) if tempo_us else None,
            "program": program, "velocities": sorted(velocities), "notes": notes}


def bar_pitches(notes, bar_ticks, n_bars):
    """Sorted pitches of the note-ons that start in each bar."""
    bars = [[] for _ in range(n_bars)]
    for tick, pitch, _vel in notes:
        b = tick // bar_ticks
        if b < n_bars:
            bars[b].append(pitch)
    return [sorted(b) for b in bars]


def probe_brightness(probe, rows):
    """Engine Brightness for each (tonic, scale, root, suffix, notes) row, via `make_pairs check`."""
    lines = []
    for tonic, scale, root, suffix, notes in rows:
        lines.append(f"{tonic} {scale} {root} {suffix if suffix else '-'} 0 {len(notes)} " + " ".join(map(str, notes)))
    out = subprocess.run([probe, "check"], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
    vals = [float(x) for x in out.stdout.split()]
    assert len(vals) == len(rows), "probe returned a different number of rows"
    return vals


def build_key(raw, out_dir):
    key_pairs = []
    for pair in raw:
        pid = pair["pair_id"]
        entry = {"pair_id": pid, "key": pair["key"], "type": pair["type"],
                 "tonic": pair["tonic"], "scale": pair["scale"], "files": {}}
        for letter in ("A", "B"):
            loop = pair[letter]
            fname = f"{pid}_{letter}.mid"
            bars = [s["notes"] for s in loop["slots"]]
            assert all(len(set(b)) == len(b) for b in bars), f"{fname}: repeated pitch"
            end_tick = write_midi(os.path.join(out_dir, fname), bars, f"{pid}{letter}")
            entry["files"][letter] = {
                "file": fname,
                "label": loop["label"],
                "upper_mean": round(loop["upper_mean"], 4),
                "brightness": round(loop["brightness"], 6),
                "duration_beats": end_tick // r10.TPQ,
                "chords": [{"chord": s["chord"], "root": s["root"], "suffix": s["suffix"],
                            "pcs": s["pcs"], "notes": s["notes"],
                            "brightness": round(s["brightness"], 6)} for s in loop["slots"]],
            }
        a, b = entry["files"]["A"], entry["files"]["B"]
        entry["delta_upper"] = round(b["upper_mean"] - a["upper_mean"], 4)
        entry["delta_brightness"] = round(b["brightness"] - a["brightness"], 6)
        entry["higher_register_file"] = "A" if a["upper_mean"] > b["upper_mean"] else "B"
        entry["higher_brightness_file"] = "A" if a["brightness"] > b["brightness"] else "B"
        if pair["type"] == "register":
            entry["contrast"] = ("Identical chords in the same order. In one file the top voices of each chord sit "
                                 "about 8 semitones lower; the bass and the chord content are unchanged.")
        elif pair["type"] == "conflict":
            low = "A" if a["upper_mean"] < b["upper_mean"] else "B"
            entry["lower_register_file"] = low
            entry["brighter_colour_file"] = low  # by construction: brighter chord qualities sit in the lower register
            entry["contrast"] = ("Register and chord colour pull in opposite directions at the same engine size. One file "
                                 "has brighter chord qualities about 8 semitones lower; the other has darker qualities at "
                                 "the higher register. Net engine Brightness is within 0.03. Choosing the higher register "
                                 "means register dominates; choosing the brighter chords means colour dominates.")
        else:
            entry["contrast"] = ("Different chord qualities on the same roots, with the voicing chosen so that the mean "
                                 "pitch of the upper notes is the same in both files.")
        key_pairs.append(entry)
    return key_pairs


def check_files(key_pairs, out_dir, probe):
    """Re-read each MIDI file, compare it with the probe's chords and recompute Brightness from the MIDI notes."""
    bar_ticks = BAR_BEATS * r10.TPQ
    all_ok = True
    probe_rows, owners = [], []  # owners: (pair_id, letter, bar index)
    print("Table 1. MIDI files re-read (meta, program, velocity, bars, pitch classes against the probe's chords)")
    print(f"{'file':<16}{'fmt':>4}{'bpm':>8}{'prog':>6}{'vel':>6}{'bars':>6}{'pcs_ok':>8}"
          f"{'upper(MIDI)':>13}{'upper(probe)':>14}")
    for entry in key_pairs:
        for letter in ("A", "B"):
            f = entry["files"][letter]
            smf = read_smf(os.path.join(out_dir, f["file"]))
            n_bars = len(f["chords"])
            bars = bar_pitches(smf["notes"], bar_ticks, n_bars)
            meta_ok = (smf["bpm"] is not None and abs(smf["bpm"] - BPM) < 0.01 and smf["program"] == PROGRAM
                       and smf["velocities"] == [VELOCITY] and smf["format"] == 0
                       and sum(len(b) for b in bars) == len(smf["notes"]))
            pcs_ok = True
            up_sum, up_n = 0.0, 0
            for bi, (got, cinfo) in enumerate(zip(bars, f["chords"])):
                if sorted({p % 12 for p in got}) != sorted(cinfo["pcs"]) or got != sorted(cinfo["notes"]):
                    pcs_ok = False
                up_sum += sum(p for p in got if p >= UPPER_FLOOR)
                up_n += sum(1 for p in got if p >= UPPER_FLOOR)
                probe_rows.append((entry["tonic"], entry["scale"], cinfo["root"], cinfo["suffix"], got))
                owners.append((entry["pair_id"], letter, bi, cinfo["chord"]))
            up_midi = up_sum / up_n
            all_ok = all_ok and meta_ok and pcs_ok and len(bars) == n_bars
            print(f"{f['file']:<16}{smf['format']:>4}{smf['bpm']:>8}{smf['program']:>6}"
                  f"{','.join(map(str, smf['velocities'])):>6}{n_bars:>6}{'yes' if pcs_ok else 'NO':>8}"
                  f"{up_midi:>13.2f}{f['upper_mean']:>14.2f}")
            f["_upper_midi"] = up_midi
            f["_pcs_ok"] = pcs_ok and meta_ok

    bright = probe_brightness(probe, probe_rows)
    print()
    print("Table 2. Engine Brightness of each bar, recomputed from the MIDI notes")
    print(f"{'pair':<8}{'loop':>5}{'bar':>4}{'chord':>8}{'MIDI notes':>34}{'brightness(MIDI)':>18}{'brightness(probe)':>19}")
    per_file = {}
    for (pid, letter, bi, chord), val, row in zip(owners, bright, probe_rows):
        probe_val = None
        for entry in key_pairs:
            if entry["pair_id"] == pid:
                probe_val = entry["files"][letter]["chords"][bi]["brightness"]
        notes_s = " ".join(str(n) for n in row[4])
        print(f"{pid:<8}{letter:>5}{bi + 1:>4}{chord:>8}{notes_s:>34}{val:>18.4f}{probe_val:>19.4f}")
        per_file.setdefault((pid, letter), []).append(val)

    print()
    print("Table 3. Pair summary (register = mean upper pitch of notes >= MIDI 48; Brightness = mean over the 4 bars)")
    print(f"{'pair':<8}{'type':>9}{'key':>4}{'upperA':>9}{'upperB':>9}{'dUpper':>9}{'brightA':>9}{'brightB':>9}"
          f"{'dBright':>9}{'higher reg':>12}{'higher bright':>15}")
    for entry in key_pairs:
        ma = sum(per_file[(entry["pair_id"], "A")]) / 4
        mb = sum(per_file[(entry["pair_id"], "B")]) / 4
        a, b = entry["files"]["A"], entry["files"]["B"]
        d_up = a["_upper_midi"] - b["_upper_midi"]
        print(f"{entry['pair_id']:<8}{entry['type']:>9}{entry['key']:>4}{a['_upper_midi']:>9.2f}{b['_upper_midi']:>9.2f}"
              f"{b['_upper_midi'] - a['_upper_midi']:>+9.2f}{ma:>9.4f}{mb:>9.4f}{mb - ma:>+9.4f}"
              f"{entry['higher_register_file']:>12}{entry['higher_brightness_file']:>15}")
        if abs(ma - a["brightness"]) > 0.0005 or abs(mb - b["brightness"]) > 0.0005:
            all_ok = False
        if entry["type"] == "register" and not (7.0 <= abs(d_up) <= 12.0):
            all_ok = False
        if entry["type"] == "colour" and (abs(d_up) > 1.0 or abs(mb - ma) < 0.25):
            all_ok = False
        if entry["type"] == "colour_small" and (abs(d_up) > 1.0 or not 0.075 <= abs(mb - ma) <= 0.105):
            all_ok = False
        if entry["type"] == "conflict" and (not 7.0 <= abs(d_up) <= 12.0 or abs(mb - ma) > 0.03):
            all_ok = False
        for letter in ("A", "B"):
            entry["files"][letter].pop("_upper_midi", None)
            entry["files"][letter].pop("_pcs_ok", None)
    return all_ok


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("raw", help="JSON from make_pairs")
    ap.add_argument("--out-dir", default=os.path.join(HERE, "pairs"))
    ap.add_argument("--json", default=os.path.join(HERE, "pairs.json"))
    ap.add_argument("--probe", default=os.path.join(HERE, "make_pairs"))
    ap.add_argument("--r10-dir", default=DEFAULT_R10, help="folder holding make_stimuli.py (R10 SMF writer)")
    args = ap.parse_args()

    load_r10(args.r10_dir)
    with open(args.raw, encoding="utf-8") as fh:
        raw = json.load(fh)
    os.makedirs(args.out_dir, exist_ok=True)

    key_pairs = build_key(raw, args.out_dir)
    all_ok = check_files(key_pairs, args.out_dir, args.probe)
    print()
    print("all MIDI checks passed" if all_ok else "SOME CHECKS FAILED")

    doc = {
        "description": "Operator key for the Task 5 Brightness pairs. Do not give this file to listeners.",
        "tempo_bpm": BPM, "meter": "4/4", "program": PROGRAM, "program_name": "Acoustic Grand Piano (GM 0)",
        "velocity": VELOCITY, "bars_per_file": 4, "beats_per_bar": BAR_BEATS,
        "upper_register": "mean of the sounding notes at or above MIDI 48, as in the engine's registerBrightness",
        "brightness": "engine Brightness of the slot (combineBrightness of contextQuality, modal and register terms)",
        "pairs": key_pairs,
    }
    with open(args.json, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=2, ensure_ascii=False)
        fh.write("\n")
    print(f"wrote {args.json}")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
