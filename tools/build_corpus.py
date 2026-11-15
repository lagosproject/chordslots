#!/usr/bin/env python3
"""
tools/build_corpus.py: Hand-authored style prior (provisional): transition weights
written by hand from common pop/rock, minor-loop and soul/R&B practice. Not derived
from a corpus yet; replacement with licence-clean corpus data is planned (IMPLEMENTATION_PLAN.md, P4).

Generates core/include/chordslots/Corpus.h with quantized (uint8_t) bigram surprise/PMI matrices.
Usage:
    python3 tools/build_corpus.py [--verify] [--out path/to/Corpus.h]
"""

import sys
import os
import math
import hashlib
from typing import Dict, Tuple, List

# Tokenization:
# Token = degree * 5 + qualityClass
# degree: 0..11 relative to tonic
# qualityClass:
#   0 = Major (triad, maj7, 6, add9, maj9, 5)
#   1 = Minor (triad, m7, m6, madd9, m9, mMaj7)
#   2 = Dominant (7, 9)
#   3 = Diminished (dim, dim7, m7b5)
#   4 = SusOrAug (sus2, sus4, 7sus4, aug)

NUM_DEGREES = 12
NUM_QUALITIES = 5
NUM_TOKENS = NUM_DEGREES * NUM_QUALITIES  # 60 tokens

# 3 Style Clusters:
# 0: PopRock (Major tonality oriented: I, IV, V, vi, bVII, bVI, bIII, ii, iv)
# 1: MinorLoops (Minor/Aeolian tonality oriented: i, bVI, bVII, iv, v/V, bIII, ii°)
# 2: SoulRnb (Jazz/Soul tonality oriented: ii->V, iii->vi, IV, I, secondary dominants)
NUM_CLUSTERS = 3

def token(deg: int, qual: int) -> int:
    return (deg % 12) * NUM_QUALITIES + qual

