"""Sizes of named global data, for defining their extent in Ghidra.

  python tools/data_sizes.py <dll> [--pdb PDB]

Writes tools/symbols/<dll>_datasizes.csv (name,size,source). Sizes come
from our compiled objects in build/base/<dll>/ (the layout we build) and,
for names no object defines yet, from the global data symbols of the PDB.
tools/build.py adds them to the symbol map; DelinkUnits.java then defines
each named global as a byte array of that size, so references into the
middle of a global (g_engfuncs+0xF4, g_vecZero+8) become offsets from its
symbol instead of separate DAT_ labels.
"""
import argparse
import csv
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdbfile  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

IMAGE_SCN_CNT_CODE = 0x20
IMAGE_SCN_CNT_INITIALIZED_DATA = 0x40
IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x80
IMAGE_SCN_LNK_COMDAT = 0x1000
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3


def coff_data_sizes(path):
    data = open(path, "rb").read()
    _m, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    secs = []
    for i in range(nsec):
        o = 20 + opt + 40 * i
        name = data[o:o + 8]
        size, _p, _r, _l, _nr, _nl, chars = struct.unpack_from("<IIIIHHI", data, o + 16)
        secs.append((name, size, chars))
    strtab = symptr + 18 * nsym
    syms = []
    i = 0
    while i < nsym:
        o = symptr + 18 * i
        raw = data[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            end = data.index(b"\0", strtab + off)
            name = data[strtab + off:end].decode("latin-1")
        else:
            name = raw.rstrip(b"\0").decode("latin-1")
        value, secnum, _typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        syms.append((name, value, secnum, cls))
        i += 1 + naux
    out = {}
    by_sec = {}
    for name, value, secnum, cls in syms:
        if cls not in (IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC):
            continue
        if secnum == 0 and cls == IMAGE_SYM_CLASS_EXTERNAL and value > 0:
            out[name] = value  # common symbol: value is the size
            continue
        if secnum <= 0:
            continue
        _sname, ssize, chars = secs[secnum - 1]
        if chars & IMAGE_SCN_CNT_CODE or not chars & (IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_CNT_UNINITIALIZED_DATA):
            continue
        if name.startswith(".") or name.startswith("$"):
            continue
        by_sec.setdefault(secnum, []).append((value, name))
    for secnum, lst in by_sec.items():
        ssize = secs[secnum - 1][1]
        lst.sort()
        for k, (value, name) in enumerate(lst):
            nxt = lst[k + 1][0] if k + 1 < len(lst) else ssize
            if nxt > value:
                out[name] = nxt - value
    return out


def pdb_data_sizes(path):
    import pdbtypes
    pdb = pdbfile.PDB(path)
    types = pdbtypes.Types(pdb)
    publics = {}
    for rva, _f, name in pdb.publics():
        publics.setdefault(rva, name)
    out = {}
    for _p, kind, d in pdb.global_symbols():
        if kind == pdbfile.S_GDATA32:
            ti, off, seg = struct.unpack_from("<IIH", d, 0)
            rva = pdb.rva(seg, off)
            name = publics.get(rva)
            if not name:
                continue
            try:
                size = types.size(ti)
            except (KeyError, ValueError, struct.error):
                size = None
            if size:
                out[name] = size
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dll")
    ap.add_argument("--pdb", default=None)
    args = ap.parse_args()
    pdb = args.pdb or {"hl": "versions/1.55/cl_dlls/hl.pdb"}.get(args.dll)
    sizes = {}
    if pdb:
        for name, size in pdb_data_sizes(os.path.join(ROOT, pdb)).items():
            sizes[name] = (size, "pdb")
    for obj in glob.glob(os.path.join(ROOT, "build", "base", args.dll, "**", "*.obj"), recursive=True):
        for name, size in coff_data_sizes(obj).items():
            # object sizes run to the next symbol and include alignment
            # padding; the PDB type size is exact when they only differ by it
            if name in sizes and sizes[name][1] == "pdb" and 0 <= size - sizes[name][0] <= 3:
                continue
            sizes[name] = (size, "obj")
    out = os.path.join(ROOT, "tools", "symbols", "%s_datasizes.csv" % args.dll)
    with open(out, "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["name", "size", "source"])
        for name in sorted(sizes):
            w.writerow([name, sizes[name][0], sizes[name][1]])
    print("wrote %d sizes to %s" % (len(sizes), os.path.relpath(out, ROOT)))


if __name__ == "__main__":
    main()
