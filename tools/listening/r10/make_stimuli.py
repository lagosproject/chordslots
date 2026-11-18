#!/usr/bin/env python3
"""Generate the R10 pairwise tritone stimuli as Standard MIDI Files.

Standard library only. Writes format-0 SMF files to stimuli/ and a pairs.json
(operator-only key: chord content and which file is which) next to this script.

Conventions
- Scientific pitch: C3 = MIDI 48, C4 = 60, C5 = 72. Every note lies in 48..72.
- Voicing: root in the bass, the remaining chord tones stacked in close position
  above it. The same voicing rule is used for every chord in the set.
- Instrument: General MIDI program 0 (Acoustic Grand Piano), channel 0.
- Tempo 80 bpm, 4/4, velocity 80 for every note, 480 ticks per beat.
- Isolated chords: 2 beats. Cadences: one chord per bar (4 beats each).

Usage: python3 make_stimuli.py [--out-dir DIR] [--pairs-json PATH]
"""

import argparse
import json
import os
import struct

TPQ = 480            # ticks per quarter note
BPM = 80
PROGRAM = 0          # Acoustic Grand Piano
CHANNEL = 0
VELOCITY = 80
BEAT = TPQ
LOW, HIGH = 48, 72   # C3..C5 register limit

# Chord content as MIDI notes (root in bass, close position above).
CHORDS = {
    "G7":    [55, 59, 62, 65],  # G3 B3 D4 F4
    "Gmaj7": [55, 59, 62, 66],  # G3 B3 D4 F#4
    "Bdim":  [59, 62, 65],      # B3 D4 F4
    "Bm7b5": [59, 62, 65, 69],  # B3 D4 F4 A4
    "C":     [48, 52, 55, 60],  # C3 E3 G3 C4
    "F":     [53, 57, 60, 65],  # F3 A3 C4 F4
    "G":     [55, 59, 62],      # G3 B3 D4
}

PAIRS = [
    {
        "pair_id": "pair01",
        "type": "isolated",
        "label": "G7 vs Gmaj7",
        "A": {"name": "G7", "segments": [("G7", 2)]},
        "B": {"name": "Gmaj7", "segments": [("Gmaj7", 2)]},
        "contrast": ("Same bass G and same upper voices except the seventh (F vs F#). "
                     "G7 contains the tritone B-F; Gmaj7 does not."),
    },
    {
        "pair_id": "pair02",
        "type": "isolated",
        "label": "Bdim vs Bm7b5",
        "A": {"name": "Bdim", "segments": [("Bdim", 2)]},
        "B": {"name": "Bm7b5", "segments": [("Bm7b5", 2)]},
        "contrast": ("Both contain the tritone B-F. They differ by the added seventh A in "
                     "Bm7b5, so this pair does not vary the tritone itself."),
    },
    {
        "pair_id": "pair03",
        "type": "cadence",
        "label": "V7 vs IV, resolving to C",
        "A": {"name": "G7-C", "segments": [("G7", 4), ("C", 4)]},
        "B": {"name": "F-C", "segments": [("F", 4), ("C", 4)]},
        "contrast": ("Key of C. The first bar is G7 (tritone B-F) or F (no tritone); "
                     "the second bar is C in both files."),
    },
    {
        "pair_id": "pair04",
        "type": "cadence",
        "label": "V7 vs V, I-V-I in C",
        "A": {"name": "C-G7-C", "segments": [("C", 4), ("G7", 4), ("C", 4)]},
        "B": {"name": "C-G-C", "segments": [("C", 4), ("G", 4), ("C", 4)]},
        "contrast": ("Key of C, I-V-I with the dominant as seventh chord (A) or triad (B). "
                     "Only the middle bar differs."),
    },
]


def pitch_classes(notes):
    return sorted({n % 12 for n in notes})


def has_tritone(notes):
    pcs = pitch_classes(notes)
    return any(((b - a) % 12) == 6 for i, a in enumerate(pcs) for b in pcs[i + 1:])


