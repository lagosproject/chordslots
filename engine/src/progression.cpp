#include "chordslots/progression.hpp"

#include <cstddef>

namespace cs {

std::int64_t totalTicks(const Progression& prog) {
    std::int64_t sum = 0;
    for (const Slot& s : prog.slots) sum += s.durationTicks;
    return sum;
}

int barTicks(int beatsPerBar) {
    return beatsPerBar < 1 ? 0 : beatsPerBar * kTicksPerQuarter;
}

std::vector<Issue> validate(const Progression& prog) {
    std::vector<Issue> issues;
    const int n = static_cast<int>(prog.slots.size());

    if (n < kMinSlots) issues.push_back({IssueCode::TooFewSlots, -1});
    if (n > kMaxSlots) issues.push_back({IssueCode::TooManySlots, -1});

    for (int i = 0; i < n; ++i) {
        const Slot& s = prog.slots[static_cast<std::size_t>(i)];

        if (s.durationTicks <= 0) {
            issues.push_back({IssueCode::DurationNotPositive, i});
        } else if (s.durationTicks % kTicksPerQuarter != 0) {
            issues.push_back({IssueCode::DurationNotWholeBeat, i});
        }

        if (s.chord.degree < 0 || s.chord.degree >= kDegreesPerScale) {
            issues.push_back({IssueCode::DegreeOutOfRange, i});
        }
        const int maxInversion = s.chord.seventh ? 3 : 2;
        if (s.chord.inversion < 0 || s.chord.inversion > maxInversion) {
            issues.push_back({IssueCode::InversionOutOfRange, i});
        }
        if (s.chord.octave < -1 || s.chord.octave > 1) {
            issues.push_back({IssueCode::OctaveOutOfRange, i});
        }
    }
    return issues;
}

std::vector<Issue> validate(const Settings& s) {
    std::vector<Issue> issues;

    if (s.tonic < 0 || s.tonic > 11) {
        issues.push_back({IssueCode::TonicOutOfRange, -1});
    }
    if (s.startOffsetBars < 0) {
        issues.push_back({IssueCode::StartOffsetNegative, -1});
    }
    if ((s.allowedDegreesMask & 0x80) != 0) {
        issues.push_back({IssueCode::DegreeMaskOutOfRange, -1});
    }
    if ((s.allowedDegreesMask & 0x7F) == 0) {
        issues.push_back({IssueCode::NoDegreesAllowed, -1});
    }
    if (s.minBeats < 1 || s.maxBeats < s.minBeats) {
        issues.push_back({IssueCode::BeatRangeInvalid, -1});
    }
    return issues;
}

Progression makeDefaultProgression() {
    static const int kDegrees[4] = {0, 5, 2, 6};  // i, VI, III, VII

    Progression p;
    for (int degree : kDegrees) {
        Slot s;
        s.durationTicks = 8 * kTicksPerQuarter;  // 2 compases de 4/4
        s.chord.degree = degree;
        p.slots.push_back(s);
    }
    p.seed = 1;
    return p;
}

PluginState makeDefaultState() {
    PluginState st;  // schemaVersion=1, La menor natural, NextBar
    const Progression p = makeDefaultProgression();
    for (int i = 0; i < kNumVariations; ++i) st.variations[i] = p;
    return st;
}

}  // namespace cs
