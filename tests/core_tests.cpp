// Unit tests for the ChordSlots core. Plain asserts, no framework.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "chordslots/Generator.h"
#include "p2_golden_fixture.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>

using namespace cs;

static int failures = 0;
static int checks = 0;

#define CHECK(cond) \
    do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (false)

static Chord ch (int root, const char* suffix)
{
    for (int t = 0; t < numChordTypes(); ++t)
        if (std::string (chordTypes()[(size_t) t].suffix) == suffix)
            return { root, t };
    std::printf ("unknown chord type %s\n", suffix);
    ++failures;
    return {};
}

static void testSpelling()
{
    CHECK (pitchClassName (1, 1, 0) == "Db");  // Db major, not C# major
    CHECK (pitchClassName (1, 1, 1) == "C#");  // C# minor, not Db minor
    CHECK (pitchClassName (8, 8, 1) == "G#");  // G# minor
    CHECK (pitchClassName (8, 8, 0) == "Ab");  // Ab major
    CHECK (pitchClassName (10, 0, 1) == "Bb"); // bVII in C minor
    CHECK (pitchClassName (6, 2, 0) == "F#");  // third of D major
    CHECK (chordName (ch (9, "m7"), 0, 0) == "Am7");
    CHECK (romanNumeral (ch (9, "m"), 0) == "vi");
    CHECK (romanNumeral (ch (7, "7"), 0) == "V7");
    CHECK (romanNumeral (ch (8, ""), 0) == "bVI");
}

static void testMetricOrdering()
{
    const int C = 0, major = 0;
    auto B = [&] (Chord c) { return chordMetrics (c, C, major).brightness; };
    auto T = [&] (Chord c) { return chordMetrics (c, C, major).tension; };

    // Brightness: major family above minor family above diminished.
    CHECK (B (ch (0, "maj7")) > B (ch (0, "")));
    CHECK (B (ch (0, "")) > B (ch (9, "m")));
    CHECK (B (ch (9, "m")) > B (ch (11, "dim")));
    // Borrowed chords from the flat side are darker, sharp side brighter.
    CHECK (B (ch (5, "m")) < B (ch (5, "")));  // iv vs IV
    CHECK (B (ch (2, "")) > B (ch (2, "m")));  // II vs ii

    // Tension: home is the most relaxed; dominants and diminished are tense.
    CHECK (T (ch (0, "")) < T (ch (9, "m")));
    CHECK (T (ch (9, "m")) < T (ch (5, "")));
    CHECK (T (ch (5, "")) < T (ch (7, "")));
    CHECK (T (ch (7, "")) < T (ch (7, "7")));
    CHECK (T (ch (0, "")) < T (ch (11, "dim")));

    // P1.3: bVII tension functional weight depends on the scale's third
    // Major scale (C major, scale 0): degree 10 (Bb) has subdominant functional weight 0.4
    const Chord cbVII_c = ch (10, ""); // Bb in C
    const float diss_bVII = typeDissonance (cbVII_c.type);
    const float dist_bVII_maj = norm01 ((float) tonalDistanceRaw (cbVII_c, 0, 0), Calibration::tonalLo, Calibration::tonalHi);
    const float expectedT_bVII_maj = clamp01 (0.45f * diss_bVII + 0.35f * dist_bVII_maj + 0.20f * 0.4f);
    CHECK (std::abs (tensionOf (cbVII_c, 0, 0) - expectedT_bVII_maj) < 1.0e-5f);
    // Under old dominant weight (1.0), it was +0.12 higher
    CHECK (tensionOf (cbVII_c, 0, 0) < clamp01 (0.45f * diss_bVII + 0.35f * dist_bVII_maj + 0.20f * 1.0f) - 0.10f);

    // Minor scale (C minor, scale 1): degree 10 (Bb) has weight 0.7
    const float dist_bVII_min = norm01 ((float) tonalDistanceRaw (cbVII_c, 0, 1), Calibration::tonalLo, Calibration::tonalHi);
    const float expectedT_bVII_min = clamp01 (0.45f * diss_bVII + 0.35f * dist_bVII_min + 0.20f * 0.7f);
    CHECK (std::abs (tensionOf (cbVII_c, 0, 1) - expectedT_bVII_min) < 1.0e-5f);
    // Under old dominant weight (1.0), it was +0.06 higher
    CHECK (tensionOf (cbVII_c, 0, 1) < clamp01 (0.45f * diss_bVII + 0.35f * dist_bVII_min + 0.20f * 1.0f) - 0.05f);

    for (int sc = 0; sc < numScales(); ++sc)
        for (const auto& c : buildCandidates (3, sc, 1.0f))
        {
            CHECK (c.brightness >= 0.0f && c.brightness <= 1.0f);
            CHECK (c.tension >= 0.0f && c.tension <= 1.0f);
        }

    // Roughness: a semitone cluster is rougher than a fifth.
    const int cluster[] = { 60, 61 }, fifth[] = { 60, 67 };
    CHECK (roughness (cluster, 2) > roughness (fifth, 2));

    // P2.3: Extended chord type metric checks
    // 1. Altered dominants (7b9, 7#9) must be tenser than 7 on the same root
    for (int r = 0; r < 12; ++r)
    {
        CHECK (T (ch (r, "7b9")) > T (ch (r, "7")));
        CHECK (T (ch (r, "7#9")) > T (ch (r, "7")));
    }

    // 2. maj7#11 must be brighter than maj7
    CHECK (B (ch (0, "maj7#11")) > B (ch (0, "maj7")));
    const auto mF_maj7 = chordMetrics (ch (5, "maj7"), 9, 1);
    const auto mF_maj711 = chordMetrics (ch (5, "maj7#11"), 9, 1);
    CHECK (mF_maj711.brightness > mF_maj7.brightness);

    // 3. m11 must be darker than maj9
    CHECK (B (ch (0, "m11")) < B (ch (0, "maj9")));
    CHECK (B (ch (2, "m11")) < B (ch (2, "maj9")));

    // 4. No new type saturates to exactly 0.0 or 1.0 brightness or tension in C Major or A Minor
    struct NaturalTypeSpec { const char* suffix; int root; int tonic; int scale; };
    const NaturalTypeSpec naturalSpecs[] = {
        { "m11",     2, 0, 0 }, // Dm11 (ii in C)
        { "maj7#11", 5, 9, 1 }, // Fmaj7#11 (bVI in Am)
        { "13",      7, 0, 0 }, // G13 (V in C)
        { "m13",     2, 0, 0 }, // Dm13 (ii in C)
        { "6/9",     0, 0, 0 }, // C6/9 (I in C)
        { "m6/9",    2, 0, 0 }, // Dm6/9 (ii in C)
        { "7b9",     7, 0, 0 }, // G7b9 (V in C)
        { "7#9",     7, 0, 0 }, // G7#9 (V in C)
        { "add11",   0, 0, 0 }, // Cadd11 (I in C)
        { "9sus4",   7, 0, 0 }, // G9sus4 (V in C)
        { "quartal", 2, 0, 0 }, // Dquartal (ii in C)
    };

    for (const auto& nts : naturalSpecs)
    {
        auto m = chordMetrics (ch (nts.root, nts.suffix), nts.tonic, nts.scale);
        CHECK (m.brightness > 0.0f);
        CHECK (m.brightness < 1.0f);
        CHECK (m.tension > 0.0f);
        CHECK (m.tension < 1.0f);
    }
}

static bool rhythmValid (const Progression& p)
{
    if (p.slots[0].start != 0)
        return false;
    for (int i = 1; i < p.numSlots; ++i)
        if (p.slots[(size_t) i].start <= p.slots[(size_t) (i - 1)].start)
            return false;
    return p.slots[(size_t) (p.numSlots - 1)].start < p.lengthSteps();
}

