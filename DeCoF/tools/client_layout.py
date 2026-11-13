"""Reconstruct the object layout of a DLL without a PDB (client.dll).

  python tools/client_layout.py [<dll>] [--write]

The linker places the functions of one object together, objects in link
order. A function whose name came from a reference object
(evidence "refbytes:<repo>:<dir>__<file>.obj") or that has a confirmed
source_file is an anchor for that file. This walks the functions in
address order and prints, per file, the address range spanned by its
anchors, how many functions lie in it and the anchors of other files
inside it (a conflict: a wrong byte match, an inline function emitted in
the first object that uses it, or a file that really is split).

With --write, every non-LIB row between two anchors of the same file
(with no anchor of another file in between) gets that file as its
source_file ("guess:" kept for rows that are not anchors), written to
docs/progress.csv. Rows that are anchors themselves get their file if
they have none.
"""
import csv
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import csvio  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def obj_file(obj):
    """cl_dll__ammo.obj -> cl_dll/ammo.cpp (or .c), as it exists under src/."""
    stem = obj[:-4].replace("__", "/")
    for ext in (".cpp", ".c"):
        if os.path.exists(os.path.join(ROOT, "src", stem + ext)):
            return stem + ext
    return stem + ".cpp"


def anchor(r, defined_in):
    m = re.match(r"refbytes:(\w+):([\w.]+\.obj)", r["evidence"])
    if m:
        # an inline function of a header is emitted in the first object
        # that uses it, not in the object it was matched against
        where = defined_in.get(m.group(1).upper(), {}).get(r["demangled"], "")
        if where.endswith(".h"):
            return None
        f = obj_file(m.group(2))
        # a C object defines no decorated C++ name (the byte match picked
        # an identical C function): the source_file column decides then
        if not (f.endswith(".c") and r["name"].startswith("?")):
            return f
    if r["source_file"] and not r["source_file"].startswith("guess:"):
        return r["source_file"]
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    dll = args[0] if args else "client"
    write = "--write" in sys.argv
    path = os.path.join(ROOT, "docs", "progress.csv")
    allrows = list(csv.DictReader(open(path, newline="")))
    fields = list(allrows[0].keys())
    rows = sorted((r for r in allrows if r["dll"] == dll and r["class"] != "LIB"), key=lambda r: int(r["va"], 16))
    # where each reference function is defined (tools/ref_index.py)
    index = json.load(open(os.path.join(ROOT, "build", "ref_index.json")))
    side = "client" if dll == "client" else "server"
    defined_in = {k.split("/")[0]: v for k, v in index.items() if k.endswith("/" + side)}
    anchors = [(i, anchor(r, defined_in)) for i, r in enumerate(rows)]
    anchors = [(i, f) for i, f in anchors if f]
    # runs of consecutive anchors of one file; a file keeps only its
    # largest run, its other runs are outliers (an identical function of
    # another object took this file's name in the byte match)
    runs = []
    for i, f in anchors:
        if runs and runs[-1][0] == f:
            runs[-1][1].append(i)
        else:
            runs.append((f, [i]))
    best = {}
    for f, idx in runs:
        if len(idx) > len(best.get(f, [])):
            best[f] = idx
    outliers = [(i, f) for f, idx in runs if idx is not best[f] for i in idx]
    if outliers:
        print("outliers (anchor outside the main run of its file):")
        for i, f in outliers:
            r = rows[i]
            print("  %s %-50s %s  %s" % (r["va"], r["name"][:50], f, r["evidence"][:40]))
    skip = {i for i, _f in outliers}
    anchors = [(i, f) for i, f in anchors if i not in skip]
    # per file: first and last anchor index
    span = {}
    for i, f in anchors:
        lo, hi, n = span.get(f, (i, i, 0))
        span[f] = (min(lo, i), max(hi, i), n + 1)
    out = []
    for f, (lo, hi, n) in sorted(span.items(), key=lambda x: x[1][0]):
        others = sorted({g for i, g in anchors if lo < i < hi and g != f})
        out.append("%s-%s  %4d rows  %3d anchors  %-45s %s" % (
            rows[lo]["va"], rows[hi]["va"], hi - lo + 1, n, f,
            ("conflicts: " + ", ".join(others[:6]) + (" ..." if len(others) > 6 else "")) if others else ""))
    print("\n".join(out))
    if not write:
        return
    changed = 0
    for k in range(len(anchors) - 1):
        i, f = anchors[k]
        j, g = anchors[k + 1]
        if f != g:
            continue
        for r in rows[i + 1:j]:
            if not r["source_file"] or (r["source_file"].startswith("guess:") and r["source_file"] != "guess:" + f):
                r["source_file"] = "guess:" + f
                changed += 1
    for i, f in anchors:
        if not rows[i]["source_file"] or rows[i]["source_file"].startswith("guess:"):
            rows[i]["source_file"] = f
            changed += 1
    csvio.write_rows(path, fields, allrows)
    print("updated %d rows" % changed)


if __name__ == "__main__":
    main()
