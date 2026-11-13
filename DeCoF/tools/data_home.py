"""Suggest the source file that should define a global no object defines.

  python tools/data_home.py <dll> <decorated name> [...]
  python tools/data_home.py <dll> --link-log     (the unresolved data symbols of the last link)
  --window N   also list the N named globals on each side (any map row, with its object or "-")

The linker places the data of each object together, in link order, so a
global belongs to the object that defines its retail neighbours. For each
name this looks up its retail address in the symbol maps and prints the
nearest named globals below and above it that one of our objects defines,
with that object.
"""
import bisect
import csv
import glob
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def defined_symbols(obj):
    data = open(obj, "rb").read()
    _m, nsec, _ts, symptr, nsym, _opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    strtab = symptr + nsym * 18
    out = set()
    i = 0
    while i < nsym:
        o = symptr + i * 18
        raw = data[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            name = data[strtab + off:data.index(b"\0", strtab + off)]
        else:
            name = raw.rstrip(b"\0")
        _v, sec, typ, sclass, naux = struct.unpack_from("<IhHBB", data, o + 8)
        if sec > 0 and sclass == 2 and typ != 0x20:
            out.add(name.decode("latin-1"))
        i += 1 + naux
    return out


def main():
    dll = sys.argv[1]
    names = sys.argv[2:]
    window = 0
    if "--window" in names:
        i = names.index("--window")
        window = int(names[i + 1])
        del names[i:i + 2]
    if names == ["--link-log"]:
        log = open(os.path.join(ROOT, "build", "link", dll, dll + ".dll.log"), encoding="latin-1").read()
        names = sorted(set(n for n in re.findall(r"unresolved external symbol \"[^\"]*\" \((\?[^)]*@@3[^)]*)\)", log)))
    vas = {}
    for p in glob.glob(os.path.join(ROOT, "tools", "symbols", "%s_*.csv" % dll)):
        if p.endswith(("_forget.csv", "_datasizes.csv")):
            continue
        for r in csv.DictReader(open(p, encoding="latin-1")):
            if r.get("kind", "data") == "data" and r.get("va"):
                vas.setdefault(r["name"], int(r["va"], 16))
    home = {}
    for obj in glob.glob(os.path.join(ROOT, "build", "base", dll, "**", "*.obj"), recursive=True):
        for n in defined_symbols(obj):
            home.setdefault(n, os.path.relpath(obj, os.path.join(ROOT, "build", "base", dll)).replace("\\", "/")[:-4])
    known = sorted((va, n) for n, va in vas.items() if n in home)
    every = sorted((va, n) for n, va in vas.items())
    allkeys = [k[0] for k in every]
    keys = [k[0] for k in known]
    for n in names:
        if n not in vas:
            print("%-50s no retail address in the maps" % n[:50])
            continue
        va = vas[n]
        i = bisect.bisect_left(keys, va)
        lo = known[i - 1] if i > 0 else None
        hi = known[i] if i < len(known) else None
        print("%-50s %08X  below: %s %s  above: %s %s" % (n[:50], va,
              lo and "%08X" % lo[0], lo and home[lo[1]], hi and "%08X" % hi[0], hi and home[hi[1]]))
        if window:
            j = bisect.bisect_left(allkeys, va)
            for a, m in every[max(0, j - window):j + window]:
                print("    %08X %-50s %s" % (a, m[:50], home.get(m, "-")))


if __name__ == "__main__":
    main()
