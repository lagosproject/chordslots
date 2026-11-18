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

#include "SpiralTension.h"

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
    static constexpr float voicedRoughLo = 0.13f, voicedRoughHi = 0.63f;
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

/** Harmonic consonance is computed from the chord's pitch-class set. */
inline float harmonicDissonance (int type)
{
    const float consonance = norm01 ((float) tivNorm (tivOf (chordMask ({ 0, type }))),
                                     Calibration::consLo, Calibration::consHi);
    return 1.0f - consonance;
}

inline float typeDissonance (int type)
{
    const float rough = norm01 (typeRoughness (type), Calibration::roughLo, Calibration::roughHi);
    return 0.5f * rough + 0.5f * harmonicDissonance (type);
}

/** Weight of the tritone-resolution term in Tension. Zero keeps the previous
    behaviour. The audits in docs/audits suggest a small positive value, but it
    stays off until it is validated on data not used to choose it (PLAN-GENERAL, R2). */
inline constexpr float kTritoneWeight = 0.0f;

/** True when the chord's own tones (intervals below one octave, so extensions
    such as the #11 of maj7#11 are ignored) contain a tritone. These are the
    chords with an inherent need to resolve: dominant sevenths, dim, m7b5, dim7. */
inline bool hasChordTritone (int type)
{
    static const std::vector<bool> table = []
    {
        std::vector<bool> v;
        for (const auto& t : chordTypes())
        {
            bool found = false;
            for (size_t a = 0; a < t.iv.size(); ++a)
                for (size_t b = a + 1; b < t.iv.size(); ++b)
                    if (t.iv[a] < 12 && t.iv[b] < 12 && std::abs (t.iv[a] - t.iv[b]) == 6)
                        found = true;
            v.push_back (found);
        }
        return v;
    }();
    return table[(size_t) type];
}

inline float tensionFromDissonance (Chord c, int tonic, int scaleIdx, float dissonance,
                                    float tritoneWeight = kTritoneWeight)
{
    const float dist = norm01 ((float) tonalDistanceRaw (c, tonic, scaleIdx), Calibration::tonalLo, Calibration::tonalHi);
    const auto function = functionOf (c, tonic);
    const int degree = mod12 (c.root - tonic);
    const float pull = degree == 10 ? (scaleHasMinorThird (scaleIdx) ? 0.7f : 0.4f)
                                   : (function == Function::tonic ? 0.0f : (function == Function::subdominant ? 0.4f : 1.0f));
    const float tritone = hasChordTritone (c.type) ? tritoneWeight : 0.0f;
    return clamp01 (0.45f * dissonance + 0.35f * dist + 0.20f * pull + tritone);
}

/** Chord-symbol estimate for candidate pruning and the legacy listening score. */
inline float tensionOf (Chord c, int tonic, int scaleIdx)
{
    return tensionFromDissonance (c, tonic, scaleIdx, typeDissonance (c.type));
}

/** Static tension with the sounding register and intervals in the roughness term. */
inline float voicedTension (Chord c, int tonic, int scaleIdx, const int* notes, int n)
{
    n = std::min (n, 16);
    if (n <= 0)
        return tensionOf (c, tonic, scaleIdx);
    const float rough = norm01 ((float) (roughness (notes, n) / n),
                                Calibration::voicedRoughLo, Calibration::voicedRoughHi);
    return tensionFromDissonance (c, tonic, scaleIdx, 0.5f * rough + 0.5f * harmonicDissonance (c.type));
}

// P6 preview. Change this only after comparing the blind listening scores.
inline constexpr bool kUseContextualTension = true;

struct ContextualTension
{
    static constexpr float staticWeight = 0.86f;
    static constexpr float strainWeight = 0.09f;
    static constexpr float momentumWeight = 0.05f;
    static constexpr float strainCeiling = 0.33f;
    static constexpr float momentumCeiling = 0.36f;
};

inline float blendContextualTension (float staticTension, float strain, float momentum) noexcept
{
    return clamp01 (ContextualTension::staticWeight * staticTension
                    + ContextualTension::strainWeight * clamp01 (strain / ContextualTension::strainCeiling)
                    + ContextualTension::momentumWeight * clamp01 (momentum / ContextualTension::momentumCeiling));
}

/** How far the quality term of a major-third chord on the fifth degree of a
    minor-third scale is pulled towards a minor triad's. 0 keeps the previous
    behaviour. In a minor key the major V (the harmonic-minor dominant) is part
    of the key's own harmony, not a borrowed bright chord, yet its major third
    makes it read as bright and it loses to the dark targets of minor moods.
    Off until validated (PLAN-GENERAL, H9 and R11). */
inline constexpr float kMinorDominantContext = 0.0f;

/** Quality term of Brightness, with the minor-key dominant context applied. */
inline float contextQualityBrightness (Chord c, int tonic, int scaleIdx,
                                       float context = kMinorDominantContext)
{
    const float q = qualityBrightness (c.type);
    if (context <= 0.0f || ! scaleHasMinorThird (scaleIdx)
        || mod12 (c.root - tonic) != 7 || ! hasMajorThird (c.type))
        return q;
    constexpr int kMinorTriad = 1; // chordTypes()[1] is "m", checked in core_tests
    return q + clamp01 (context) * (qualityBrightness (kMinorTriad) - q);
}

/** Metrics for a chord with a neutral register (used while choosing chords,
    before a voicing exists). */
inline ChordMetrics chordMetrics (Chord c, int tonic, int scaleIdx, float reg = 0.5f)
{
    ChordMetrics m;
    m.brightness = combineBrightness (contextQualityBrightness (c, tonic, scaleIdx),
                                      modalBrightness (c, tonic, scaleIdx), reg);
    m.tension = tensionOf (c, tonic, scaleIdx);
    return m;
}

} // namespace cs