def vlq(value):
    """Variable-length quantity encoding used by SMF delta times."""
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(out))


def build_events(segments):
    """Turn [(chord_name, beats), ...] into sorted (tick, order, bytes) events.

    order: 0 = meta/program, 1 = note-off, 2 = note-on. Note-offs sort before
    note-ons at the same tick so a repeated pitch is retriggered cleanly.
    """
    events = []
    start = 0
    for chord_name, beats in segments:
        dur = beats * BEAT
        for note in CHORDS[chord_name]:
            if not (LOW <= note <= HIGH):
                raise ValueError(f"{chord_name}: note {note} outside C3..C5")
            events.append((start, 2, bytes([0x90 | CHANNEL, note, VELOCITY])))
            events.append((start + dur, 1, bytes([0x80 | CHANNEL, note, 0])))
        start += dur
    end = start
    return events, end


def write_smf(path, segments):
    events, end_tick = build_events(segments)
    usec_per_quarter = int(round(60_000_000 / BPM))
    head = [
        (0, 0, b"\xFF\x51\x03" + usec_per_quarter.to_bytes(3, "big")),   # tempo
        (0, 0, b"\xFF\x58\x04\x04\x02\x18\x08"),                          # 4/4
        (0, 0, bytes([0xC0 | CHANNEL, PROGRAM])),                         # program
    ]
    allev = head + events
    allev.sort(key=lambda e: (e[0], e[1]))

    track = bytearray()
    last = 0
    for tick, _order, data in allev:
        track += vlq(tick - last) + data
        last = tick
    track += vlq(end_tick - last) + b"\xFF\x2F\x00"   # end of track

    smf = b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPQ)
    smf += b"MTrk" + struct.pack(">I", len(track)) + bytes(track)
    with open(path, "wb") as fh:
        fh.write(smf)
    return end_tick


def describe(variant):
    """Operator-readable description of one file's content."""
    segs = []
    all_notes = []
    for chord_name, beats in variant["segments"]:
        notes = CHORDS[chord_name]
        all_notes.extend(notes)
        segs.append({"chord": chord_name, "midi": notes, "beats": beats,
                     "contains_tritone": has_tritone(notes)})
    return segs, has_tritone(all_notes)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out-dir", default=os.path.join(here, "stimuli"))
    ap.add_argument("--pairs-json", default=os.path.join(here, "pairs.json"))
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    manifest = {
        "tempo_bpm": BPM,
        "meter": "4/4",
        "program": PROGRAM,
        "program_name": "Acoustic Grand Piano (GM 0)",
        "velocity": VELOCITY,
        "ticks_per_quarter": TPQ,
        "register": "C3 (MIDI 48) to C5 (MIDI 72); root in bass, close position above",
        "operator_only": "Do not give this file to listeners. The A/B content key is here.",
        "pairs": [],
    }
    for pair in PAIRS:
        entry = {"pair_id": pair["pair_id"], "type": pair["type"],
                 "label": pair["label"], "contrast": pair["contrast"], "files": {}}
        for letter in ("A", "B"):
            variant = pair[letter]
            fname = f"r10_{pair['pair_id']}_{letter}.mid"
            fpath = os.path.join(args.out_dir, fname)
            end_tick = write_smf(fpath, variant["segments"])
            segs, tritone = describe(variant)
            entry["files"][letter] = {
                "file": fname,
                "name": variant["name"],
                "segments": segs,
                "contains_tritone": tritone,
                "duration_beats": end_tick // BEAT,
                "duration_seconds": round(end_tick / TPQ * 60 / BPM, 3),
            }
            print(f"wrote {fpath} ({end_tick // BEAT} beats)")
        manifest["pairs"].append(entry)

    with open(args.pairs_json, "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2, ensure_ascii=False)
        fh.write("\n")
    print(f"wrote {args.pairs_json}")


if __name__ == "__main__":
    main()
