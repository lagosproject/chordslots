// Task R3 probe: Tension of named chords in C major and of all 32 chord types on root C.
// Built once per header copy (variants a, b, c). Progression scoring is done by engine.cpp + analyze.mjs.
#include "chordslots/Generator.h"
#include <cstdio>
#include <string>
using namespace cs;

static int typeIndex (const std::string& name)
{
    const auto& t = chordTypes();
    for (int i = 0; i < (int) t.size(); ++i)
        if (std::string (t[(size_t) i].suffix) == name) return i;
    return -1;
}

int main (int argc, char** argv)
{
    const char* tag = argc > 1 ? argv[1] : "variant";
    const int tonic = 0, scale = 0; // C major

    // Gmaj7, G7, Cmaj7, Cm, Bdim, Bm7b5, Caug, Cdim7
    const struct { const char* label; int root; const char* type; } named[] = {
        { "Gmaj7", 7, "maj7" }, { "G7", 7, "7" }, { "Cmaj7", 0, "maj7" }, { "Cm", 0, "m" },
        { "Bdim", 11, "dim" }, { "Bm7b5", 11, "m7b5" }, { "Caug", 0, "aug" }, { "Cdim7", 0, "dim7" } };
    for (const auto& n : named)
    {
        Chord c { n.root, typeIndex (n.type) };
        if (c.type < 0) { std::printf("MISSING,%s\n", n.type); return 1; }
        std::printf("NAMED,%s,%s,%s,%.6f\n", tag, n.label, chordName (c, tonic, scale).c_str(), tensionOf (c, tonic, scale));
    }
    for (int i = 0; i < numChordTypes(); ++i)
    {
        Chord c { 0, i };
        std::printf("TYPE,%s,%d,%s,%.6f\n", tag, i, chordTypes()[(size_t) i].suffix, tensionOf (c, tonic, scale));
    }
    return 0;
}
