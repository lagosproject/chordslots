// Tablas de escalas y conversion de grados a semitonos.
#pragma once

#include "chordslots/model.hpp"

namespace cs {

// Semitonos desde la tonica del grado `degreeIndex` de la escala.
// `degreeIndex` puede ser cualquier entero: 7 es la tonica una octava arriba
// (12 semitonos), 8 es el segundo grado una octava arriba, etc.
// Los indices negativos descienden por octavas.
int scaleInterval(ScaleType scale, int degreeIndex);

// Normaliza cualquier entero a una clase de altura 0..11.
int normalizePitchClass(int value);

}  // namespace cs
