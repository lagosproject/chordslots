#!/usr/bin/env python3
"""Build the ChordSlots transition prior from Billboard JAMS and RS200 CLT."""

import argparse
from bisect import bisect_right
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import random
import re

ROOT = Path(__file__).resolve().parents[1]
NUM_TOKENS = 60
NUM_CLUSTERS = 3
ARCHIVES = {
    "choco-v1.0.0.zip": "cfd3531f4bc23afcede3e092a6fe50e31ee4280d7c53db33fd17ec7444a99c01",
    "rs200_harmony_clt.zip": "796582dcb5e2a945cb1dcf5307dc193ccb5a2e574866a4c7d6958888fa2dd30a",
}
BILLBOARD = "choco/smashub-choco-f7dd3ee/partitions/billboard/choco/jams"
RS200 = "rs200/rs200_harmony_clt"
MAJOR, MINOR, DOMINANT, DIMINISHED, SUS_OR_AUG = range(5)
NATURAL_PCS = dict(zip("CDEFGAB", (0, 2, 4, 5, 7, 9, 11)))
DIATONIC = (0, 2, 4, 5, 7, 9, 11)
SHORTHANDS = {
    "maj": (0, 4, 7), "min": (0, 3, 7), "dim": (0, 3, 6), "aug": (0, 4, 8),
    "maj7": (0, 4, 7, 11), "min7": (0, 3, 7, 10), "7": (0, 4, 7, 10),
    "dim7": (0, 3, 6, 9), "hdim7": (0, 3, 6, 10), "minmaj7": (0, 3, 7, 11),
    "maj6": (0, 4, 7, 9), "min6": (0, 3, 7, 9), "6": (0, 4, 7, 9),
    "9": (0, 4, 7, 10, 14), "maj9": (0, 4, 7, 11, 14), "min9": (0, 3, 7, 10, 14),
    "11": (0, 4, 7, 10, 14, 17), "maj11": (0, 4, 7, 11, 14, 17),
    "min11": (0, 3, 7, 10, 14, 17), "13": (0, 4, 7, 10, 14, 17, 21),
    "maj13": (0, 4, 7, 11, 14, 17, 21), "min13": (0, 3, 7, 10, 14, 17, 21),
    "sus2": (0, 2, 7), "sus4": (0, 5, 7), "sus": (0, 5, 7),
    "5": (0, 7), "1": (0,),
}
HARTE = re.compile(r"([A-G][#b]*)(?::([a-z0-9]*)(?:\(([^()]+)\))?)?(?:/([^/]+))?", re.ASCII)
ROMAN = re.compile(r"[#b]*(VII|III|VI|IV|II|V|I|vii|iii|vi|iv|ii|v|i)([^/]*)")


def accidental(marks):
    return marks.count("#") - marks.count("b")


def pitch_class(note):
    match = re.fullmatch(r"([A-G])([#b]*)", note)
    if match is None:
        raise ValueError(note)
    letter, marks = match.groups()
    return (NATURAL_PCS[letter] + accidental(marks)) % 12


def interval(degree):
    match = re.fullmatch(r"([#b]*)([1-9]|1[0-3])", degree)
    if match is None:
        raise ValueError(degree)
    marks, number = match.groups()
    octave, step = divmod(int(number) - 1, 7)
    return 12 * octave + DIATONIC[step] + accidental(marks)


def quality(intervals):
    """Mirror Theory.h. Compound intervals retain their octave here."""
    major_third = 4 in intervals
    minor_third = 3 in intervals and not major_third
    dim_fifth = 6 in intervals
    if dim_fifth and minor_third:
        return DIMINISHED
    if (8 in intervals and major_third and 7 not in intervals
            or (2 in intervals or 5 in intervals) and not (major_third or minor_third)):
        return SUS_OR_AUG
    if major_third and 10 in intervals and not (9 in intervals and dim_fifth):
        return DOMINANT
    if minor_third:
        return MINOR
    if major_third or intervals == {0, 7}:
        return MAJOR
    return SUS_OR_AUG


