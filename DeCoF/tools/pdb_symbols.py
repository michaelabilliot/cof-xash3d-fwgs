"""Export functions, publics and global data from a PDB to CSV.

Usage: python tools/pdb_symbols.py <file.pdb> <image_base hex> <out.csv>

One row per address. Procedures come from the module symbol streams
(S_GPROC32 / S_LPROC32, which carry the size and the owning object file),
decorated names from S_PUB32, data from S_GDATA32 / S_LDATA32.
"""
import csv
import ctypes
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdbfile  # noqa: E402

_dbghelp = ctypes.windll.dbghelp
_dbghelp.UnDecorateSymbolName.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                          ctypes.c_uint32, ctypes.c_uint32]


def undecorate(name):
    if not name.startswith("?"):
        return name
    buf = ctypes.create_string_buffer(4096)
    if _dbghelp.UnDecorateSymbolName(name.encode("latin-1"), buf, 4096, 0):
        return buf.value.decode("latin-1")
    return name


def obj_of(mod):
    name = mod.obj_name if mod.obj_name.lower().endswith(".lib") else mod.name
    return name.replace("\\", "/")


def main():
    path, base, out = sys.argv[1], int(sys.argv[2], 16), sys.argv[3]
    pdb = pdbfile.PDB(path)

    def section_of(rva):
        for name, va, size in pdb.sections:
            if va <= rva < va + max(size, 1):
                return name
        return ""

    rows = {}
    for mod, kind, rva, length, name in pdb.procedures():
        if rva is None:
            continue
        rows[rva] = {
            "kind": "func" if kind == pdbfile.S_GPROC32 else "func_static",
            "rva": rva, "size": length, "object": obj_of(mod),
            "name": name, "decorated": "",
        }
    # Module index of every contribution, to attach objects to publics
    contribs = sorted((pdb.rva(s, o), sz, m) for s, o, sz, _c, m in pdb.contribs
                      if pdb.rva(s, o) is not None)
    starts = [c[0] for c in contribs]

    import bisect

    def module_at(rva):
        i = bisect.bisect_right(starts, rva) - 1
        if i >= 0 and contribs[i][0] <= rva < contribs[i][0] + contribs[i][1]:
            return obj_of(pdb.modules[contribs[i][2]])
        return ""

    for rva, flags, name in pdb.publics():
        if rva is None:
            continue
        row = rows.get(rva)
        if row is not None:
            if not row["decorated"]:
                row["decorated"] = name
            continue
        rows[rva] = {
            "kind": "public_code" if flags & 1 else "public_data",
            "rva": rva, "size": "", "object": module_at(rva),
            "name": undecorate(name), "decorated": name,
        }
    for _p, kind, d in pdb.global_symbols():
        if kind in (pdbfile.S_GDATA32, pdbfile.S_LDATA32):
            _ti, off, seg = struct.unpack_from("<IIH", d, 0)
            rva = pdb.rva(seg, off)
            if rva is None or rva in rows:
                continue
            rows[rva] = {
                "kind": "data" if kind == pdbfile.S_GDATA32 else "data_static",
                "rva": rva, "size": "", "object": module_at(rva),
                "name": pdbfile._cstr(d, 10)[0], "decorated": "",
            }

    fields = ["va", "rva", "section", "kind", "size", "object", "name",
              "undecorated", "decorated"]
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader()
        for rva in sorted(rows):
            r = rows[rva]
            w.writerow({
                "va": "%08X" % (base + rva), "rva": "%08X" % rva,
                "section": section_of(rva), "kind": r["kind"],
                "size": r["size"], "object": r["object"], "name": r["name"],
                "undecorated": undecorate(r["decorated"]) if r["decorated"] else "",
                "decorated": r["decorated"],
            })
    print("%s: %d symbols -> %s" % (path, len(rows), out))


if __name__ == "__main__":
    main()
