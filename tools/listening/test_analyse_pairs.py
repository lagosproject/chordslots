#!/usr/bin/env python3
"""Self-test for analyse_pairs.py with simulated listeners (no real data). Run: python3 test_analyse_pairs.py"""
import csv, os, random, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import analyse_pairs as ap

def run(*a):
    return subprocess.run([sys.executable, *a], check=True, capture_output=True, text=True).stdout

def fill(path, qs, chooser, rng):
    rows = list(csv.DictReader(open(path, newline="", encoding="utf-8")))
    for r in rows:
        first = ap.letter_of(r["first_file"])
        for q in qs:
            want = chooser(r["pair_id"], q, rng)       # letter, 'same' or 'unsure'
            if want in ("A", "B"):
                r[q] = "first" if want == first else "second"
            else:
                r[q] = want
    with open(path, "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)

def other(l): return "B" if l == "A" else "A"

def main():
    tmp = tempfile.mkdtemp()
    rng = random.Random(5)
    r10 = ap.load_pairs("r10"); br = ap.load_pairs("brightness"); fl = ap.load_pairs("flagab")
    run(os.path.join(HERE, "r10", "make_session_sheet.py"), "--participants", "6", "--seed", "3", "--out-dir", f"{tmp}/r10")
    run(os.path.join(HERE, "make_pair_sheets.py"), "brightness", "--participants", "6", "--seed", "3", "--out-dir", f"{tmp}/br")
    run(os.path.join(HERE, "make_pair_sheets.py"), "flagab", "--participants", "6", "--seed", "3", "--out-dir", f"{tmp}/fl")

    def r10_ch(pid, q, rng):  # strong tritone listener
        t = [k for k, v in r10[pid]["files"].items() if v["contains_tritone"]]
        if len(t) != 1: return rng.choice("AB")
        return t[0] if rng.random() < 0.92 else other(t[0])
    def br_ch(pid, q, rng):  # register dominates, colour weak
        p = br[pid]
        if p["type"] in ("register", "conflict"): good = p["higher_register_file"]; pr = 0.9
        else: good = p["higher_brightness_file"]; pr = 0.6
        return good if rng.random() < pr else other(good)
    def fl_ch(pid, q, rng):  # flag 'mood' liked, soft clip disliked, tritone unclear
        p = fl[pid]; on = p["on_file"]
        pr = {"kMoodDiatonicContext": 0.9, "kBrightnessSoftClip": 0.1}.get(p["flag"], 0.5)
        return on if rng.random() < pr else other(on)
    for d, qs, ch in ((f"{tmp}/r10", ("more_tense", "asks_to_resolve"), r10_ch),
                      (f"{tmp}/br", ("brighter", "lighter_open_use"), br_ch),
                      (f"{tmp}/fl", ("fits_mood", "would_use"), fl_ch)):
        for f in sorted(os.listdir(d)):
            fill(os.path.join(d, f), qs, ch, rng)

    o1 = run(os.path.join(HERE, "analyse_pairs.py"), "r10", f"{tmp}/r10")
    o2 = run(os.path.join(HERE, "analyse_pairs.py"), "brightness", f"{tmp}/br")
    o3 = run(os.path.join(HERE, "analyse_pairs.py"), "flagab", f"{tmp}/fl")
    assert "PROPOSE kTritoneWeight = 0.05" in o1, o1
    assert "REGISTER dominates colour" in o2, o2
    assert "kMoodDiatonicContext" in o3 and "PROPOSE turning the flag on" in o3, o3
    assert "KEEP OFF for good" in o3, o3
    print(o1); print(o2); print(o3)
    # sparse data must not trigger
    assert ap.sign_test_p(6, 6) < 0.04 and ap.sign_test_p(3, 6) == 1.0
    print("analyse_pairs self-test: OK")

main()
