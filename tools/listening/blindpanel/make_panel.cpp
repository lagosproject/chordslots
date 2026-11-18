// R7 blind panel: engine bridge for the ChordSlots condition and the voicing of the other conditions.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Build (from the worktree root):
//   g++ -std=c++17 -O2 -I core/include tools/listening/blindpanel/make_panel.cpp -o tools/listening/blindpanel/make_panel
//
// Usage:
//   make_panel roll MOOD TONIC SEED
//       Rolls one ChordSlots progression with the engine (applyMood, layoutEven, roll) and prints one line
//       per slot: "SLOT root suffix bassInt brightness tension energy n1 n2 ...".
//       The rhythm is locked to one chord per bar (locks.rhythm), so the four slots are one bar each.
//   make_panel voice SEED
//       Reads progressions from stdin, one per line: "root:suffix root:suffix root:suffix root:suffix".
//       Voices each with the engine's voicing (voiceAll with default slot targets, the same path the
//       product uses for a chord with no mood) and prints one line per progression:
//       "n1 n2 n3 | n1 n2 ... | ..." with one group per chord.
//
// Pitch classes are 0..11 (C = 0). Suffixes are the engine's chordTypes() suffix strings; the empty
// suffix is the major triad and is written as "-" on the command line and in the input.
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

static int typeIndex (const std::string& suffix)
{
    const auto& t = chordTypes();
    for (size_t i = 0; i < t.size(); ++i)
        if (suffix == t[i].suffix)
            return (int) i;
    std::fprintf (stderr, "unknown chord suffix '%s'\n", suffix.c_str());
    std::exit (1);
}

static Chord mk (int root, const std::string& suffix, int bass = 0)
{
    Chord c;
    c.root = mod12 (root);
    c.type = typeIndex (suffix);
    c.bassInt = bass;
    return c;
}

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

static int runRoll (int argc, char** argv)
{
    if (argc < 5)
        return 2;
    const int moodIdx = moodIndex (argv[2]);
    const int tonic = std::atoi (argv[3]);
    const unsigned seed = (unsigned) std::strtoul (argv[4], nullptr, 10);

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
    return 0;
}

static int runVoice (int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const unsigned seed = (unsigned) std::strtoul (argv[2], nullptr, 10);
    Rng rng (seed);

    std::string line;
    while (std::getline (std::cin, line))
    {
        std::istringstream ss (line);
        std::string tok;
        std::vector<Chord> chords;
        while (ss >> tok)
        {
            const auto colon = tok.find (':');
            if (colon == std::string::npos)
                continue;
            std::string suffix = tok.substr (colon + 1);
            if (suffix == "-")
                suffix.clear();
            chords.push_back (mk (std::atoi (tok.substr (0, colon).c_str()), suffix));
        }
        if (chords.size() != 4)
            continue;

        Progression p;
        p.tonic = 0;
        p.scale = 0;
        p.bars = 4;
        p.numSlots = 4;
        p.locks.rhythm = true;
        layoutEven (p);
        for (int i = 0; i < 4; ++i)
            p.slots[(size_t) i].chord = chords[(size_t) i];
        voiceAll (p, rng, nullptr);

        for (int i = 0; i < 4; ++i)
        {
            if (i)
                std::printf (" | ");
            const auto notes = sortedNotes (p.slots[(size_t) i].voicing);
            for (size_t j = 0; j < notes.size(); ++j)
                std::printf ("%s%d", j ? " " : "", notes[j]);
        }
        std::printf ("\n");
    }
    return 0;
}

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: make_panel roll MOOD TONIC SEED | make_panel voice SEED\n");
        return 2;
    }
    if (std::strcmp (argv[1], "roll") == 0)
        return runRoll (argc, argv);
    if (std::strcmp (argv[1], "voice") == 0)
        return runVoice (argc, argv);
    std::fprintf (stderr, "unknown mode '%s'\n", argv[1]);
    return 2;
}
