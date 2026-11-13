"""Name the functions of a VC6 engine DLL by source order from the Linux DWARF.

  python tools/align_linux.py <dll> [--show <file>] [--write]        (dll: hw)

Each object's code in the Windows build is contiguous and, built with /Od,
keeps the functions in source order (functions GCC inlined away are still
separate functions there). The Linux steam_legacy build's DWARF gives every
function of every source file with its declaration line
(tools/elf_files.py, build/match/<dll>_linux_files.csv and
<dll>_linux_inlined.csv). For every C source file this takes the Windows
functions already matched to a Linux function (build/match/<dll>_linux_to_1.6a.csv
and the names in the symbol map) as anchors, keeps the anchors whose order
agrees, and between two consecutive anchors pairs the unnamed Windows
functions one to one with the Linux functions declared between them, but
only when both counts are equal. C++ files are skipped (MSVC decorated
names cannot be derived from the Linux names). Prints a summary; --write
records the names in tools/symbols/<dll>_position.csv and the source files
in docs/progress.csv.
"""
import bisect
import collections
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import name_unit  # noqa: E402

GHIDRA_ELF_BASE = 0x10000  # Ghidra loads the Linux .so 0x10000 higher


def lis(pairs):
    """Longest chain of (windows index, linux index) pairs increasing in both."""
    pairs = sorted(pairs)
    tails, prev, idx = [], [None] * len(pairs), []
    for i, (_w, l) in enumerate(pairs):
        j = bisect.bisect_left([pairs[t][1] for t in tails], l)
        if j == len(tails):
            tails.append(i)
        else:
            tails[j] = i
        prev[i] = tails[j - 1] if j else None
    out, k = [], tails[-1] if tails else None
    while k is not None:
        out.append(pairs[k])
        k = prev[k]
    return out[::-1]


