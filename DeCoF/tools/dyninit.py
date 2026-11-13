"""Name the dynamic initializers of a DLL (??__E<global> and ??__F<global>).

  python tools/dyninit.py <dll> [--write] [--pair <va>=<name> ...]

MSVC emits, for every global with a constructor or destructor, a dynamic
initializer ??__E<name>@@YAXXZ (construct, then atexit of the destructor
thunk) in .text$di at the start of the code, and the destructor thunk
??__F<name>@@YAXXZ in .text$yd near its end. They are static symbols of
the object that defines the global, so they are compiled anyway; they only
need their retail address in the symbol map and a place in the unit's
function list to be scored.

Pairing works on the symbols each function points at (_atexit left out):
- first the destructor thunks (global and destructor),
- then the initializers (global, constructor and the destructor thunk
  they register, named by the first pass), so an initializer that only
  registers a thunk (trivial constructor, gHUD) pairs too.
A retail target the map does not name yet matches any one of our targets;
when a pairing is unique that also names the global. The retail .text$di
area is scanned directly (it may lack progress rows). Prints the pairings;
--write records the names in tools/symbols/<dll>_manual.csv, appends the
functions to their unit lists and adds missing progress.csv rows.
--pair adds a pairing checked by hand (a thunk with an exception frame, a
vtable Ghidra spells differently).
"""
import csv
import bisect
import glob
import json
import os
import struct
import sys

import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import name_unit  # noqa: E402

IMAGES = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll", "client": "versions/1.6a/cryoffear/cl_dlls/client.dll"}


