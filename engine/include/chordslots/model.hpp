// Modelo de datos del motor. Sin dependencias (solo la libreria estandar).
// Ver seccion 4 de la especificacion del MVP.
#pragma once

#include <cstdint>
#include <vector>

namespace cs {

// ---- Constantes ------------------------------------------------------------

constexpr int kTicksPerQuarter = 480;   // 1 beat (negra) = 480 ticks
constexpr int kMinSlots        = 1;
constexpr int kMaxSlots        = 16;
constexpr int kNumVariations   = 4;     // A..D
constexpr int kDegreesPerScale = 7;

// ---- Escalas ---------------------------------------------------------------

enum class ScaleType {
    Major,
    NaturalMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    HarmonicMinor,
};

constexpr int kNumScaleTypes = 7;

// ---- Acordes y huecos ------------------------------------------------------

// Acorde diatonico (siempre dentro de la escala en el MVP).
struct Chord {
    int  degree    = 0;      // 0..6 (I..VII)
    bool seventh   = false;  // false: triada, true: septima
    int  inversion = 0;      // 0..2 en triadas, 0..3 en septimas
    int  octave    = 0;      // desplazamiento de octava, -1..+1
};

struct Slot {
    int   durationTicks = 4 * kTicksPerQuarter;  // multiplo de 480 en el MVP
    Chord chord;
    bool  lockDuration = false;
    bool  lockChord    = false;
};

// Una variacion (A..D).
struct Progression {
    std::vector<Slot> slots;
    std::uint32_t     seed = 1;
};

// ---- Ajustes ---------------------------------------------------------------

struct Settings {
    int       tonic = 9;                          // 0..11 (C=0). Por defecto La.
    ScaleType scale = ScaleType::NaturalMinor;
    int       startOffsetBars = 0;                // desplazamiento de arranque del bucle

    // Opciones de randomize (se usan a partir del hito M2).
    std::uint8_t allowedDegreesMask = 0x7F;       // 7 bits, todos permitidos
    bool         useSevenths = false;
    bool         randomizeInversions = false;
    int          minBeats = 4;                    // rango de duracion al regenerar
    int          maxBeats = 16;
};

enum class SwitchQuantize {
    Immediate,
    NextBar,
    NextChord,
    EndOfLoop,
};

// Estado completo del plugin (lo que se guarda en el proyecto de la DAW).
struct PluginState {
    int            schemaVersion = 1;
    Settings       settings;
    Progression    variations[kNumVariations];
    int            activeVariation = 0;           // 0..3
    SwitchQuantize quantize = SwitchQuantize::NextBar;
};

}  // namespace cs
