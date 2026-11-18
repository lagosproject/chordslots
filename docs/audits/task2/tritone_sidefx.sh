#!/bin/bash
# Tritone side-effect measurement: kTritoneWeight in {0, 0.04, 0.06}, all 8 moods.
# usage: tritone_sidefx.sh SEEDS OUTFILE
# Works on header copies in $SCR (default: a fresh mktemp dir); the repo headers are never edited.
# Builds run one at a time (-j1), so at most 2 compiles are ever active.
set -e
SEEDS=${1:?usage: tritone_sidefx.sh SEEDS OUTFILE}
OUT=${2:?usage: tritone_sidefx.sh SEEDS OUTFILE}
D=$(cd "$(dirname "$0")" && pwd)
INC="$D/../../../core/include"
S=${SCR:-$(mktemp -d)}
for w in 0.00 0.04 0.06; do
  W=$S/w$w; rm -rf "$W"; mkdir -p "$W"
  cp -r "$INC/chordslots" "$W/"
  sed -i "s/kTritoneWeight = [0-9.]*f/kTritoneWeight = ${w}f/" "$W/chordslots/Metrics.h"
  grep -q "kTritoneWeight = ${w}f" "$W/chordslots/Metrics.h" || { echo "sed failed for $w" >&2; exit 1; }
  g++ -std=c++17 -O2 -I "$W" "$D/tritone_sidefx.cpp" -o "$W/probe"
done
for w in 0.00 0.04 0.06; do
  "$S/w$w/probe" "$SEEDS" "w$w" > "$S/w$w.out" &
done
wait
cat "$S/w0.00.out" "$S/w0.04.out" "$S/w0.06.out" > "$OUT"
echo "wrote $OUT" >&2