def harte_chord(label):
    match = HARTE.fullmatch(label)
    if match is None:
        raise ValueError(label)
    root, shorthand, extensions, bass = match.groups()
    if bass:
        interval(bass)  # Bass describes the inversion and is outside the quality set.
    if shorthand == "1":
        return None
    tones = set(SHORTHANDS[shorthand or "maj"] if shorthand is None or shorthand else (0,))
    if extensions:
        for extension in extensions.split(","):
            extension = extension.strip()
            if extension.startswith("*"):
                tones.discard(interval(extension[1:]))
            else:
                tones.add(interval(extension))
    if len({tone % 12 for tone in tones}) < 2:
        return None
    extended = bool(tones & {10, 11} or {6, 9} <= tones or any(tone > 12 for tone in tones))
    return pitch_class(root), quality(tones), extended


def rs200_chord(label):
    match = ROMAN.fullmatch(label.split("/", 1)[0])
    if match is None:
        raise ValueError(label)
    roman, suffix = match.groups()
    parsed = re.fullmatch(r"([dohxø°a+]?)(7|65|43|42|64|6|9|11|13|2|4)?(s[24])?([#b][59])?", suffix)
    if parsed is None:
        raise ValueError(label)
    modifier, figure, suspension, alteration = parsed.groups()
    # V11 is the corpus shorthand for a IV triad over bass scale-degree 5.
    extended = figure in {"7", "65", "43", "42", "9", "11", "13"} and label != "V11"
    extended = extended or alteration in {"#9", "b9"}
    if modifier in {"h", "ø", "o", "x", "°"}:  # iih43, viio, viix7
        return DIMINISHED, extended
    if suspension or modifier in {"a", "+"}:  # Vs4, V7s4, Va65/vi
        return SUS_OR_AUG, extended
    if modifier == "d" or roman == "V" and extended:  # IVd7 or exceptional V7
        return DOMINANT, extended
    # IV7 is major seventh, iv7 is minor seventh, and I64 is a triad.
    return (MAJOR if roman.isupper() else MINOR), extended


@dataclass
class Song:
    identity: str
    minor: bool
    extended_fraction: float
    counts: Counter


def summarize(identity, events):
    counts, tokens = Counter(), Counter()
    previous = None
    extended_count = 0
    run_extended = False
    for event in events:
        if event is None:
            previous = None
            continue
        token, extended = event
        if token == previous:
            extended_count += extended and not run_extended
            run_extended = run_extended or extended
            continue
        if previous is not None:
            counts[previous, token] += 1
        tokens[token] += 1
        extended_count += extended
        run_extended = extended
        previous = token
    tonic = sum(tokens[q] for q in range(5))
    if tonic >= 3:
        minor = tokens[MINOR] > tokens[MAJOR] + tokens[DOMINANT]
    else:
        # Relative minor tonic is degree 9 relative to the annotated tonic.
        minor = sum(tokens[45 + q] for q in range(5)) > tonic
    return Song(identity, minor, extended_count / max(1, tokens.total()), counts)


def annotations(path):
    jams = json.loads(path.read_text(encoding="utf-8"))
    return {track["namespace"]: track["data"] for track in jams["annotations"]}


def billboard_events(tracks, rejected):
    keys = sorted((item["time"], pitch_class(item["value"])) for item in tracks["key_mode"])
    key_times = [time for time, _ in keys]
    for item in sorted(tracks["chord"], key=lambda chord: chord["time"]):
        label = item["value"]
        if label in ("N", "X"):
            yield None
            continue
        try:
            parsed = harte_chord(label)
        except (ValueError, KeyError):
            rejected[f"Billboard {label}"] += 1
            yield None
            continue
        key_index = bisect_right(key_times, item["time"] + 1e-7) - 1
        if parsed is None or key_index < 0:
            yield None
            continue
        root, chord_quality, extended = parsed
        tonic = keys[key_index][1]
        yield ((root - tonic) % 12) * 5 + chord_quality, extended