def main():
    dll = sys.argv[1]
    write = "--write" in sys.argv
    m = os.path.join(ROOT, "build", "match")
    lfuncs = collections.defaultdict(dict)  # file -> name -> line
    by_va = {}
    for r in csv.DictReader(open(os.path.join(m, "%s_linux_files.csv" % dll))):
        if r["file"] and r["line"]:
            f = r["file"].replace("../", "")
            # the DWARF name (unmangled) comes from the symbol: C names are plain
            lfuncs[f].setdefault(r["name"], int(r["line"]))
            by_va[int(r["va"], 16)] = (f, r["name"])
    for r in csv.DictReader(open(os.path.join(m, "%s_linux_inlined.csv" % dll))):
        if r["line"]:
            lfuncs[r["file"].replace("../", "")].setdefault(r["name"], int(r["line"]))
    # windows rows
    rows = [r for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"), encoding="utf-8")) if r["dll"] == dll]
    rows.sort(key=lambda r: int(r["va"], 16))
    widx = {r["va"]: i for i, r in enumerate(rows)}
    # anchors: windows va -> (file, linux name)
    anchor = {}
    for r in csv.DictReader(open(os.path.join(m, "%s_linux_to_1.6a.csv" % dll))):
        x = by_va.get(int(r["from_va"], 16) - GHIDRA_ELF_BASE)
        if x:
            anchor[r["to_va"].upper()] = x
    # virtual methods paired through the vtables (tools/vtable_linux.py)
    vp = os.path.join(m, "%s_vtable_linux.csv" % dll)
    if os.path.exists(vp):
        for r in csv.DictReader(open(vp)):
            x = by_va.get(int(r["linux_va"], 16))
            if x:
                anchor.setdefault(r["windows_va"].upper(), x)
    for r in rows:
        if r["va"] not in anchor and r["name"].startswith("_") and not r["name"].startswith("__"):
            n = r["name"][1:]
            f = r["source_file"].replace("guess:", "")
            if f in lfuncs and n in lfuncs[f]:
                anchor[r["va"]] = (f, n)
    proposals = []
    cpp = []  # C++ rows: only the source file (and the Linux symbol as a hint)
    skipped = collections.Counter()
    for f, names in lfuncs.items():
        if not f.lower().endswith(".c"):
            order = sorted(names, key=lambda n: (names[n], n))
            lpos = {n: i for i, n in enumerate(order)}
            pairs = [(widx[va], lpos[n]) for va, (af, n) in anchor.items() if af == f and va in widx and n in lpos]
            chain = lis(pairs)
            for (wa, la), (wb, lb) in zip(chain, chain[1:]):
                ws = list(range(wa, wb + 1))
                ls = order[la:lb + 1]
                if len(ws) != len(ls):
                    skipped["count differs (C++)"] += 1
                    continue
                for wi, n in zip(ws, ls):
                    if rows[wi]["class"] != "LIB":
                        cpp.append((rows[wi]["va"], f, n))
            continue
        order = sorted(names, key=lambda n: (names[n], n))
        lpos = {n: i for i, n in enumerate(order)}
        pairs = [(widx[va], lpos[n]) for va, (af, n) in anchor.items() if af == f and va in widx and n in lpos]
        chain = lis(pairs)
        for (wa, la), (wb, lb) in zip(chain, chain[1:]):
            ws = list(range(wa + 1, wb))
            ls = order[la + 1:lb]
            if not ws and not ls:
                continue
            if len(ws) != len(ls):
                skipped["count differs"] += 1
                continue
            for wi, n in zip(ws, ls):
                r = rows[wi]
                if r["name"] or r["class"] == "LIB":
                    skipped["already named or LIB"] += 1
                    continue
                proposals.append((r["va"], "_" + n, f, rows[wa]["name"] or anchor.get(rows[wa]["va"], ("", "?"))[1],
                                  rows[wb]["name"] or anchor.get(rows[wb]["va"], ("", "?"))[1]))
    print("%d names proposed from source order, %d C++ rows given a file; skipped: %s"
          % (len(proposals), len(cpp), dict(skipped)))
    show = sys.argv[sys.argv.index("--show") + 1] if "--show" in sys.argv else None
    for p in [p for p in proposals if show is None or show in p[2]][:60 if show else 15]:
        print("  %s %-34s %s" % (p[0], p[1], p[2]))
    if not write or not (proposals or cpp):
        return
    name_unit.lock()
    try:
        pp = os.path.join(ROOT, "tools", "symbols", "%s_position.csv" % dll)
        old = list(csv.DictReader(open(pp))) if os.path.exists(pp) else []
        have = {r["va"] for r in old}
        for va, n, f, a, b in proposals:
            if va not in have:
                old.append({"va": va, "name": n, "kind": "func",
                            "evidence": "position in %s between %s and %s (Linux DWARF source order, equal counts)" % (f, a.lstrip("_"), b.lstrip("_"))})
        with open(pp, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=["va", "name", "kind", "evidence"], lineterminator="\n")
            w.writeheader()
            w.writerows(old)
        p = os.path.join(ROOT, "docs", "progress.csv")
        rd = csv.DictReader(open(p, newline="", encoding="utf-8"))
        fields = rd.fieldnames
        allrows = list(rd)
        prop = {va: (n, f) for va, n, f, a, b in proposals}
        cprop = {va: (f, n) for va, f, n in cpp}
        for r in allrows:
            if r["dll"] != dll:
                continue
            if r["va"] in prop:
                n, f = prop[r["va"]]
                r["name"], r["demangled"], r["source_file"] = n, n[1:], f
                if r["class"] == "UNKNOWN" and dll == "hw":
                    r["class"] = "ENGINE"
            elif r["va"] in cprop and (not r["source_file"] or r["source_file"].startswith("guess:")):
                f, n = cprop[r["va"]]
                r["source_file"] = f
                if not r["notes"]:
                    r["notes"] = "linux: %s" % n
        with open(p, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=fields, lineterminator="\n")
            w.writeheader()
            w.writerows(allrows)
    finally:
        name_unit.unlock()


if __name__ == "__main__":
    main()