def build_cluster_transitions() -> List[Dict[Tuple[int, int], float]]:
    # Hand-authored style prior (provisional): transition weights written by hand from
    # common pop/rock, minor-loop and soul/R&B practice. Not derived from a corpus yet;
    # replacement with licence-clean corpus data is planned (IMPLEMENTATION_PLAN.md, P4).
    clusters: List[Dict[Tuple[int, int], float]] = [{} for _ in range(NUM_CLUSTERS)]

    # --------------------------------------------------------------------------
    # Cluster 0: PopRock (Ionian / Mixolydian / Rock loops)
    # --------------------------------------------------------------------------
    c0 = clusters[0]
    # I (0, 0)
    c0[(token(0, 0), token(5, 0))] = 550.0  # I -> IV
    c0[(token(0, 0), token(7, 0))] = 510.0  # I -> V
    c0[(token(0, 0), token(7, 2))] = 220.0  # I -> V7
    c0[(token(0, 0), token(9, 1))] = 380.0  # I -> vi
    c0[(token(0, 0), token(2, 1))] = 190.0  # I -> ii
    c0[(token(0, 0), token(10, 0))] = 160.0 # I -> bVII
    c0[(token(0, 0), token(4, 1))] = 120.0  # I -> iii
    c0[(token(0, 0), token(8, 0))] = 75.0   # I -> bVI
    c0[(token(0, 0), token(3, 0))] = 55.0   # I -> bIII
    c0[(token(0, 0), token(5, 1))] = 50.0   # I -> iv
    
    # IV (5, 0)
    c0[(token(5, 0), token(0, 0))] = 680.0  # IV -> I (plagal)
    c0[(token(5, 0), token(7, 0))] = 540.0  # IV -> V
    c0[(token(5, 0), token(7, 2))] = 180.0  # IV -> V7
    c0[(token(5, 0), token(9, 1))] = 190.0  # IV -> vi
    c0[(token(5, 0), token(2, 1))] = 110.0  # IV -> ii
    c0[(token(5, 0), token(10, 0))] = 80.0  # IV -> bVII
    c0[(token(5, 0), token(5, 1))] = 50.0   # IV -> iv (minor plagal shift)

    # V (7, 0 and 7, 2)
    for qv in [0, 2]:
        c0[(token(7, qv), token(0, 0))] = 740.0  # V -> I (authentic)
        c0[(token(7, qv), token(5, 0))] = 430.0  # V -> IV (rock/pop cadence)
        c0[(token(7, qv), token(9, 1))] = 330.0  # V -> vi (deceptive cadence)
        c0[(token(7, qv), token(2, 1))] = 90.0   # V -> ii
        c0[(token(7, qv), token(4, 1))] = 60.0   # V -> iii

    # vi (9, 1)
    c0[(token(9, 1), token(5, 0))] = 520.0  # vi -> IV (classic 4-chord loop)
    c0[(token(9, 1), token(7, 0))] = 340.0  # vi -> V
    c0[(token(9, 1), token(7, 2))] = 120.0  # vi -> V7
    c0[(token(9, 1), token(2, 1))] = 220.0  # vi -> ii
    c0[(token(9, 1), token(0, 0))] = 180.0  # vi -> I
    c0[(token(9, 1), token(4, 1))] = 70.0   # vi -> iii

    # ii (2, 1)
    c0[(token(2, 1), token(7, 0))] = 610.0  # ii -> V
    c0[(token(2, 1), token(7, 2))] = 210.0  # ii -> V7
    c0[(token(2, 1), token(5, 0))] = 210.0  # ii -> IV
    c0[(token(2, 1), token(0, 0))] = 150.0  # ii -> I
    c0[(token(2, 1), token(9, 1))] = 70.0   # ii -> vi

    # iii (4, 1)
    c0[(token(4, 1), token(9, 1))] = 290.0  # iii -> vi
    c0[(token(4, 1), token(5, 0))] = 220.0  # iii -> IV
    c0[(token(4, 1), token(0, 0))] = 100.0  # iii -> I
    c0[(token(4, 1), token(2, 1))] = 60.0   # iii -> ii

    # bVII (10, 0)
    c0[(token(10, 0), token(5, 0))] = 380.0 # bVII -> IV (double plagal)
    c0[(token(10, 0), token(0, 0))] = 240.0 # bVII -> I (rock cadence)
    c0[(token(10, 0), token(7, 0))] = 85.0  # bVII -> V

    # bVI (8, 0)
    c0[(token(8, 0), token(10, 0))] = 260.0 # bVI -> bVII (Aeolian/epic)
    c0[(token(8, 0), token(0, 0))] = 120.0  # bVI -> I
    c0[(token(8, 0), token(7, 0))] = 70.0   # bVI -> V

    # bIII (3, 0)
    c0[(token(3, 0), token(5, 0))] = 140.0  # bIII -> IV
    c0[(token(3, 0), token(10, 0))] = 100.0 # bIII -> bVII
    c0[(token(3, 0), token(0, 0))] = 80.0   # bIII -> I

    # iv (5, 1)
    c0[(token(5, 1), token(0, 0))] = 240.0  # iv -> I (minor plagal resolution)

    # --------------------------------------------------------------------------
    # Cluster 1: MinorLoops (Aeolian / Dorian / Modal minor loops)
    # --------------------------------------------------------------------------
    c1 = clusters[1]
    # i (0, 1)
    c1[(token(0, 1), token(8, 0))] = 480.0  # i -> bVI
    c1[(token(0, 1), token(5, 1))] = 430.0  # i -> iv
    c1[(token(0, 1), token(10, 0))] = 400.0 # i -> bVII
    c1[(token(0, 1), token(7, 1))] = 210.0  # i -> v
    c1[(token(0, 1), token(7, 0))] = 190.0  # i -> V (harmonic minor dominant)
    c1[(token(0, 1), token(7, 2))] = 160.0  # i -> V7
    c1[(token(0, 1), token(3, 0))] = 180.0  # i -> bIII
    c1[(token(0, 1), token(5, 0))] = 110.0  # i -> IV (Dorian major IV)

    # bVI (8, 0)
    c1[(token(8, 0), token(10, 0))] = 540.0 # bVI -> bVII
    c1[(token(8, 0), token(0, 1))] = 310.0  # bVI -> i
    c1[(token(8, 0), token(5, 1))] = 210.0  # bVI -> iv
    c1[(token(8, 0), token(7, 0))] = 150.0  # bVI -> V
    c1[(token(8, 0), token(7, 2))] = 110.0  # bVI -> V7

    # bVII (10, 0)
    c1[(token(10, 0), token(0, 1))] = 560.0 # bVII -> i
    c1[(token(10, 0), token(8, 0))] = 330.0 # bVII -> bVI
    c1[(token(10, 0), token(3, 0))] = 240.0 # bVII -> bIII
    c1[(token(10, 0), token(5, 1))] = 120.0 # bVII -> iv
    c1[(token(10, 0), token(7, 0))] = 90.0  # bVII -> V

    # iv (5, 1)
    c1[(token(5, 1), token(7, 0))] = 340.0  # iv -> V
    c1[(token(5, 1), token(7, 2))] = 190.0  # iv -> V7
    c1[(token(5, 1), token(7, 1))] = 220.0  # iv -> v
    c1[(token(5, 1), token(0, 1))] = 380.0  # iv -> i
    c1[(token(5, 1), token(10, 0))] = 200.0 # iv -> bVII
    c1[(token(5, 1), token(8, 0))] = 140.0  # iv -> bVI

    # v / V (7, 1 and 7, 0/2)
    c1[(token(7, 1), token(0, 1))] = 490.0  # v -> i
    c1[(token(7, 1), token(8, 0))] = 210.0  # v -> bVI
    c1[(token(7, 0), token(0, 1))] = 710.0  # V -> i
    c1[(token(7, 2), token(0, 1))] = 480.0  # V7 -> i
    c1[(token(7, 0), token(8, 0))] = 280.0  # V -> bVI
    c1[(token(7, 2), token(8, 0))] = 180.0  # V7 -> bVI

    # bIII (3, 0)
    c1[(token(3, 0), token(10, 0))] = 330.0 # bIII -> bVII
    c1[(token(3, 0), token(8, 0))] = 300.0  # bIII -> bVI
    c1[(token(3, 0), token(5, 1))] = 150.0  # bIII -> iv
    c1[(token(3, 0), token(0, 1))] = 95.0   # bIII -> i

    # ii° / ii (2, 3 and 2, 1)
    c1[(token(2, 3), token(7, 0))] = 390.0  # ii° -> V
    c1[(token(2, 3), token(7, 2))] = 270.0  # ii° -> V7
    c1[(token(2, 1), token(7, 0))] = 240.0  # ii -> V
    c1[(token(2, 3), token(0, 1))] = 120.0  # ii° -> i

    # IV (5, 0 - Dorian)
    c1[(token(5, 0), token(0, 1))] = 260.0  # IV -> i
    c1[(token(5, 0), token(7, 0))] = 190.0  # IV -> V

    # --------------------------------------------------------------------------
    # Cluster 2: SoulRnb (Jazz / Soul / ii-V / Secondary dominants)
    # --------------------------------------------------------------------------
    c2 = clusters[2]
    # ii (2, 1) -> V
    c2[(token(2, 1), token(7, 0))] = 580.0
    c2[(token(2, 1), token(7, 2))] = 490.0  # ii -> V7
    c2[(token(2, 1), token(0, 0))] = 160.0
    c2[(token(2, 1), token(5, 0))] = 140.0

    # V (7, 0/2) -> I / vi
    c2[(token(7, 0), token(0, 0))] = 750.0
    c2[(token(7, 2), token(0, 0))] = 680.0  # V7 -> Imaj7
    c2[(token(7, 0), token(9, 1))] = 280.0  # V -> vi
    c2[(token(7, 2), token(9, 1))] = 210.0  # V7 -> vi

    # I (0, 0)
    c2[(token(0, 0), token(9, 1))] = 410.0  # I -> vi
    c2[(token(0, 0), token(2, 1))] = 360.0  # I -> ii
    c2[(token(0, 0), token(5, 0))] = 350.0  # I -> IV
    c2[(token(0, 0), token(4, 1))] = 190.0  # I -> iii
    c2[(token(0, 0), token(2, 2))] = 160.0  # I -> II7 (V/V)
    c2[(token(0, 0), token(9, 2))] = 140.0  # I -> VI7 (V/ii)

    # vi (9, 1)
    c2[(token(9, 1), token(2, 1))] = 440.0  # vi -> ii
    c2[(token(9, 1), token(5, 0))] = 310.0  # vi -> IV
    c2[(token(9, 1), token(7, 0))] = 190.0  # vi -> V

    # iii (4, 1)
    c2[(token(4, 1), token(9, 1))] = 510.0  # iii -> vi
    c2[(token(4, 1), token(2, 1))] = 220.0  # iii -> ii
    c2[(token(4, 1), token(5, 0))] = 160.0  # iii -> IV

    # Secondary dominants
    c2[(token(2, 2), token(7, 0))] = 360.0  # II7 (V/V) -> V
    c2[(token(2, 2), token(7, 2))] = 290.0
    c2[(token(9, 2), token(2, 1))] = 280.0  # VI7 (V/ii) -> ii
    c2[(token(4, 2), token(9, 1))] = 240.0  # III7 (V/vi) -> vi
    c2[(token(0, 2), token(5, 0))] = 260.0  # I7 (V/IV) -> IV

    # Minor plagal
    c2[(token(5, 1), token(0, 0))] = 250.0  # iv -> I

    return clusters

