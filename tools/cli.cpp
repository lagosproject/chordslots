// chordslots-cli: exercise the core from a terminal, no plugin host needed.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
//   chordslots-cli table [scale]                metric table for every candidate chord in C
//   chordslots-cli roll [mood] [count] [seed]   generate progressions for a mood
//   chordslots-cli eval [N] [--out file] [--bigrams file] [--compare baseline]

#include "chordslots/Generator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

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

//==============================================================================
// Evaluation Harness & Baseline
//==============================================================================

static bool voicingHasLowIntervalViolation (const Voicing& v)
{
    return violatesLowIntervalLimit (v);
}

// Guard thresholds table.
// Guards are one-directional, because later phases are meant to change the numbers.
// Note: std::uniform_real_distribution differs between standard libraries, so the
// baseline will not match exactly on MSVC or macOS. That is why the check uses guards
// and not equality.
enum class GuardDirection
{
    MustNotDrop, // fails when baseline - now > maxDelta
    MustNotRise  // fails when now - baseline > maxDelta
};

struct GuardThreshold
{
    const char* metricKey;
    GuardDirection direction;
    double maxDelta;
};

static constexpr GuardThreshold kGuardThresholds[] = {
    { "tonicPresentPct", GuardDirection::MustNotDrop, 5.0 },
    { "distinctPct",     GuardDirection::MustNotDrop, 10.0 },
    { "noBassSlotPct",   GuardDirection::MustNotRise, 1.0 },
    { "lowIntervalPct",  GuardDirection::MustNotRise, 2.0 },
    { "meanFit",         GuardDirection::MustNotDrop, 0.03 },
    { "ababPct",         GuardDirection::MustNotRise, 3.0 }
};

struct EvalConfig
{
    const char* name;
    int slots;
};

static constexpr EvalConfig kConfigs[] = {
    { "s4", 4 },
    { "s8", 8 }
};

static const char* const kMetricKeys[] = {
    "tonicPresentPct",
    "distinctPct",
    "noBassSlotPct",
    "lowIntervalPct",
    "meanFit",
    "ababPct",
    "meanUniqueChords",
    "meanOutsideTones"
};

struct MetricResult
{
    std::string key;
    double value = 0.0;
};

struct MoodConfigResult
{
    std::string moodName;
    std::string configName;
    std::map<std::pair<int, int>, int> bigrams;
};

static std::map<std::string, double> loadFlatJson (const std::string& path)
{
    std::map<std::string, double> result;
    std::ifstream in (path);
    if (! in)
        return result;
    std::string line;
    while (std::getline (in, line))
    {
        auto q1 = line.find ('"');
        if (q1 == std::string::npos)
            continue;
        auto q2 = line.find ('"', q1 + 1);
        if (q2 == std::string::npos)
            continue;
        std::string key = line.substr (q1 + 1, q2 - (q1 + 1));
        auto colon = line.find (':', q2 + 1);
        if (colon == std::string::npos)
            continue;
        double val = std::strtod (line.c_str() + colon + 1, nullptr);
        result[key] = val;
    }
    return result;
}

