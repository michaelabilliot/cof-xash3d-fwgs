"""Source file and line of every procedure in a VC 7.x debug PDB.

  python tools/pdb_lines.py <pdb> <out.csv>

Columns: file (path below the source root), line (first line of the
function, from the C11 line table), name (undecorated), decorated (from
the public symbol at the same address), rva, size, module.

The 1.0, 1.4 and 1.55 hl.pdb files come from debug builds, so every
function, including inline functions emitted from headers, has a line
number. This gives the order of the functions inside each original source
file and the line of each inline function in the headers.
"""
import bisect
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdbfile  # noqa: E402


def short(path):
    p = path.replace("\\", "/")
    m = re.search(r"(?i)/sourcecode/(.*)$", p)
    return m.group(1) if m else p


def main():
    pdb = pdbfile.PDB(sys.argv[1])
    publics = {}
    for rva, _flags, name in pdb.publics():
        publics.setdefault(rva, name)
    rows = []
    lines_cache = {}
    for mod, _kind, rva, length, name in pdb.procedures():
        if mod.index not in lines_cache:
            lines = pdb.module_lines(mod)
            lines_cache[mod.index] = (lines, [x[1] for x in lines])
        lines, rvas = lines_cache[mod.index]
        i = bisect.bisect_left(rvas, rva)
        if i < len(lines) and lines[i][1] < rva + max(length, 1):
            f, line = short(lines[i][0]), lines[i][2]
        else:
            f, line = "", 0
        rows.append((f, line, name, publics.get(rva, ""), "%08X" % rva, length,
                     mod.name.replace("\\", "/")))
    rows.sort(key=lambda r: (r[0].lower(), r[1], r[4]))
    with open(sys.argv[2], "w", newline="") as fh:
        w = csv.writer(fh, lineterminator="\n")
        w.writerow(["file", "line", "name", "decorated", "rva", "size", "module"])
        w.writerows(rows)
    print("%d procedures, %d with a line" % (len(rows), sum(1 for r in rows if r[1])))


if __name__ == "__main__":
    main()
