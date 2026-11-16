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
