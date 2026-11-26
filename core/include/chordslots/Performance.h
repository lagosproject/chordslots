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
constexpr int kMaxCompHitsPerSlot = 64;
inline constexpr std::array<int, 5> kArpRateSteps { 24, 16, 12, 8, 6 };
inline constexpr const char* kArpRateNames[] { "1/4", "1/4T", "1/8", "1/8T", "1/16" };
inline constexpr const char* kArpDirectionNames[] { "Up", "Down", "Up & Down" };
constexpr int kMaxArpOctaves = 3;

enum class CompStyle : int
{
    Sustain = 0,    // Default sustained chord
    PulseQuarter,   // 1/4 note pulse (every 24 steps)
    PulseEighth,    // 1/8 note pulse (every 12 steps)
    OffbeatReggae,  // Upbeat 1/8 notes (offset 12, 36...)
    BalladStabs,    // Dotted 1/4 + 1/8 syncopation (offsets 0, 36)
    TrapStabs,      // 1/16th stabs on each beat and its 2-and (0, 18; repeats every 24 steps)
    TripletSwing,   // 1/8T swing pulses (offsets 0, 16, 32)
    Clave32,
    GarageShuffle,
    CinematicPush,
    ShldPop,
    ShldPop2,
    ShldSoul,
    ShldHipHop2,
    ArpUp,
    ArpDown,
    ArpUpDown
};

inline constexpr const char* kCompStyleNames[] {
    "Sustain", "1/4 Pulse", "1/8 Pulse", "Offbeat Reggae", "Ballad Stabs",
    "Trap Stabs", "Triplet Swing", "3-2 Clave", "Garage Shuffle", "Cinematic Push"
};
inline constexpr int kNumCompStyles = sizeof (kCompStyleNames) / sizeof (kCompStyleNames[0]);
inline constexpr const char* kExtraCompStyleNames[] {
    "SHLD Pop", "SHLD Pop 2", "SHLD Soul", "SHLD Hip Hop", "Arp Up", "Arp Down", "Arp Up & Down"
};
inline constexpr int kNumExtraCompStyles = sizeof (kExtraCompStyleNames) / sizeof (kExtraCompStyleNames[0]);

inline constexpr bool isShldRhythm (CompStyle style) noexcept
{
    return style >= CompStyle::ShldPop && style <= CompStyle::ShldHipHop2;
}

inline constexpr bool isArpeggio (CompStyle style) noexcept
{
    return style >= CompStyle::ArpUp && style <= CompStyle::ArpUpDown;
}

inline constexpr bool isPulse (CompStyle style) noexcept
{
    return style == CompStyle::PulseQuarter || style == CompStyle::PulseEighth;
}

inline constexpr bool hasCompHitLength (CompStyle style) noexcept
{
    return style != CompStyle::Sustain && ! isArpeggio (style);
}

inline constexpr bool hasCompPatternAccents (CompStyle style) noexcept
{
    return hasCompHitLength (style) && ! isShldRhythm (style)
        && style != CompStyle::PulseQuarter && style != CompStyle::OffbeatReggae;
}

inline constexpr const char* compStyleName (int index) noexcept
{
    return index < kNumCompStyles ? kCompStyleNames[index] : kExtraCompStyleNames[index - kNumCompStyles];
}

inline constexpr int compPreviewBars (CompStyle style) noexcept
{
    return style == CompStyle::Clave32 || isShldRhythm (style) ? 2 : 1;
}

struct CompHit
{
    int stepOffset = 0;      // in 24-step timebase relative to slot start
    int durationSteps = 0;   // gate length in steps
    float velScale = 1.0f;   // velocity multiplier
    int arpVoice = 0;        // zero plays all voices, signed ordinals count up or down
};

inline int arpSequenceIndex (int ordinal, int count, bool bounce) noexcept
{
    if (count <= 1)
        return 0;
    if (bounce)
    {
        const int cycle = 2 * (count - 1);
        const int phase = (ordinal - 1) % cycle;
        return std::min (phase, cycle - phase);
    }
    return ordinal > 0 ? (ordinal - 1) % count : count - 1 - (-ordinal - 1) % count;
}

inline int arpDurationSteps (int rate, float length) noexcept
{
    return std::clamp ((int) std::lround (rate * length), 1, rate);
}

