# Corpus data and attribution

ChordSlots ships derived transition counts and smoothed transition costs. The
original annotations and archives are distributed by their respective sources.

| Source | Archive | SHA-256 | Licence |
| --- | --- | --- | --- |
| McGill Billboard via ChoCo v1.0.0 | [choco-v1.0.0.zip](https://zenodo.org/api/records/7706751/files/smashub/choco-v1.0.0.zip/content) | `cfd3531f4bc23afcede3e092a6fe50e31ee4280d7c53db33fd17ec7444a99c01` | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| RS200 timed harmonic analyses | [rs200_harmony_clt.zip](http://rockcorpus.midside.com/harmonic_analyses/rs200_harmony_clt.zip) | `796582dcb5e2a945cb1dcf5307dc193ccb5a2e574866a4c7d6958888fa2dd30a` | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| RS200 original notation (reference) | [rs200_harmony.zip](http://rockcorpus.midside.com/harmonic_analyses/rs200_harmony.zip) | `ad715fbd132380a3f6ecc686a87008d5e6d6ae8d05cf23ea09094229eabe3bab` | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |

- J. A. Burgoyne, J. Wild and I. Fujinaga, “An Expert Ground-Truth Set for Audio
  Chord Recognition and Music Analysis,” ISMIR 2011. The ChoCo metadata lists
  Billboard as CC0 and the tagged licence file lists CC BY 4.0. ChordSlots
  follows CC BY 4.0 and credits the authors. [ChoCo v1.0.0 licence](https://github.com/smashub/choco/blob/v1.0.0/LICENSE.md)
- J. de Berardinis et al., “ChoCo: a Chord Corpus and a Data Transformation
  Workflow for Musical Harmony Knowledge Graphs,” Scientific Data 10, 2023.
  ChoCo supplies the Billboard annotations in JAMS format under CC BY 4.0.
- T. de Clercq and D. Temperley, “A corpus analysis of rock harmony,” Popular
  Music 30(1), 2011. ChordSlots uses one timed analysis per song.
  [RS200 project and licence](https://rockcorpus.midside.com/)

The pipeline normalizes chords relative to the annotated tonic, groups them by
quality, collapses repeats, and aggregates transitions into smoothed cost
matrices. Billboard is the sole ChoCo partition used. RS200 comes directly from
its source to avoid counting the ChoCo copy twice.

Excluded partitions include JAAH, Chordify and Mozart Piano Sonatas (CC BY-NC-SA),
Band-in-a-Box (non-commercial), Isophonics, Robbie Williams, USPOP2002, RWC Pop,
Real Book, Jazz Corpus and Nottingham (unspecified in the handoff), Weimar
(ODbL), When in Rome (CC BY-SA), Wikifonia and iReal Pro (unclear terms).

## SHLD chord rhythms

ChordSlots adapts the `pop`, `pop2`, `soul`, and `hiphop2` timing definitions from
[SHLD Free MIDI Chord Packs](https://github.com/ldrolez/free-midi-chords/blob/95533fae2c1a2aae032211b9abed0441ac8ba298/src/chords2midi/c2mpatterns.py).
The beat durations are converted to ChordSlots steps, and the source chord
instructions are replaced with strikes on the current ChordSlots voicing.

MIT License

Copyright (c) 2019 Ludovic Drolez

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Acoustic Piano preview recordings

The plugin embeds twelve adapted Yamaha C5 recordings from **Salamander Grand Piano V3** by Alexander Holm. CC BY 3.0 Unported permits redistribution and adaptation with attribution. The original collection, source files and license are linked below. The sample attribution is also embedded in each plugin binary.

- [Original collection](https://archive.org/details/SalamanderGrandPianoV3)
- [Source FLAC recordings](https://github.com/sfzinstruments/SalamanderGrandPiano/tree/master/Samples)
- [CC BY 3.0 Unported](https://creativecommons.org/licenses/by/3.0/)
- [Sample selection and edits](plugin/assets/acoustic-piano/ATTRIBUTION.txt)