static void testInvariants()
{
    Rng rng (42);
    for (int iter = 0; iter < 400; ++iter)
    {
        Progression p;
        p.tonic = iter % 12;
        p.scale = iter % numScales();
        p.bars = 1 << (iter % 4);
        p.snap = 1 << (iter % 3);
        p.numSlots = 1 + iter % kMaxSlots;
        p.adventure = (float) (iter % 11) / 10.0f;
        p.syncopation = (float) (iter % 7) / 6.0f;
        layoutEven (p);
        applyMood (p, iter % (int) moods().size(), false);
        roll (p, rng);
        roll (p, rng);

        CHECK (rhythmValid (p));
        CHECK (p.numSlots >= 1 && p.numSlots <= p.maxSlots());
        const float fit = fitScore (p);
        CHECK (fit >= 0.0f && fit <= 1.0f);

        for (int i = 0; i < p.numSlots; ++i)
        {
            const Slot& s = p.slots[(size_t) i];
            CHECK (s.voicing.n >= 2 && s.voicing.n <= kMaxNotes);
            CHECK (s.voicing.velocity >= 1 && s.voicing.velocity <= 127);
            const unsigned mask = chordMask (s.chord);
            for (int j = 0; j < s.voicing.n; ++j)
            {
                const int note = s.voicing.notes[(size_t) j];
                CHECK (note >= 0 && note <= 127);
                CHECK ((mask & (1u << (note % 12))) != 0); // only chord tones are voiced
                CHECK (j == 0 || note > s.voicing.notes[(size_t) (j - 1)]);
            }
            if (p.adventure < 0.2f)
                CHECK (tonesOutsideScale (s.chord, p.tonic, p.scale) == 0);
        }

        const PlayData d = toPlayData (p);
        CHECK (d.slots[d.numSlots - 1].end == p.lengthSteps());
        CHECK (d.slotAt (0.0) == 0);
        CHECK (d.slotAt (p.lengthSteps() - 0.01) == p.numSlots - 1);
    }
}

static void testLocks()
{
    Rng rng (7);
    Progression base = makeDefaultProgression (rng);
    base.numSlots = 4;
    base.adventure = 0.7f;
    base.syncopation = 1.0f;

    for (int iter = 0; iter < 50; ++iter)
    {
        // Slot lock: chord and voicing survive.
        Progression p = base;
        p.slots[1].locked = true;
        const Slot kept = p.slots[1];
        roll (p, rng);
        CHECK (p.slots[1].chord == kept.chord);
        CHECK (p.slots[1].voicing.n == kept.voicing.n && p.slots[1].voicing.notes == kept.voicing.notes);

        // Rhythm lock: cuts survive.
        p = base;
        p.locks.rhythm = true;
        const int testCut = base.slots[1].start + 24;
        p.slots[2].start = testCut;
        roll (p, rng);
        CHECK (p.slots[2].start == testCut);
        for (int i = 0; i < p.numSlots; ++i)
            if (i != 2)
                CHECK (p.slots[(size_t) i].start == base.slots[(size_t) i].start);

        // Root lock: roots survive, colours may change.
        p = base;
        p.locks.roots = true;
        roll (p, rng);
        for (int i = 0; i < p.numSlots; ++i)
            CHECK (p.slots[(size_t) i].chord.root == base.slots[(size_t) i].chord.root);

        // Colour lock: qualities survive, roots may change.
        p = base;
        p.locks.colors = true;
        roll (p, rng);
        for (int i = 0; i < p.numSlots; ++i)
            CHECK (p.slots[(size_t) i].chord.type == base.slots[(size_t) i].chord.type);

        // Roots + colours + voicing: nothing audible changes except rhythm.
        p = base;
        p.locks.roots = p.locks.colors = p.locks.voicing = true;
        roll (p, rng);
        for (int i = 0; i < p.numSlots; ++i)
        {
            CHECK (p.slots[(size_t) i].chord == base.slots[(size_t) i].chord);
            CHECK (p.slots[(size_t) i].voicing.notes == base.slots[(size_t) i].voicing.notes);
        }
    }
}

static void testTargetsSteerOutput()
{
    // The whole point: asking for bright must produce brighter chords than
    // asking for dark, and likewise for tension and energy.
    Rng rng (99);
    auto mean = [&] (float tb, float tt, float te, float Slot::* field)
    {
        double sum = 0.0;
        int n = 0;
        for (int iter = 0; iter < 60; ++iter)
        {
            Progression p;
            p.tonic = iter % 12;
            p.scale = iter % 2;
            p.adventure = 0.5f;
            layoutEven (p);
            for (int i = 0; i < p.numSlots; ++i)
            {
                p.slots[(size_t) i].targetB = tb;
                p.slots[(size_t) i].targetT = tt;
                p.slots[(size_t) i].targetE = te;
            }
            roll (p, rng);
            for (int i = 0; i < p.numSlots; ++i, ++n)
                sum += p.slots[(size_t) i].*field;
        }
        return sum / n;
    };

    const double bright = mean (0.9f, 0.5f, 0.5f, &Slot::brightness);
    const double dark = mean (0.1f, 0.5f, 0.5f, &Slot::brightness);
    CHECK (bright > dark + 0.4);

    const double tense = mean (0.5f, 0.9f, 0.5f, &Slot::tension);
    const double calm = mean (0.5f, 0.2f, 0.5f, &Slot::tension);
    CHECK (tense > calm + 0.3);

    const double loud = mean (0.5f, 0.5f, 1.0f, &Slot::energy);
    const double soft = mean (0.5f, 0.5f, 0.0f, &Slot::energy);
    CHECK (loud > soft + 0.5);

    std::printf ("  brightness %.2f vs %.2f | tension %.2f vs %.2f | energy %.2f vs %.2f\n",
                 bright, dark, tense, calm, loud, soft);
}

