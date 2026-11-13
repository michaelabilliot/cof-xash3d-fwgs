"""Rename anonymous-namespace symbols of a symbol map to the hash our objects use.

  python tools/anon_names.py <dll> [--write]

MSVC names an anonymous namespace ?A0x<hash>, and the hash depends on the
path of the source file at compile time. Names that came from a reference
build (refbytes) carry that build's hash, so objdiff never pairs them with
our functions. For every map name with ?A0x<hash>@ this finds the symbol of
our objects that is the same name with another hash (exactly one) and, with
--write, records it in tools/symbols/<dll>_manual.csv; unit lists that hold
the old name get the new one.
"""
import csv
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import name_unit  # noqa: E402

ANON = re.compile(r"\?A0x[0-9a-f]+@")


def norm(name):
    return ANON.sub("?A0x@", name)


def main():
    args = sys.argv[1:]
    dll = args[0]
    write = "--write" in args
    ours = {}
    for obj in glob.glob(os.path.join(ROOT, "build", "base", dll, "**", "*.obj"), recursive=True):
        data = open(obj, "rb").read()
        for m in re.findall(rb"\?[A-Za-z0-9_?@$]*\?A0x[0-9a-f]+@[A-Za-z0-9_?@$]*", data):
            n = m.decode("latin-1")
            ours.setdefault(norm(n), set()).add(n)
    rows = []
    for p in glob.glob(os.path.join(ROOT, "tools", "symbols", "%s_*.csv" % dll)):
        if p.endswith(("_manual.csv", "_forget.csv", "_datasizes.csv")):
            continue
        for r in csv.DictReader(open(p, encoding="latin-1")):
            n = r.get("name", "")
            if "?A0x" in n:
                rows.append((r["va"], n, r.get("kind", "func")))
    new = []
    for va, n, kind in rows:
        cand = ours.get(norm(n), set()) - {n}
        if len(cand) == 1:
            new.append((va, n, cand.pop(), kind))
    for va, old, n, kind in new:
        print(va, old[:50], "->", n[:60])
    print(len(new), "of", len(rows), "renamed")
    if not write or not new:
        return
    name_unit.lock()
    try:
        p = os.path.join(ROOT, "tools", "symbols", "%s_manual.csv" % dll)
        man = list(csv.DictReader(open(p)))
        for r in man:
            extra = r.pop(None, None)
            if extra:
                r["evidence"] = ",".join([r["evidence"]] + extra)
        have = {r["va"] for r in man}
        for va, old, n, kind in new:
            if va not in have:
                man.append({"va": va, "name": n, "kind": kind,
                            "evidence": "anonymous namespace hash of our build; was %s" % old})
        with open(p + ".tmp", "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["va", "name", "kind", "evidence"], lineterminator="\n")
            w.writeheader()
            w.writerows(man)
        os.replace(p + ".tmp", p)
        ren = {old: n for va, old, n, kind in new}
        for lf in glob.glob(os.path.join(ROOT, "tools", "units", "%s_*.txt" % dll)):
            lines = [l.strip() for l in open(lf) if l.strip()]
            out = [ren.get(l, l) for l in lines]
            if out != lines:
                open(lf, "w").write("\n".join(out) + "\n")
    finally:
        name_unit.unlock()


if __name__ == "__main__":
    main()
