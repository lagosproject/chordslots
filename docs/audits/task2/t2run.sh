#!/bin/bash
# usage: t2run.sh SEEDS  -> builds ctx=true and ctx=false variants, prints CSV-ish lines
S=${SCR:-/tmp/claude-1000/-home-vant-Documentos-ChordSlots/7958082c-71bd-4ee4-8aa9-bb86531f6b44/scratchpad}
for v in true false; do
  D=$S/t2_$v; rm -rf $D; mkdir -p $D; cp -r "$(dirname "$0")/../../../core/include/chordslots" $D/
  sed -i "s/kUseContextualTension = true/kUseContextualTension = $v/" $D/chordslots/Metrics.h
  g++ -std=c++17 -O2 -I $D "$(dirname "$0")/t2.cpp" -o $D/p 2>&1 | head -20 &
done; wait
for v in true false; do $S/t2_$v/p $1 ctx_$v > $S/t2_$v.out & done; wait
cat $S/t2_true.out $S/t2_false.out
