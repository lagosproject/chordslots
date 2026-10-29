// Operaciones sobre progresiones: longitud, validacion y valores por defecto.
#pragma once

#include <cstdint>
#include <vector>

#include "chordslots/model.hpp"

namespace cs {

// Longitud total de la progresion en ticks (suma de las duraciones).
// Usa 64 bits para que sumar 16 huecos nunca desborde.
std::int64_t totalTicks(const Progression& prog);

// Duracion de un compas en ticks, a partir de los beats por compas del host.
// Devuelve 0 si beatsPerBar < 1.
int barTicks(int beatsPerBar);

// ---- Validacion ------------------------------------------------------------

enum class IssueCode {
    TooFewSlots,
    TooManySlots,
    DurationNotPositive,
    DurationNotWholeBeat,
    DegreeOutOfRange,
    InversionOutOfRange,
    OctaveOutOfRange,
    TonicOutOfRange,
    StartOffsetNegative,
    NoDegreesAllowed,
    DegreeMaskOutOfRange,
    BeatRangeInvalid,
};

struct Issue {
    IssueCode code;
    int       slotIndex;  // -1 si el problema no es de un hueco concreto
};

// Devuelve la lista de problemas (vacia si es valida). No lanza excepciones.
std::vector<Issue> validate(const Progression& prog);
std::vector<Issue> validate(const Settings& settings);

// ---- Valores por defecto ---------------------------------------------------

// Cuatro huecos de 2 compases (8 beats): grados i, VI, III, VII (0, 5, 2, 6),
// triadas, sin candados.
Progression makeDefaultProgression();

// La menor natural, cuatro variaciones iguales a la progresion por defecto.
PluginState makeDefaultState();

}  // namespace cs
