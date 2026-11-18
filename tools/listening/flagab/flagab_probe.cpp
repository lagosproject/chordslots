// Flag A/B probe: rolls ChordSlots progressions for the minimal-pair search of the three engine flags.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Build (the python driver build_flagab.py does this on patched header copies):
//   g++ -std=c++17 -O2 -I <include dir> tools/listening/flagab/flagab_probe.cpp -o flagab_probe
//
// Usage:
//   flagab_probe roll MOOD TONIC SEED
//       Same progression and output as make_panel roll (applyMood, layoutEven, roll, one chord per bar):
//       "PROG mood=.. tonic=.. scale=.." then "SLOT i root suffix bassInt brightness tension energy n1 ...".
//   flagab_probe batch
//       Reads "MOOD TONIC SEED" lines from stdin. For each, prints the same PROG and SLOT lines as roll,
//       then a line "END". Same numbers as roll, one process for many rolls.
//   flagab_probe types
//       Prints "TYPE index suffix iv1 iv2 ..." for every chordTypes() entry (for the chord-tone check).
#include "chordslots/Generator.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace cs;

static int moodIndex (const std::string& name)
{
    const auto& m = moods();
    for (size_t i = 0; i < m.size(); ++i)
        if (name == m[i].name)
            return (int) i;
    std::fprintf (stderr, "unknown mood '%s'\n", name.c_str());
    std::exit (1);
}

static std::vector<int> sortedNotes (const Voicing& v)
{
    std::vector<int> n (v.notes.begin(), v.notes.begin() + v.n);
    std::sort (n.begin(), n.end());
    return n;
}

static void rollOne (int moodIdx, int tonic, unsigned seed)
{
    Progression p;
    p.tonic = mod12 (tonic);
    p.bars = 4;
    p.numSlots = 4;
    p.locks.rhythm = true; // one chord per bar for every condition
    layoutEven (p);
    applyMood (p, moodIdx);
    Rng rng (seed);
    roll (p, rng);

    std::printf ("PROG mood=%s tonic=%d scale=%d\n", moods()[(size_t) moodIdx].name, p.tonic, p.scale);
    for (int i = 0; i < p.numSlots; ++i)
    {
        const Slot& s = p.slots[(size_t) i];
        const auto notes = sortedNotes (s.voicing);
        const char* suffix = chordTypes()[(size_t) s.chord.type].suffix;
        std::printf ("SLOT %d %d %s %d %.6f %.6f %.6f", i, s.chord.root, suffix[0] ? suffix : "-", s.chord.bassInt,
                     s.brightness, s.tension, s.energy);
        for (int n : notes)
            std::printf (" %d", n);
        std::printf ("\n");
    }
}

static int runRoll (int argc, char** argv)
{
    if (argc < 5)
        return 2;
    rollOne (moodIndex (argv[2]), std::atoi (argv[3]), (unsigned) std::strtoul (argv[4], nullptr, 10));
    return 0;
}

static int runBatch()
{
    std::string line;
    while (std::getline (std::cin, line))
    {
        std::istringstream ss (line);
        std::string mood;
        int tonic = 0;
        unsigned long seed = 0;
        if (! (ss >> mood >> tonic >> seed))
            continue;
        rollOne (moodIndex (mood), tonic, (unsigned) seed);
        std::printf ("END\n");
    }
    return 0;
}

static int runTypes()
{
    const auto& t = chordTypes();
    for (size_t i = 0; i < t.size(); ++i)
    {
        std::printf ("TYPE %d %s", (int) i, t[i].suffix[0] ? t[i].suffix : "-");
        for (int iv : t[i].iv)
            std::printf (" %d", iv);
        std::printf ("\n");
    }
    return 0;
}

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: flagab_probe roll MOOD TONIC SEED | batch | types\n");
        return 2;
    }
    if (std::strcmp (argv[1], "roll") == 0)
        return runRoll (argc, argv);
    if (std::strcmp (argv[1], "batch") == 0)
        return runBatch();
    if (std::strcmp (argv[1], "types") == 0)
        return runTypes();
    std::fprintf (stderr, "unknown mode '%s'\n", argv[1]);
    return 2;
}
