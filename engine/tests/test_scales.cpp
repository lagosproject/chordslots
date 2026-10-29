#include <array>

#include <catch2/catch_test_macros.hpp>

#include "chordslots/scales.hpp"

using namespace cs;

TEST_CASE("scaleInterval: intervalos de cada escala", "[scales]") {
    using Row = std::array<int, 7>;

    struct Case {
        ScaleType scale;
        Row       expected;
    };
    const Case cases[] = {
        {ScaleType::Major,         {0, 2, 4, 5, 7, 9, 11}},
        {ScaleType::NaturalMinor,  {0, 2, 3, 5, 7, 8, 10}},
        {ScaleType::Dorian,        {0, 2, 3, 5, 7, 9, 10}},
        {ScaleType::Phrygian,      {0, 1, 3, 5, 7, 8, 10}},
        {ScaleType::Lydian,        {0, 2, 4, 6, 7, 9, 11}},
        {ScaleType::Mixolydian,    {0, 2, 4, 5, 7, 9, 10}},
        {ScaleType::HarmonicMinor, {0, 2, 3, 5, 7, 8, 11}},
    };

    for (const Case& c : cases) {
        for (int d = 0; d < 7; ++d) {
            INFO("escala " << static_cast<int>(c.scale) << ", grado " << d);
            REQUIRE(scaleInterval(c.scale, d) == c.expected[static_cast<std::size_t>(d)]);
        }
    }
}

TEST_CASE("scaleInterval: indices por encima de una octava", "[scales]") {
    // El grado 7 es la tonica una octava arriba.
    REQUIRE(scaleInterval(ScaleType::Major, 7) == 12);
    REQUIRE(scaleInterval(ScaleType::Major, 8) == 14);
    REQUIRE(scaleInterval(ScaleType::Major, 10) == 17);   // 4.o grado + 12
    REQUIRE(scaleInterval(ScaleType::NaturalMinor, 12) == 20);
    REQUIRE(scaleInterval(ScaleType::Major, 14) == 24);   // dos octavas
}

TEST_CASE("scaleInterval: indices negativos bajan por octavas", "[scales]") {
    REQUIRE(scaleInterval(ScaleType::Major, -1) == -1);   // 7.o grado, octava abajo
    REQUIRE(scaleInterval(ScaleType::Major, -7) == -12);
    REQUIRE(scaleInterval(ScaleType::Major, -8) == -13);
}

TEST_CASE("normalizePitchClass", "[scales]") {
    REQUIRE(normalizePitchClass(0) == 0);
    REQUIRE(normalizePitchClass(11) == 11);
    REQUIRE(normalizePitchClass(12) == 0);
    REQUIRE(normalizePitchClass(25) == 1);
    REQUIRE(normalizePitchClass(-1) == 11);
    REQUIRE(normalizePitchClass(-13) == 11);
}
