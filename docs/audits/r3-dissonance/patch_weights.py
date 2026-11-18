#!/usr/bin/env python3
"""Copy the chordslots headers and set the roughness/harmonic weights of the dissonance term.

usage: patch_weights.py SRC_DIR DST_DIR WR WH

The 0.5/0.5 split is written in two places in Metrics.h:
  - typeDissonance (symbolic chord tension, used by tensionOf)
  - voicedTension (roughness from the sounding voicing; used by the static-with-voicing and contextual scores)
Both are patched with the same weights so that the dissonance term is consistent across the models.
Every pattern must match exactly once, otherwise the script stops.
"""
import difflib
import os
import shutil
import sys

PATTERNS = [
    "return 0.5f * rough + 0.5f * harmonicDissonance (type);",
    "return tensionFromDissonance (c, tonic, scaleIdx, 0.5f * rough + 0.5f * harmonicDissonance (c.type));",
]


def main() -> int:
    src, dst, wr, wh = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, os.path.join(dst, "chordslots"))
    path = os.path.join(dst, "chordslots", "Metrics.h")
    with open(path, encoding="utf-8") as f:
        text = f.read()
    original = text
    for pat in PATTERNS:
        if text.count(pat) != 1:
            print(f"pattern count {text.count(pat)} for: {pat}", file=sys.stderr)
            return 1
        repl = pat.replace("0.5f * rough", f"{wr:.1f}f * rough").replace("0.5f * harmonicDissonance", f"{wh:.1f}f * harmonicDissonance")
        text = text.replace(pat, repl)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    diff = difflib.unified_diff(
        original.splitlines(keepends=True), text.splitlines(keepends=True),
        fromfile="original/Metrics.h", tofile=f"variant/Metrics.h (wr={wr:.1f}, wh={wh:.1f})")
    sys.stdout.writelines(diff)
    return 0


if __name__ == "__main__":
    sys.exit(main())