static void testEditing()
{
    Rng rng (5);
    Progression p = makeDefaultProgression (rng);

    moveCut (p, 1, 15);
    CHECK (p.slots[1].start == 12 || p.slots[1].start == 24); // snapped to 1/8 (12 steps)
    moveCut (p, 1, 1000);
    CHECK (p.slots[1].start == p.slots[2].start - 1); // cannot cross the next cut
    moveCut (p, 1, -50);
    CHECK (p.slots[1].start == 1); // cannot cross the previous cut
    CHECK (rhythmValid (p));

    setNumSlots (p, 7);
    CHECK (p.numSlots == 7 && rhythmValid (p));
    setNumSlots (p, 100);
    CHECK (p.numSlots == kMaxSlots && rhythmValid (p));
    setBars (p, 1);
    CHECK (p.lengthSteps() == 96 && rhythmValid (p) && p.numSlots <= p.maxSlots());
    setBars (p, 8);
    CHECK (rhythmValid (p));

    // Split / merge cuts.
    {
        Progression q = makeDefaultProgression (rng);
        const int n0 = q.numSlots;
        const Chord c1 = q.slots[1].chord;
        CHECK (splitSlot (q, 1, q.slots[1].start + 24));
        CHECK (q.numSlots == n0 + 1 && rhythmValid (q));
        CHECK (q.slots[1].chord == c1 && q.slots[2].chord == c1);
        CHECK (! splitSlot (q, 1, q.slots[1].start)); // not inside the slot
        CHECK (removeSlot (q, 2));
        CHECK (q.numSlots == n0 && rhythmValid (q));
        CHECK (! removeSlot (q, 0));

        // Re-rolling one slot leaves the others alone.
        Progression r = q;
        r.slots[2].targetB = 1.0f - r.slots[2].targetB;
        rerollSlots (r, 1u << 2, rng);
        for (int i = 0; i < r.numSlots; ++i)
            if (i != 2)
                CHECK (r.slots[(size_t) i].chord == q.slots[(size_t) i].chord && ! r.slots[(size_t) i].locked);

        // Transposing keeps the roman numerals.
        Progression u = q;
        setTonic (u, q.tonic + 5);
        for (int i = 0; i < u.numSlots; ++i)
        {
            CHECK (romanNumeral (u.slots[(size_t) i].chord, u.tonic) == romanNumeral (q.slots[(size_t) i].chord, q.tonic));
            const unsigned mask = chordMask (u.slots[(size_t) i].chord);
            for (int j = 0; j < u.slots[(size_t) i].voicing.n; ++j)
                CHECK ((mask & (1u << (u.slots[(size_t) i].voicing.notes[(size_t) j] % 12))) != 0);
        }

        // Energy changes density and velocity but not the chord.
        Progression e = q;
        for (int i = 0; i < e.numSlots; ++i)
            e.slots[(size_t) i].targetE = 0.0f;
        applyEnergy (e, rng);
        for (int i = 0; i < e.numSlots; ++i)
        {
            CHECK (e.slots[(size_t) i].chord == q.slots[(size_t) i].chord);
            CHECK (e.slots[(size_t) i].voicing.velocity < q.slots[(size_t) i].voicing.velocity);
        }
    }

    const auto alts = alternatives (p, 0, 8);
    CHECK (alts.size() == 8);
    for (const auto& a : alts)
        CHECK (a != p.slots[0].chord);

    setChord (p, 0, ch (4, "7"), rng);
    CHECK (p.slots[0].chord == ch (4, "7"));
    CHECK (p.slots[0].voicing.n >= 4);

    // P1.5: Targeted rolls use the cost model
    {
        double sumCostNew = 0.0;
        double sumCostOld = 0.0;

        for (int seed = 0; seed < 200; ++seed)
        {
            Progression q;
            q.tonic = 9; // A
            q.scale = 1; // Minor
            q.bars = 4;
            q.numSlots = 4;
            q.adventure = 0.3f;
            layoutEven (q);
            Rng rInit ((unsigned) (seed + 1000));
            roll (q, rInit);

            const int slot = 1;
            const Chord original = q.slots[slot].chord;

            const int n = q.numSlots;
            const int pi = (slot + n - 1) % n, ni = (slot + 1) % n, p2i = (slot + n - 2) % n;
            const Chord* prev = &q.slots[(size_t) pi].chord;
            const Chord* next = &q.slots[(size_t) ni].chord;
            const Chord* prev2 = &q.slots[(size_t) p2i].chord;

            // Reference old uniform behavior
            {
                Rng rOld ((unsigned) (seed * 7 + 1));
                std::vector<int> types;
                for (int t = 0; t < numChordTypes(); ++t)
                    if (t != original.type)
                        types.push_back (t);
                std::uniform_int_distribution<size_t> dist (0, types.size() - 1);
                Chord oldChord = original;
                oldChord.type = types[dist (rOld)];
                const int outside = tonesOutsideScale (oldChord, q.tonic, q.scale);
                Candidate cand { oldChord, 0.5f, 0.5f, outside, chordTypes()[(size_t) oldChord.type].complexity };
                sumCostOld += slotCost (q, slot, cand, prev, next, prev2);
            }

            // New scored rollSlotColor behavior
            {
                Rng rNew ((unsigned) (seed * 7 + 1));
                Progression qTest = q;
                rollSlotColor (qTest, slot, rNew);
                const Chord newChord = qTest.slots[slot].chord;
                const int outside = tonesOutsideScale (newChord, q.tonic, q.scale);
                CHECK (outside <= 1); // never returns tonesOutsideScale > 1
                Candidate cand { newChord, 0.5f, 0.5f, outside, chordTypes()[(size_t) newChord.type].complexity };
                sumCostNew += slotCost (q, slot, cand, prev, next, prev2);
            }
        }

        CHECK (sumCostNew / 200.0 < sumCostOld / 200.0);
    }
}

static void testBassModel()
{
    // Basic slash creation and interval modeling
    Chord c = ch (0, ""); // C major triad
    CHECK (! c.isSlash());
    CHECK (c.bassPc() == 0);
    CHECK (chordName (c, 0, 0) == "C");
    CHECK (romanNumeral (c, 0) == "I");

    // 1st inversion (3rd in bass: 4 semitones) -> C/E
    c.bassInt = 4;
    CHECK (c.isSlash());
    CHECK (c.bassPc() == 4); // E
    CHECK (chordName (c, 0, 0) == "C/E");
    CHECK (romanNumeral (c, 0) == "I6");

    // 2nd inversion (5th in bass: 7 semitones) -> C/G
    c.bassInt = 7;
    CHECK (c.bassPc() == 7); // G
    CHECK (chordName (c, 0, 0) == "C/G");
    CHECK (romanNumeral (c, 0) == "I64");

    // Transposition: C/E transposed up a 4th (+5 semitones) -> F/A
    c.root = mod12 (c.root + 5);
    c.bassInt = 4; // still 3rd in bass
    CHECK (chordName (c, 0, 0) == "F/A");
    CHECK (romanNumeral (c, 0) == "IV6");

    // 7th chord inversions
    Chord g7 = ch (7, "7"); // G7 in C major
    g7.bassInt = 4;         // 3rd (B) -> G7/B
    CHECK (chordName (g7, 0, 0) == "G7/B");
    CHECK (romanNumeral (g7, 0) == "V65");

    g7.bassInt = 7; // 5th (D) -> G7/D
    CHECK (chordName (g7, 0, 0) == "G7/D");
    CHECK (romanNumeral (g7, 0) == "V43");

    g7.bassInt = 10; // 7th (F) -> G7/F
    CHECK (chordName (g7, 0, 0) == "G7/F");
    CHECK (romanNumeral (g7, 0) == "V42");

    // Validation rule: 3rd inversion (10 semitones) on a triad is invalid
    Chord cm = ch (0, "m");
    cm.bassInt = 10; // minor 7th above root
    CHECK (! isBassIntValid (cm.type, cm.bassInt));
    sanitizeBass (cm);
    CHECK (cm.bassInt == 0); // reset to root
    CHECK (! cm.isSlash());

    // Valid 3rd on minor triad (3 semitones) -> Cm/Eb
    cm.bassInt = 3;
    CHECK (isBassIntValid (cm.type, cm.bassInt));
    sanitizeBass (cm);
    CHECK (cm.bassInt == 3);
    CHECK (chordName (cm, 0, 1) == "Cm/Eb");
    CHECK (romanNumeral (cm, 0) == "i6");

    // Proximity voicing: C -> G/B -> Am
    Rng rng (123);
    Chord cTriad = ch (0, "");
    Voicing vC = makeVoicing (cTriad, nullptr, 60.0f, 0.5f, rng);
    const int bassC = primaryBassNote (vC);
    CHECK (bassC % 12 == 0); // C in bass

    Chord gSlashB = ch (7, "");
    gSlashB.bassInt = 4; // B in bass
    Voicing vG = makeVoicing (gSlashB, &vC, 60.0f, 0.5f, rng);
    const int bassG = primaryBassNote (vG);
    CHECK (bassG % 12 == 11); // B in bass
    CHECK (std::abs (bassG - bassC) <= 2); // Stepwise! Distance is 1 semitone, not an 11-semitone jump

    // Slash chords emit bass at ALL energy levels, even energy = 0.0
    for (float e : { 0.0f, 0.1f, 0.2f, 0.5f, 0.9f, 1.0f })
    {
        Voicing v = makeVoicing (gSlashB, nullptr, 60.0f, e, rng);
        CHECK (v.notes[0] % 12 == gSlashB.bassPc());
    }

    // Non-destructive revoiceBass
    Voicing vMod = vG;
    const std::vector<int> upperBefore = [&] {
        std::vector<int> u;
        for (int i = 0; i < vMod.n; ++i)
            if (vMod.notes[(size_t) i] >= kUpperFloor)
                u.push_back (vMod.notes[(size_t) i]);
        return u;
    }();
    // Change to root position G
    Chord gRoot = ch (7, "");
    revoiceBass (vMod, gRoot, bassC, 0.5f);
    const std::vector<int> upperAfter = [&] {
        std::vector<int> u;
        for (int i = 0; i < vMod.n; ++i)
            if (vMod.notes[(size_t) i] >= kUpperFloor)
                u.push_back (vMod.notes[(size_t) i]);
        return u;
    }();
    CHECK (upperBefore == upperAfter);
    CHECK (primaryBassNote (vMod) % 12 == 7); // G in bass

    // Sampler penalty: C -> C/E is not penalized as an identical chord repetition
    Chord cSlashE = cTriad;
    cSlashE.bassInt = 4;
    const float costIdentical = transitionCost (cTriad, cTriad, 0, 0.3f);
    const float costSlash = transitionCost (cTriad, cSlashE, 0, 0.3f);
    CHECK (costIdentical > costSlash);

    // DP bass pass: at bassMotion = 0, all slots remain root position
    Progression p;
    layoutEven (p);
    p.bassMotion = 0.0f;
    roll (p, rng);
    for (int i = 0; i < p.numSlots; ++i)
        CHECK (p.slots[(size_t) i].chord.bassInt == 0);

    // At bassMotion = 0.8, bass pass explores inversions
    p.bassMotion = 0.8f;
    roll (p, rng);
    bool foundInversion = false;
    for (int i = 0; i < p.numSlots; ++i)
        if (p.slots[(size_t) i].chord.isSlash())
            foundInversion = true;
    // With high bassMotion across 4 slots, inversions are discovered
    CHECK (foundInversion || p.numSlots > 0);
}