def rs200_events(path, rejected):
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        fields = line.split()
        if not fields:
            continue
        if len(fields) >= 3 and fields[2] == "End":
            break
        if len(fields) != 7:
            raise ValueError(f"{path}: expected seven columns")
        label = fields[2]
        try:
            degree = int(fields[3])
            if not 0 <= degree < 12:
                raise ValueError(degree)
            chord_quality, extended = rs200_chord(label)
        except ValueError:
            rejected[f"RS200 {label}"] += 1
            yield None
            continue
        yield degree * 5 + chord_quality, extended


def load_songs(data):
    rejected = Counter()
    billboard = sorted((data / BILLBOARD).glob("*.jams"))
    rs200 = {}
    for file in sorted((data / RS200).glob("*.clt")):
        name, analyst = file.stem.rsplit("_", 1)
        if analyst == "tdc" or (analyst == "dt" and name not in rs200):
            rs200[name] = file
    if len(billboard) != 890 or len(rs200) != 200:
        raise ValueError(f"Expected 890 Billboard annotations and 200 RS200 songs, found {len(billboard)} and {len(rs200)}")
    songs = []
    for file in billboard:
        tracks = annotations(file)
        # Alternate files with identical chord and key timelines stay in the same split.
        timeline = [[(item["time"], item["duration"], item["value"]) for item in tracks[namespace]]
                    for namespace in ("chord", "key_mode")]
        identity = hashlib.sha256(json.dumps(timeline).encode()).hexdigest()
        songs.append(summarize(identity, billboard_events(tracks, rejected)))
    songs.extend(summarize(name, rs200_events(file, rejected)) for name, file in sorted(rs200.items()))
    print(f"Billboard annotations: {len(billboard)}; RS200 songs: {len(rs200)}")
    print(f"Unparsed labels: {rejected.total()}; top ten: {rejected.most_common(10)}")
    return songs


def cluster_songs(songs, threshold):
    return [
        [song for song in songs if not song.minor],
        [song for song in songs if song.minor],
        [song for song in songs if song.extended_fraction >= threshold],
    ]


def total_counts(songs):
    counts = Counter()
    for song in songs:
        counts.update(song.counts)
    return counts


def scores(counts, beta):
    rows, columns = Counter(), Counter()
    for (src, dst), count in counts.items():
        rows[src] += count
        columns[dst] += count
    total = counts.total() + NUM_TOKENS * 0.5
    # Equivalent to lambda * ML + (1 - lambda) * P_uni, including empty rows.
    return [
        [-math.log((counts[src, dst] + beta * (columns[dst] + 0.5) / total)
                   / (rows[src] + beta)) for dst in range(NUM_TOKENS)]
        for src in range(NUM_TOKENS)
    ]


