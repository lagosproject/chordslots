#!/bin/bash
# R2 ablation: tritone weight w in {0,0.04,0.06}, contextual on. usage: t2r2.sh SEEDS
S=${SCR:-/tmp/claude-1000/-home-vant-Documentos-ChordSlots/7958082c-71bd-4ee4-8aa9-bb86531f6b44/scratchpad}
for w in 0.00 0.04 0.06; do
  D=$S/r2_$w; rm -rf $D; mkdir -p $D; cp -r "$(dirname "$0")/../../../core/include/chordslots" $D/
  sed -i "s/kTritoneWeight = [0-9.]*f/kTritoneWeight = ${w}f/" $D/chordslots/Metrics.h
  grep -q "kTritoneWeight = ${w}f" $D/chordslots/Metrics.h || echo "sed failed $w"
  g++ -std=c++17 -O2 -I $D "$(dirname "$0")/t2.cpp" -o $D/p &
done; wait
for w in 0.00 0.04 0.06; do $S/r2_$w/p $1 w$w > $S/r2_$w.out & done; wait