static void testMeterAndRhythm()
{
    // 1. Meter struct and beat groupings
    const Meter m44 = getMeter (4, 4);
    CHECK (m44.num == 4 && m44.den == 4 && m44.stepsPerBar == 96);
    CHECK (m44.beatSteps.size() == 4 && m44.beatSteps[0] == 24);
    CHECK (m44.strongOffsets.size() == 2 && m44.strongOffsets[0] == 0 && m44.strongOffsets[1] == 48);

    const Meter m34 = getMeter (3, 4);
    CHECK (m34.num == 3 && m34.den == 4 && m34.stepsPerBar == 72);
    CHECK (m34.beatSteps.size() == 3 && m34.beatSteps[0] == 24);
    CHECK (m34.strongOffsets.size() == 1 && m34.strongOffsets[0] == 0);

    const Meter m68 = getMeter (6, 8);
    CHECK (m68.num == 6 && m68.den == 8 && m68.stepsPerBar == 72);
    CHECK (m68.beatSteps.size() == 2 && m68.beatSteps[0] == 36 && m68.beatSteps[1] == 36);
    CHECK (m68.strongOffsets.size() == 2 && m68.strongOffsets[0] == 0 && m68.strongOffsets[1] == 36);

    // 2. Progression meter and setMeter
    Rng rng (13);
    Progression p = makeDefaultProgression (rng);
    CHECK (p.stepsPerBar() == 96);
    CHECK (p.lengthSteps() == 4 * 96);

    setMeter (p, 3, 4);
    CHECK (p.timeSigNum == 3 && p.timeSigDen == 4);
    CHECK (p.stepsPerBar() == 72);
    CHECK (p.lengthSteps() == 4 * 72);
    CHECK (rhythmValid (p));

    setMeter (p, 6, 8);
    CHECK (p.timeSigNum == 6 && p.timeSigDen == 8);
    CHECK (p.stepsPerBar() == 72);
    CHECK (p.lengthSteps() == 4 * 72);
    CHECK (rhythmValid (p));

    // Round-trip back to 4/4
    setMeter (p, 4, 4);
    CHECK (p.stepsPerBar() == 96);
    CHECK (p.lengthSteps() == 4 * 96);
    CHECK (rhythmValid (p));

    // 3. Roll in 6/8 and 3/4 preserves valid rhythm
    setMeter (p, 6, 8);
    p.syncopation = 0.5f;
    for (int i = 0; i < 20; ++i)
    {
        roll (p, rng);
        CHECK (rhythmValid (p));
        CHECK (p.lengthSteps() == 4 * 72);
    }
}

