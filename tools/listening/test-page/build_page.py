#!/usr/bin/env python3
"""Builds ../listening-test.html: one self-contained file with every stimulus and the sheet logic embedded.

Run after regenerating any stimuli:  python3 tools/listening/test-page/build_page.py

Only neutral identifiers go into the page (pair ids, file base names, the mood word of block 4, item ids). The
operator keys (what A and B are, engine values, flag on/off) stay in the repo and are used by the analysis scripts.
"""
import argparse, base64, csv, json, os

HERE = os.path.dirname(os.path.abspath(__file__))
L = os.path.dirname(HERE)


def b64(path):
    with open(path, "rb") as fh:
        return base64.b64encode(fh.read()).decode()


def pairs_from(keys, midi_dir, media, with_mood=False):
    out = []
    for path in keys:
        doc = json.load(open(os.path.join(L, path), encoding="utf-8"))
        for p in doc["pairs"]:
            base = {}
            for letter in "AB":
                fname = os.path.basename(p["files"][letter]["file"])
                key = os.path.splitext(fname)[0]
                base[letter] = key
                media[key.lower()] = b64(os.path.join(L, midi_dir, fname))
            out.append({"id": p["pair_id"], "mood": p.get("mood", "") if with_mood else "", "base": base})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--contact", default="", help="text shown to listeners: where to send the results file, e.g. an email address")
    args = ap.parse_args()
    media = {}
    blocks = {
        "r10": {"pairs": pairs_from(["r10/pairs.json"], "r10/stimuli", media)},
        "brightness": {"pairs": pairs_from(["brightness/pairs.json", "brightness/pairs_matched.json"], "brightness/pairs", media)},
        "flagab": {"pairs": pairs_from(["flagab/operator/key.json"], "flagab/midi", media, with_mood=True)},
    }
    items = [r["item"] for r in csv.DictReader(open(os.path.join(L, "blindpanel/operator/key.csv"), newline="", encoding="utf-8"))]
    for it in items:
        media[it.lower()] = b64(os.path.join(L, "blindpanel/midi", it + ".mid"))
    blocks["panel"] = {"items": items}
    template = open(os.path.join(HERE, "template.html"), encoding="utf-8").read()
    data = json.dumps({"blocks": blocks, "media": media}, separators=(",", ":"))
    assert "/*DATA*/null/*END-DATA*/" in template
    html = template.replace("/*DATA*/null/*END-DATA*/", data)
    html = html.replace("/*CONTACT*/''/*END-CONTACT*/", json.dumps(args.contact).replace("</", "<\\/"))
    out = os.path.join(L, "listening-test.html")
    with open(out, "w", encoding="utf-8") as fh:
        fh.write(html)
    print(f"wrote {out}: {len(media)} sound files, {sum(len(b.get('pairs', b.get('items', []))) for b in blocks.values())} pairs/items, {len(html) // 1024} KB")


if __name__ == "__main__":
    main()
