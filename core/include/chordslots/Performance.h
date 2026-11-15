// core/include/chordslots/Performance.h
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Phase 2 of the pipeline: turns a clean voicing into a "played" one.
// Pure function, no JUCE, no allocation, no globals -> safe to call from the
// PlayData bake step, the MIDI exporter, and (later) the incoming-MIDI trigger.
//
// IMPORTANT: call this DOWNSTREAM of Metrics.h. Energy/Fit must be measured on
// the clean Phase 1 voicing, or the Fit score will drift when groove knobs move.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace cs {

constexpr int kPerfMaxNotes = 12;

enum class CompStyle : int
{
    Sustain = 0,    // Default sustained chord
    PulseQuarter,   // 1/4 note pulse (every 24 steps)
    PulseEighth,    // 1/8 note pulse (every 12 steps)
    OffbeatReggae,  // Upbeat 1/8 notes (offset 12, 36...)
    BalladStabs,    // Dotted 1/4 + 1/8 syncopation (offsets 0, 36)
    TrapStabs,      // 1/16th stabs on 1 and 2-and (offsets 0, 18)
    TripletSwing    // 1/8T swing pulses (offsets 0, 16, 32)
};

struct CompHit
{
    int stepOffset;      // in 24-step timebase relative to slot start
    int durationSteps;   // gate length in steps
    float velScale;      // velocity multiplier
};

inline std::vector<CompHit> getCompHits (CompStyle style, int slotSteps, int /*meterNum*/ = 4, int /*meterDen*/ = 4)
{
    std::vector<CompHit> hits;
    if (style == CompStyle::Sustain || slotSteps <= 0)
    {
        hits.push_back ({ 0, slotSteps, 1.0f });
        return hits;
    }

    if (style == CompStyle::PulseQuarter)
    {
        for (int st = 0; st < slotSteps; st += 24)
        {
            int dur = std::min (18, slotSteps - st);
            hits.push_back ({ st, dur, 1.0f });
        }
    }
    else if (style == CompStyle::PulseEighth)
    {
        for (int st = 0; st < slotSteps; st += 12)
        {
            int dur = std::min (9, slotSteps - st);
            float v = (st % 24 == 0) ? 1.0f : 0.85f;
            hits.push_back ({ st, dur, v });
        }
    }
    else if (style == CompStyle::OffbeatReggae)
    {
        for (int st = 12; st < slotSteps; st += 24)
        {
            int dur = std::min (8, slotSteps - st);
            hits.push_back ({ st, dur, 1.0f });
        }
        if (hits.empty())
            hits.push_back ({ 0, std::min (12, slotSteps), 1.0f });
    }
    else if (style == CompStyle::BalladStabs)
    {
        for (int st = 0; st < slotSteps; st += 48)
        {
            hits.push_back ({ st, std::min (24, slotSteps - st), 1.0f });
            if (st + 36 < slotSteps)
                hits.push_back ({ st + 36, std::min (10, slotSteps - (st + 36)), 0.9f });
        }
    }
    else if (style == CompStyle::TrapStabs)
    {
        for (int st = 0; st < slotSteps; st += 24)
        {
            hits.push_back ({ st, std::min (5, slotSteps - st), 1.0f });
            if (st + 18 < slotSteps)
                hits.push_back ({ st + 18, std::min (5, slotSteps - (st + 18)), 0.85f });
        }
    }
    else if (style == CompStyle::TripletSwing)
    {
        for (int st = 0; st < slotSteps; st += 16)
        {
            int dur = std::min (10, slotSteps - st);
            float v = (st % 48 == 0) ? 1.0f : 0.88f;
            hits.push_back ({ st, dur, v });
        }
    }

    if (hits.empty())
        hits.push_back ({ 0, slotSteps, 1.0f });
    return hits;
}

struct PerformanceParams
{
    CompStyle compStyle      = CompStyle::Sustain;
    // --- Strum ---------------------------------------------------------
    float strum              = 0.0f;   // -1..1. sign = direction, |value| scales spread.
                                       //  >0 down-strum (bass -> treble), <0 up-strum.
    float strumMaxMs         = 80.0f;  // total spread at |strum| == 1
    float strumSlotFraction  = 0.35f;  // hard cap: offsets never exceed this * slot length
    bool  strumIncludesBass  = false;  // false: bass stays on the grid, upper voices roll

    // --- Voicing dynamics (melody accent) ------------------------------
    float accent             = 0.0f;   // 0..1. top voice louder, inner voices softer

    // --- Humanize ------------------------------------------------------
    float humanize           = 0.0f;   // 0..1
    float humanizeMaxMs      = 6.0f;   // late-only timing jitter at humanize == 1
    float humanizeMaxVel     = 5.0f;   // +/- velocity drift at humanize == 1
    uint32_t seed            = 1;      // "re-feel" button changes this
};