def compute_matrices():
    raw_clusters = build_cluster_transitions()
    matrices = []

    k_smooth = 0.5  # Add-k Laplace smoothing parameter

    for c_idx, counts in enumerate(raw_clusters):
        # Compute unigrams and total counts
        total_transitions = sum(counts.values())
        row_totals = [0.0] * NUM_TOKENS
        col_totals = [0.0] * NUM_TOKENS

        for (src, dst), cnt in counts.items():
            row_totals[src] += cnt
            col_totals[dst] += cnt

        # Smoothed conditional probabilities P(B | A) and marginals P(B)
        # P(B | A) = (cnt(A, B) + k) / (row_total(A) + k * NUM_TOKENS)
        # P(B) = (col_total(B) + k) / (total_transitions + k * NUM_TOKENS)
        scores = [[0.0] * NUM_TOKENS for _ in range(NUM_TOKENS)]
        min_score = 1e9
        max_score = -1e9

        for src in range(NUM_TOKENS):
            denom_src = row_totals[src] + k_smooth * NUM_TOKENS
            for dst in range(NUM_TOKENS):
                cnt = counts.get((src, dst), 0.0)
                p_cond = (cnt + k_smooth) / denom_src
                p_dst = (col_totals[dst] + k_smooth) / (total_transitions + k_smooth * NUM_TOKENS)
                
                # Balanced score: -ln P(B | A) + 0.3 * ln P(B) (PMI discount)
                score = -math.log(p_cond) + 0.3 * math.log(p_dst)
                scores[src][dst] = score
                if score < min_score:
                    min_score = score
                if score > max_score:
                    max_score = score

        # Normalize score into [0.0, 1.0] and quantize into uint8_t (0..255)
        # Clamp upper at 98th percentile of max_score to keep high contrast
        norm_range = max(1e-6, max_score - min_score)
        matrix = [[255] * NUM_TOKENS for _ in range(NUM_TOKENS)]
        for src in range(NUM_TOKENS):
            for dst in range(NUM_TOKENS):
                norm = (scores[src][dst] - min_score) / norm_range
                norm = max(0.0, min(1.0, norm))
                matrix[src][dst] = int(round(norm * 255.0))

        matrices.append(matrix)

    return matrices

