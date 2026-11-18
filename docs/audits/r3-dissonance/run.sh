#!/bin/bash
# Task R3: does Tension need both dissonance terms? Three header variants of the 0.5/0.5 split:
#   a = 0.5 roughness + 0.5 harmonic (default), b = roughness only (1/0), c = harmonic only (0/1).
# usage: run.sh   (SCR overrides the scratch directory; it must be absolute)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
S="${SCR:-/tmp/claude-1000/-home-vant-Documentos-ChordSlots/1f0d8a57-76e5-468b-8266-c3e4a3a52a01/scratchpad}/r3"
RAW="$HERE/raw"
MAT="$S/materials/repo"
mkdir -p "$S" "$RAW"

# Study materials (pinned revision used by docs/audits/REPORT.md). Not committed.
if [ ! -d "$MAT/.git" ]; then
  mkdir -p "$S/materials"
  git clone -q https://github.com/merismeris/tonal-tension-TIS.git "$MAT"
  git -C "$MAT" checkout -q --detach a5ca4a211e4ab34dcc9b7ba4f69cb863d801494f
fi

build_variant() {  # name wr wh
  local name=$1 wr=$2 wh=$3
  local D="$S/variant_$name"
  rm -rf "$D"; mkdir -p "$D/run"
  python3 "$HERE/patch_weights.py" "$ROOT/core/include/chordslots" "$D/inc" "$wr" "$wh" > "$RAW/patch_$name.diff"
  g++ -std=c++17 -O2 -I "$D/inc" "$HERE/r3probe.cpp" -o "$D/probe"
  g++ -std=c++17 -O2 -I "$D/inc" "$ROOT/docs/audits/engine.cpp" -o "$D/run/engine.exe"
  "$D/probe" "$name" > "$RAW/probe_$name.csv"
  cp "$ROOT/docs/audits/stimuli.mjs" "$ROOT/docs/audits/analyze.mjs" "$D/run/"
  # analyze.mjs writes the tension tables before its brightness step, which needs the separate
  # brightness sweep CSVs; that later failure is expected here and is ignored.
  (cd "$D/run" && CHORDSLOTS_AUDIT_MATERIALS="$MAT" node analyze.mjs > "$RAW/analyze_stdout_$name.txt" 2>&1 || true)
  cp "$D/run/tension-comparison.csv" "$RAW/tension-comparison_$name.csv"
  cp "$D/run/tension-profiles.csv" "$RAW/tension-profiles_$name.csv"
}

build_variant a 0.5 0.5 &
build_variant b 1 0 &
wait
build_variant c 0 1
python3 "$HERE/summarize.py" "$RAW" | tee "$RAW/summary.md"
