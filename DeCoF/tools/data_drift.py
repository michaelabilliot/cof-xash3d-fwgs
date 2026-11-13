"""Where the data layout of a linked DLL drifts from retail.

  python tools/data_drift.py <dll> [--min <bytes>] [--code]

For every global the symbol maps name (kind data), this takes its retail
address and its address in our link map (build/link/<dll>/<dll>.dll.map),
sorts by retail address and prints each place where the difference
(ours - retail) changes by at least --min bytes (default 16), with the
globals on both sides. A jump means the globals between the two neighbours
take more or less room in our build: a missing global, a wrong array size,
or another object order. --code does the same for functions (progress.csv
names and addresses), which points at missing or resized functions.
"""
import csv
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    dll = sys.argv[1]
    minimum = int(sys.argv[sys.argv.index("--min") + 1]) if "--min" in sys.argv else 16
    retail = {}
    if "--code" in sys.argv:
        for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"), encoding="utf-8")):
            if r["dll"] == dll and r["name"]:
                retail.setdefault(r["name"], int(r["va"], 16))
    for p in [] if "--code" in sys.argv else glob.glob(os.path.join(ROOT, "tools", "symbols", "%s_*.csv" % dll)):
        if p.endswith(("_forget.csv", "_datasizes.csv")):
            continue
        for r in csv.DictReader(open(p, encoding="latin-1")):
            if r.get("kind") == "data" and r.get("va"):
                retail.setdefault(r["name"], int(r["va"], 16))
    ours = {}
    mp = os.path.join(ROOT, "build", "link", dll, "%s.dll.map" % dll)
    for line in open(mp, encoding="latin-1"):
        m = re.match(r"\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s", line)
        if m:
            ours[m.group(1)] = int(m.group(2), 16)
    pairs = sorted((retail[n], ours[n], n) for n in retail if n in ours)
    prev = None
    for va, oa, n in pairs:
        d = oa - va
        if prev is not None and abs(d - prev[0]) >= minimum:
            print("%+8d at %08X: after %s, before %s (%+d)" % (d - prev[0], va, prev[1][:50], n[:50], d))
        prev = (d, n)
    print("%d globals compared" % len(pairs))


if __name__ == "__main__":
    main()