static int runEval (int argc, char** argv)
{
    int N = 1000;
    std::string outPath;
    std::string bigramsPath;
    std::string comparePath;

    for (int i = 2; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--out") == 0 && i + 1 < argc)
        {
            outPath = argv[++i];
        }
        else if (std::strcmp (argv[i], "--bigrams") == 0 && i + 1 < argc)
        {
            bigramsPath = argv[++i];
        }
        else if (std::strcmp (argv[i], "--compare") == 0 && i + 1 < argc)
        {
            comparePath = argv[++i];
        }
        else if (argv[i][0] != '-')
        {
            int parsedN = std::atoi (argv[i]);
            if (parsedN > 0)
                N = parsedN;
        }
    }

    std::vector<MetricResult> allMetrics;
    std::vector<MoodConfigResult> allMoodConfigs;

    // Run all 8 moods x 2 configurations
    for (size_t m = 0; m < moods().size(); ++m)
    {
        const auto& mood = moods()[m];
        for (const auto& cfg : kConfigs)
        {
            MoodConfigResult mcResult;
            mcResult.moodName = mood.name;
            mcResult.configName = cfg.name;

            int tonicLoops = 0;
            std::set<std::vector<std::tuple<int, int, int>>> distinctSeqs;
            int noBassSlots = 0;
            int lowIntervalSlots = 0;
            double sumFit = 0.0;
            int ababLoops = 0;
            int sumUniqueChords = 0;
            int sumOutsideTones = 0;

            const int totalSlots = N * cfg.slots;

            for (int seed = 0; seed < N; ++seed)
            {
                Rng rng (static_cast<unsigned> (seed));
                Progression p;
                p.bars = 4;
                p.timeSigNum = 4;
                p.timeSigDen = 4;
                p.numSlots = cfg.slots;
                layoutEven (p);
                applyMood (p, (int) m);
                p.numSlots = cfg.slots;
                roll (p, rng);

                // Metric 1: tonicPresentPct
                // Definition: % of loops where at least one slot's chord root equals the tonic
                bool hasTonic = false;
                for (int s = 0; s < p.numSlots; ++s)
                {
                    if (p.slots[(size_t) s].chord.root == p.tonic)
                    {
                        hasTonic = true;
                        break;
                    }
                }
                if (hasTonic)
                    tonicLoops++;

                // Metric 2: distinctPct
                // Definition: number of distinct chord sequences ÷ N × 100. A sequence is the ordered list of (root, type, bassInt)
                std::vector<std::tuple<int, int, int>> seq;
                seq.reserve ((size_t) p.numSlots);
                for (int s = 0; s < p.numSlots; ++s)
                {
                    const auto& c = p.slots[(size_t) s].chord;
                    seq.emplace_back (c.root, c.type, c.bassInt);
                }
                distinctSeqs.insert (seq);

                // Metric 3: noBassSlotPct
                // Definition: % of slots whose voicing has no note below kUpperFloor (primaryBassNote(v) < 0)
                for (int s = 0; s < p.numSlots; ++s)
                {
                    if (primaryBassNote (p.slots[(size_t) s].voicing) < 0)
                        noBassSlots++;
                }

                // Metric 4: lowIntervalPct
                // Definition: % of slots whose sorted voicing has any adjacent pair (lower note L, interval i in semitones) that breaks the low-interval table below
                for (int s = 0; s < p.numSlots; ++s)
                {
                    if (voicingHasLowIntervalViolation (p.slots[(size_t) s].voicing))
                        lowIntervalSlots++;
                }

                // Metric 5: meanFit
                // Definition: mean of fitScore(p)
                sumFit += (double) fitScore (p);

                // Metric 6: ababPct
                // Definition: % of loops (numSlots >= 4) with A,B,A,B on 4 consecutive slots, read circularly, where A ≠ B (compare Chords)
                if (p.numSlots >= 4)
                {
                    bool hasAbab = false;
                    for (int s = 0; s < p.numSlots; ++s)
                    {
                        const auto& c0 = p.slots[(size_t) s].chord;
                        const auto& c1 = p.slots[(size_t) ((s + 1) % p.numSlots)].chord;
                        const auto& c2 = p.slots[(size_t) ((s + 2) % p.numSlots)].chord;
                        const auto& c3 = p.slots[(size_t) ((s + 3) % p.numSlots)].chord;
                        if (c0 == c2 && c1 == c3 && c0 != c1)
                        {
                            hasAbab = true;
                            break;
                        }
                    }
                    if (hasAbab)
                        ababLoops++;
                }

                // Metric 7: meanUniqueChords
                // Definition: mean number of distinct chords per loop
                std::set<std::tuple<int, int, int>> loopChords;
                for (int s = 0; s < p.numSlots; ++s)
                {
                    const auto& c = p.slots[(size_t) s].chord;
                    loopChords.emplace (c.root, c.type, c.bassInt);
                }
                sumUniqueChords += (int) loopChords.size();

                // Metric 8: meanOutsideTones
                // Definition: mean tonesOutsideScale per slot
                for (int s = 0; s < p.numSlots; ++s)
                {
                    sumOutsideTones += tonesOutsideScale (p.slots[(size_t) s].chord, p.tonic, p.scale);
                }

                // Bigrams: counts of transitions between corpus tokens (including wrap-around)
                if (! bigramsPath.empty())
                {
                    for (int s = 0; s < p.numSlots; ++s)
                    {
                        int tA = corpusTokenOf (p.slots[(size_t) s].chord, p.tonic);
                        int tB = corpusTokenOf (p.slots[(size_t) ((s + 1) % p.numSlots)].chord, p.tonic);
                        mcResult.bigrams[{ tA, tB }]++;
                    }
                }
            }

            allMoodConfigs.push_back (mcResult);

            const double tonicPresentPct = (double) tonicLoops / (double) N * 100.0;
            const double distinctPct = (double) distinctSeqs.size() / (double) N * 100.0;
            const double noBassSlotPct = (double) noBassSlots / (double) totalSlots * 100.0;
            const double lowIntervalPct = (double) lowIntervalSlots / (double) totalSlots * 100.0;
            const double meanFit = sumFit / (double) N;
            const double ababPct = (double) ababLoops / (double) N * 100.0;
            const double meanUniqueChords = (double) sumUniqueChords / (double) N;
            const double meanOutsideTones = (double) sumOutsideTones / (double) totalSlots;

            const std::string prefix = std::string (mood.name) + "." + cfg.name + ".";
            allMetrics.push_back ({ prefix + "tonicPresentPct", tonicPresentPct });
            allMetrics.push_back ({ prefix + "distinctPct", distinctPct });
            allMetrics.push_back ({ prefix + "noBassSlotPct", noBassSlotPct });
            allMetrics.push_back ({ prefix + "lowIntervalPct", lowIntervalPct });
            allMetrics.push_back ({ prefix + "meanFit", meanFit });
            allMetrics.push_back ({ prefix + "ababPct", ababPct });
            allMetrics.push_back ({ prefix + "meanUniqueChords", meanUniqueChords });
            allMetrics.push_back ({ prefix + "meanOutsideTones", meanOutsideTones });
        }
    }

    // Format flat JSON
    std::ostringstream jsonStream;
    jsonStream << "{\n";
    for (size_t i = 0; i < allMetrics.size(); ++i)
    {
        char valBuf[64];
        std::snprintf (valBuf, sizeof (valBuf), "%.4f", allMetrics[i].value);
        jsonStream << "  \"" << allMetrics[i].key << "\": " << valBuf;
        if (i + 1 < allMetrics.size())
            jsonStream << ",";
        jsonStream << "\n";
    }
    jsonStream << "}\n";
    const std::string jsonOutput = jsonStream.str();

    // Write --out file if requested
    if (! outPath.empty())
    {
        std::ofstream out (outPath);
        if (! out)
        {
            std::fprintf (stderr, "Error: cannot open output file %s\n", outPath.c_str());
            return 1;
        }
        out << jsonOutput;
    }

    // Write --bigrams file if requested
    if (! bigramsPath.empty())
    {
        std::ofstream out (bigramsPath);
        if (! out)
        {
            std::fprintf (stderr, "Error: cannot open bigrams file %s\n", bigramsPath.c_str());
            return 1;
        }
        out << "{\n";
        for (size_t mcIdx = 0; mcIdx < allMoodConfigs.size(); ++mcIdx)
        {
            const auto& mc = allMoodConfigs[mcIdx];
            out << "  \"" << mc.moodName << "." << mc.configName << "\": {\n";
            size_t count = 0;
            for (const auto& kv : mc.bigrams)
            {
                if (kv.second > 0)
                {
                    if (count > 0)
                        out << ",\n";
                    out << "    \"" << kv.first.first << "->" << kv.first.second << "\": " << kv.second;
                    count++;
                }
            }
            out << "\n  }" << (mcIdx + 1 < allMoodConfigs.size() ? ",\n" : "\n");
        }
        out << "}\n";
    }

    // If no --out and no --compare, dump JSON to stdout
    if (outPath.empty() && comparePath.empty())
    {
        std::cout << jsonOutput;
    }

    // Handle --compare <baseline>
    if (! comparePath.empty())
    {
        const auto baseline = loadFlatJson (comparePath);
        if (baseline.empty())
        {
            std::fprintf (stderr, "Error: failed to load baseline from %s\n", comparePath.c_str());
            return 1;
        }

        std::printf ("%-38s %10s %10s %10s  %s\n", "Metric", "Baseline", "Now", "Delta", "Status");
        std::printf ("--------------------------------------------------------------------------------\n");

        int guardFailures = 0;
        for (const auto& m : allMetrics)
        {
            auto it = baseline.find (m.key);
            if (it == baseline.end())
            {
                std::printf ("%-38s %10s %10.4f %10s  MISSING_BASELINE\n", m.key.c_str(), "N/A", m.value, "N/A");
                guardFailures++;
                continue;
            }

            const double bVal = it->second;
            const double nowVal = m.value;
            const double delta = nowVal - bVal;

            std::string status = "OK";
            for (const auto& g : kGuardThresholds)
            {
                if (m.key.find (g.metricKey) != std::string::npos)
                {
                    if (g.direction == GuardDirection::MustNotDrop)
                    {
                        if (bVal - nowVal > g.maxDelta)
                        {
                            status = "FAIL (dropped > " + std::to_string (g.maxDelta) + ")";
                            guardFailures++;
                        }
                    }
                    else if (g.direction == GuardDirection::MustNotRise)
                    {
                        if (nowVal - bVal > g.maxDelta)
                        {
                            status = "FAIL (rose > " + std::to_string (g.maxDelta) + ")";
                            guardFailures++;
                        }
                    }
                    break;
                }
            }

            std::printf ("%-38s %10.4f %10.4f %+10.4f  %s\n", m.key.c_str(), bVal, nowVal, delta, status.c_str());
        }

        std::printf ("--------------------------------------------------------------------------------\n");
        std::printf ("Eval comparison: %d metrics checked, %d failures\n", (int) allMetrics.size(), guardFailures);
        if (guardFailures > 0)
            return 1;
    }

    return 0;
}

int main (int argc, char** argv)
{
    const char* cmd = argc > 1 ? argv[1] : "roll";

    if (std::strcmp (cmd, "eval") == 0)
    {
        return runEval (argc, argv);
    }

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
        const int count = argc > 3 && argv[3][0] != '-' ? std::atoi (argv[3]) : 5;
        Rng rng ((unsigned) (argc > 4 && argv[4][0] != '-' ? std::atoi (argv[4]) : 1));

        float bassMotion = 0.0f;
        for (int a = 2; a < argc; ++a)
        {
            if (std::strcmp (argv[a], "--bass-motion") == 0 && a + 1 < argc)
                bassMotion = (float) std::atof (argv[++a]);
        }

        for (int m = 0; m < (int) moods().size(); ++m)
        {
            if (argc > 2 && argv[2][0] != '-' && std::strcmp (argv[2], "all") != 0 && std::strcmp (argv[2], moods()[(size_t) m].name) != 0)
                continue;

            Progression p;
            p.bassMotion = bassMotion;
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

    std::fprintf (stderr, "usage: chordslots-cli table [scaleIndex] | roll [mood|all] [count] [seed] | eval [N] [--out file] [--bigrams file] [--compare baseline]\n");
    return 1;
}
