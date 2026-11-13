"""Pair retail functions with our compiled functions by identical bytes.

  python tools/bytematch.py [--dll client] [--write] [--std] [--at va,va,...] [<unit> ...]

A retail function that is unnamed in the symbol map (or carries a name
that no object of ours defines) never meets its source counterpart in
objdiff. For every such retail function inside the address range of a
unit, this compares its bytes with every function of our compiled object
for that unit that has no retail counterpart either. Relocated fields are
masked (our relocation offsets, applied to both sides, so the
instructions must be identical); the sizes must be equal (for a function
whose COMDAT also holds jump tables, the retail range runs to the next
function). Only one-to-one matches are proposed: one retail function,
one function of ours, nothing else identical.

--at tests only the given retail addresses (hex, comma separated), with
our function's length, for functions that have no row in progress.csv
(Ghidra merged them into a neighbour).

Prints the proposals; --write stores them in tools/symbols/<dll>_bytes.csv
with their evidence (merged by tools/build.py before <dll>_manual.csv).
Units default to every unit of the dll (hl unless --dll) with both objects.
The address range of a unit runs from the last function of the file
before it to the first function of the file after it, so functions not
yet attributed to any file (client.dll) are candidates too.
"""
import bisect
import csv
import json
import os
import struct
import sys

import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll", "client": "versions/1.6a/cryoffear/cl_dlls/client.dll",
          "hw": "versions/1.6a/hw.dll", "gameui": "versions/1.6a/cryoffear/cl_dlls/GameUI.dll"}
DEFAULT = ("FUN_", "LAB_", "thunk_", "SUB_")