def obj_initializers(path):
    """{??__E/??__F name: set of relocation target names} of one object."""
    data = open(path, "rb").read()
    _m, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    strtab = symptr + 18 * nsym
    names, sec_of = [], {}
    i = 0
    while i < nsym:
        o = symptr + 18 * i
        raw = data[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            name = data[strtab + off:data.index(b"\0", strtab + off)].decode("latin-1")
        else:
            name = raw.rstrip(b"\0").decode("latin-1")
        value, sec, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        names.append(name)
        names.extend([None] * naux)
        if name.startswith(("??__E", "??__F")) and sec > 0 and typ == 0x20:
            sec_of[name] = sec
        i += 1 + naux
    out = {}
    for name, sec in sec_of.items():
        o = 20 + opt + 40 * (sec - 1)
        _size, _praw, prel, _pl, nrel = struct.unpack_from("<IIIIH", data, o + 16)
        targets = set()
        for k in range(nrel):
            _va, si, _typ = struct.unpack_from("<IIH", data, prel + 10 * k)
            t = names[si]
            if t and t != "_atexit" and not t.startswith(("??__E", "$")):
                targets.add(t)
        out[name] = targets
    return out


def scan_small(img, base, va, limit):
    """Length of a tiny initializer at va (mov ecx,imm / push imm / call /
    jmp / pop ecx / ret), or 0 when it uses anything else."""
    k = va - base
    start = k
    while k - start < limit:
        op = img[k]
        if op in (0xB9, 0x68, 0xE8):
            k += 5
        elif op == 0xC7 and img[k + 1] == 0x05:  # mov dword [abs], imm (an inlined vftable store)
            k += 10
        elif op == 0xE9:
            return k + 5 - start
        elif op == 0x59:
            k += 1
        elif op == 0xC3:
            return k + 1 - start
        else:
            return 0
    return 0


def main():
    dll = sys.argv[1]
    write = "--write" in sys.argv
    cfg = json.load(open(os.path.join(ROOT, "tools", "units.json")))
    unit_of_obj = {os.path.normcase(os.path.join(ROOT, "build", "base", dll, u["name"] + ".obj")): u
                   for u in cfg["dlls"][dll]["units"]}
    ours = {}
    for path in glob.glob(os.path.join(ROOT, "build", "base", dll, "**", "*.obj"), recursive=True):
        u = unit_of_obj.get(os.path.normcase(path))
        if u is not None:
            for name, targets in obj_initializers(path).items():
                ours[name] = (u, targets)
    names = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll))):
        names[int(r["va"], 16)] = r["name"]
    prog = [r for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"), encoding="utf-8"))
            if r["dll"] == dll]
    rows = {int(r["va"], 16): (int(r["size"] or 0), r["name"]) for r in prog}
    pe = pefile.PE(os.path.join(ROOT, IMAGES[dll]))
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    relocs = sorted(base + e.rva for blk in pe.DIRECTORY_ENTRY_BASERELOC for e in blk.entries if e.type == 3)
    text = [sec for sec in pe.sections if sec.Name.startswith(b".text")][0]
    tlo = base + text.VirtualAddress

    # candidate retail functions: small progress rows, plus the .text$di area
    cand = {va: size for va, (size, name) in rows.items() if 0 < size <= 64}
    di_end = min(va for va, (size, name) in rows.items()
                 if va > tlo and name and not name.startswith("??__E"))
    va = tlo
    while va < di_end:
        if img[va - base] == 0xCC:
            va += 1
            continue
        n = scan_small(img, base, va, 64)
        if n:
            cand.setdefault(va, n)
            va += n
        elif va in rows:
            va += max(rows[va][0], 1)
        else:
            va += 1

    # the .text$yd area (destructor thunks) near the end of the code
    yd = [va for va, (size, name) in rows.items() if name.startswith("??__F") or "initlocks" in name]
    if yd:
        va = min(yd) - 0x40
        text_end = tlo + text.Misc_VirtualSize
        while va < text_end:
            if img[va - base] in (0xCC, 0x00):
                va += 1
                continue
            n = scan_small(img, base, va, 64)
            if n:
                cand.setdefault(va, n)
                va += n
            else:
                va += 1

    def targets_of(va, size, fnames):
        out = set()
        i = bisect.bisect_left(relocs, va)
        while i < len(relocs) and relocs[i] < va + size:
            t = struct.unpack_from("<I", img, relocs[i] - base)[0]
            out.add(fnames.get(t) or names.get(t) or "%08X" % t)
            i += 1
        code = img[va - base:va - base + size]
        for k in range(len(code) - 4):
            if code[k] in (0xE8, 0xE9):
                t = (va + k + 5 + struct.unpack_from("<i", code, k + 1)[0]) & 0xFFFFFFFF
                out.add(fnames.get(t) or names.get(t) or "%08X" % t)
        return out

    def pair(kind, fnames):
        """{our name: (retail va, {hex target: our name})} for one kind.
        Initializers come from the .text$di area, destructor thunks are the
        other functions of the thunk shape that do not call _atexit."""
        retail = {}
        for va, size in cand.items():
            in_di = va < di_end
            if (kind == "??__E") != in_di:
                continue
            if not in_di and not 0 < scan_small(img, base, va, 64) <= size:
                continue
            t = targets_of(va, size, fnames)
            if kind == "??__F" and "_atexit" in t:
                continue
            t.discard("_atexit")
            t = {x for x in t if "initlocks" not in x}  # the old wrong labels of the thunks
            retail[va] = t
        found = {}
        for name, (u, ot) in ours.items():
            if not name.startswith(kind):
                continue
            exact, loose = [], []
            for va, rt in retail.items():
                hexes = sorted(x for x in rt if len(x) == 8 and all(c in "0123456789ABCDEF" for c in x))
                named = rt - set(hexes)
                if not named <= ot or len(ot) != len(rt):
                    continue
                rest = sorted(ot - named)
                if not hexes:
                    exact.append((va, {}))
                elif len(hexes) == 1 and len(rest) == 1:
                    loose.append((va, {hexes[0]: rest[0]}))
            hits = exact or loose
            if len(hits) == 1:
                found[name] = hits[0]
            elif len(hits) > 1:
                print("  ambiguous %s: %s" % (name, " ".join("%08X" % h[0] for h in hits)))
        return found

    fpairs = pair("??__F", {})
    fnames = {va: name for name, (va, _x) in fpairs.items()}
    epairs = pair("??__E", fnames)
    found = dict(fpairs)
    found.update(epairs)
    for arg in sys.argv[2:]:
        if "=" in arg and not arg.startswith("--"):
            va_s, name = arg.split("=", 1)
            if name in ours:
                found[name] = (int(va_s, 16), {})
                size = scan_small(img, base, int(va_s, 16), 64) or rows.get(int(va_s, 16), (0, ""))[0]
                cand.setdefault(int(va_s, 16), size)
    taken = {}
    for name, (va, glob_names) in sorted(found.items(), key=lambda x: x[1][0]):
        if va in taken:
            print("  conflict at %08X: %s and %s" % (va, taken[va], name))
            continue
        taken[va] = name
    data_names = {}
    for name, (va, glob_names) in found.items():
        for hx, ourname in (glob_names or {}).items():
            if ourname.startswith("?") and "@@3" in ourname:
                data_names[int(hx, 16)] = ourname
    for va, name in sorted(taken.items()):
        old = rows.get(va, (0, None))[1]
        if old != name:
            print("  %08X %-48s %s%s" % (va, name, ours[name][0]["name"], " (no row)" if va not in rows else ""))
    for va, name in sorted(data_names.items()):
        print("  data %08X %s" % (va, name))
    missing = [n for n in ours if n not in found]
    print("%d of %d initializers paired; not paired: %s" % (len(found), len(ours), " ".join(sorted(missing))))
    if not write:
        return
    name_unit.lock()
    try:
        p = os.path.join(ROOT, "tools", "symbols", "%s_manual.csv" % dll)
        old = list(csv.DictReader(open(p, encoding="latin-1")))
        fields = list(old[0].keys())
        have = {r["va"]: r for r in old}
        for va, name in sorted(taken.items()):
            what = "dynamic initializer" if name.startswith("??__E") else "atexit destructor thunk"
            row = {"va": "%08X" % va, "name": name, "kind": "func",
                   "evidence": "%s: same global, constructor or destructor and thunk as ours (tools/dyninit.py)" % what}
            if row["va"] in have:
                have[row["va"]].update(row)
            else:
                old.append(row)
        for va, name in sorted(data_names.items()):
            if "%08X" % va not in have:
                old.append({"va": "%08X" % va, "name": name, "kind": "data",
                            "evidence": "the global its dynamic initializer and destructor thunk construct and destroy (tools/dyninit.py)"})
        with open(p, "w", newline="", encoding="latin-1") as f:
            w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
            w.writeheader()
            w.writerows(old)
        for va, name in taken.items():
            u = ours[name][0]
            lst = os.path.join(ROOT, u["functions_file"])
            text_ = open(lst, newline="").read()
            if name not in text_.split():
                nl = "\r\n" if "\r\n" in text_ else "\n"
                if text_ and not text_.endswith(nl):
                    text_ += nl
                open(lst, "w", newline="").write(text_ + name + nl)
        # progress rows
        pp = os.path.join(ROOT, "docs", "progress.csv")
        allrows = list(csv.DictReader(open(pp, encoding="utf-8", newline="")))
        pfields = list(allrows[0].keys())
        idx = {(r["dll"], int(r["va"], 16)): r for r in allrows}
        added = 0
        for va, name in taken.items():
            u = ours[name][0]
            src = os.path.relpath(os.path.join(ROOT, u["source"]), os.path.join(ROOT, "src")).replace("\\", "/")
            r = idx.get((dll, va))
            if r is None:
                r = {f: "" for f in pfields}
                r.update(dll=dll, va="%08X" % va, size=str(cand[va]), status="TODO", evidence="tools/dyninit.py")
                r["class"] = "UNKNOWN"
                allrows.append(r)
                added += 1
            r["name"] = name
            r["demangled"] = ("`dynamic initializer for '%s''" if name.startswith("??__E")
                              else "`dynamic atexit destructor for '%s''") % name[5:-7]
            r["source_file"] = src
            if r["class"] == "LIB":
                r["class"] = "UNKNOWN"
        order = {}
        for i, r in enumerate(allrows):
            order.setdefault(r["dll"], i)
        allrows.sort(key=lambda r: (order[r["dll"]], int(r["va"], 16)))
        with open(pp, "w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=pfields, lineterminator="\n")
            w.writeheader()
            w.writerows(allrows)
        print("recorded; %d progress rows added" % added)
    finally:
        name_unit.unlock()


if __name__ == "__main__":
    main()