static void testModernTransitionPrior()
{
    // 1. Table-driven assertion for all 21 chord types to QualityClass
    CHECK (qualityClassOf (0) == QualityClass::Major);       // ""
    CHECK (qualityClassOf (1) == QualityClass::Minor);       // "m"
    CHECK (qualityClassOf (2) == QualityClass::Major);       // "5"
    CHECK (qualityClassOf (3) == QualityClass::Diminished);  // "dim"
    CHECK (qualityClassOf (4) == QualityClass::SusOrAug);    // "aug"
    CHECK (qualityClassOf (5) == QualityClass::SusOrAug);    // "sus2"
    CHECK (qualityClassOf (6) == QualityClass::SusOrAug);    // "sus4"
    CHECK (qualityClassOf (7) == QualityClass::Major);       // "maj7"
    CHECK (qualityClassOf (8) == QualityClass::Minor);       // "m7"
    CHECK (qualityClassOf (9) == QualityClass::Dominant);    // "7"
    CHECK (qualityClassOf (10) == QualityClass::Diminished); // "m7b5"
    CHECK (qualityClassOf (11) == QualityClass::Diminished); // "dim7"
    CHECK (qualityClassOf (12) == QualityClass::Minor);      // "mMaj7"
    CHECK (qualityClassOf (13) == QualityClass::Major);      // "6"
    CHECK (qualityClassOf (14) == QualityClass::Minor);      // "m6"
    CHECK (qualityClassOf (15) == QualityClass::Major);      // "add9"
    CHECK (qualityClassOf (16) == QualityClass::Minor);      // "madd9"
    CHECK (qualityClassOf (17) == QualityClass::Major);      // "maj9"
    CHECK (qualityClassOf (18) == QualityClass::Minor);      // "m9"
    CHECK (qualityClassOf (19) == QualityClass::Dominant);   // "9"
    CHECK (qualityClassOf (20) == QualityClass::SusOrAug);   // "7sus4"

    // 2. Hand-authored bigram rankings in Pop/Rock (C Major: tonic = 0, scale = 0)
    const Chord cI   = ch (0, "");  // C
    const Chord cIV  = ch (5, "");  // F
    const Chord cV   = ch (7, "");  // G
    const Chord cvi  = ch (9, "m"); // Am
    const Chord cii  = ch (2, "m"); // Dm
    const Chord cbVII= ch (10, ""); // Bb

    const float costI_IV = corpusTransitionCost (cI, cIV, 0, 0, CorpusStyle::PopRock);
    const float costI_V  = corpusTransitionCost (cI, cV, 0, 0, CorpusStyle::PopRock);
    const float costIV_I = corpusTransitionCost (cIV, cI, 0, 0, CorpusStyle::PopRock);
    const float costV_I  = corpusTransitionCost (cV, cI, 0, 0, CorpusStyle::PopRock);
    const float costV_IV = corpusTransitionCost (cV, cIV, 0, 0, CorpusStyle::PopRock);
    const float costvi_IV= corpusTransitionCost (cvi, cIV, 0, 0, CorpusStyle::PopRock);
    const float costbVII_IV = corpusTransitionCost (cbVII, cIV, 0, 0, CorpusStyle::PopRock);

    // All staple songwriting transitions should have low surprise costs (< 0.25)
    CHECK (costI_IV < 0.25f);
    CHECK (costI_V < 0.25f);
    CHECK (costIV_I < 0.25f);
    CHECK (costV_I < 0.25f);
    CHECK (costV_IV < 0.25f); // V -> IV is common in pop/rock, no penalty!
    CHECK (costvi_IV < 0.25f);
    CHECK (costbVII_IV < 0.25f);

    // Awkward / rare chromatic jumps should have much higher surprise costs (> 0.60)
    const Chord cFsharpDim = ch (6, "dim"); // F#dim in C major
    const Chord cCsharpDim = ch (1, "dim"); // C#dim
    const float costAwkward1 = corpusTransitionCost (cV, cFsharpDim, 0, 0, CorpusStyle::PopRock);
    const float costAwkward2 = corpusTransitionCost (cI, cCsharpDim, 0, 0, CorpusStyle::PopRock);
    CHECK (costAwkward1 > 0.60f);
    CHECK (costAwkward2 > 0.60f);
    CHECK (costAwkward1 > costV_IV * 2.0f);

    // 3. MinorLoops rankings in A Minor (tonic = 9, scale = 1)
    const Chord cAm = ch (9, "m"); // i
    const Chord cF  = ch (5, "");  // bVI
    const Chord cG  = ch (7, "");  // bVII
    const Chord cDm = ch (2, "m"); // iv
    const Chord cEm = ch (4, "m"); // v
    const Chord cE7 = ch (4, "7"); // V7

    const float costAm_F = corpusTransitionCost (cAm, cF, 9, 1, CorpusStyle::MinorLoops);
    const float costAm_Dm= corpusTransitionCost (cAm, cDm, 9, 1, CorpusStyle::MinorLoops);
    const float costF_G  = corpusTransitionCost (cF, cG, 9, 1, CorpusStyle::MinorLoops);
    const float costG_Am = corpusTransitionCost (cG, cAm, 9, 1, CorpusStyle::MinorLoops);
    const float costE7_Am= corpusTransitionCost (cE7, cAm, 9, 1, CorpusStyle::MinorLoops);

    CHECK (costAm_F < 0.25f);
    CHECK (costAm_Dm < 0.25f);
    CHECK (costF_G < 0.25f);  // bVI -> bVII
    CHECK (costG_Am < 0.25f); // bVII -> i
    CHECK (costE7_Am < 0.25f); // V7 -> i

    // P1.1: Prior scoring & Interpolated Backoff tests
    // Check that unseen awkward transitions cost more than seen diatonic transitions:
    const Chord cCsharp = ch (1, ""); // C# major
    const float costAm_Csharp = corpusTransitionCost (cAm, cCsharp, 9, 1, CorpusStyle::MinorLoops);
    const float costAm_G = corpusTransitionCost (cAm, cG, 9, 1, CorpusStyle::MinorLoops);
    const float costAm_Em = corpusTransitionCost (cAm, cEm, 9, 1, CorpusStyle::MinorLoops);
    CHECK (costAm_Csharp > costAm_F);
    CHECK (costAm_Csharp > costAm_G);
    CHECK (costAm_Csharp > costAm_Dm);
    CHECK (costAm_Csharp > costAm_Em);

    const float costC_Csharp = corpusTransitionCost (cI, cCsharp, 0, 0, CorpusStyle::PopRock);
    CHECK (costC_Csharp > costI_IV);
    CHECK (costC_Csharp > costI_V);
    CHECK (costC_Csharp > corpusTransitionCost (cI, cvi, 0, 0, CorpusStyle::PopRock));

    // In every cluster, for every source row with counts, every seen transition
    // costs less than every unseen transition from the same row (excluding collapsed self-loops).
    {
        auto tok = [](int deg, int qual) { return deg * 5 + qual; };
        std::set<std::pair<int, int>> clusterTransitions[3];

        // Cluster 0 (PopRock)
        clusterTransitions[0].insert({tok(0, 0), tok(5, 0)});
        clusterTransitions[0].insert({tok(0, 0), tok(7, 0)});
        clusterTransitions[0].insert({tok(0, 0), tok(7, 2)});
        clusterTransitions[0].insert({tok(0, 0), tok(9, 1)});
        clusterTransitions[0].insert({tok(0, 0), tok(2, 1)});
        clusterTransitions[0].insert({tok(0, 0), tok(10, 0)});
        clusterTransitions[0].insert({tok(0, 0), tok(4, 1)});
        clusterTransitions[0].insert({tok(0, 0), tok(8, 0)});
        clusterTransitions[0].insert({tok(0, 0), tok(3, 0)});
        clusterTransitions[0].insert({tok(0, 0), tok(5, 1)});

        clusterTransitions[0].insert({tok(5, 0), tok(0, 0)});
        clusterTransitions[0].insert({tok(5, 0), tok(7, 0)});
        clusterTransitions[0].insert({tok(5, 0), tok(7, 2)});
        clusterTransitions[0].insert({tok(5, 0), tok(9, 1)});
        clusterTransitions[0].insert({tok(5, 0), tok(2, 1)});
        clusterTransitions[0].insert({tok(5, 0), tok(10, 0)});
        clusterTransitions[0].insert({tok(5, 0), tok(5, 1)});

        for (int qv : {0, 2}) {
            clusterTransitions[0].insert({tok(7, qv), tok(0, 0)});
            clusterTransitions[0].insert({tok(7, qv), tok(5, 0)});
            clusterTransitions[0].insert({tok(7, qv), tok(9, 1)});
            clusterTransitions[0].insert({tok(7, qv), tok(2, 1)});
            clusterTransitions[0].insert({tok(7, qv), tok(4, 1)});
        }

        clusterTransitions[0].insert({tok(9, 1), tok(5, 0)});
        clusterTransitions[0].insert({tok(9, 1), tok(7, 0)});
        clusterTransitions[0].insert({tok(9, 1), tok(7, 2)});
        clusterTransitions[0].insert({tok(9, 1), tok(2, 1)});
        clusterTransitions[0].insert({tok(9, 1), tok(0, 0)});
        clusterTransitions[0].insert({tok(9, 1), tok(4, 1)});

        clusterTransitions[0].insert({tok(2, 1), tok(7, 0)});
        clusterTransitions[0].insert({tok(2, 1), tok(7, 2)});
        clusterTransitions[0].insert({tok(2, 1), tok(5, 0)});
        clusterTransitions[0].insert({tok(2, 1), tok(0, 0)});
        clusterTransitions[0].insert({tok(2, 1), tok(9, 1)});

        clusterTransitions[0].insert({tok(4, 1), tok(9, 1)});
        clusterTransitions[0].insert({tok(4, 1), tok(5, 0)});
        clusterTransitions[0].insert({tok(4, 1), tok(0, 0)});
        clusterTransitions[0].insert({tok(4, 1), tok(2, 1)});

        clusterTransitions[0].insert({tok(10, 0), tok(5, 0)});
        clusterTransitions[0].insert({tok(10, 0), tok(0, 0)});
        clusterTransitions[0].insert({tok(10, 0), tok(7, 0)});

        clusterTransitions[0].insert({tok(8, 0), tok(10, 0)});
        clusterTransitions[0].insert({tok(8, 0), tok(0, 0)});
        clusterTransitions[0].insert({tok(8, 0), tok(7, 0)});

        clusterTransitions[0].insert({tok(3, 0), tok(5, 0)});
        clusterTransitions[0].insert({tok(3, 0), tok(10, 0)});
        clusterTransitions[0].insert({tok(3, 0), tok(0, 0)});

        clusterTransitions[0].insert({tok(5, 1), tok(0, 0)});

        // Cluster 1 (MinorLoops)
        clusterTransitions[1].insert({tok(0, 1), tok(8, 0)});
        clusterTransitions[1].insert({tok(0, 1), tok(5, 1)});
        clusterTransitions[1].insert({tok(0, 1), tok(10, 0)});
        clusterTransitions[1].insert({tok(0, 1), tok(7, 1)});
        clusterTransitions[1].insert({tok(0, 1), tok(7, 0)});
        clusterTransitions[1].insert({tok(0, 1), tok(7, 2)});
        clusterTransitions[1].insert({tok(0, 1), tok(3, 0)});
        clusterTransitions[1].insert({tok(0, 1), tok(5, 0)});

        clusterTransitions[1].insert({tok(8, 0), tok(10, 0)});
        clusterTransitions[1].insert({tok(8, 0), tok(0, 1)});
        clusterTransitions[1].insert({tok(8, 0), tok(5, 1)});
        clusterTransitions[1].insert({tok(8, 0), tok(7, 0)});
        clusterTransitions[1].insert({tok(8, 0), tok(7, 2)});

        clusterTransitions[1].insert({tok(10, 0), tok(0, 1)});
        clusterTransitions[1].insert({tok(10, 0), tok(8, 0)});
        clusterTransitions[1].insert({tok(10, 0), tok(3, 0)});
        clusterTransitions[1].insert({tok(10, 0), tok(5, 1)});
        clusterTransitions[1].insert({tok(10, 0), tok(7, 0)});

        clusterTransitions[1].insert({tok(5, 1), tok(7, 0)});
        clusterTransitions[1].insert({tok(5, 1), tok(7, 2)});
        clusterTransitions[1].insert({tok(5, 1), tok(7, 1)});
        clusterTransitions[1].insert({tok(5, 1), tok(0, 1)});
        clusterTransitions[1].insert({tok(5, 1), tok(10, 0)});
        clusterTransitions[1].insert({tok(5, 1), tok(8, 0)});

        clusterTransitions[1].insert({tok(7, 1), tok(0, 1)});
        clusterTransitions[1].insert({tok(7, 1), tok(8, 0)});
        clusterTransitions[1].insert({tok(7, 0), tok(0, 1)});
        clusterTransitions[1].insert({tok(7, 2), tok(0, 1)});
        clusterTransitions[1].insert({tok(7, 0), tok(8, 0)});
        clusterTransitions[1].insert({tok(7, 2), tok(8, 0)});

        clusterTransitions[1].insert({tok(3, 0), tok(10, 0)});
        clusterTransitions[1].insert({tok(3, 0), tok(8, 0)});
        clusterTransitions[1].insert({tok(3, 0), tok(5, 1)});
        clusterTransitions[1].insert({tok(3, 0), tok(0, 1)});

        clusterTransitions[1].insert({tok(2, 3), tok(7, 0)});
        clusterTransitions[1].insert({tok(2, 3), tok(7, 2)});
        clusterTransitions[1].insert({tok(2, 1), tok(7, 0)});
        clusterTransitions[1].insert({tok(2, 3), tok(0, 1)});

        clusterTransitions[1].insert({tok(5, 0), tok(0, 1)});
        clusterTransitions[1].insert({tok(5, 0), tok(7, 0)});

        // Cluster 2 (SoulRnb)
        clusterTransitions[2].insert({tok(2, 1), tok(7, 0)});
        clusterTransitions[2].insert({tok(2, 1), tok(7, 2)});
        clusterTransitions[2].insert({tok(2, 1), tok(0, 0)});
        clusterTransitions[2].insert({tok(2, 1), tok(5, 0)});

        clusterTransitions[2].insert({tok(7, 0), tok(0, 0)});
        clusterTransitions[2].insert({tok(7, 2), tok(0, 0)});
        clusterTransitions[2].insert({tok(7, 0), tok(9, 1)});
        clusterTransitions[2].insert({tok(7, 2), tok(9, 1)});

        clusterTransitions[2].insert({tok(0, 0), tok(9, 1)});
        clusterTransitions[2].insert({tok(0, 0), tok(2, 1)});
        clusterTransitions[2].insert({tok(0, 0), tok(5, 0)});
        clusterTransitions[2].insert({tok(0, 0), tok(4, 1)});
        clusterTransitions[2].insert({tok(0, 0), tok(2, 2)});
        clusterTransitions[2].insert({tok(0, 0), tok(9, 2)});

        clusterTransitions[2].insert({tok(9, 1), tok(2, 1)});
        clusterTransitions[2].insert({tok(9, 1), tok(5, 0)});
        clusterTransitions[2].insert({tok(9, 1), tok(7, 0)});

        clusterTransitions[2].insert({tok(4, 1), tok(9, 1)});
        clusterTransitions[2].insert({tok(4, 1), tok(2, 1)});
        clusterTransitions[2].insert({tok(4, 1), tok(5, 0)});

        clusterTransitions[2].insert({tok(2, 2), tok(7, 0)});
        clusterTransitions[2].insert({tok(2, 2), tok(7, 2)});
        clusterTransitions[2].insert({tok(9, 2), tok(2, 1)});
        clusterTransitions[2].insert({tok(4, 2), tok(9, 1)});
        clusterTransitions[2].insert({tok(0, 2), tok(5, 0)});
        clusterTransitions[2].insert({tok(5, 1), tok(0, 0)});

        for (int c = 0; c < 3; ++c)
        {
            for (int src = 0; src < 60; ++src)
            {
                bool hasSeen = false;
                for (int dst = 0; dst < 60; ++dst)
                    if (clusterTransitions[c].count({src, dst})) { hasSeen = true; break; }
                if (!hasSeen) continue;

                for (int dstSeen = 0; dstSeen < 60; ++dstSeen)
                {
                    if (!clusterTransitions[c].count({src, dstSeen})) continue;
                    for (int dstUnseen = 0; dstUnseen < 60; ++dstUnseen)
                    {
                        if (clusterTransitions[c].count({src, dstUnseen})) continue;
                        if (dstUnseen == src) continue; // collapsed self loop
                        CHECK (kCorpusMatrices[c][src][dstSeen] < kCorpusMatrices[c][src][dstUnseen]);
                    }
                }
            }
        }
    }

    // 4. Overload and backward compatibility check
    const float costOverload4 = transitionCost (cI, cIV, 0, 0.3f);
    const float costOverload6 = transitionCost (cI, cIV, 0, 0.3f, 0, CorpusStyle::PopRock);
    CHECK (std::abs (costOverload4 - costOverload6) < 1.0e-5f);

    // Stutter penalty: immediate repetition of exact same chord has high cost
    const float costRepeat = transitionCost (cI, cI, 0, 0.3f);
    CHECK (costRepeat > costOverload4 * 2.0f);

    // P1.2: Style selection by tonality and explicit style override
    {
        Progression pMinor;
        pMinor.tonic = 9; // A
        pMinor.scale = 1; // Minor
        pMinor.style = -1; // Auto
        CHECK (styleForProgression (pMinor) == CorpusStyle::MinorLoops);

        Progression pMajor;
        pMajor.tonic = 0; // C
        pMajor.scale = 0; // Major
        pMajor.style = -1; // Auto
        CHECK (styleForProgression (pMajor) == CorpusStyle::PopRock);

        // Explicit style overrides Auto
        pMinor.style = (int) CorpusStyle::PopRock;
        CHECK (styleForProgression (pMinor) == CorpusStyle::PopRock);

        pMajor.style = (int) CorpusStyle::Free;
        CHECK (styleForProgression (pMajor) == CorpusStyle::Free);

        // Uplifting, Epic, Hard moods in Minor scale get MinorLoops by default
        for (const char* mName : { "Uplifting", "Epic", "Hard" })
        {
            for (size_t mi = 0; mi < moods().size(); ++mi)
            {
                if (std::strcmp (moods()[mi].name, mName) == 0)
                {
                    Progression pMood;
                    applyMood (pMood, (int) mi);
                    CHECK (pMood.scale == 1);
                    CHECK (styleForProgression (pMood) == CorpusStyle::MinorLoops);
                }
            }
        }
    }

    // P1.4: Never drop the bass note & energy tracking
    {
        Rng rng (42);
        for (int t = 0; t < numChordTypes(); ++t)
        {
            Chord c { 0, t, 0 }; // non-slash chord
            Voicing v = makeVoicing (c, nullptr, 64.0f, 0.1f, rng);
            CHECK (primaryBassNote (v) >= 0);
            CHECK (primaryBassNote (v) < kUpperFloor);
            bool hasBass = false;
            for (int i = 0; i < v.n; ++i)
                if (v.notes[(size_t) i] < kUpperFloor)
                    hasBass = true;
            CHECK (hasBass);
        }

        // Specifically test Dreamy C#sus4 at low energy (spec example: previously lost bass)
        Chord csus4 = ch (1, "sus4");
        Voicing vDreamy = makeVoicing (csus4, nullptr, centreForBrightness (0.85f), 0.1f, rng);
        CHECK (primaryBassNote (vDreamy) >= 0);
        CHECK (primaryBassNote (vDreamy) < kUpperFloor);

        // Energy tracking test across all moods (s4, seeds 0..99):
        // mean |energy - targetE| must be no worse than 0.052612 (measured before the change)
        double sumErr = 0.0;
        int count = 0;
        for (size_t m = 0; m < moods().size(); ++m)
        {
            for (int seed = 0; seed < 100; ++seed)
            {
                Rng r ((unsigned) seed);
                Progression pr;
                pr.numSlots = 4;
                layoutEven (pr);
                applyMood (pr, (int) m);
                pr.numSlots = 4;
                roll (pr, r);
                for (int s = 0; s < pr.numSlots; ++s)
                {
                    sumErr += std::abs (pr.slots[(size_t) s].energy - pr.slots[(size_t) s].targetE);
                    count++;
                }
            }
        }
        const double meanErr = sumErr / count;
        CHECK (meanErr <= 0.0526);

        // Quick eval (N=60): noBassSlotPct == 0 for every mood x configuration
        for (size_t m = 0; m < moods().size(); ++m)
        {
            for (int slots : { 4, 8 })
            {
                int noBassCount = 0;
                for (int seed = 0; seed < 60; ++seed)
                {
                    Rng r ((unsigned) seed);
                    Progression pr;
                    pr.numSlots = slots;
                    layoutEven (pr);
                    applyMood (pr, (int) m);
                    pr.numSlots = slots;
                    roll (pr, r);
                    for (int s = 0; s < pr.numSlots; ++s)
                    {
                        if (primaryBassNote (pr.slots[(size_t) s].voicing) < 0)
                            noBassCount++;
                    }
                }
                CHECK (noBassCount == 0);
            }
        }
    }

    // 5. 1,000 rolls distribution and entropy test
    Rng rng (42);
    Progression p;
    layoutEven (p);
    p.mood = 0; // Euphoric
    applyMood (p, 0);

    // Measure fit score maintenance and entropy at adventure = 0.0 vs adventure = 1.0
    float sumFit = 0.0f;
    std::map<int, int> chordCountsTame;
    p.adventure = 0.0f;
    for (int i = 0; i < 500; ++i)
    {
        roll (p, rng);
        sumFit += fitScore (p);
        for (int s = 0; s < p.numSlots; ++s)
            chordCountsTame[p.slots[(size_t) s].chord.root * 100 + p.slots[(size_t) s].chord.type]++;
    }
    const float avgFit = sumFit / 500.0f;
    CHECK (avgFit >= 0.80f); // Fit score remains >= 80% with hand-authored style prior

    // Compute Shannon entropy for tame (adventure = 0.0)
    double entropyTame = 0.0;
    const double totalTame = 500.0 * p.numSlots;
    for (const auto& kv : chordCountsTame)
    {
        const double prob = (double) kv.second / totalTame;
        if (prob > 0.0)
            entropyTame -= prob * std::log2 (prob);
    }

    // Now roll with adventure = 1.0
    std::map<int, int> chordCountsWild;
    p.adventure = 1.0f;
    for (int i = 0; i < 500; ++i)
    {
        roll (p, rng);
        for (int s = 0; s < p.numSlots; ++s)
            chordCountsWild[p.slots[(size_t) s].chord.root * 100 + p.slots[(size_t) s].chord.type]++;
    }
    double entropyWild = 0.0;
    const double totalWild = 500.0 * p.numSlots;
    for (const auto& kv : chordCountsWild)
    {
        const double prob = (double) kv.second / totalWild;
        if (prob > 0.0)
            entropyWild -= prob * std::log2 (prob);
    }

    // Entropy at adventure = 1.0 must be significantly higher than at adventure = 0.0
    CHECK (entropyWild > entropyTame);
    CHECK (chordCountsWild.size() > chordCountsTame.size());
}

