// chordslots-cli: exercise the core from a terminal, no plugin host needed.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
//   chordslots-cli table [scale]            metric table for every candidate chord in C
//   chordslots-cli roll [mood] [count] [seed]   generate progressions for a mood

#include "chordslots/Generator.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace cs;

static void printProgression (const Progression& p)
{
    for (int i = 0; i < p.numSlots; ++i)
    {
        const Slot& s = p.slots[(size_t) i];
        std::printf ("  %-8s", chordName (s.chord, p.tonic, p.scale).c_str());
    }
    std::printf ("  fit %3.0f%%\n", 100.0f * fitScore (p));
}

static void printDetail (const Progression& p)
{
    for (int i = 0; i < p.numSlots; ++i)
    {
        const Slot& s = p.slots[(size_t) i];
        std::printf ("    step %3d-%3d  %-8s %-9s B %.2f (want %.2f)  T %.2f (want %.2f)  E %.2f  vel %3d  notes",
                     s.start, p.slotEnd (i), chordName (s.chord, p.tonic, p.scale).c_str(),
                     romanNumeral (s.chord, p.tonic).c_str(), s.brightness, s.targetB, s.tension, s.targetT,
                     s.energy, s.voicing.velocity);
        for (int j = 0; j < s.voicing.n; ++j)
            std::printf (" %d", s.voicing.notes[(size_t) j]);
        std::printf ("\n");
    }
}

int main (int argc, char** argv)
{
    const char* cmd = argc > 1 ? argv[1] : "roll";

    if (std::strcmp (cmd, "table") == 0)
    {
        const int scale = argc > 2 ? std::clamp (std::atoi (argv[2]), 0, numScales() - 1) : 0;
        std::printf ("C %s - every candidate at adventure 1.0\n", scales()[(size_t) scale].name);
        std::printf ("%-9s %-10s %-4s %-6s %-6s\n", "chord", "roman", "out", "bright", "tension");
        for (const auto& c : buildCandidates (0, scale, 1.0f))
            std::printf ("%-9s %-10s %-4d %-6.2f %-6.2f\n", chordName (c.chord, 0, scale).c_str(),
                         romanNumeral (c.chord, 0).c_str(), c.outside, c.brightness, c.tension);
        return 0;
    }

    if (std::strcmp (cmd, "roll") == 0)
    {
        const int count = argc > 3 ? std::atoi (argv[3]) : 5;
        Rng rng ((unsigned) (argc > 4 ? std::atoi (argv[4]) : 1));

        for (int m = 0; m < (int) moods().size(); ++m)
        {
            if (argc > 2 && std::strcmp (argv[2], "all") != 0 && std::strcmp (argv[2], moods()[(size_t) m].name) != 0)
                continue;

            Progression p;
            layoutEven (p);
            applyMood (p, m);
            std::printf ("%s  (%s %s, adventure %.2f)\n", moods()[(size_t) m].name,
                         pitchClassName (p.tonic, p.tonic, p.scale).c_str(), scales()[(size_t) p.scale].name, p.adventure);
            for (int i = 0; i < count; ++i)
            {
                roll (p, rng);
                printProgression (p);
            }
            printDetail (p);
        }
        return 0;
    }

    std::fprintf (stderr, "usage: chordslots-cli table [scaleIndex] | roll [mood|all] [count] [seed]\n");
    return 1;
}
