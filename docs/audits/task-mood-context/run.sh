#!/bin/bash
# usage: run.sh <flag true|false> <hard_value> [seeds]   (header copy; repo untouched)
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=$(mktemp -d); cp -r $R/core/include/chordslots $D/
sed -i "s/kMoodDiatonicContext = false/kMoodDiatonicContext = $1/" $D/chordslots/Generator.h
sed -i "s/std::nullopt, 0.40f }/std::nullopt, ${2}f }/" $D/chordslots/Generator.h
g++ -std=c++17 -O2 -I $D "$(dirname "$0")/probe.cpp" -o $D/p && $D/p ${3:-600}