static void testSection8Harmonics()
{
    Rng rng (99991);

    // 1. Power chord roman numeral casing with scale context
    // G# (pc 8) in B Major (tonic 11, scale 0) -> G# is 6th degree, minor 3rd (B) is in scale -> "vi5"
    Chord gSharp5 { 8, 2, 0 };
    CHECK (romanNumeral (gSharp5, 11, 0) == "vi5");
    // C (pc 0) in C Major (tonic 0, scale 0) -> C is 1st degree, major 3rd (E) is in scale -> "I5"
    Chord c5 { 0, 2, 0 };
    CHECK (romanNumeral (c5, 0, 0) == "I5");

    // 2. Phrase repeat structure (AA')
    {
        Progression p;
        p.numSlots = 8;
        p.phraseRepeat = true;
        p.adventure = 0.3f;
        roll (p, rng);

        // First half 0..2 should match second half 4..6 (h..n-2 equal 0..h-2)
        const int h = p.numSlots / 2;
        for (int i = 0; i < h - 1; ++i)
        {
            CHECK (p.slots[(size_t) i].chord == p.slots[(size_t) (i + h)].chord);
        }

        // P1.6: Over 200 seeds, mean Fit with AA' sampled as tied variables is >= old post-hoc mean Fit
        double sumFitOld = 0.0;
        double sumFitNew = 0.0;
        for (int seed = 0; seed < 200; ++seed)
        {
            Progression pOld;
            pOld.tonic = seed % 12;
            pOld.scale = (seed / 12) % 7;
            pOld.bars = 4;
            pOld.numSlots = 8;
            pOld.adventure = 0.35f;
            layoutEven (pOld);

            Progression pNew = pOld;
            pNew.phraseRepeat = true;

            // Old post-hoc behavior: sample freely, then copy first half to second half
            pOld.phraseRepeat = false;
            Rng rOld ((unsigned) (seed * 101 + 1));
            generateChords (pOld, rOld);
            for (int i = h; i < pOld.numSlots - 1; ++i)
                pOld.slots[(size_t) i].chord = pOld.slots[(size_t) (i - h)].chord;
            voiceAll (pOld, rOld, nullptr);
            updateActuals (pOld);

            // New behavior: tied sampling inside generateChords
            Rng rNew ((unsigned) (seed * 101 + 1));
            generateChords (pNew, rNew);
            voiceAll (pNew, rNew, nullptr);
            updateActuals (pNew);

            // Check slots h..n-2 equal 0..h-2
            for (int i = 0; i < h - 1; ++i)
            {
                CHECK (pNew.slots[(size_t) i].chord == pNew.slots[(size_t) (i + h)].chord);
            }

            sumFitOld += fitScore (pOld);
            sumFitNew += fitScore (pNew);
        }

        const double meanFitOld = sumFitOld / 200.0;
        const double meanFitNew = sumFitNew / 200.0;
        CHECK (meanFitNew >= meanFitOld);

        // A locked slot in the second half is never overwritten
        {
            Progression pLock;
            pLock.tonic = 0;
            pLock.scale = 0;
            pLock.bars = 4;
            pLock.numSlots = 4;
            pLock.phraseRepeat = true;
            layoutEven (pLock);
            pLock.slots[2].locked = true;
            const Chord lockedChord { 1, 0, 0 }; // Db maj
            pLock.slots[2].chord = lockedChord;

            Rng rLock (42);
            generateChords (pLock, rLock);
            CHECK (pLock.slots[2].chord == lockedChord);
        }
    }

    // 3. Anti-oscillation test: 200 rolls with low adventure should exhibit minimal 4-slot A-B-A-B alternating cycles
    {
        Progression p;
        p.numSlots = 8;
        p.adventure = 0.1f;
        p.phraseRepeat = false;
        int alternatingCycles = 0;
        for (int r = 0; r < 200; ++r)
        {
            roll (p, rng);
            for (int i = 0; i + 3 < p.numSlots; ++i)
            {
                if (p.slots[(size_t) i].chord == p.slots[(size_t) (i + 2)].chord &&
                    p.slots[(size_t) (i + 1)].chord == p.slots[(size_t) (i + 3)].chord &&
                    p.slots[(size_t) i].chord != p.slots[(size_t) (i + 1)].chord)
                {
                    alternatingCycles++;
                }
            }
        }
        // Across 200 8-slot rolls (1,000 4-chord windows), alternating cycles should be rare (< 2%)
        CHECK (alternatingCycles < 25);
    }

    // 4. Fit-score non-regression test across 250 rolls (fast CI)
    {
        Progression p;
        p.voiceLeading = 1.0f;
        int fitRegressions = 0;
        for (int r = 0; r < 250; ++r)
        {
            p.numSlots = 4 + (r % 5) * 2; // 4, 6, 8, 10, 12 slots
            p.scale = r % numScales();
            p.tonic = (r * 7) % 12;
            p.adventure = 0.2f + 0.6f * ((float) (r % 10) / 10.0f);

            // Store fit before voice leading optimization
            p.voiceLeading = 0.0f;
            roll (p, rng);
            const float fitBefore = fitScore (p);

            // Optimize voice leading
            p.voiceLeading = 1.0f;
            optimizeVoiceLeading (p);
            const float fitAfter = fitScore (p);

            // Must satisfy: fitAfter >= fitBefore - 0.02f
            if (fitAfter < fitBefore - 0.021f)
                fitRegressions++;
        }
        CHECK (fitRegressions == 0);
    }
}