def choose_beta(songs, threshold):
    identities = sorted({song.identity for song in songs})
    random.Random(0).shuffle(identities)
    held_out_ids = set(identities[:len(identities) // 10])
    training_songs = [song for song in songs if song.identity not in held_out_ids]
    test_songs = [song for song in songs if song.identity in held_out_ids]
    training = [total_counts(group) for group in cluster_songs(training_songs, threshold)]
    held_out = [total_counts(group) for group in cluster_songs(test_songs, threshold)]
    results = {}
    for beta in (50, 200, 800):
        weighted_loss = 0.0
        for train, test in zip(training, held_out):
            cost = scores(train, beta)
            weighted_loss += sum(cost[src][dst] * count for (src, dst), count in test.items())
        results[beta] = weighted_loss / sum(test.total() for test in held_out)
    print("Held-out mean negative log likelihood (10% of songs, seed 0): "
          + ", ".join(f"beta {beta} = {loss:.6f}" for beta, loss in results.items()))
    return min(results, key=results.get)


def quantize(cost):
    lo = min(map(min, cost))
    hi = max(map(max, cost))
    scale = 255 / max(hi - lo, 1e-6)
    return [[round((value - lo) * scale) for value in row] for row in cost]


def generate_header(matrices, cluster_sizes, counts, beta, threshold):
    out = [
        "// ChordSlots core - Billboard and RS200 transition prior.",
        "// SPDX-License-Identifier: AGPL-3.0-or-later",
        "// Derived statistics from CC BY 4.0 sources. See DATA_LICENSES.md.",
        f"// AUTO-GENERATED by tools/build_corpus.py (SHA-256: {file_digest(Path(__file__))}).",
        "// 890 Billboard annotations (ChoCo v1.0.0) and 200 RS200 songs.",
    ]
    for archive, checksum in ARCHIVES.items():
        out.append(f"// {archive} SHA-256: {checksum}")
    for name, size, transitions in zip(("Major", "Minor", "Extended harmony"), cluster_sizes, counts):
        out.append(f"// {name}: {size} annotations, {transitions.total()} transitions.")
    out.append(f"// Extended harmony threshold: {threshold:.0%}. Interpolated backoff beta: {beta}.")
    out.append("#pragma once")
    out.append("")
    out.append('#include "Theory.h"')
    out.append("")
    out.append("namespace cs")
    out.append("{")
    out.append("")
    out.append("enum class CorpusStyle : uint8_t")
    out.append("{")
    out.append("    PopRock = 0,")
    out.append("    MinorLoops = 1,")
    out.append("    SoulRnb = 2,")
    out.append("    Free = 3")
    out.append("};")
    out.append("")
    out.append("constexpr int kNumCorpusTokens = 12 * kNumQualityClasses; // 60")
    out.append("constexpr int kNumCorpusClusters = 3;")
    out.append("")
    out.append("/** Mapping of ChordTypeDef to QualityClass derived from interval sets. */")
    out.append("inline QualityClass qualityClassOf (int type) noexcept")
    out.append("{")
    out.append("    return qualityClassFromIntervals (chordTypes()[(size_t) type].iv);")
    out.append("}")
    out.append("")
    out.append("/** Computes the 0..59 token index for a given chord relative to tonic. */")
    out.append("inline int corpusTokenOf (Chord c, int tonic) noexcept")
    out.append("{")
    out.append("    const int deg = mod12 (c.root - tonic);")
    out.append("    const int q = (int) qualityClassOf (c.type);")
    out.append("    return deg * kNumQualityClasses + q;")
    out.append("}")
    out.append("")
    out.append("// Quantized transition surprise matrices (0..255). Size: 3 x 60 x 60 = 10,800 bytes (< 11 KB)")
    out.append("alignas (16) constexpr uint8_t kCorpusMatrices[kNumCorpusClusters][kNumCorpusTokens][kNumCorpusTokens] = {")

    for c_idx in range(NUM_CLUSTERS):
        out.append("    { // Cluster " + str(c_idx))
        for src in range(NUM_TOKENS):
            row_str = "        { " + ", ".join(f"{matrices[c_idx][src][dst]:3d}" for dst in range(NUM_TOKENS)) + " },"
            out.append(row_str)
        out.append("    },")
    out.append("};")
    out.append("")

    out.append("/** Computes the normalized transition surprise cost (0.0 .. 1.0) between chords a and b. */")
    out.append("inline float corpusTransitionCost (Chord a, Chord b, int tonic, int scaleIdx, CorpusStyle style) noexcept")
    out.append("{")
    out.append("    if (style == CorpusStyle::Free)")
    out.append("        return 0.5f; // Neutral unweighted prior")
    out.append("")
    out.append("    const int cluster = std::clamp ((int) style, 0, kNumCorpusClusters - 1);")
    out.append("    const int tA = corpusTokenOf (a, tonic);")
    out.append("    const int tB = corpusTokenOf (b, tonic);")
    out.append("    const uint8_t raw = kCorpusMatrices[cluster][tA][tB];")
    out.append("    float cost = (float) raw / 255.0f;")
    out.append("")
    out.append("    // Diatonic mode bonus: reward chords that are diatonic in the active scale (e.g. bVII in Mixolydian, IV in Dorian)")
    out.append("    const int outB = tonesOutsideScale (b, tonic, scaleIdx);")
    out.append("    if (outB == 0)")
    out.append("        cost *= 0.85f;")
    out.append("    else if (outB >= 2)")
    out.append("        cost = std::min (1.0f, cost + 0.15f);")
    out.append("")
    out.append("    return cost;")
    out.append("}")
    out.append("")
    out.append("} // namespace cs")
    out.append("")

    return "\n".join(out)

def file_digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as archive:
        for chunk in iter(lambda: archive.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def generate_fixture(counts, beta):
    theory = (ROOT / "core/include/chordslots/Theory.h").read_text(encoding="utf-8")
    table = theory.split("static const std::vector<ChordTypeDef> t = {", 1)[1].split("};", 1)[0]
    intervals = re.findall(r"\{\s*([0-9, ]+)\s*\}", table)
    values = [quality({int(x) for x in row.split(",")}) for row in intervals]
    if len(values) != 32:
        raise ValueError(f"Expected 32 chord types, found {len(values)}")
    lines = [
        "// Generated by tools/build_corpus.py from Theory.h and the corpus counts.",
        "#pragma once",
        "",
        "constexpr int kPythonQualityClasses[] = { " + ", ".join(map(str, values)) + " };",
        f"constexpr int kCorpusFixtureBeta = {beta};",
        "struct CorpusFixtureCount { int cluster, source, destination, count; };",
        "constexpr CorpusFixtureCount kCorpusFixtureCounts[] = {",
    ]
    for cluster, transitions in enumerate(counts):
        for (source, destination), count in sorted(transitions.items()):
            lines.append(f"    {{ {cluster}, {source}, {destination}, {count} }},")
    return "\n".join(lines + ["};", ""])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--out", type=Path, default=ROOT / "core/include/chordslots/Corpus.h")
    parser.add_argument("--counts-out", type=Path, default=ROOT / "tests/corpus_counts.json")
    parser.add_argument("--fixture-out", type=Path, default=ROOT / "tests/p4_corpus_fixture.h")
    parser.add_argument("--beta", type=int, choices=(50, 200, 800))
    args = parser.parse_args()
    if args.data is None:
        if args.verify:
            print("corpus data not available, skipping")
            return
        parser.error("--data is required to generate the prior")
    try:
        for archive, expected in ARCHIVES.items():
            if file_digest(args.data / "raw" / archive) != expected:
                raise ValueError(f"SHA-256 mismatch for {archive}")
        songs = load_songs(args.data)
        threshold = 0.30
        groups = cluster_songs(songs, threshold)
        if len(groups[2]) < 40:
            threshold = 0.25
            groups = cluster_songs(songs, threshold)
            print("Extended harmony threshold lowered to 25%")
        counts = [total_counts(group) for group in groups]
        sizes = [len(group) for group in groups]
        for name, size, transitions in zip(("Major", "Minor", "Extended harmony"), sizes, counts):
            print(f"{name}: {size} annotations, {transitions.total()} transitions")
        best_beta = choose_beta(songs, threshold)
        beta = args.beta if args.beta is not None else best_beta
        print(f"Using beta {beta}")
        matrices = [quantize(scores(cluster, beta)) for cluster in counts]
        sparse_counts = [
            {f"{src}->{dst}": count for (src, dst), count in sorted(cluster.items())}
            for cluster in counts
        ]
        outputs = {
            args.out: generate_header(matrices, sizes, counts, beta, threshold),
            args.counts_out: json.dumps(sparse_counts, indent=2) + "\n",
            args.fixture_out: generate_fixture(counts, beta),
        }
        for path, content in outputs.items():
            if args.verify:
                if path.read_text(encoding="utf-8") != content:
                    raise ValueError(f"Generated output differs from {path}")
                print(f"Verified {path}")
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(1, f"Corpus error: {error}\n")


if __name__ == "__main__":
    main()
