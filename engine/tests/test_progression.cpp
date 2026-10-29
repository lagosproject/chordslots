#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "chordslots/progression.hpp"

using namespace cs;

namespace {

bool hasIssue(const std::vector<Issue>& issues, IssueCode code, int slot = -2) {
    return std::any_of(issues.begin(), issues.end(), [&](const Issue& i) {
        return i.code == code && (slot == -2 || i.slotIndex == slot);
    });
}

Progression withSlots(int n, int beatsEach = 4) {
    Progression p;
    for (int i = 0; i < n; ++i) {
        Slot s;
        s.durationTicks = beatsEach * kTicksPerQuarter;
        p.slots.push_back(s);
    }
    return p;
}

}  // namespace

// ---- Longitud --------------------------------------------------------------

TEST_CASE("totalTicks: suma de las duraciones", "[progression]") {
    // El ejemplo del usuario: 3, 5 y 2 compases de 4/4 = 40 beats.
    Progression p;
    for (int bars : {3, 5, 2}) {
        Slot s;
        s.durationTicks = bars * 4 * kTicksPerQuarter;
        p.slots.push_back(s);
    }
    REQUIRE(totalTicks(p) == 10 * 4 * kTicksPerQuarter);
}

TEST_CASE("totalTicks: progresion vacia", "[progression]") {
    REQUIRE(totalTicks(Progression{}) == 0);
}

TEST_CASE("totalTicks: no desborda con 16 huecos largos", "[progression]") {
    Progression p = withSlots(16, 1000000);
    REQUIRE(totalTicks(p) == static_cast<std::int64_t>(16) * 1000000 * kTicksPerQuarter);
}

TEST_CASE("barTicks", "[progression]") {
    REQUIRE(barTicks(4) == 1920);
    REQUIRE(barTicks(3) == 1440);
    REQUIRE(barTicks(7) == 7 * 480);
    REQUIRE(barTicks(0) == 0);
    REQUIRE(barTicks(-2) == 0);
}

// ---- Validacion de progresiones -------------------------------------------

TEST_CASE("validate: la progresion por defecto es valida", "[validate]") {
    REQUIRE(validate(makeDefaultProgression()).empty());
}

TEST_CASE("validate: numero de huecos", "[validate]") {
    REQUIRE(hasIssue(validate(Progression{}), IssueCode::TooFewSlots));
    REQUIRE(validate(withSlots(1)).empty());
    REQUIRE(validate(withSlots(16)).empty());
    REQUIRE(hasIssue(validate(withSlots(17)), IssueCode::TooManySlots));
}

TEST_CASE("validate: duraciones", "[validate]") {
    Progression p = withSlots(4);

    p.slots[1].durationTicks = 0;
    p.slots[2].durationTicks = -480;
    p.slots[3].durationTicks = 481;  // no es multiplo de un beat

    const auto issues = validate(p);
    REQUIRE_FALSE(hasIssue(issues, IssueCode::DurationNotPositive, 0));
    REQUIRE(hasIssue(issues, IssueCode::DurationNotPositive, 1));
    REQUIRE(hasIssue(issues, IssueCode::DurationNotPositive, 2));
    REQUIRE(hasIssue(issues, IssueCode::DurationNotWholeBeat, 3));
}

TEST_CASE("validate: acordes", "[validate]") {
    Progression p = withSlots(5);

    p.slots[0].chord.degree = 7;
    p.slots[1].chord.degree = -1;
    p.slots[2].chord.inversion = 3;     // triada: maximo 2
    p.slots[3].chord.octave = 2;
    p.slots[4].chord.seventh = true;
    p.slots[4].chord.inversion = 3;     // septima: 3 es valido

    const auto issues = validate(p);
    REQUIRE(hasIssue(issues, IssueCode::DegreeOutOfRange, 0));
    REQUIRE(hasIssue(issues, IssueCode::DegreeOutOfRange, 1));
    REQUIRE(hasIssue(issues, IssueCode::InversionOutOfRange, 2));
    REQUIRE(hasIssue(issues, IssueCode::OctaveOutOfRange, 3));
    REQUIRE_FALSE(hasIssue(issues, IssueCode::InversionOutOfRange, 4));
}

// ---- Validacion de ajustes -------------------------------------------------

TEST_CASE("validate(Settings): valores por defecto validos", "[validate]") {
    REQUIRE(validate(Settings{}).empty());
}

TEST_CASE("validate(Settings): tonica y desplazamiento", "[validate]") {
    Settings s;
    s.tonic = 12;
    REQUIRE(hasIssue(validate(s), IssueCode::TonicOutOfRange));
    s.tonic = -1;
    REQUIRE(hasIssue(validate(s), IssueCode::TonicOutOfRange));
    s.tonic = 0;
    s.startOffsetBars = -1;
    REQUIRE(hasIssue(validate(s), IssueCode::StartOffsetNegative));
}

TEST_CASE("validate(Settings): mascara de grados", "[validate]") {
    Settings s;
    s.allowedDegreesMask = 0;
    REQUIRE(hasIssue(validate(s), IssueCode::NoDegreesAllowed));
    s.allowedDegreesMask = 0x80;
    REQUIRE(hasIssue(validate(s), IssueCode::DegreeMaskOutOfRange));
    s.allowedDegreesMask = 0x01;  // solo el grado I
    REQUIRE(validate(s).empty());
}

TEST_CASE("validate(Settings): rango de duraciones al regenerar", "[validate]") {
    Settings s;
    s.minBeats = 0;
    REQUIRE(hasIssue(validate(s), IssueCode::BeatRangeInvalid));
    s.minBeats = 8;
    s.maxBeats = 4;
    REQUIRE(hasIssue(validate(s), IssueCode::BeatRangeInvalid));
    s.maxBeats = 8;
    REQUIRE(validate(s).empty());
}

// ---- Valores por defecto ---------------------------------------------------

TEST_CASE("makeDefaultProgression: cuatro huecos de 2 compases i-VI-III-VII", "[defaults]") {
    const Progression p = makeDefaultProgression();
    REQUIRE(p.slots.size() == 4);
    const int degrees[4] = {0, 5, 2, 6};
    for (std::size_t i = 0; i < 4; ++i) {
        REQUIRE(p.slots[i].chord.degree == degrees[i]);
        REQUIRE(p.slots[i].chord.seventh == false);
        REQUIRE(p.slots[i].durationTicks == 8 * kTicksPerQuarter);
        REQUIRE(p.slots[i].lockDuration == false);
        REQUIRE(p.slots[i].lockChord == false);
    }
    REQUIRE(totalTicks(p) == 32 * kTicksPerQuarter);
}

TEST_CASE("makeDefaultState: cuatro variaciones iguales y validas", "[defaults]") {
    const PluginState st = makeDefaultState();
    REQUIRE(st.schemaVersion == 1);
    REQUIRE(st.activeVariation == 0);
    REQUIRE(st.settings.tonic == 9);
    REQUIRE(st.settings.scale == ScaleType::NaturalMinor);
    REQUIRE(validate(st.settings).empty());

    for (int i = 0; i < kNumVariations; ++i) {
        REQUIRE(validate(st.variations[i]).empty());
        REQUIRE(st.variations[i].slots.size() == st.variations[0].slots.size());
        REQUIRE(totalTicks(st.variations[i]) == totalTicks(st.variations[0]));
    }
}
