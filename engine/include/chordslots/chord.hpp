// Resolucion de acordes a notas MIDI y nombres legibles.
#pragma once

#include <string>
#include <vector>

#include "chordslots/model.hpp"

namespace cs {

// Registro: la fundamental de cada acorde (antes de inversiones y octava)
// cae en MIDI 48..59 (C3..B3).
constexpr int kRootBaseMidi = 48;

// Devuelve las notas MIDI del acorde, ordenadas de grave a agudo.
//  - Apila terceras sobre el grado: 1-3-5 (y 7 si chord.seventh).
//  - La inversion n sube una octava las n notas mas graves.
//  - chord.octave desplaza todo +-12 por unidad.
// Valores fuera de rango (grado, inversion, octava) se limitan al rango valido;
// `tonic` se normaliza a 0..11. No lanza excepciones.
std::vector<int> resolveChord(int tonic, ScaleType scale, const Chord& chord);

// Nombre del acorde, p. ej. "Am7", "Fmaj7", "Bdim", "C/E".
// Usa sostenidos para los nombres de nota (sin bemoles en el MVP).
std::string chordName(int tonic, ScaleType scale, const Chord& chord);

// Nombre de una clase de altura (0..11), p. ej. 1 -> "C#".
const char* pitchClassName(int pitchClass);

}  // namespace cs