def coff_functions(path):
    """{name: (bytes, set of masked offsets)} for the function symbols of a
    COFF object: one COMDAT section per function (/Gy), or several
    functions in one section (VC6 without /Gy), each running to the next
    function symbol of its section or to the section end."""
    data = open(path, "rb").read()
    _m, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    secs = []
    for i in range(nsec):
        o = 20 + opt + 40 * i
        size, praw, prel, _pl, nrel, _nl, chars = struct.unpack_from("<IIIIHHI", data, o + 16)
        raw = data[praw:praw + size] if praw else b""
        masked = set()
        for k in range(nrel):
            va, _si, typ = struct.unpack_from("<IIH", data, prel + 10 * k)
            masked.update(range(va, va + 4))
        secs.append((raw, masked, chars))
    strtab = symptr + 18 * nsym
    funcs = {}  # section -> [(value, name)]
    i = 0
    while i < nsym:
        o = symptr + 18 * i
        raw = data[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            name = data[strtab + off:data.index(b"\0", strtab + off)].decode("latin-1")
        else:
            name = raw.rstrip(b"\0").decode("latin-1")
        value, secnum, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        if secnum > 0 and typ == 0x20 and cls in (2, 3) and secs[secnum - 1][2] & 0x20:
            funcs.setdefault(secnum, []).append((value, name))
        i += 1 + naux
    out = {}
    for secnum, lst in funcs.items():
        code, masked, _chars = secs[secnum - 1]
        lst.sort()
        for k, (value, name) in enumerate(lst):
            if len(lst) == 1 and value != 0:
                continue
            end = lst[k + 1][0] if k + 1 < len(lst) else len(code)
            out.setdefault(name, (code[value:end], {m - value for m in masked if value <= m < end}))
    return out


def coff_relocs(path):
    """{function name: [(offset, type, target symbol, addend)]} of a COFF
    object: what each relocated field of our code points at."""
    data = open(path, "rb").read()
    _m, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    strtab = symptr + 18 * nsym
    names = {}
    i = 0
    while i < nsym:
        o = symptr + 18 * i
        raw = data[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            names[i] = data[strtab + off:data.index(b"\0", strtab + off)].decode("latin-1")
        else:
            names[i] = raw.rstrip(b"\0").decode("latin-1")
        naux = data[o + 17]
        i += 1 + naux
    secrel = []
    for k in range(nsec):
        o = 20 + opt + 40 * k
        size, praw, prel, _pl, nrel, _nl, chars = struct.unpack_from("<IIIIHHI", data, o + 16)
        rel = []
        for j in range(nrel):
            va, si, typ = struct.unpack_from("<IIH", data, prel + 10 * j)
            addend = struct.unpack_from("<i", data, praw + va)[0] if praw else 0
            rel.append((va, typ, names.get(si, ""), addend))
        secrel.append(rel)
    out = {}
    i = 0
    while i < nsym:
        o = symptr + 18 * i
        value, secnum, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        if secnum > 0 and typ == 0x20 and cls == 2 and value == 0:
            out[names[i]] = secrel[secnum - 1]
        i += 1 + naux
    return out


def relocs_agree(img, base, va, relocs, by_name):
    """None when no relocation target of ours has a retail address, else
    whether every one that has points where the retail code points."""
    known = 0
    for off, typ, sym, addend in relocs:
        targets = by_name.get(sym)
        if not targets or len(targets) != 1:
            continue
        field = struct.unpack_from("<i", img, va - base + off)[0]
        if typ == 0x14:  # IMAGE_REL_I386_REL32
            actual = va + off + 4 + field
            expect = targets[0] + addend
        elif typ == 0x06:  # IMAGE_REL_I386_DIR32
            actual = field & 0xFFFFFFFF
            expect = targets[0] + addend
        else:
            continue
        known += 1
        if actual != expect:
            return False
    return True if known else None


def main():
    argv = sys.argv[1:]
    dll = "hl"
    if "--dll" in argv:
        i = argv.index("--dll")
        dll = argv[i + 1]
        del argv[i:i + 2]
    at = []
    if "--at" in argv:
        i = argv.index("--at")
        at = [int(x, 16) for x in argv[i + 1].split(",")]
        del argv[i:i + 2]
    # --std also names STL instantiations (tools/sync_names.py then makes
    # them class LIB: toolchain code that is not decompiled)
    with_std = "--std" in argv
    args = [a for a in argv if a not in ("--write", "--std")]
    write = "--write" in sys.argv
    cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
    units = args or [u["name"] for u in cfg["units"] if u["name"].startswith(dll + "/")]
    paths = {u["name"]: u for u in cfg["units"]}
    pe = pefile.PE(os.path.join(ROOT, IMAGES[dll]), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    # retail functions: address -> (name, size, source file)
    rows = [r for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))) if r["dll"] == dll]
    starts = sorted(int(r["va"], 16) for r in rows)
    names = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll))):
        names[int(r["va"], 16)] = r["name"]
    by_name = {}
    for va, n in names.items():
        by_name.setdefault(n, []).append(va)
    ours_all = {}
    relocs_all = {}
    for unit in units:
        u = paths.get(unit)
        if not u or not os.path.exists(os.path.join(ROOT, u["base_path"])):
            continue
        # VC6 names every object's dynamic initializers _$E1, _$E2...: the
        # same name in many objects, which a symbol map can not hold
        # (tools/vc6_anon.py respells them _E$1, _E$2... in our objects)
        ours_all[unit] = {n: f for n, f in coff_functions(os.path.join(ROOT, u["base_path"])).items()
                          if not n.lstrip("_").startswith(("$E", "E$"))}
        relocs_all[unit] = coff_relocs(os.path.join(ROOT, u["base_path"]))
    defined = set()
    for funcs in ours_all.values():
        defined.update(funcs)
    proposals = []
    for unit, funcs in ours_all.items():
        mine = [r for r in rows if r["source_file"].replace("guess:", "") == unit.split("/", 1)[1] + ".cpp"
                or r["source_file"].replace("guess:", "") == unit.split("/", 1)[1] + ".c"]
        if not mine:
            continue
        lo = min(int(r["va"], 16) for r in mine)
        hi = max(int(r["va"], 16) for r in mine)
        # widen to the neighbouring files: the rows in between belong to no
        # file yet (non-LIB rows only; the CRT sits after all objects)
        src = unit.split("/", 1)[1]
        others = sorted(int(r["va"], 16) for r in rows if r["source_file"] and r["class"] != "LIB"
                        and os.path.splitext(r["source_file"].replace("guess:", ""))[0] != src)
        k = bisect.bisect_left(others, lo)
        lo = others[k - 1] + 1 if k else lo
        k = bisect.bisect_right(others, hi)
        hi = others[k] - 1 if k < len(others) else hi
        # retail candidates: unpaired functions inside the unit's range
        cands = []
        for r in rows:
            va = int(r["va"], 16)
            if not lo <= va <= hi or not r["size"]:
                continue
            n = names.get(va, "")
            if n and not n.startswith(DEFAULT) and n in defined:
                continue
            k = bisect.bisect_right(starts, va)
            end = starts[k] if k < len(starts) else va + int(r["size"])
            cands.append((va, n, int(r["size"]), end - va))
        if at:
            # addresses given by hand (no function row there, for example
            # where Ghidra merged two functions): sizes come from our side
            cands = [(va, names.get(va, ""), None, None) for va in at if lo - 0x10000 <= va <= hi + 0x10000]
        # our candidates: functions whose name has no retail address
        unpaired = {n: f for n, f in funcs.items() if n not in by_name}
        hits = {}
        for va, rn, size, span in cands:
            for n, (code, masked) in unpaired.items():
                if size is not None and len(code) not in (size, span):
                    continue
                ret = img[va - base:va - base + len(code)]
                if all(ret[i] == code[i] for i in range(len(code)) if i not in masked):
                    hits.setdefault(va, []).append(n)
        # identical bytes with relocations masked can still differ in what
        # the relocations point at (the same code for another class): keep
        # the pairs whose relocation targets agree with the retail code
        rel = relocs_all.get(unit, {})
        ambiguous = {va for va, ns in hits.items() if len(ns) > 1}
        counts = {}
        for va, ns in hits.items():
            for n in ns:
                counts[n] = counts.get(n, 0) + 1
        by_reloc = set()
        for va in list(hits):
            ns = hits[va]
            if va in ambiguous or any(counts[n] > 1 for n in ns):
                good = [n for n in ns if relocs_agree(img, base, va, rel.get(n, []), by_name) is True]
                if good:
                    hits[va] = good
                    by_reloc.add(va)
        rev = {}
        for va, ns in hits.items():
            for n in ns:
                rev.setdefault(n, []).append(va)
        # several of ours that agree with one retail function and with no
        # other: the linker folded them (/OPT:ICF); the first name stands for
        # all of them
        folded = {}
        for va, ns in list(hits.items()):
            agree = all(relocs_agree(img, base, va, rel.get(n, []), by_name) is not False for n in ns)
            if len(ns) > 1 and all(len(rev[n]) == 1 for n in ns) and agree:
                folded[va] = sorted(ns)
                hits[va] = [sorted(ns)[0]]
        for va, ns in sorted(hits.items()):
            # STL instantiations are toolchain code (class LIB) and many are
            # identical to each other
            if len(ns) == 1 and len(rev[ns[0]]) == 1 and ("@std@@" not in ns[0] or with_std):
                old = names.get(va, "")
                note = " (relocation targets agree)" if va in by_reloc else ""
                if va in folded:
                    note = " (folded by /OPT:ICF with %s)" % ", ".join(folded[va][1:])
                proposals.append((va, ns[0], old, unit + note))
    for va, n, old, unit in proposals:
        print("%08X  %-60s (now %s)  %s" % (va, n, old or "unnamed", unit))
    print("%d proposals" % len(proposals))
    if write:
        # earlier proposals are named in the merged map now and are not
        # proposed again: keep them and add the new ones
        path = os.path.join(ROOT, "tools", "symbols", "%s_bytes.csv" % dll)
        rows = {}
        if os.path.exists(path):
            for r in csv.DictReader(open(path)):
                rows[int(r["va"], 16)] = (r["name"], r["kind"], r["evidence"])
        for va, n, old, unit in proposals:
            rows[va] = (n, "func", "bytes identical to %s in %s (relocated fields masked), one-to-one%s"
                        % (n, unit, "; was %s" % old if old else ""))
        with open(path, "w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["va", "name", "kind", "evidence"])
            for va in sorted(rows):
                w.writerow(["%08X" % va] + list(rows[va]))
        print("wrote %s (%d entries)" % (os.path.relpath(path, ROOT), len(rows)))


if __name__ == "__main__":
    main()
