#include "chordslots/scales.hpp"

#include <array>
#include <cstddef>

namespace cs {

namespace {

using Intervals = std::array<int, kDegreesPerScale>;

constexpr Intervals kMajor         {0, 2, 4, 5, 7, 9, 11};
constexpr Intervals kNaturalMinor  {0, 2, 3, 5, 7, 8, 10};
constexpr Intervals kDorian        {0, 2, 3, 5, 7, 9, 10};
constexpr Intervals kPhrygian      {0, 1, 3, 5, 7, 8, 10};
constexpr Intervals kLydian        {0, 2, 4, 6, 7, 9, 11};
constexpr Intervals kMixolydian    {0, 2, 4, 5, 7, 9, 10};
constexpr Intervals kHarmonicMinor {0, 2, 3, 5, 7, 8, 11};

const Intervals& intervalsFor(ScaleType scale) {
    switch (scale) {
        case ScaleType::Major:         return kMajor;
        case ScaleType::NaturalMinor:  return kNaturalMinor;
        case ScaleType::Dorian:        return kDorian;
        case ScaleType::Phrygian:      return kPhrygian;
        case ScaleType::Lydian:        return kLydian;
        case ScaleType::Mixolydian:    return kMixolydian;
        case ScaleType::HarmonicMinor: return kHarmonicMinor;
    }
    return kMajor;  // valor fuera del enum: no deberia ocurrir
}

// Division entera con redondeo hacia abajo (valida con negativos).
int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

}  // namespace

int normalizePitchClass(int value) {
    const int r = value % 12;
    return r < 0 ? r + 12 : r;
}

int scaleInterval(ScaleType scale, int degreeIndex) {
    const int octave = floorDiv(degreeIndex, kDegreesPerScale);
    const int degree = degreeIndex - octave * kDegreesPerScale;  // 0..6
    return intervalsFor(scale)[static_cast<std::size_t>(degree)] + 12 * octave;
}

}  // namespace cs
