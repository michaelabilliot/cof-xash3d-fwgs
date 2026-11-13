"""Try source variants of one function and keep the best.

  python tools/try_variants.py <unit> <symbol> <variants file> [--keep]

The variants file holds the exact text to replace and its alternatives:

    @@@ src/dlls/CoF_Glock.cpp
    <<<
    text currently in the file
    ===
    first alternative
    ===
    second alternative
    >>>

Each alternative (and the original) is compiled and diffed; the match of
<symbol> is printed for each. With --keep the best one stays in the file,
otherwise the file is restored. Files keep their encoding and line endings.
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import build  # noqa: E402


def parse(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    target = None
    blocks = []
    i = 0
    while i < len(lines):
        if lines[i].startswith("@@@ "):
            target = lines[i][4:].strip()
        elif lines[i] == "<<<":
            parts, cur = [], []
            i += 1
            while lines[i] != ">>>":
                if lines[i] == "===":
                    parts.append("\n".join(cur))
                    cur = []
                else:
                    cur.append(lines[i])
                i += 1
            parts.append("\n".join(cur))
            blocks.append((target, parts[0], parts[1:]))
        i += 1
    return blocks


def score(unit, symbol):
    units = build.load_units([unit])
    devnull = open(os.devnull, "w")
    old = sys.stdout
    sys.stdout = devnull
    try:
        build.check(units)
    finally:
        sys.stdout = old
    tag = unit.replace("/", "_")
    rep = json.load(open(os.path.join(ROOT, "build", "check_project", tag, "report.json")))
    for u in rep.get("units", []):
        for fn in u.get("functions", []):
            if fn["name"] == symbol:
                return fn.get("fuzzy_match_percent", 0.0)
    return -1.0


def main():
    unit, symbol, vfile = sys.argv[1:4]
    keep = "--keep" in sys.argv
    for target, original, alts in parse(vfile):
        full = os.path.join(ROOT, target)
        raw = open(full, "rb").read().decode("latin-1")
        crlf = "\r\n" in raw
        text = raw.replace("\r\n", "\n")
        if text.count(original) != 1:
            sys.exit("original text found %d times in %s" % (text.count(original), target))
        results = []
        for k, alt in enumerate([original] + alts):
            new = text.replace(original, alt)
            out = new.replace("\n", "\r\n") if crlf else new
            open(full, "wb").write(out.encode("latin-1"))
            pct = score(unit, symbol)
            results.append((pct, k))
            print("variant %d: %7.3f%%  %s" % (k, pct, alt.strip().splitlines()[0][:70] if alt.strip() else "(empty)"))
        # ties keep the earliest variant (the original first)
        best = max(results, key=lambda r: (r[0], -r[1]))[1] if keep else 0
        final = text.replace(original, ([original] + alts)[best])
        open(full, "wb").write((final.replace("\n", "\r\n") if crlf else final).encode("latin-1"))
        print("kept variant %d" % best)


if __name__ == "__main__":
    main()
