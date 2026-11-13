"""Reconstruct the translation unit list and link order of the 1.6a DLLs.

Usage: python tools/make_units.py

hl.dll: the 1.55 PDB gives every object, its source file and its .text
contributions (the 1.55 link order). The 1.6a order comes from the address
ranges of the functions attributed to each source file in docs/progress.csv.
client.dll: no PDB in any version, so only the 1.6a order derived from
progress.csv (sources found through reference objects) is available.

Writes docs/units_hl.csv and docs/units_client.csv.
"""
import collections
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pdbfile import PDB  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def origin_of(path):
    """Which reference tree has a file of this name."""
    name = os.path.basename(path).lower()
    trees = []
    for tag, root in (("HLSDK", "build/hlsdk_c7240b9"), ("SPIRIT", "refs/Spirit/SpiritSource"),
                      ("PARANOIA", "refs/Paranoia/code_src")):
        for dp, _dn, fn in os.walk(os.path.join(ROOT, root)):
            if name in (f.lower() for f in fn):
                trees.append(tag)
                break
    return trees[0] if trees else "COF"


def hl_155_order():
    pdb = PDB(os.path.join(ROOT, "versions", "1.55", "cl_dlls", "hl.pdb"))
    first = {}
    for sect, off, size, chars, mod in pdb.contribs:
        if sect == 1 and size > 0:
            first[mod] = min(first.get(mod, 1 << 32), off)
    out = []
    for mod, off in sorted(first.items(), key=lambda kv: kv[1]):
        m = pdb.modules[mod]
        if "debughl" not in m.name.lower():
            continue
        cpp = [f for f in pdb.module_files[m.index] if f.lower().endswith((".cpp", ".c"))]
        src = re.sub(r"^.*?/sourcecode/", "", cpp[0].replace("\\", "/"), flags=re.I) if cpp else \
            "dlls/" + os.path.basename(m.name)[:-4] + ".cpp"
        out.append((src.lower(), os.path.basename(m.name)))
    return out


def main():
    rows = list(csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))))
    for dll in ("hl", "client"):
        drows = [r for r in rows if r["dll"] == dll and r["class"] != "LIB"]
        # attribute only functions defined in a .cpp/.c file (headers are
        # inline code placed in whichever object used it first)
        seq = [(int(r["va"], 16), r["source_file"].lower(), r["source_file"]) for r in drows
               if r["source_file"] and not r["source_file"].startswith("guess:")
               and r["source_file"].lower().endswith((".cpp", ".c"))]
        seq.sort()
        # core run per file: longest run of consecutive attributed functions
        runs = collections.defaultdict(list)
        cur = None
        for va, key, src in seq:
            if cur and cur[0] == key:
                cur[2] = va
                cur[3] += 1
            else:
                cur = [key, va, va, 1, src]
                runs[key].append(cur)
        spans = collections.OrderedDict()
        for key, rs in runs.items():
            best = max(rs, key=lambda r: r[3])
            spans[key] = [best[1], best[2], sum(r[3] for r in rs), best[4], len(rs)]
        total_between = collections.Counter()
        for r in drows:
            va = int(r["va"], 16)
            for key, sp in spans.items():
                if sp[0] <= va <= sp[1]:
                    total_between[key] += 1
        order = sorted(spans.items(), key=lambda kv: kv[1][0])
        o155 = {}
        if dll == "hl":
            o155 = {src: (i + 1, obj) for i, (src, obj) in enumerate(hl_155_order())}
        out = []
        for i, (key, (lo, hi, n, src, nruns)) in enumerate(order):
            overlap = [k for k, v in spans.items() if k != key and v[0] < hi and v[1] > lo]
            out.append({
                "order_1.6a": i + 1, "source_file": src, "first_va": "%08X" % lo, "last_va": "%08X" % hi,
                "named_functions": n, "functions_in_range": total_between[key],
                "order_1.55": o155.get(key, ("", ""))[0], "object_1.55": o155.get(key, ("", ""))[1],
                "origin": origin_of(src), "runs": nruns, "overlaps": ";".join(sorted(overlap))[:200],
            })
        if dll == "hl":
            seen = {r["source_file"].lower() for r in out}
            for key, (n155, obj) in o155.items():
                if key not in seen:
                    out.append({"order_1.6a": "", "source_file": key, "first_va": "", "last_va": "",
                                "named_functions": 0, "functions_in_range": 0, "order_1.55": n155,
                                "object_1.55": obj, "origin": origin_of(key), "runs": 0, "overlaps": ""})
        path = os.path.join(ROOT, "docs", "units_%s.csv" % dll)
        with open(path, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(out[0].keys()), lineterminator="\n")
            w.writeheader()
            w.writerows(out)
        print("%s: %d units in 1.6a order -> %s" % (dll, len([o for o in out if o["order_1.6a"]]), path))


if __name__ == "__main__":
    main()
