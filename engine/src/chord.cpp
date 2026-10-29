#include "chordslots/chord.hpp"

#include <algorithm>
#include <cstddef>

#include "chordslots/scales.hpp"

namespace cs {

namespace {

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Grado limitado a 0..6, inversion y octava limitadas al rango valido.
Chord sanitized(const Chord& in) {
    Chord c = in;
    c.degree = clampInt(c.degree, 0, kDegreesPerScale - 1);
    c.inversion = clampInt(c.inversion, 0, c.seventh ? 3 : 2);
    c.octave = clampInt(c.octave, -1, 1);
    return c;
}

// Notas del acorde en posicion fundamental, sin inversion ni octava.
std::vector<int> rootPositionNotes(int tonic, ScaleType scale, const Chord& c) {
    const int degreeOffset = scaleInterval(scale, c.degree);
    const int pc = normalizePitchClass(tonic + degreeOffset);
    const int root = kRootBaseMidi + pc;

    const int count = c.seventh ? 4 : 3;
    std::vector<int> notes;
    notes.reserve(static_cast<std::size_t>(count));
    for (int k = 0; k < count; ++k) {
        // Terceras apiladas: grados d, d+2, d+4, d+6 de la escala.
        const int offset = scaleInterval(scale, c.degree + 2 * k) - degreeOffset;
        notes.push_back(root + offset);
    }
    return notes;
}

}  // namespace

const char* pitchClassName(int pitchClass) {
    static const char* const kNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return kNames[normalizePitchClass(pitchClass)];
}

std::vector<int> resolveChord(int tonic, ScaleType scale, const Chord& chord) {
    const Chord c = sanitized(chord);
    std::vector<int> notes = rootPositionNotes(tonic, scale, c);

    // Inversion n: las n notas mas graves suben una octava.
    for (int i = 0; i < c.inversion; ++i) {
        notes[static_cast<std::size_t>(i)] += 12;
    }
    for (int& n : notes) n += 12 * c.octave;

    std::sort(notes.begin(), notes.end());
    return notes;
}

std::string chordName(int tonic, ScaleType scale, const Chord& chord) {
    const Chord c = sanitized(chord);
    const std::vector<int> base = rootPositionNotes(tonic, scale, c);

    const int third = base[1] - base[0];
    const int fifth = base[2] - base[0];
    const int seventh = c.seventh ? base[3] - base[0] : -1;

    const char* quality = "?";
    if (!c.seventh) {
        if (third == 4 && fifth == 7)      quality = "";
        else if (third == 3 && fifth == 7) quality = "m";
        else if (third == 3 && fifth == 6) quality = "dim";
        else if (third == 4 && fifth == 8) quality = "aug";
    } else {
        if (third == 4 && fifth == 7 && seventh == 11)      quality = "maj7";
        else if (third == 3 && fifth == 7 && seventh == 10) quality = "m7";
        else if (third == 4 && fifth == 7 && seventh == 10) quality = "7";
        else if (third == 3 && fifth == 6 && seventh == 10) quality = "m7b5";
        else if (third == 3 && fifth == 6 && seventh == 9)  quality = "dim7";
        else if (third == 3 && fifth == 7 && seventh == 11) quality = "mMaj7";
        else if (third == 4 && fifth == 8 && seventh == 11) quality = "augMaj7";
        else if (third == 4 && fifth == 8 && seventh == 10) quality = "7#5";
    }

    std::string name = pitchClassName(base[0]);
    name += quality;

    if (c.inversion > 0) {
        // La nota del bajo es la mas grave tras invertir.
        const std::vector<int> notes = resolveChord(tonic, scale, c);
        name += "/";
        name += pitchClassName(notes.front());
    }
    return name;
}

}  // namespace cs