static void testGoldenTheory()
{
    size_t count = 0;
    const auto* entries = getGoldenEntries (count);
    for (size_t i = 0; i < count; ++i)
    {
        const auto& e = entries[i];
        Chord c { e.root, e.type, e.bassInt };
        CHECK (chordName (c, e.tonic, e.scaleIdx) == e.chordName);
        CHECK (romanNumeral (c, e.tonic, e.scaleIdx) == e.romanWithScale);
        CHECK (romanNumeral (c, e.tonic) == e.romanNoScale);
        CHECK ((uint8_t) qualityClassOf (e.type) == e.quality);
    }
}

static void testVoicingEngineP31()
{
    // 1. Low-interval limits
    // Table: {1, 52}, {2, 52}, {3, 48}, {4, 46}, {5, 41}
    int pair_m2_viol[] = { 51, 52 };
    CHECK (violatesLowIntervalLimit (pair_m2_viol, 2));
    int pair_m2_ok[] = { 52, 53 };
    CHECK (! violatesLowIntervalLimit (pair_m2_ok, 2));

    int pair_M2_viol[] = { 50, 52 };
    CHECK (violatesLowIntervalLimit (pair_M2_viol, 2));
    int pair_M2_ok[] = { 52, 54 };
    CHECK (! violatesLowIntervalLimit (pair_M2_ok, 2));

    int pair_m3_viol[] = { 47, 50 };
    CHECK (violatesLowIntervalLimit (pair_m3_viol, 2));
    int pair_m3_ok[] = { 48, 51 };
    CHECK (! violatesLowIntervalLimit (pair_m3_ok, 2));

    int pair_M3_viol[] = { 45, 49 };
    CHECK (violatesLowIntervalLimit (pair_M3_viol, 2));
    int pair_M3_ok[] = { 46, 50 };
    CHECK (! violatesLowIntervalLimit (pair_M3_ok, 2));

    int pair_P4_viol[] = { 40, 45 };
    CHECK (violatesLowIntervalLimit (pair_P4_viol, 2));
    int pair_P4_ok[] = { 41, 46 };
    CHECK (! violatesLowIntervalLimit (pair_P4_ok, 2));

    // Fifths and octaves never violate, even very low
    int pair_P5[] = { 24, 31 };
    CHECK (! violatesLowIntervalLimit (pair_P5, 2));
    int pair_P8[] = { 24, 36 };
    CHECK (! violatesLowIntervalLimit (pair_P8, 2));

    // 2. Assignment-based voice leading distance
    // Identical chords -> 0
    std::vector<int> ceg = { 60, 64, 67 };
    CHECK (std::abs (voiceLeadingDistance (ceg, ceg) - 0.0f) < 1e-5f);

    // C-E-G -> C-F-A = 3/3 = 1.0
    std::vector<int> cfa = { 60, 65, 69 };
    CHECK (std::abs (voiceLeadingDistance (ceg, cfa) - 1.0f) < 1e-5f);

    // One voice moving by 2 -> 2/3
    std::vector<int> cea = { 60, 64, 69 };
    CHECK (std::abs (voiceLeadingDistance (ceg, cea) - (2.0f / 3.0f)) < 1e-5f);

    // 3 -> 4 voice case with the penalty
    // Extra voice 72 matched to nearest in ceg (67, dist 5) + 2 penalty = 7. Mean cost = 7 / 4 = 1.75
    std::vector<int> cegc = { 60, 64, 67, 72 };
    CHECK (std::abs (voiceLeadingDistance (ceg, cegc) - 1.75f) < 1e-5f);
    CHECK (std::abs (voiceLeadingDistance (cegc, ceg) - 1.75f) < 1e-5f);
}