def generate_header(matrices: List[List[List[int]]]) -> str:
    # Compute script hash for provenance
    with open(__file__, "rb") as f:
        script_sha = hashlib.sha256(f.read()).hexdigest()[:16]

    out = []
    out.append("// ChordSlots core - Hand-authored style prior (provisional).")
    out.append("// SPDX-License-Identifier: AGPL-3.0-or-later")
    out.append("//")
    out.append(f"// AUTO-GENERATED by tools/build_corpus.py (script hash: {script_sha}).")
    out.append("// Hand-authored style prior (provisional): transition weights written by hand from")
    out.append("// common pop/rock, minor-loop and soul/R&B practice. Not derived from a corpus yet;")
    out.append("// replacement with licence-clean corpus data is planned (IMPLEMENTATION_PLAN.md, P4).")
    out.append("// Matrices: 3 Style Clusters x 60 Tokens x 60 Tokens quantized to uint8_t.")
    out.append("#pragma once")
    out.append("")
    out.append('#include "Theory.h"')
    out.append("")
    out.append("namespace cs")
    out.append("{")
    out.append("")
    out.append("enum class QualityClass : uint8_t")
    out.append("{")
    out.append("    Major = 0,")
    out.append("    Minor = 1,")
    out.append("    Dominant = 2,")
    out.append("    Diminished = 3,")
    out.append("    SusOrAug = 4")
    out.append("};")
    out.append("")
    out.append("enum class CorpusStyle : uint8_t")
    out.append("{")
    out.append("    PopRock = 0,")
    out.append("    MinorLoops = 1,")
    out.append("    SoulRnb = 2,")
    out.append("    Free = 3")
    out.append("};")
    out.append("")
    out.append("constexpr int kNumQualityClasses = 5;")
    out.append("constexpr int kNumCorpusTokens = 12 * kNumQualityClasses; // 60")
    out.append("constexpr int kNumCorpusClusters = 3;")
    out.append("")
    out.append("/** Deterministic table-driven mapping of all 21 ChordTypeDef types to QualityClass. */")
    out.append("inline QualityClass qualityClassOf (int type) noexcept")
    out.append("{")
    out.append("    switch (type)")
    out.append("    {")
    out.append("        // Major: triad, power chord, maj7, 6, add9, maj9")
    out.append("        case 0: case 2: case 7: case 13: case 15: case 17:")
    out.append("            return QualityClass::Major;")
    out.append("        // Minor: m, m7, mMaj7, m6, madd9, m9")
    out.append("        case 1: case 8: case 12: case 14: case 16: case 18:")
    out.append("            return QualityClass::Minor;")
    out.append("        // Dominant: 7, 9")
    out.append("        case 9: case 19:")
    out.append("            return QualityClass::Dominant;")
    out.append("        // Diminished / Half-diminished: dim, m7b5, dim7")
    out.append("        case 3: case 10: case 11:")
    out.append("            return QualityClass::Diminished;")
    out.append("        // Sus or Augmented: aug, sus2, sus4, 7sus4")
    out.append("        case 4: case 5: case 6: case 20:")
    out.append("        default:")
    out.append("            return QualityClass::SusOrAug;")
    out.append("    }")
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
    out.append("// Quantized transition surprise/PMI matrices (0..255). Size: 3 x 60 x 60 = 10,800 bytes (< 11 KB)")
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

def main():
    verify_only = "--verify" in sys.argv
    out_path = "core/include/chordslots/Corpus.h"
    if "--out" in sys.argv:
        idx = sys.argv.index("--out")
        if idx + 1 < len(sys.argv):
            out_path = sys.argv[idx + 1]

    matrices = compute_matrices()
    content = generate_header(matrices)

    if verify_only:
        if not os.path.exists(out_path):
            print(f"Error: {out_path} does not exist for verification.")
            sys.exit(1)
        with open(out_path, "r", encoding="utf-8") as f:
            existing = f.read()
        if existing == content:
            print("Corpus.h is in sync with build_corpus.py.")
            sys.exit(0)
        else:
            print("Mismatch: Corpus.h differs from generator output.")
            sys.exit(1)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Successfully generated {out_path} ({len(content)} bytes).")

if __name__ == "__main__":
    main()