struct PerfInput
{
    std::array<int, kPerfMaxNotes> pitch{};
    std::array<int, kPerfMaxNotes> velocity{};
    int n         = 0;
    int bassCount = 0;
};

struct PerfNote
{
    int   pitch    = 0;
    int   velocity = 0;
    float offsetMs = 0.0f;   // delay after slot start; always >= 0
};

struct PerfOutput
{
    std::array<PerfNote, kPerfMaxNotes> notes{};   // ascending by pitch
    int n = 0;
};

namespace detail
{
    // splitmix64: stateless, so results depend only on (seed, slot, pass, note).
    inline uint64_t mix64 (uint64_t x) noexcept
    {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    inline float unit01 (uint64_t h) noexcept          // [0,1)
    {
        return (float) (h >> 40) * (1.0f / 16777216.0f);
    }

    inline uint64_t noteHash (uint32_t seed, int slot, int pass, int rank, int salt) noexcept
    {
        uint64_t h = mix64 (seed);
        h = mix64 (h ^ (uint64_t) (uint32_t) slot);
        h = mix64 (h ^ ((uint64_t) (uint32_t) pass << 20));
        h = mix64 (h ^ ((uint64_t) (uint32_t) rank << 8) ^ (uint64_t) salt);
        return h;
    }

    inline int clampVel (float v) noexcept
    {
        return std::clamp ((int) std::lround (v), 1, 127);
    }
}

// slotLenMs : length of the slot in ms at the current tempo (caller converts).
// slotIndex : index in the progression (part of the humanize seed).
// pass      : loop pass counter (0 for exports / offline bakes).
inline PerfOutput applyPerformance (const PerfInput& in,
                                    const PerformanceParams& p,
                                    double slotLenMs,
                                    int slotIndex,
                                    int pass) noexcept
{
    PerfOutput out;
    const int n = std::clamp (in.n, 0, kPerfMaxNotes);
    out.n = n;
    if (n == 0)
        return out;

    // Rank notes ascending by pitch (stable, so equal pitches keep input order).
    std::array<int, kPerfMaxNotes> order{};
    std::iota (order.begin(), order.begin() + n, 0);
    std::stable_sort (order.begin(), order.begin() + n,
                      [&] (int a, int b) { return in.pitch[(size_t) a] < in.pitch[(size_t) b]; });

    const int bassCount = std::clamp (in.bassCount, 0, n);
    const float cap = (float) std::max (0.0, slotLenMs * (double) p.strumSlotFraction);

    // --- Strum spread --------------------------------------------------
    const float strumAmt = std::clamp (p.strum, -1.0f, 1.0f);
    const float spread   = std::min (std::fabs (strumAmt) * p.strumMaxMs, cap);
    const bool  down     = strumAmt >= 0.0f;

    const int firstStrummed = p.strumIncludesBass ? 0 : bassCount;
    const int m = n - firstStrummed;                     // strummed voice count

    for (int rank = 0; rank < n; ++rank)
    {
        const int src = order[(size_t) rank];
        PerfNote& o = out.notes[(size_t) rank];
        o.pitch = in.pitch[(size_t) src];

        // timing: strum
        float offset = 0.0f;
        if (rank >= firstStrummed && m >= 2 && spread > 0.0f)
        {
            const float k = (float) (rank - firstStrummed) / (float) (m - 1);   // 0..1 low->high
            offset = spread * (down ? k : 1.0f - k);
        }

        // velocity: melody accent (bass untouched)
        float vel = (float) in.velocity[(size_t) src];
        if (p.accent > 0.0f && rank >= bassCount)
        {
            const float a = std::clamp (p.accent, 0.0f, 1.0f);
            vel *= (rank == n - 1) ? (1.0f + 0.25f * a) : (1.0f - 0.15f * a);
        }

        // humanize: late-only timing jitter (keeps offset >= 0), symmetric velocity drift
        if (p.humanize > 0.0f)
        {
            const float h = std::clamp (p.humanize, 0.0f, 1.0f);
            offset += detail::unit01 (detail::noteHash (p.seed, slotIndex, pass, rank, 1))
                      * p.humanizeMaxMs * h;
            vel += (detail::unit01 (detail::noteHash (p.seed, slotIndex, pass, rank, 2)) * 2.0f - 1.0f)
                   * p.humanizeMaxVel * h;
        }

        o.offsetMs = std::clamp (offset, 0.0f, cap);
        o.velocity = detail::clampVel (vel);
    }
    return out;
}

} // namespace cs
