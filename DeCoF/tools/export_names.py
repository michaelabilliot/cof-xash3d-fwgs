"""Record a DLL's export names in its symbol map.

  python tools/export_names.py <dll> [--write]

An export names its address for certain. Decorated exports (C++ `?...`,
stdcall `_name@N`) are the object symbol already; a plain C export (cdecl,
`CreateInterface`) is `_name` in the object. Every export whose address no
map row names yet goes to tools/symbols/<dll>_manual.csv (kind func in
the code section, data elsewhere: exported vtables and variables).
"""
import csv
import glob
import os
import sys

import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import name_unit  # noqa: E402

IMAGES = {"hw": "versions/1.6a/hw.dll", "gameui": "versions/1.6a/cryoffear/cl_dlls/GameUI.dll",
          "client": "versions/1.6a/cryoffear/cl_dlls/client.dll", "hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll"}


def main():
    dll = sys.argv[1]
    write = "--write" in sys.argv
    pe = pefile.PE(os.path.join(ROOT, IMAGES[dll]))
    base = pe.OPTIONAL_HEADER.ImageBase
    text = [sec for sec in pe.sections if sec.Name.startswith(b".text")][0]
    tlo, thi = base + text.VirtualAddress, base + text.VirtualAddress + text.Misc_VirtualSize
    named = set()
    for p in glob.glob(os.path.join(ROOT, "tools", "symbols", "%s_*.csv" % dll)):
        if p.endswith(("_forget.csv", "_datasizes.csv")):
            continue
        for r in csv.DictReader(open(p, encoding="latin-1")):
            if r.get("va"):
                named.add(r["va"].upper())
    new = []
    for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if not s.name:
            continue
        n = s.name.decode()
        sym = n if n.startswith(("?", "_")) else "_" + n
        va = "%08X" % (base + s.address)
        if va not in named:
            new.append((va, sym, n, "func" if tlo <= base + s.address < thi else "data"))
            named.add(va)
    print("%d exports without a map name" % len(new))
    for va, sym, n, kind in new[:8]:
        print("  %s %s" % (va, sym))
    if not write:
        return
    name_unit.lock()
    try:
        p = os.path.join(ROOT, "tools", "symbols", "%s_manual.csv" % dll)
        rows = list(csv.DictReader(open(p, encoding="latin-1")))
        fields = list(rows[0].keys())
        for va, sym, n, kind in new:
            rows.append({"va": va, "name": sym, "kind": kind, "evidence": "export %s" % n})
        with open(p, "w", newline="", encoding="latin-1") as f:
            w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
            w.writeheader()
            w.writerows(rows)
    finally:
        name_unit.unlock()
    print("recorded")


if __name__ == "__main__":
    main()
