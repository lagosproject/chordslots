#include <algorithm>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "chordslots/chord.hpp"

using namespace cs;
using Notes = std::vector<int>;

namespace {

Chord triad(int degree, int inversion = 0, int octave = 0) {
    Chord c;
    c.degree = degree;
    c.seventh = false;
    c.inversion = inversion;
    c.octave = octave;
    return c;
}

Chord seventh(int degree, int inversion = 0, int octave = 0) {
    Chord c = triad(degree, inversion, octave);
    c.seventh = true;
    return c;
}

constexpr int C = 0, D = 2, G = 7, A = 9, B = 11;

}  // namespace

// ---- Notas MIDI ------------------------------------------------------------

TEST_CASE("resolveChord: triadas en Do mayor", "[chord]") {
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0)) == Notes{48, 52, 55});  // C E G
    REQUIRE(resolveChord(C, ScaleType::Major, triad(1)) == Notes{50, 53, 57});  // D F A
    REQUIRE(resolveChord(C, ScaleType::Major, triad(4)) == Notes{55, 59, 62});  // G B D
    REQUIRE(resolveChord(C, ScaleType::Major, triad(6)) == Notes{59, 62, 65});  // B D F
}

TEST_CASE("resolveChord: septimas en Do mayor", "[chord]") {
    REQUIRE(resolveChord(C, ScaleType::Major, seventh(0)) == Notes{48, 52, 55, 59});  // Cmaj7
    REQUIRE(resolveChord(C, ScaleType::Major, seventh(4)) == Notes{55, 59, 62, 65});  // G7
    REQUIRE(resolveChord(C, ScaleType::Major, seventh(6)) == Notes{59, 62, 65, 69});  // Bm7b5
}

TEST_CASE("resolveChord: La menor natural", "[chord]") {
    REQUIRE(resolveChord(A, ScaleType::NaturalMinor, triad(0)) == Notes{57, 60, 64});  // Am
    REQUIRE(resolveChord(A, ScaleType::NaturalMinor, triad(5)) == Notes{53, 57, 60});  // F
    REQUIRE(resolveChord(A, ScaleType::NaturalMinor, seventh(0)) == Notes{57, 60, 64, 67});  // Am7
    REQUIRE(resolveChord(A, ScaleType::NaturalMinor, seventh(5)) == Notes{53, 57, 60, 64});  // Fmaj7
}

TEST_CASE("resolveChord: otras tonicas", "[chord]") {
    REQUIRE(resolveChord(G, ScaleType::Major, triad(0)) == Notes{55, 59, 62});   // G B D
    REQUIRE(resolveChord(B, ScaleType::Major, triad(0)) == Notes{59, 63, 66});   // B D# F#
}

TEST_CASE("resolveChord: tonica fuera de rango se normaliza", "[chord]") {
    REQUIRE(resolveChord(12, ScaleType::Major, triad(0)) == resolveChord(0, ScaleType::Major, triad(0)));
    REQUIRE(resolveChord(-1, ScaleType::Major, triad(0)) == resolveChord(11, ScaleType::Major, triad(0)));
}

TEST_CASE("resolveChord: la fundamental cae siempre en MIDI 48..59", "[chord]") {
    for (int tonic = 0; tonic < 12; ++tonic) {
        for (int s = 0; s < kNumScaleTypes; ++s) {
            for (int d = 0; d < 7; ++d) {
                const Notes n = resolveChord(tonic, static_cast<ScaleType>(s), triad(d));
                REQUIRE(n.front() >= kRootBaseMidi);
                REQUIRE(n.front() <= kRootBaseMidi + 11);
            }
        }
    }
}

TEST_CASE("resolveChord: salida ordenada y con el numero de notas correcto", "[chord]") {
    for (int s = 0; s < kNumScaleTypes; ++s) {
        for (int d = 0; d < 7; ++d) {
            for (int inv = 0; inv <= 3; ++inv) {
                const Notes t = resolveChord(5, static_cast<ScaleType>(s), triad(d, inv));
                const Notes q = resolveChord(5, static_cast<ScaleType>(s), seventh(d, inv));
                REQUIRE(t.size() == 3);
                REQUIRE(q.size() == 4);
                REQUIRE(std::is_sorted(t.begin(), t.end()));
                REQUIRE(std::is_sorted(q.begin(), q.end()));
            }
        }
    }
}

// ---- Inversiones -----------------------------------------------------------

TEST_CASE("resolveChord: inversiones de triada", "[chord][inversion]") {
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 0)) == Notes{48, 52, 55});
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 1)) == Notes{52, 55, 60});  // E G C
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 2)) == Notes{55, 60, 64});  // G C E
}

TEST_CASE("resolveChord: inversiones de septima", "[chord][inversion]") {
    REQUIRE(resolveChord(C, ScaleType::Major, seventh(4, 1)) == Notes{59, 62, 65, 67});  // B D F G
    REQUIRE(resolveChord(C, ScaleType::Major, seventh(4, 2)) == Notes{62, 65, 67, 71});  // D F G B
    REQUIRE(resolveChord(C, ScaleType::Major, seventh(4, 3)) == Notes{65, 67, 71, 74});  // F G B D
}

