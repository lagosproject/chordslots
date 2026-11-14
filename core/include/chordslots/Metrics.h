// ChordSlots core - perceptual metrics computed from the notes themselves.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Nothing here is a hand-tagged "mood". Every number is derived from the
// pitch content, using published, openly documented models:
//
//  * Brightness  - position on the line of fifths (Temperley, "The Cognition
//                  of Basic Musical Structures", 2001) for chord quality and
//                  for the chord's colour inside the key, plus register.
//  * Roughness   - Plomp & Levelt (1965) sensory dissonance in Sethares'
//                  parameterisation ("Tuning, Timbre, Spectrum, Scale", 1998).
//  * Tonal pull  - distance in the Tonal Interval Space, a weighted DFT of
//                  the chroma vector (Bernardes et al., "A multi-level tonal
//                  interval space for modelling pitch relatedness and musical
//                  consonance", JNMR 2016).
#pragma once

#include "Theory.h"

#include <complex>

namespace cs
{

//==============================================================================
// Brightness

/** Chord quality on its own: mean line-of-fifths position of the chord tones
    relative to the chord root. maj7 > maj > sus > 5 > m > m7 > dim. 0..1 */
inline float qualityBrightness (int type)
{
    const auto& t = chordTypes()[(size_t) type];
    bool hasMaj3 = false, hasP5 = false;
    for (int iv : t.iv)
    {
        hasMaj3 = hasMaj3 || mod12 (iv) == 4;
        hasP5 = hasP5 || mod12 (iv) == 7;
    }

    double sum = 0.0;
    for (int iv : t.iv)
    {
        int f = fifthsOf (iv, ! hasMaj3);
        if (mod12 (iv) == 8 && hasMaj3 && ! hasP5)
            f = 8; // augmented fifth, not a flat sixth
        sum += f;
    }
    const double mean = sum / (double) t.iv.size();
    return clamp01 ((float) ((mean + 3.0) / 7.0));
}

/** Colour of the chord inside the key: mean line-of-fifths position of the
    chord tones relative to the tonic. Borrowed flat-side chords (iv, bVI,
    bII) read dark, sharp-side chords (II, #IV) read bright. 0..1 */
inline float modalBrightness (Chord c, int tonic, int scaleIdx)
{
    const auto& t = chordTypes()[(size_t) c.type];
    const bool flat = scaleHasMinorThird (scaleIdx);
    double sum = 0.0;
    for (int iv : t.iv)
        sum += fifthsOf (c.root + iv - tonic, flat);
    const double mean = sum / (double) t.iv.size();
    return clamp01 ((float) ((mean + 5.0) / 11.0));
}

/** Register term: mean pitch of the sounding notes, C3..C6 -> 0..1 */
inline float registerBrightness (const int* notes, int n)
{
    if (n <= 0)
        return 0.5f;
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += notes[i];
    return clamp01 ((float) ((sum / n - 48.0) / 36.0));
}

//==============================================================================
// Roughness (sensory dissonance)

inline double midiToHz (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

/** Sethares' pairwise dissonance summed over 6 harmonics of each note. */
inline double roughness (const int* notes, int n)
{
    constexpr int numPartials = 6;
    double freqs[16 * numPartials];
    double amps[16 * numPartials];
    int count = 0;
    n = std::min (n, 16);

    for (int i = 0; i < n; ++i)
    {
        const double f0 = midiToHz (notes[i]);
        for (int k = 1; k <= numPartials; ++k)
        {
            freqs[count] = f0 * k;
            amps[count] = std::pow (0.88, k - 1);
            ++count;
        }
    }

    double total = 0.0;
    for (int i = 0; i < count; ++i)
        for (int j = i + 1; j < count; ++j)
        {
            const double fmin = std::min (freqs[i], freqs[j]);
            const double df = std::abs (freqs[i] - freqs[j]);
            const double s = 0.24 / (0.021 * fmin + 19.0);
            total += std::min (amps[i], amps[j]) * (std::exp (-3.5 * s * df) - std::exp (-5.75 * s * df));
        }
    return total;
}

/** Roughness of a chord type in close position on middle C, per note. */
inline float typeRoughness (int type)
{
    static const std::vector<float> cache = []
    {
        std::vector<float> v;
        for (const auto& t : chordTypes())
        {
            int notes[8];
            int n = 0;
            for (int iv : t.iv)
                notes[n++] = 60 + iv;
            v.push_back ((float) (roughness (notes, n) / n));
        }
        return v;
    }();
    return cache[(size_t) type];
}

//==============================================================================
// Tonal Interval Space

using Tiv = std::array<std::complex<double>, 6>;

inline Tiv tivOf (unsigned pcMask)
{
    static const double w[6] = { 3.0, 8.0, 11.5, 15.0, 14.5, 7.5 };
    constexpr double twoPi = 6.283185307179586;
    Tiv t {};
    int n = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (pcMask & (1u << pc))
            ++n;
    if (n == 0)
        return t;
    for (int k = 1; k <= 6; ++k)
    {
        std::complex<double> acc (0.0, 0.0);
        for (int pc = 0; pc < 12; ++pc)
            if (pcMask & (1u << pc))
                acc += std::polar (1.0, -twoPi * k * pc / 12.0);
        t[(size_t) k - 1] = acc * (w[k - 1] / n);
    }
    return t;
}

inline double tivNorm (const Tiv& a)
{
    double d = 0.0;
    for (size_t k = 0; k < 6; ++k)
        d += std::norm (a[k]);
    return std::sqrt (d);
}

inline double tivDistance (const Tiv& a, const Tiv& b)
{
    double d = 0.0;
    for (size_t k = 0; k < 6; ++k)
        d += std::norm (a[k] - b[k]);
    return std::sqrt (d);
}

inline unsigned rotateMask (unsigned m, int semis)
{
    semis = mod12 (semis);
    return ((m << semis) | (m >> (12 - semis))) & 0xfffu;
}

/** Tonic triad of a scale (degrees 1-3-5), as an absolute pitch-class mask. */
inline unsigned tonicTriadMask (int tonic, int scaleIdx)
{
    const auto& pcs = scales()[(size_t) scaleIdx].pcs;
    const unsigned rel = (1u << pcs[0]) | (1u << pcs[2]) | (1u << pcs[4]);
    return rotateMask (rel, tonic);
}

/** Raw (uncalibrated) tonal distance of a chord from "home". */
inline double tonalDistanceRaw (Chord c, int tonic, int scaleIdx)
{
    const Tiv chord = tivOf (chordMask (c));
    const double toTonic = tivDistance (chord, tivOf (tonicTriadMask (tonic, scaleIdx)));
    const double toKey = tivDistance (chord, tivOf (rotateMask (scaleMask (scaleIdx), tonic)));
    return 0.6 * toTonic + 0.4 * toKey;
}

/** Perceptual distance between two chords, roughly 0..1. */
inline float chordDistance (Chord a, Chord b)
{
    return clamp01 ((float) (tivDistance (tivOf (chordMask (a)), tivOf (chordMask (b))) / 22.0));
}

//==============================================================================
// Calibration: maps the raw model outputs onto a usable 0..1 range so that the
// plain diatonic vocabulary spans most of the dial. Constants were fitted by
// inspecting `chordslots-cli table` across all scales.

struct Calibration
{
    static constexpr float roughLo = 0.05f, roughHi = 0.55f;
    static constexpr float consLo = 9.0f, consHi = 20.0f; // TIV norm: low = dissonant
    static constexpr float tonalLo = 5.0f, tonalHi = 23.0f;
    static constexpr float brightLo = 0.24f, brightHi = 0.74f;
};

struct ChordMetrics
{
    float brightness = 0.5f;
    float tension = 0.5f;
};

/** Brightness from its three ingredients. */
inline float combineBrightness (float quality, float modal, float reg)
{
    const float raw = 0.55f * quality + 0.25f * modal + 0.20f * reg;
    return clamp01 ((raw - Calibration::brightLo) / (Calibration::brightHi - Calibration::brightLo));
}

inline float norm01 (float x, float lo, float hi) { return clamp01 ((x - lo) / (hi - lo)); }

/** Dissonance of the chord type by itself: sensory roughness + lack of
    harmonic consonance (tritones, clusters). 0..1 */
inline float typeDissonance (int type)
{
    const float rough = norm01 (typeRoughness (type), Calibration::roughLo, Calibration::roughHi);
    const float cons = norm01 ((float) tivNorm (tivOf (chordMask ({ 0, type }))), Calibration::consLo, Calibration::consHi);
    return 0.5f * rough + 0.5f * (1.0f - cons);
}

/** Tension = how dissonant the chord is + how far it sits from home + a small
    functional term (dominants want to resolve). The last one is a plain
    music-theory heuristic, not a measured quantity. */
inline float tensionOf (Chord c, int tonic, int scaleIdx)
{
    const float diss = typeDissonance (c.type);
    const float dist = norm01 ((float) tonalDistanceRaw (c, tonic, scaleIdx), Calibration::tonalLo, Calibration::tonalHi);
    const Function f = functionOf (c, tonic);
    const float func = f == Function::tonic ? 0.0f : (f == Function::subdominant ? 0.4f : 1.0f);
    return clamp01 (0.45f * diss + 0.35f * dist + 0.20f * func);
}

/** Metrics for a chord with a neutral register (used while choosing chords,
    before a voicing exists). */
inline ChordMetrics chordMetrics (Chord c, int tonic, int scaleIdx, float reg = 0.5f)
{
    ChordMetrics m;
    m.brightness = combineBrightness (qualityBrightness (c.type), modalBrightness (c, tonic, scaleIdx), reg);
    m.tension = tensionOf (c, tonic, scaleIdx);
    return m;
}

} // namespace cs
