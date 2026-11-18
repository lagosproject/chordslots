#!/bin/bash
# usage: t3run.sh SEEDS OUTFILE ; builds t3 and runs baseline + 10 ablation pairs in parallel
D=$(dirname "$0"); B=${BIN:-/tmp/t3bin}
g++ -std=c++17 -O2 -I "$D/../../../core/include" "$D/t3.cpp" -o "$B" || exit 1
( "$B" $1 > "$2.base" ) &
for k in 0 1 2 3 4 5 6 7 8 9; do ( "$B" $1 $k > "$2.$k" ) & done
wait; cat "$2.base" "$2".[0-9] > "$2"
