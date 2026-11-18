// ChordSlots core - music theory primitives (no dependencies beyond the STL).
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace cs
{

inline int mod12 (int x) noexcept { return ((x % 12) + 12) % 12; }
inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

//==============================================================================
struct ScaleDef
{
    const char* name;
    std::vector<int> pcs; // pitch classes relative to the tonic
};

inline const std::vector<ScaleDef>& scales()
{
    static const std::vector<ScaleDef> s = {
        { "Major",             { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor",             { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian",            { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",          { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",            { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",        { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",           { 0, 1, 3, 5, 6, 8, 10 } },
        { "Harmonic Minor",    { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",     { 0, 2, 3, 5, 7, 9, 11 } },
        { "Phrygian Dominant", { 0, 1, 4, 5, 7, 8, 10 } },
    };
    return s;
}

inline int numScales() { return (int) scales().size(); }

/** 12-bit mask of the pitch classes (relative to the tonic) in a scale. */
inline unsigned scaleMask (int scaleIdx)
{
    unsigned m = 0;
    for (int pc : scales()[(size_t) scaleIdx].pcs)
        m |= 1u << pc;
    return m;
}

inline bool scaleHasMinorThird (int scaleIdx) { return (scaleMask (scaleIdx) & (1u << 3)) != 0; }

//==============================================================================
enum class QualityClass : uint8_t
{
    Major = 0,
    Minor = 1,
    Dominant = 2,
    Diminished = 3,
    SusOrAug = 4
};

constexpr int kNumQualityClasses = 5;

enum class SeventhKind : uint8_t
{
    None = 0,
    Minor7 = 1,       // 10
    Major7 = 2,       // 11
    Diminished7 = 3   // 9 with a dim fifth
};

//==============================================================================
// Pure interval-derived properties
inline bool hasMajorThird (const std::vector<int>& iv) noexcept
{
    return std::find (iv.begin(), iv.end(), 4) != iv.end();
}

inline bool hasMinorThird (const std::vector<int>& iv) noexcept
{
    return std::find (iv.begin(), iv.end(), 3) != iv.end()
        && std::find (iv.begin(), iv.end(), 4) == iv.end();
}

inline bool hasPerfectFifth (const std::vector<int>& iv) noexcept
{
    return std::find (iv.begin(), iv.end(), 7) != iv.end();
}

inline bool hasDimFifth (const std::vector<int>& iv) noexcept
{
    return std::find (iv.begin(), iv.end(), 6) != iv.end();
}

inline bool hasAugFifth (const std::vector<int>& iv) noexcept
{
    return std::find (iv.begin(), iv.end(), 8) != iv.end()
        && hasMajorThird (iv)
        && ! hasPerfectFifth (iv);
}

inline SeventhKind seventhKind (const std::vector<int>& iv) noexcept
{
    if (std::find (iv.begin(), iv.end(), 9) != iv.end() && hasDimFifth (iv))
        return SeventhKind::Diminished7;
    if (std::find (iv.begin(), iv.end(), 10) != iv.end())
        return SeventhKind::Minor7;
    if (std::find (iv.begin(), iv.end(), 11) != iv.end())
        return SeventhKind::Major7;
    return SeventhKind::None;
}

inline bool seventhIsMajor (const std::vector<int>& iv) noexcept
{
    return seventhKind (iv) == SeventhKind::Major7;
}

inline bool isSus (const std::vector<int>& iv) noexcept
{
    const bool has2or5 = (std::find (iv.begin(), iv.end(), 2) != iv.end())
                      || (std::find (iv.begin(), iv.end(), 5) != iv.end());
    const bool hasThird = hasMajorThird (iv) || hasMinorThird (iv);
    return has2or5 && ! hasThird;
}

inline bool isPower (const std::vector<int>& iv) noexcept
{
    return iv.size() == 2 && iv[0] == 0 && iv[1] == 7;
}

inline unsigned pitchClassMask (const std::vector<int>& iv) noexcept
{
    unsigned m = 0;
    for (int x : iv)
        m |= (1u << mod12 (x));
    return m;
}

inline int numDistinctPitchClasses (const std::vector<int>& iv) noexcept
{
    unsigned m = pitchClassMask (iv);
    int count = 0;
    while (m > 0)
    {
        count += (int) (m & 1u);
        m >>= 1;
    }
    return count;
}

inline bool isTriadType (const std::vector<int>& iv) noexcept
{
    const int pcs = numDistinctPitchClasses (iv);
    if (pcs != 3)
        return false;
    const unsigned m = pitchClassMask (iv);
    return hasMajorThird (iv) || hasMinorThird (iv)
        || m == ((1u << 0) | (1u << 3) | (1u << 6))
        || m == ((1u << 0) | (1u << 4) | (1u << 8));
}

inline bool isSeventhType (const std::vector<int>& iv) noexcept
{
    return numDistinctPitchClasses (iv) == 4 && seventhKind (iv) != SeventhKind::None;
}

inline QualityClass qualityClassFromIntervals (const std::vector<int>& iv) noexcept
{
    if (hasDimFifth (iv) && hasMinorThird (iv))
        return QualityClass::Diminished;
    if (hasAugFifth (iv) || isSus (iv))
        return QualityClass::SusOrAug;
    if (hasMajorThird (iv) && seventhKind (iv) == SeventhKind::Minor7)
        return QualityClass::Dominant;
    if (hasMinorThird (iv))
        return QualityClass::Minor;
    if (hasMajorThird (iv))
        return QualityClass::Major;
    if (isPower (iv))
        return QualityClass::Major;
    return QualityClass::SusOrAug;
}

//==============================================================================
struct ChordTypeDef
{
    const char* suffix;      // "m7", "sus4"...
    const char* romanSuffix; // suffix used after a roman numeral
    std::vector<int> iv;     // intervals from the root (may exceed 12)
    int complexity;          // 0 = plain triad ... 2 = colourful
    bool minorish;           // has a minor third (lower-case numeral)
};

inline const std::vector<ChordTypeDef>& chordTypes()
{
    static const std::vector<ChordTypeDef> t = {
        { "",      "",      { 0, 4, 7 },         0, false },
        { "m",     "",      { 0, 3, 7 },         0, true  },
        { "5",     "5",     { 0, 7 },            1, false },
        { "dim",   "\xc2\xb0", { 0, 3, 6 },      1, true  },
        { "aug",   "+",     { 0, 4, 8 },         2, false },
        { "sus2",  "sus2",  { 0, 2, 7 },         1, false },
        { "sus4",  "sus4",  { 0, 5, 7 },         1, false },
        { "maj7",  "maj7",  { 0, 4, 7, 11 },     1, false },
        { "m7",    "7",     { 0, 3, 7, 10 },     1, true  },
        { "7",     "7",     { 0, 4, 7, 10 },     1, false },
        { "m7b5",  "\xc3\xb8" "7", { 0, 3, 6, 10 }, 2, true },
        { "dim7",  "\xc2\xb0" "7", { 0, 3, 6, 9 },  2, true },
        { "mMaj7", "maj7",  { 0, 3, 7, 11 },     2, true  },
        { "6",     "6",     { 0, 4, 7, 9 },      1, false },
        { "m6",    "6",     { 0, 3, 7, 9 },      2, true  },
        { "add9",  "add9",  { 0, 4, 7, 14 },     1, false },
        { "madd9", "add9",  { 0, 3, 7, 14 },     1, true  },
        { "maj9",  "maj9",  { 0, 4, 7, 11, 14 }, 2, false },
        { "m9",    "9",     { 0, 3, 7, 10, 14 }, 2, true  },
        { "9",     "9",     { 0, 4, 7, 10, 14 }, 2, false },
        { "7sus4",   "7sus4",   { 0, 5, 7, 10 },          2, false },
        { "m11",     "11",      { 0, 3, 10, 14, 17 },     2, true  },
        { "maj7#11", "maj7#11", { 0, 4, 7, 11, 18 },     2, false },
        { "13",      "13",      { 0, 4, 10, 14, 21 },     2, false },
        { "m13",     "13",      { 0, 3, 10, 14, 21 },     2, true  },
        { "6/9",     "6/9",     { 0, 4, 7, 9, 14 },       1, false },
        { "m6/9",    "6/9",     { 0, 3, 7, 9, 14 },       2, true  },
        { "7b9",     "7b9",     { 0, 4, 7, 10, 13 },      2, false },
        { "7#9",     "7#9",     { 0, 4, 7, 10, 15 },      2, false },
        { "add11",   "add11",   { 0, 4, 7, 17 },          1, false },
        { "9sus4",   "9sus4",   { 0, 5, 7, 10, 14 },      2, false },
        { "quartal", "q",       { 0, 5, 10, 15 },         2, false },
    };
    return t;
}

inline int numChordTypes() { return (int) chordTypes().size(); }

//==============================================================================
// Cached chord properties derived from intervals
struct ChordTypeProperties
{
    bool hasMajorThird = false;
    bool hasMinorThird = false;
    bool hasPerfectFifth = false;
    bool hasDimFifth = false;
    bool hasAugFifth = false;
    SeventhKind seventhKind = SeventhKind::None;
    bool isSus = false;
    bool isPower = false;
    bool isTriadType = false;
    bool isSeventhType = false;
    bool seventhIsMajor = false;
    QualityClass qualityClass = QualityClass::SusOrAug;
};

inline const std::vector<ChordTypeProperties>& chordTypePropertiesTable()
{
    static const std::vector<ChordTypeProperties> table = []
    {
        const auto& types = chordTypes();
        std::vector<ChordTypeProperties> v;
        v.reserve (types.size());
        for (const auto& t : types)
        {
            ChordTypeProperties p;
            p.hasMajorThird = hasMajorThird (t.iv);
            p.hasMinorThird = hasMinorThird (t.iv);
            p.hasPerfectFifth = hasPerfectFifth (t.iv);
            p.hasDimFifth = hasDimFifth (t.iv);
            p.hasAugFifth = hasAugFifth (t.iv);
            p.seventhKind = seventhKind (t.iv);
            p.isSus = isSus (t.iv);
            p.isPower = isPower (t.iv);
            p.isTriadType = isTriadType (t.iv);
            p.isSeventhType = isSeventhType (t.iv);
            p.seventhIsMajor = (p.seventhKind == SeventhKind::Major7);
            p.qualityClass = qualityClassFromIntervals (t.iv);
            v.push_back (p);
        }
        return v;
    }();
    return table;
}

inline const ChordTypeProperties& chordProperties (int type) noexcept
{
    return chordTypePropertiesTable()[(size_t) type];
}

inline bool hasMajorThird (int type) noexcept { return chordProperties (type).hasMajorThird; }
inline bool hasMinorThird (int type) noexcept { return chordProperties (type).hasMinorThird; }
inline bool hasPerfectFifth (int type) noexcept { return chordProperties (type).hasPerfectFifth; }
inline bool hasDimFifth (int type) noexcept { return chordProperties (type).hasDimFifth; }
inline bool hasAugFifth (int type) noexcept { return chordProperties (type).hasAugFifth; }
inline SeventhKind seventhKind (int type) noexcept { return chordProperties (type).seventhKind; }
inline bool seventhIsMajor (int type) noexcept { return chordProperties (type).seventhIsMajor; }
inline bool isSus (int type) noexcept { return chordProperties (type).isSus; }
inline bool isPower (int type) noexcept { return chordProperties (type).isPower; }
inline bool isTriadType (int type) noexcept { return chordProperties (type).isTriadType; }
inline bool isSeventhType (int type) noexcept { return chordProperties (type).isSeventhType; }


struct Chord
{
    int root = 0;    // absolute pitch class 0..11
    int type = 0;    // index into chordTypes()
    int bassInt = 0; // semitones above root; 0 = root position

    int bassPc() const noexcept { return mod12 (root + bassInt); }
    bool isSlash() const noexcept { return bassInt != 0; }

    bool operator== (const Chord& o) const noexcept
    {
        return root == o.root && type == o.type && bassInt == o.bassInt;
    }
    bool operator!= (const Chord& o) const noexcept { return ! (*this == o); }
};

/** Checks whether a bass interval corresponds to a chord tone of this type. */
inline bool isBassIntValid (int type, int bassInt) noexcept
{
    if (bassInt == 0)
        return true;
    for (int iv : chordTypes()[(size_t) type].iv)
        if (mod12 (iv) == mod12 (bassInt))
            return true;
    return false;
}

/** Resets bassInt to 0 (root position) if it is no longer valid for the chord type. */
inline void sanitizeBass (Chord& c) noexcept
{
    if (! isBassIntValid (c.type, c.bassInt))
        c.bassInt = 0;
}

/** 12-bit mask of absolute pitch classes in a chord. */
inline unsigned chordMask (Chord c)
{
    unsigned m = 0;
    for (int iv : chordTypes()[(size_t) c.type].iv)
        m |= 1u << mod12 (c.root + iv);
    if (c.isSlash())
        m |= 1u << c.bassPc();
    return m;
}

/** When true, the leading tone (the major third of a V-family chord) does not
    count as outside a scale with a minor third. In a minor key the raised
    seventh belongs to the dominant (harmonic-minor practice), so V and V7 are
    not "borrowed" chords. Off until validated (GENERAL-PLAN, H9 and R11). */
inline constexpr bool kMinorLeadingToneInScale = false;

/** Number of chord tones that fall outside the scale. */
inline int tonesOutsideScale (Chord c, int tonic, int scaleIdx,
                              bool leadingToneInScale = kMinorLeadingToneInScale)
{
    const unsigned sm = scaleMask (scaleIdx);
    // The leading tone sits 11 semitones above the tonic and is the third of a
    // major-third chord whose root is a fifth above it.
    const bool exemptLeadingTone = leadingToneInScale && scaleHasMinorThird (scaleIdx)
                                   && mod12 (c.root - tonic) == 7 && hasMajorThird (c.type);
    int n = 0;
    for (int iv : chordTypes()[(size_t) c.type].iv)
        if ((sm & (1u << mod12 (c.root + iv - tonic))) == 0
            && ! (exemptLeadingTone && mod12 (c.root + iv - tonic) == 11))
            ++n;
    if (c.isSlash() && (sm & (1u << mod12 (c.bassPc() - tonic))) == 0)
    {
        bool wasChordTone = false;
        for (int iv : chordTypes()[(size_t) c.type].iv)
            if (mod12 (c.root + iv) == c.bassPc())
                wasChordTone = true;
        if (! wasChordTone)
            ++n;
    }
    return n;
}

//==============================================================================
// Line of fifths. Position of an interval relative to a reference pitch:
// ... bVI(-4) bIII(-3) bVII(-2) IV(-1) I(0) V(1) II(2) VI(3) III(4) VII(5) #IV(6)
// Moving right = "sharper/brighter", moving left = "flatter/darker".

inline int fifthsOf (int interval, bool tritoneIsFlat) noexcept
{
    static const int table[12] = { 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5 };
    const int i = mod12 (interval);
    if (i == 6)
        return tritoneIsFlat ? -6 : 6;
    return table[i];
}

/** Line-of-fifths index for the tonic's spelling (C = 0, G = 1, F = -1...),
    choosing the enharmonic that yields the simplest key signature. */
inline int tonicLineOfFifths (int tonic, int scaleIdx)
{
    const bool flat = scaleHasMinorThird (scaleIdx);
    double mean = 0.0;
    for (int pc : scales()[(size_t) scaleIdx].pcs)
        mean += fifthsOf (pc, flat);
    mean /= (double) scales()[(size_t) scaleIdx].pcs.size();
    const int shift = (int) std::lround (mean) - 2; // distance to the "relative major"

    const int base = fifthsOf (tonic, false);
    int best = base;
    int bestCost = 1000;
    for (int cand : { base, base - 12, base + 12 })
    {
        const int cost = std::abs (cand + shift) * 2 - (cand + shift > 0 ? 1 : 0); // ties -> sharps
        if (cost < bestCost)
        {
            bestCost = cost;
            best = cand;
        }
    }
    return best;
}

inline std::string noteNameFromFifths (int lof)
{
    static const char letters[] = "FCGDAEB";
    const int k = lof + 1;
    const int letter = ((k % 7) + 7) % 7;
    const int acc = (k - letter) / 7;
    std::string s (1, letters[letter]);
    for (int i = 0; i < std::abs (acc); ++i)
        s += (acc > 0 ? "#" : "b");
    return s;
}

/** Correctly spelled name of a pitch class inside a key. */
inline std::string pitchClassName (int pc, int tonic, int scaleIdx)
{
    const int lof = tonicLineOfFifths (tonic, scaleIdx)
                    + fifthsOf (pc - tonic, scaleHasMinorThird (scaleIdx));
    return noteNameFromFifths (lof);
}

inline std::string chordName (Chord c, int tonic, int scaleIdx)
{
    std::string s = pitchClassName (c.root, tonic, scaleIdx) + chordTypes()[(size_t) c.type].suffix;
    if (c.isSlash())
        s += "/" + pitchClassName (c.bassPc(), tonic, scaleIdx);
    return s;
}

inline std::string romanNumeral (Chord c, int tonic, int scaleIdx = -1)
{
    static const char* upper[12] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
    static const char* lower[12] = { "i", "bii", "ii", "biii", "iii", "iv", "#iv", "v", "bvi", "vi", "bvii", "vii" };
    const auto& t = chordTypes()[(size_t) c.type];
    const int d = mod12 (c.root - tonic);
    bool isMinor = hasMinorThird (c.type);
    if (isPower (c.type) && scaleIdx >= 0) // "5" power chord: infer quality from scale's 3rd degree
    {
        const unsigned sm = scaleMask (scaleIdx);
        const bool scaleHasMin3 = (sm & (1u << mod12 (c.root + 3 - tonic))) != 0;
        const bool scaleHasMaj3 = (sm & (1u << mod12 (c.root + 4 - tonic))) != 0;
        if (scaleHasMin3 && ! scaleHasMaj3)
            isMinor = true;
    }
    const std::string base = std::string (isMinor ? lower[d] : upper[d]);

    if (! c.isSlash())
        return base + t.romanSuffix;

    // Figured bass for standard triads and 7ths
    const int bi = mod12 (c.bassInt);
    const bool isTriad = isTriadType (c.type);
    const bool is7th = isSeventhType (c.type);

    if (isTriad)
    {
        if (bi == 3 || bi == 4)
            return base + "6";
        if (bi == 6 || bi == 7 || bi == 8)
            return base + "64";
    }
    else if (is7th)
    {
        const std::string sfx = seventhIsMajor (c.type) ? "maj" : "";
        if (bi == 3 || bi == 4)
            return base + sfx + "65";
        if (bi == 6 || bi == 7 || bi == 8)
            return base + sfx + "43";
        if (bi == 9 || bi == 10 || bi == 11)
            return base + sfx + "42";
    }

    const int bd = mod12 (c.bassPc() - tonic);
    return base + t.romanSuffix + "/" + lower[bd];
}

//==============================================================================
/** Harmonic function, used only as a light prior on chord-to-chord motion. */
enum class Function { tonic, subdominant, dominant };

inline Function functionOf (Chord c, int tonic)
{
    switch (mod12 (c.root - tonic))
    {
        case 0: case 3: case 4: case 9:  return Function::tonic;       // I, bIII, iii, vi
        case 7: case 11: case 10:        return Function::dominant;    // V, vii, bVII
        default:                         return Function::subdominant; // ii, IV, bII, bVI, #IV
    }
}

} // namespace cs