inline CompHit arpHit (int index, int slotSteps, int rate, int duration, int direction) noexcept
{
    const int step = index * rate;
    return { step, std::min (duration, slotSteps - step), step % 24 == 0 ? 1.0f : 0.85f,
             direction * (index + 1) };
}

inline CompHit pulseHit (int index, int slotSteps, int rate, float legacyLength = 1.0f,
                         float accent = 1.0f) noexcept
{
    const int step = index * rate;
    const int duration = std::min (rate * 3 / 4, slotSteps - step);
    return { step, std::max (1, (int) std::lround (duration * legacyLength)),
             step % 24 == 0 ? 1.0f : 1.0f - 0.15f * accent };
}

/** Non-allocating hit generator writing directly into a caller-provided fixed buffer. */
inline int getCompHits (CompStyle style, int slotSteps, CompHit* outHits, int maxHits,
                        int /*meterNum*/ = 4, int /*meterDen*/ = 4, int slotStart = 0,
                        int arpRateSteps = 12, float arpLength = 0.83f) noexcept
{
    if (outHits == nullptr || maxHits <= 0)
        return 0;

    int count = 0;
    auto addHit = [&] (int stepOffset, int durationSteps, float velScale)
    {
        if (count < maxHits)
            outHits[count++] = { stepOffset, durationSteps, velScale };
    };

    if (isShldRhythm (style))
    {
        // SHLD c2mpatterns.py (MIT, see DATA_LICENSES.md). N and S strike the current
        // voicing. X is silent. Durations are converted from beats to steps.
        constexpr int phraseSteps = 8 * 24;
        constexpr CompHit rhythms[][5] {
            { { 0, 36 }, { 36, 60 }, { 96, 36 }, { 132, 60 } },
            { { 0, 42 }, { 42, 54 }, { 96, 42 }, { 138, 54 } },
            { { 0, 24 }, { 24, 24 }, { 48, 36 }, { 84, 48 }, { 132, 60 } },
            { { 0, 24 }, { 42, 54 }, { 114, 30 }, { 144, 48 } }
        };
        const int index = static_cast<int> (style) - kNumCompStyles;
        const int slotEnd = slotStart + slotSteps;
        for (int cycle = slotStart / phraseSteps * phraseSteps; cycle < slotEnd && count < maxHits; cycle += phraseSteps)
        {
            for (const auto& hit : rhythms[index])
            {
                const int start = std::max (slotStart, cycle + hit.stepOffset);
                const int end = std::min (slotEnd, cycle + hit.stepOffset + hit.durationSteps);
                if (start < end)
                    addHit (start - slotStart, end - start, hit.velScale);
            }
        }
        return count;
    }

    if (isArpeggio (style))
    {
        const int direction = style == CompStyle::ArpDown ? -1 : 1;
        const int rate = std::max (1, arpRateSteps);
        const int length = arpDurationSteps (rate, arpLength);
        const int hits = std::min (maxHits, (slotSteps + rate - 1) / rate);
        for (; count < hits; ++count)
            outHits[count] = arpHit (count, slotSteps, rate, length, direction);
        return count;
    }

    if (style == CompStyle::Sustain || slotSteps <= 0)
    {
        addHit (0, slotSteps, 1.0f);
        return count;
    }

    if (isPulse (style))
    {
        const int rate = style == CompStyle::PulseQuarter ? 24 : 12;
        for (; count < maxHits && count * rate < slotSteps; ++count)
            outHits[count] = pulseHit (count, slotSteps, rate);
    }
    else if (style == CompStyle::OffbeatReggae)
    {
        for (int st = 12; st < slotSteps; st += 24)
        {
            int dur = std::min (8, slotSteps - st);
            addHit (st, dur, 1.0f);
        }
        if (count == 0)
            addHit (0, std::min (12, slotSteps), 1.0f);
    }
    else if (style == CompStyle::BalladStabs)
    {
        for (int st = 0; st < slotSteps; st += 48)
        {
            addHit (st, std::min (24, slotSteps - st), 1.0f);
            if (st + 36 < slotSteps)
                addHit (st + 36, std::min (10, slotSteps - (st + 36)), 0.9f);
        }
    }
    else if (style == CompStyle::TrapStabs)
    {
        for (int st = 0; st < slotSteps; st += 24)
        {
            addHit (st, std::min (5, slotSteps - st), 1.0f);
            if (st + 18 < slotSteps)
                addHit (st + 18, std::min (5, slotSteps - (st + 18)), 0.85f);
        }
    }
    else if (style == CompStyle::TripletSwing)
    {
        for (int st = 0; st < slotSteps; st += 16)
        {
            int dur = std::min (10, slotSteps - st);
            float v = (st % 48 == 0) ? 1.0f : 0.88f;
            addHit (st, dur, v);
        }
    }
    else
    {
        constexpr CompHit clave[] { { 0, 9, 1.0f }, { 36, 8, 0.87f }, { 72, 9, 0.94f },
                                    { 120, 8, 0.86f }, { 144, 9, 0.94f } };
        constexpr CompHit shuffle[] { { 0, 10, 1.0f }, { 16, 5, 0.72f }, { 40, 6, 0.89f } };
        constexpr CompHit push[] { { 0, 30, 1.0f }, { 36, 17, 0.83f }, { 66, 24, 0.95f },
                                   { 90, 6, 0.73f } };

        const auto repeat = [&] (const auto& pattern, int cycleSteps)
        {
            for (int start = 0; start < slotSteps && count < maxHits; start += cycleSteps)
                for (const auto& hit : pattern)
                    if (const int step = start + hit.stepOffset; step < slotSteps)
                        addHit (step, std::min (hit.durationSteps, slotSteps - step), hit.velScale);
        };

        if (style == CompStyle::Clave32) repeat (clave, 192);
        if (style == CompStyle::GarageShuffle) repeat (shuffle, 48);
        if (style == CompStyle::CinematicPush) repeat (push, 96);
    }

    if (count == 0)
        addHit (0, slotSteps, 1.0f);
    return count;
}