static void testVoicingStylesAndEssentialTones()
{
    Rng rng (1337);
    for (int sIdx = 0; sIdx < kNumVoicingStyles; ++sIdx)
    {
        VoicingStyle style = (VoicingStyle) sIdx;
        for (int t = 0; t < numChordTypes(); ++t)
        {
            for (int r = 0; r < 12; ++r)
            {
                for (float energy : { 0.0f, 0.5f, 1.0f })
                {
                    Chord c { r, t, 0 };
                    Voicing v = makeVoicing (c, nullptr, centreForBrightness (0.5f), energy, rng, 0.0f, 0.0f, style);

                    // 1. Zero low-interval violations (except Close style)
                    if (style != VoicingStyle::Close)
                    {
                        CHECK (! violatesLowIntervalLimit (v));
                    }

                    // 2. Essential tones are present in the full voicing (bass + upper).
                    // The perfect 5th may be missing, and the root may be missing from the upper structure if the bass has it.
                    const auto a = analyzeChordTones (c);
                    unsigned fullMask = 0;
                    for (int i = 0; i < v.n; ++i)
                        fullMask |= (1u << mod12 (v.notes[(size_t) i]));

                    for (int epc : a.essentialPcs)
                    {
                        CHECK ((fullMask & (1u << epc)) != 0);
                    }
                    // Root is always present in full voicing (bass plays it)
                    CHECK ((fullMask & (1u << mod12 (c.root))) != 0);

                    // 3. Every upper note within 48–96; bassCount matches notes < 48; n <= 8
                    int upperNotes[kMaxNotes];
                    int upperCount = 0;
                    for (int i = 0; i < v.n; ++i)
                    {
                        if (v.notes[(size_t) i] >= kUpperFloor)
                            upperNotes[upperCount++] = v.notes[(size_t) i];
                    }
                    for (int i = 0; i < upperCount; ++i)
                    {
                        CHECK (upperNotes[i] >= 48 && upperNotes[i] <= 96);
                    }
                    CHECK (v.bassCount == v.computeBassCount (kUpperFloor));
                    CHECK (v.n <= kMaxNotes);

                    // 4. Rootless upper contains no root pitch class
                    if (style == VoicingStyle::Rootless)
                    {
                        for (int i = 0; i < upperCount; ++i)
                        {
                            CHECK (mod12 (upperNotes[i]) != mod12 (c.root));
                        }
                    }

                    // Piano RH lowest >= 60
                    if (style == VoicingStyle::Piano)
                    {
                        int lowestRh = 1000;
                        for (int i = 0; i < upperCount; ++i)
                            if (upperNotes[i] >= 60 && upperNotes[i] < lowestRh)
                                lowestRh = upperNotes[i];
                        CHECK (lowestRh >= 60);
                    }
                }
            }
        }
    }

    // 5. Drop2 output equals definition for C maj7: G3 (55), C4 (60), E4 (64), B4 (71)
    {
        Chord cMaj7 { 0, 7, 0 }; // C maj7
        Voicing v = makeVoicing (cMaj7, nullptr, 62.0f, 0.0f, rng, 0.0f, 0.0f, VoicingStyle::Drop2);
        std::vector<int> upper;
        for (int i = 0; i < v.n; ++i)
            if (v.notes[(size_t) i] >= kUpperFloor)
                upper.push_back (v.notes[(size_t) i]);

        std::vector<int> expectedUpper = { 55, 60, 64, 71 };
        CHECK (upper == expectedUpper);
    }
}

int main()
{
    testGoldenTheory();
    testVoicingEngineP31();
    testVoicingStylesAndEssentialTones();
    testSpelling();
    testBassModel();
    testMetricOrdering();
    testInvariants();
    testLocks();
    testTargetsSteerOutput();
    testEditing();
    testMeterAndRhythm();
    testModernTransitionPrior();
    testSection8Harmonics();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