TEST_CASE("resolveChord: inversion fuera de rango se limita", "[chord][inversion]") {
    // Una triada solo admite 0..2; 3 y 99 se tratan como 2, -5 como 0.
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 3)) == resolveChord(C, ScaleType::Major, triad(0, 2)));
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 99)) == resolveChord(C, ScaleType::Major, triad(0, 2)));
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, -5)) == resolveChord(C, ScaleType::Major, triad(0, 0)));
    // Una septima admite hasta 3.
    REQUIRE(resolveChord(C, ScaleType::Major, seventh(0, 9)) == resolveChord(C, ScaleType::Major, seventh(0, 3)));
}

// ---- Octava y limites ------------------------------------------------------

TEST_CASE("resolveChord: desplazamiento de octava", "[chord][octave]") {
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 0, +1)) == Notes{60, 64, 67});
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 0, -1)) == Notes{36, 40, 43});
}

TEST_CASE("resolveChord: octava fuera de rango se limita", "[chord][octave]") {
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 0, 5)) == resolveChord(C, ScaleType::Major, triad(0, 0, 1)));
    REQUIRE(resolveChord(C, ScaleType::Major, triad(0, 0, -5)) == resolveChord(C, ScaleType::Major, triad(0, 0, -1)));
}

TEST_CASE("resolveChord: grado fuera de rango se limita", "[chord]") {
    REQUIRE(resolveChord(C, ScaleType::Major, triad(7)) == resolveChord(C, ScaleType::Major, triad(6)));
    REQUIRE(resolveChord(C, ScaleType::Major, triad(-3)) == resolveChord(C, ScaleType::Major, triad(0)));
}

TEST_CASE("resolveChord: todas las notas son MIDI validas en el peor caso", "[chord]") {
    for (int tonic = 0; tonic < 12; ++tonic) {
        for (int s = 0; s < kNumScaleTypes; ++s) {
            for (int d = 0; d < 7; ++d) {
                for (int inv = 0; inv <= 3; ++inv) {
                    for (int oct = -1; oct <= 1; ++oct) {
                        for (bool sev : {false, true}) {
                            Chord c;
                            c.degree = d;
                            c.seventh = sev;
                            c.inversion = inv;
                            c.octave = oct;
                            for (int n : resolveChord(tonic, static_cast<ScaleType>(s), c)) {
                                REQUIRE(n >= 0);
                                REQUIRE(n <= 127);
                            }
                        }
                    }
                }
            }
        }
    }
}

// ---- Nombres ---------------------------------------------------------------

TEST_CASE("chordName: triadas diatonicas en Do mayor", "[chord][name]") {
    const char* expected[7] = {"C", "Dm", "Em", "F", "G", "Am", "Bdim"};
    for (int d = 0; d < 7; ++d) {
        INFO("grado " << d);
        REQUIRE(chordName(C, ScaleType::Major, triad(d)) == expected[d]);
    }
}

TEST_CASE("chordName: septimas diatonicas en Do mayor", "[chord][name]") {
    const char* expected[7] = {"Cmaj7", "Dm7", "Em7", "Fmaj7", "G7", "Am7", "Bm7b5"};
    for (int d = 0; d < 7; ++d) {
        INFO("grado " << d);
        REQUIRE(chordName(C, ScaleType::Major, seventh(d)) == expected[d]);
    }
}

TEST_CASE("chordName: progresion por defecto (La menor)", "[chord][name]") {
    REQUIRE(chordName(A, ScaleType::NaturalMinor, triad(0)) == "Am");
    REQUIRE(chordName(A, ScaleType::NaturalMinor, triad(5)) == "F");
    REQUIRE(chordName(A, ScaleType::NaturalMinor, triad(2)) == "C");
    REQUIRE(chordName(A, ScaleType::NaturalMinor, triad(6)) == "G");
}

TEST_CASE("chordName: calidades menos comunes", "[chord][name]") {
    // Menor armonica: Am(maj7) y Caug
    REQUIRE(chordName(A, ScaleType::HarmonicMinor, seventh(0)) == "AmMaj7");
    REQUIRE(chordName(A, ScaleType::HarmonicMinor, triad(2)) == "Caug");
    // Re dorico: IV7 = G7
    REQUIRE(chordName(D, ScaleType::Dorian, seventh(3)) == "G7");
}

TEST_CASE("chordName: inversiones con bajo", "[chord][name]") {
    REQUIRE(chordName(C, ScaleType::Major, triad(0, 1)) == "C/E");
    REQUIRE(chordName(C, ScaleType::Major, triad(0, 2)) == "C/G");
    REQUIRE(chordName(C, ScaleType::Major, seventh(4, 3)) == "G7/F");
}

TEST_CASE("chordName: usa sostenidos", "[chord][name]") {
    REQUIRE(chordName(1, ScaleType::Major, triad(0)) == "C#");
    REQUIRE(std::string(pitchClassName(10)) == "A#");
    REQUIRE(std::string(pitchClassName(-1)) == "B");
}