inline std::vector<CompHit> getCompHits (CompStyle style, int slotSteps, int meterNum = 4, int meterDen = 4)
{
    CompHit buf[kMaxCompHitsPerSlot];
    const int count = getCompHits (style, slotSteps, buf, kMaxCompHitsPerSlot, meterNum, meterDen);
    return std::vector<CompHit> (buf, buf + count);
}

/**
 * Scales per-voice strum/humanize offsets for a comp hit so that the latest voice
 * starts no later than 50% of the hit's duration:
 * scale = min(1, 0.5 * hitDurationMs / maxOffsetMs)
 */
inline double compHitOffsetScale (double hitDurationMs, double maxOffsetMs) noexcept
{
    if (maxOffsetMs <= 0.0 || hitDurationMs <= 0.0)
        return 1.0;
    return std::min (1.0, 0.5 * hitDurationMs / maxOffsetMs);
}

struct PerformanceParams
{
    CompStyle compStyle      = CompStyle::Sustain;
    CompStyle arpDirection   = CompStyle::ArpUp;
    int arpRateSteps          = 12;
    float arpLength           = 0.83f;
    int arpOctaves            = 1;
    float compHitLength       = 1.0f;
    float compPatternAccent   = 1.0f;
    int pulseRateSteps        = 0; // zero uses the saved quarter/eighth pulse style
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

inline int pulseRateFor (const PerformanceParams& perf) noexcept
{
    return perf.pulseRateSteps > 0 ? perf.pulseRateSteps : perf.compStyle == CompStyle::PulseQuarter ? 24 : 12;
}

inline void shapeCompHits (CompHit* hits, int count, const PerformanceParams& perf) noexcept
{
    const bool shorter = perf.compHitLength < 1.0f && hasCompHitLength (perf.compStyle);
    const bool accent = perf.compPatternAccent != 1.0f && hasCompPatternAccents (perf.compStyle);
    if (! shorter && ! accent)
        return;
    for (int i = 0; i < count; ++i)
    {
        auto& hit = hits[i];
        if (shorter)
            hit.durationSteps = std::max (1, (int) std::lround (hit.durationSteps * perf.compHitLength));
        if (accent)
            hit.velScale = 1.0f + (hit.velScale - 1.0f) * perf.compPatternAccent;
    }
}

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
