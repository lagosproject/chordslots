// tests/performance_tests.cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "chordslots/Performance.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                          \
    do { if (! (cond)) {                                                     \
        std::fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);\
        std::exit (1); } } while (0)

using namespace cs;

// C2 bass + C3 octave-ish upper stack: bass(36), then 55, 60, 64, 67
static PerfInput sample()
{
    PerfInput in;
    const int pitches[] = { 36, 55, 60, 64, 67 };
    for (int i = 0; i < 5; ++i) { in.pitch[(size_t) i] = pitches[i]; in.velocity[(size_t) i] = 90; }
    in.n = 5;
    in.bassCount = 1;
    return in;
}

static void zeroParamsIsIdentity()
{
    const auto out = applyPerformance (sample(), PerformanceParams{}, 2000.0, 0, 0);
    CHECK (out.n == 5);
    for (int i = 0; i < out.n; ++i)
    {
        CHECK (out.notes[(size_t) i].offsetMs == 0.0f);
        CHECK (out.notes[(size_t) i].velocity == 90);
    }
}

static void pitchesPreserved()
{
    PerformanceParams p; p.strum = 1.0f; p.humanize = 1.0f; p.accent = 1.0f;
    const auto out = applyPerformance (sample(), p, 2000.0, 3, 2);
    const int expected[] = { 36, 55, 60, 64, 67 };
    CHECK (out.n == 5);
    for (int i = 0; i < 5; ++i) CHECK (out.notes[(size_t) i].pitch == expected[i]);
}

static void downStrumOrderAndBassPinned()
{
    PerformanceParams p; p.strum = 1.0f;                       // down, bass pinned
    const auto out = applyPerformance (sample(), p, 2000.0, 0, 0);
    CHECK (out.notes[0].offsetMs == 0.0f);                     // bass on the grid
    for (int i = 2; i < out.n; ++i)                            // upper voices rise in time
        CHECK (out.notes[(size_t) i].offsetMs > out.notes[(size_t) i - 1].offsetMs);
    CHECK (out.notes[1].offsetMs == 0.0f);                     // first upper voice leads
}

static void upStrumReversesOrder()
{
    PerformanceParams p; p.strum = -1.0f;
    const auto out = applyPerformance (sample(), p, 2000.0, 0, 0);
    CHECK (out.notes[0].offsetMs == 0.0f);
    CHECK (out.notes[4].offsetMs == 0.0f);                     // treble leads
    for (int i = 2; i < out.n; ++i)
        CHECK (out.notes[(size_t) i].offsetMs < out.notes[(size_t) i - 1].offsetMs);
}

static void strumIncludesBassDelaysEverythingElse()
{
    PerformanceParams p; p.strum = 1.0f; p.strumIncludesBass = true;
    const auto out = applyPerformance (sample(), p, 2000.0, 0, 0);
    CHECK (out.notes[0].offsetMs == 0.0f);
    CHECK (out.notes[1].offsetMs > 0.0f);                      // now rolls from the bass up
}

static void offsetsNeverExceedSlotFraction()
{
    PerformanceParams p; p.strum = 1.0f; p.humanize = 1.0f;
    for (double len : { 20.0, 50.0, 250.0, 4000.0 })
    {
        const float cap = (float) (len * p.strumSlotFraction);
        for (int pass = 0; pass < 20; ++pass)
        {
            const auto out = applyPerformance (sample(), p, len, 1, pass);
            for (int i = 0; i < out.n; ++i)
            {
                CHECK (out.notes[(size_t) i].offsetMs >= 0.0f);
                CHECK (out.notes[(size_t) i].offsetMs <= cap + 1e-4f);
            }
        }
    }
}

static void zeroLengthSlotIsSafe()
{
    PerformanceParams p; p.strum = 1.0f; p.humanize = 1.0f;
    const auto out = applyPerformance (sample(), p, 0.0, 0, 0);
    for (int i = 0; i < out.n; ++i) CHECK (out.notes[(size_t) i].offsetMs == 0.0f);
}

static void humanizeIsDeterministicAndVaries()
{
    PerformanceParams p; p.humanize = 1.0f; p.seed = 42;
    const auto a = applyPerformance (sample(), p, 2000.0, 2, 5);
    const auto b = applyPerformance (sample(), p, 2000.0, 2, 5);
    bool same = true, differsAcrossPass = false;
    for (int i = 0; i < a.n; ++i)
    {
        same = same && a.notes[(size_t) i].offsetMs == b.notes[(size_t) i].offsetMs
                     && a.notes[(size_t) i].velocity == b.notes[(size_t) i].velocity;
    }
    CHECK (same);

    const auto c = applyPerformance (sample(), p, 2000.0, 2, 6);
    for (int i = 0; i < a.n; ++i)
        differsAcrossPass = differsAcrossPass
            || a.notes[(size_t) i].offsetMs != c.notes[(size_t) i].offsetMs
            || a.notes[(size_t) i].velocity != c.notes[(size_t) i].velocity;
    CHECK (differsAcrossPass);
}

static void velocitiesStayInRange()
{
    PerfInput in = sample();
    for (int i = 0; i < in.n; ++i) in.velocity[(size_t) i] = (i % 2) ? 127 : 1;
    PerformanceParams p; p.humanize = 1.0f; p.accent = 1.0f; p.humanizeMaxVel = 40.0f;
    for (int pass = 0; pass < 50; ++pass)
    {
        const auto out = applyPerformance (in, p, 2000.0, 0, pass);
        for (int i = 0; i < out.n; ++i)
            CHECK (out.notes[(size_t) i].velocity >= 1 && out.notes[(size_t) i].velocity <= 127);
    }
}

static void accentLeavesBassAlone()
{
    PerformanceParams p; p.accent = 1.0f;
    const auto out = applyPerformance (sample(), p, 2000.0, 0, 0);
    CHECK (out.notes[0].velocity == 90);                       // bass unchanged
    CHECK (out.notes[4].velocity > 90);                        // top voice boosted
    CHECK (out.notes[2].velocity < 90);                        // inner voice softened
}

static void handlesDegenerateInput()
{
    PerfInput empty;
    CHECK (applyPerformance (empty, PerformanceParams{}, 1000.0, 0, 0).n == 0);

    PerfInput one; one.pitch[0] = 60; one.velocity[0] = 80; one.n = 1;       // single note
    PerformanceParams p; p.strum = 1.0f;
    const auto out = applyPerformance (one, p, 1000.0, 0, 0);
    CHECK (out.n == 1 && out.notes[0].offsetMs == 0.0f);

    PerfInput over = sample(); over.n = 99; over.bassCount = 99;             // out-of-range counts
    CHECK (applyPerformance (over, p, 1000.0, 0, 0).n == kPerfMaxNotes);
}

int main()
{
    zeroParamsIsIdentity();
    pitchesPreserved();
    downStrumOrderAndBassPinned();
    upStrumReversesOrder();
    strumIncludesBassDelaysEverythingElse();
    offsetsNeverExceedSlotFraction();
    zeroLengthSlotIsSafe();
    humanizeIsDeterministicAndVaries();
    velocitiesStayInRange();
    accentLeavesBassAlone();
    handlesDegenerateInput();
    std::puts ("performance tests passed");
    return 0;
}
