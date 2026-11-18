// Tritone side-effect probe: how often each chord family is chosen per mood.
// The kTritoneWeight value is fixed by the header copy this is compiled against (see tritone_sidefx.sh).
// usage: tritone_sidefx SEEDS LABEL
//
// Per mood: applyMood() on a 4-slot, tonic-C progression (the mood's own scale, adventure,
// syncopation and B/T/E curves), roll() with seed 1..SEEDS, then consecutive identical chords
// are collapsed to one (a chord repeated in the next slot counts once).
//
// Classification (chord type names/intervals from chordTypes() in Theory.h):
//   dominant : intervals contain a major third (4) and a minor seventh (10) mod 12
//              -> 7, 9, 13, 7b9, 7#9 (7sus4 / 9sus4 have no major third, so they are excluded)
//   dim      : type suffix "dim" or "dim7"
//   m7b5     : type suffix "m7b5"
//   tritone  : hasChordTritone(type) from Metrics.h (any chord with an inherent tritone)
//   tonic    : chord root equals the tonic pitch class (any quality)
#include "chordslots/Generator.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
using namespace cs;

struct Counts
{
    long progs = 0, chords = 0, dom = 0, dim = 0, dim7 = 0, m7b5 = 0, tritone = 0, tonic = 0;
    std::map<std::string, long> byType;
};

static bool isDominant (const std::vector<int>& iv)
{
    bool maj3 = false, min7 = false;
    for (int x : iv)
    {
        if (x % 12 == 4) maj3 = true;
        if (x % 12 == 10) min7 = true;
    }
    return maj3 && min7;
}

int main (int argc, char** argv)
{
    if (argc < 3) { fprintf (stderr, "usage: %s SEEDS LABEL\n", argv[0]); return 1; }
    const int seeds = atoi (argv[1]);
    const std::string label = argv[2];
    const auto& M = moods();
    const int tonicPc = 0;

    for (int m = 0; m < (int) M.size(); ++m)
    {
        Counts c;
        for (int seed = 1; seed <= seeds; ++seed)
        {
            Progression p;
            p.tonic = tonicPc;
            p.numSlots = 4;
            layoutEven (p);
            applyMood (p, m, true);
            Rng rng ((unsigned) seed);
            roll (p, rng);

            c.progs += 1;
            for (int i = 0; i < p.numSlots; ++i)
            {
                const Chord ch = p.slots[(size_t) i].chord;
                if (i > 0 && ch == p.slots[(size_t) i - 1].chord)
                    continue; // collapse repeated chord
                const auto& t = chordTypes()[(size_t) ch.type];
                c.chords += 1;
                if (isDominant (t.iv)) c.dom += 1;
                if (std::string (t.suffix) == "dim") c.dim += 1;
                if (std::string (t.suffix) == "dim7") c.dim7 += 1;
                if (std::string (t.suffix) == "m7b5") c.m7b5 += 1;
                if (hasChordTritone (ch.type)) c.tritone += 1;
                if (mod12 (ch.root - p.tonic) == 0) c.tonic += 1;
                c.byType[t.suffix] += 1;
            }
        }
        printf ("MOOD,%s,%s,progs=%ld,chords=%ld,dom=%ld,dim=%ld,dim7=%ld,m7b5=%ld,tritone=%ld,tonic=%ld\n",
                label.c_str(), M[(size_t) m].name, c.progs, c.chords, c.dom, c.dim, c.dim7, c.m7b5, c.tritone, c.tonic);
        for (const auto& kv : c.byType)
            printf ("TYPE,%s,%s,%s,%ld\n", label.c_str(), M[(size_t) m].name, kv.first.c_str(), kv.second);
    }
    return 0;
}
