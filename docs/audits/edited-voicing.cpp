#include "chordslots/Generator.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cs;

void printVoicing (const Voicing& v)
{
    for (int i = 0; i < v.n; ++i)
        std::cout << ' ' << v.notes[(size_t) i];
}

#ifndef CHORDSLOTS_BEFORE_EDITED_VOICING
void check (bool condition, const char* message)
{
    if (! condition)
        throw std::runtime_error (message);
}
#endif

Progression fixture()
{
    Progression p;
    p.tonic = 5;
    p.scale = 0;
    p.adventure = 0.45f;
    p.voicingStyle = (int) VoicingStyle::Close;
    p.locks.rhythm = true;
    layoutEven (p);
    const std::array<Chord, 4> chords {{ {10, 0}, {5, 0}, {10, 0}, {5, 0} }};
    const std::array<std::array<int, 4>, 4> notes {{
        {{34, 58, 62, 65}}, {{41, 60, 65, 69}}, {{34, 58, 62, 65}}, {{41, 57, 60, 65}}
    }};
    for (int i = 0; i < p.numSlots; ++i)
    {
        auto& s = p.slots[(size_t) i];
        s.chord = chords[(size_t) i];
        s.voicing.n = 4;
        s.voicing.bassCount = 1;
        std::copy (notes[(size_t) i].begin(), notes[(size_t) i].end(), s.voicing.notes.begin());
    }
    updateActuals (p);
    return p;
}

int main()
{
    constexpr int slot = 1;
    for (int inverted : {-1, 0, 2})
    {
        auto p = fixture();
        if (inverted >= 0)
            invertChord (p, inverted, InversionDirection::Up);
        std::cout << "Inspect inverted=" << inverted << '\n';
        const auto start = std::chrono::steady_clock::now();
#ifdef CHORDSLOTS_BEFORE_EDITED_VOICING
        struct LegacyAlternative { Chord chord; Voicing voicing; };
        std::vector<LegacyAlternative> legacy;
        int inScale = 0, outside = 0;
        for (auto chord : alternatives (p, slot, std::numeric_limits<int>::max()))
        {
            const bool isOutside = scaleMembership (chord, p.tonic, p.scale).outsideChord != 0;
            if (isOutside ? outside++ >= 3 : inScale++ >= 6)
                continue;
            auto test = p;
            Rng rng (73);
            setChord (test, slot, chord, rng);
            legacy.push_back ({ chord, test.slots[slot].voicing });
        }
        std::stable_partition (legacy.begin(), legacy.end(), [&] (const auto& row)
        {
            return scaleMembership (row.chord, p.tonic, p.scale).outsideChord == 0;
        });
        const auto& displayed = legacy;
#else
        const auto displayed = inspectAlternatives (p, slot, 6, 3, Rng (73));
        check (displayed.size() == 9, "Inspect group sizes");
        check (chordName (displayed[2].chord, p.tonic, p.scale) == "Fadd11", "Fadd11 should precede C5");
        check (upperNotes (displayed.front().voicing) ==
               (inverted < 0 ? std::vector<int> {57, 62, 65} : std::vector<int> {62, 65, 69}),
               "Both neighbours should affect the Dm preview");
#endif
        std::cout << "ms=" << std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count() << '\n';
        for (const auto& row : displayed)
        {
            std::cout << chordName (row.chord, p.tonic, p.scale) << ':';
            printVoicing (row.voicing);
            std::cout << " movement=" << (voiceLeadingDistance (p.slots[0].voicing, row.voicing)
                                        + voiceLeadingDistance (row.voicing, p.slots[2].voicing)) << '\n';
        }
        for (bool selected : {false, true})
            for (bool lock : {false, true})
                for (int scenario = 0; scenario < 3; ++scenario)
                {
                    auto test = p;
                    test.slots[0].locked = test.slots[2].locked = true;
                    test.locks.voicing = lock;
                    test.locks.roots = test.locks.colors = scenario == 0;
                    test.slots[slot].locked = scenario == 2;
                    Rng rng (73);
                    if (selected)
#ifdef CHORDSLOTS_BEFORE_EDITED_VOICING
                        rerollSlots (test, 1u << slot, rng);
#else
                        rollSelectedSlot (test, slot, rng);
#endif
                    else
                        roll (test, rng);
#ifndef CHORDSLOTS_BEFORE_EDITED_VOICING
                    const auto& result = test.slots[slot];
                    check (result.chord.root == (scenario == 1 ? 2 : 5) && result.chord.type == (scenario == 1 ? 1 : 0),
                           "Roll chord identity");
                    for (int neighbour : {0, 2})
                        check (test.slots[neighbour].chord == p.slots[neighbour].chord
                               && test.slots[neighbour].voicing.notes == p.slots[neighbour].voicing.notes,
                               "Roll should preserve each locked neighbour");
                    if (scenario == 2 || (lock && scenario == 0))
                        check (result.voicing.notes == p.slots[slot].voicing.notes, "Roll should preserve exact pitches");
                    if (lock && scenario == 1)
                        check (upperNotes (result.voicing) == std::vector<int> {62, 65, 69},
                               "The changed chord should be voiced near the old slot");
#endif
                    std::cout << "Roll selected=" << selected << " lock=" << lock << " scenario=" << scenario << ' '
                              << chordName (test.slots[slot].chord, p.tonic, p.scale) << ':';
                    printVoicing (test.slots[slot].voicing);
                    std::cout << " movement=" << (voiceLeadingDistance (p.slots[0].voicing, test.slots[slot].voicing)
                                                + voiceLeadingDistance (test.slots[slot].voicing, p.slots[2].voicing)) << '\n';
                }
    }
}
