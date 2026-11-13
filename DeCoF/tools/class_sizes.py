"""sizeof() of every entity class in hl.dll 1.55 and 1.6a.

Usage: python tools/class_sizes.py [--out docs/class_sizes.csv]

Each LINK_ENTITY_TO_CLASS export calls GetClassPtr<T>, which allocates the
private data with g_engfuncs.pfnPvAllocEntPrivateData(edict, sizeof(T)).
The size is the constant pushed before that call:
  - 1.6a (release): GetClassPtr is inlined into the export;
  - 1.55 (debug, PDB): the export jumps (ILT) to the classname function,
    which calls GetClassPtr<T> (named in the PDB); the push is there.
"""
import argparse
import csv
import os
import re
import sys

import capstone
import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hdrstruct import members  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLOT = members(os.path.join(ROOT, "src", "engine", "eiface.h"), "} enginefuncs_t;").index(
    "pfnPvAllocEntPrivateData")
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


class Img(object):
    def __init__(self, path):
        self.pe = pefile.PE(path, fast_load=True)
        self.pe.parse_data_directories(directories=[0])
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.img = self.pe.get_memory_mapped_image()
        self.exports = {e.name.decode(): self.base + e.address
                        for e in self.pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}

    def code(self, va, n=0x200):
        o = va - self.base
        return list(MD.disasm_lite(self.img[o:o + n], va))


def resolve(img, va):
    """Follow incremental-link jump stubs (jmp rel32) to the real function."""
    for _ in range(3):
        ins = img.code(va, 8)
        if ins and ins[0][2] == "jmp" and ins[0][3].startswith("0x"):
            va = int(ins[0][3], 16)
        else:
            break
    return va


def alloc_size(img, va, table, depth=0, follow=None):
    """Constant pushed before 'call [table+SLOT*4]' in the function at va."""
    target = "dword ptr [0x%x]" % (table + 4 * SLOT)
    insns = img.code(va)
    loaded = set()
    for i, (a, s, m, o) in enumerate(insns):
        if m == "jmp" and o.startswith("0x") and i == 0 and depth < 3:
            return alloc_size(img, int(o, 16), table, depth + 1, follow)
        if m == "mov" and o.endswith(target):
            loaded.add(o.split(",")[0])
        if m == "call" and (o == target or o in loaded):
            pushes = []
            for a2, s2, m2, o2 in reversed(insns[max(0, i - 8):i]):
                if m2 == "push":
                    pushes.append(o2)
            # second argument pushed first (cdecl, right to left): size is the
            # farthest of the two pushes
            if len(pushes) >= 2 and re.fullmatch(r"0x[0-9a-f]+|\d+", pushes[1]):
                return int(pushes[1], 0)
            return None
        if m == "call" and o.startswith("0x") and follow and depth < 3:
            callee = resolve(img, int(o, 16))
            if follow(callee):
                r = alloc_size(img, callee, table, depth + 1, follow)
                if r:
                    return r
        if m.startswith("ret"):
            break
    return None


def debug_size(img, getclassptr_va, syms):
    """1.55 debug build: GetClassPtr<T> pushes sizeof(T) for CBaseEntity::operator new."""
    insns = img.code(getclassptr_va, 0x200)
    for i, (a, s, m, o) in enumerate(insns):
        if m == "call" and o.startswith("0x") and "operator new" in syms.get(resolve(img, int(o, 16)), ""):
            for a2, s2, m2, o2 in reversed(insns[max(0, i - 4):i]):
                if m2 == "push" and re.fullmatch(r"0x[0-9a-f]+|\d+", o2):
                    return int(o2, 0)
            return None
        if m.startswith("ret"):
            break
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "class_sizes.csv"))
    args = ap.parse_args()
    new = Img(os.path.join(ROOT, "versions", "1.6a", "cryoffear", "cl_dlls", "hl.dll"))
    old = Img(os.path.join(ROOT, "versions", "1.55", "cl_dlls", "hl.dll"))
    syms = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "docs", "symbols_1.55.csv"))):
        syms[int(r["va"], 16)] = r["name"]
    table_new = 0x10222F48
    table_old = next(int(r["va"], 16) for r in csv.DictReader(open(os.path.join(ROOT, "docs", "symbols_1.55.csv")))
                     if r["name"].endswith("g_engfuncs"))

    def is_getclassptr(va):
        return "GetClassPtr" in syms.get(va, "")

    def class_of(va, depth=0):
        for a, s, m, o in old.code(va):
            if m == "jmp" and o.startswith("0x") and depth < 3:
                return class_of(int(o, 16), depth + 1)
            if m == "call" and o.startswith("0x"):
                n = syms.get(resolve(old, int(o, 16)), "")
                mm = re.search(r"GetClassPtr<(?:class |struct )?(\w+)>", n)
                if mm:
                    return mm.group(1)
            if m.startswith("ret"):
                break
        return ""

    def getclassptr_of(va, depth=0):
        for a, s, m, o in old.code(resolve(old, va), 0x80):
            if m == "call" and o.startswith("0x"):
                t = resolve(old, int(o, 16))
                if "GetClassPtr" in syms.get(t, ""):
                    return t
            if m.startswith("ret"):
                break
        return None

    rows = []
    names = sorted(n for n in set(new.exports) | set(old.exports) if not n.startswith("?") and
                   n not in ("GiveFnptrsToDll", "GetEntityAPI", "GetEntityAPI2", "DelayedUse"))
    for n in names:
        s_new = alloc_size(new, new.exports[n], table_new) if n in new.exports else None
        s_old = None
        if n in old.exports:
            gcp = getclassptr_of(old.exports[n])
            s_old = debug_size(old, gcp, syms) if gcp else None
        cls = class_of(old.exports[n]) if n in old.exports else ""
        rows.append({"classname": n, "class": cls,
                     "size_1.55": "0x%X" % s_old if s_old else "",
                     "size_1.6a": "0x%X" % s_new if s_new else "",
                     "delta": (s_new - s_old) if s_new and s_old else ""})
    with open(args.out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()), lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    both = [r for r in rows if r["delta"] != ""]
    changed = [r for r in both if r["delta"] != 0]
    print("classnames %d, sizes in both %d, changed %d -> %s" % (len(rows), len(both), len(changed), args.out))


if __name__ == "__main__":
    main()
