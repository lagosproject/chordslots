#!/usr/bin/env python3
"""Compare CLI eval bigrams to the corpus prior (KL divergence in nats)."""

import argparse
import json
import math
from pathlib import Path

# CLI eval applies the preset scale in Auto, with Dreamy using the extended prior.
CLUSTER_BY_MOOD = {
    "Euphoric": 0, "Uplifting": 1, "Epic": 1, "Melancholic": 1,
    "Dark": 1, "Tense": 1, "Dreamy": 2, "Hard": 1,
}
CLUSTER_NAMES = ("Major", "Minor", "Extended")
NUM_TOKENS = 60
PAIRS = tuple(f"{a}->{b}" for a in range(NUM_TOKENS) for b in range(NUM_TOKENS))


def distribution(counts):
    if not isinstance(counts, dict):
        raise ValueError("Expected a sparse bigram count object")
    for pair, count in counts.items():
        if pair not in PAIR_SET or type(count) is not int or count < 0:
            raise ValueError(f"Invalid bigram count for {pair!r}")
    total = sum(counts.values()) + 0.5 * len(PAIRS)
    return [(counts.get(pair, 0) + 0.5) / total for pair in PAIRS]


PAIR_SET = frozenset(PAIRS)


def kl_divergence(generated, corpus):
    return math.fsum(p * math.log(p / q) for p, q in zip(generated, corpus))


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus_counts", type=Path)
    parser.add_argument("eval_bigrams", type=Path)
    args = parser.parse_args()
    try:
        corpus = read_json(args.corpus_counts)
        if not isinstance(corpus, list) or len(corpus) != len(CLUSTER_NAMES):
            raise ValueError("Expected three corpus clusters")
        clusters = [distribution(cluster) for cluster in corpus]
        results = read_json(args.eval_bigrams)
        if not isinstance(results, dict) or not results:
            raise ValueError("Expected eval bigrams grouped by mood and configuration")
        print(f"{'Mood.config':<20} {'Corpus':<10} {'KL (nats)':>12}")
        for config, counts in results.items():
            mood = config.split(".", 1)[0]
            if mood not in CLUSTER_BY_MOOD:
                raise ValueError(f"Unknown mood {mood!r}")
            cluster = CLUSTER_BY_MOOD[mood]
            value = kl_divergence(distribution(counts), clusters[cluster])
            print(f"{config:<20} {CLUSTER_NAMES[cluster]:<10} {value:12.6f}")
    except (OSError, ValueError) as error:
        parser.exit(1, f"Bigram comparison error: {error}\n")


if __name__ == "__main__":
    main()
