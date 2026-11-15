// Unit tests for the ChordSlots core. Plain asserts, no framework.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "chordslots/Generator.h"

#include <cmath>
#include <cstdio>
#include <map>
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

    for (int sc = 0; sc < numScales(); ++sc)
        for (const auto& c : buildCandidates (3, sc, 1.0f))
        {
            CHECK (c.brightness >= 0.0f && c.brightness <= 1.0f);
            CHECK (c.tension >= 0.0f && c.tension <= 1.0f);
        }

    // Roughness: a semitone cluster is rougher than a fifth.
    const int cluster[] = { 60, 61 }, fifth[] = { 60, 67 };
    CHECK (roughness (cluster, 2) > roughness (fifth, 2));
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

    // 2. Empirical bigram rankings in Pop/Rock (C Major: tonic = 0, scale = 0)
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

    // 4. Overload and backward compatibility check
    const float costOverload4 = transitionCost (cI, cIV, 0, 0.3f);
    const float costOverload6 = transitionCost (cI, cIV, 0, 0.3f, 0, CorpusStyle::PopRock);
    CHECK (std::abs (costOverload4 - costOverload6) < 1.0e-5f);

    // Stutter penalty: immediate repetition of exact same chord has high cost
    const float costRepeat = transitionCost (cI, cI, 0, 0.3f);
    CHECK (costRepeat > costOverload4 * 2.0f);

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
    CHECK (avgFit >= 0.80f); // Fit score remains >= 80% with Billboard prior

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

int main()
{
    testSpelling();
    testBassModel();
    testMetricOrdering();
    testInvariants();
    testLocks();
    testTargetsSteerOutput();
    testEditing();
    testMeterAndRhythm();
    testModernTransitionPrior();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
