"""Compare class layouts of our headers with the CoF PDB types.

  python tools/layoutcheck.py [--pdb PDB] [--dll hl] [--all] [class ...]

Compiles the units of <dll> syntax only (/Zs) with /d1reportAllClassLayout,
parses the layouts the compiler prints and compares every class that also
exists in the PDB: size, direct data members (name and offset), base
classes and the virtual function table (names per slot). Prints one line
per class that matches and the differences for the ones that do not.

The PDB is a 1.55 build (VC 7.1). In 1.6a CBaseEntity has one more
member, so the expected size and offsets of everything after it shift by 4
(see docs/structs.md); --delta CLASS:OFFSET:BYTES applies such a shift to
the PDB side before comparing.
"""
import argparse
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import msvc  # noqa: E402
import pdbfile  # noqa: E402
import pdbtypes  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def compile_layouts(dll, sources):
    cfg = json.load(open(os.path.join(ROOT, "tools", "units.json")))["dlls"][dll]
    layouts = {}
    vtables = {}
    for src in sources:
        cmd = [msvc.cl_path(), "/nologo", "/c", "/Zs", "/d1reportAllClassLayout", "/EHsc", "/GR"]
        cmd += ["/D" + x for x in cfg["defines"]] + ["/I" + os.path.join(ROOT, x) for x in cfg["includes"]]
        cmd.append(os.path.join(ROOT, src))
        r = subprocess.run(cmd, env=msvc.env(), capture_output=True, text=True, cwd=ROOT,
                           errors="replace")
        parse(r.stdout, layouts, vtables)
    return layouts, vtables


CLASS_RE = re.compile(r"^(?:class|struct|union) (\S+)\s+size\((\d+)\):")
VT_RE = re.compile(r"^(\S+)::\$vftable@(.*):$")


def parse(text, layouts, vtables):
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        m = CLASS_RE.match(lines[i])
        if m:
            name, size = m.group(1), int(m.group(2))
            members, bases = [], []
            i += 2  # skip "+---"
            while i < len(lines) and not lines[i].strip().startswith("+---"):
                row = lines[i]
                mm = re.match(r"^\s*(\d+)\s*\|(.*)$", row)
                if mm:
                    off, rest = int(mm.group(1)), mm.group(2)
                    depth = 0
                    while rest.startswith(" |"):
                        rest = rest[2:]
                        depth += 1
                    rest = rest.strip()
                    if depth == 0:
                        bm = re.match(r"^\+--- \(base class (.*)\)$", rest)
                        if bm:
                            bases.append((off, bm.group(1)))
                        elif rest not in ("{vfptr}", "{vbptr}") and not rest.startswith("<alignment"):
                            members.append((off, rest.split()[-1]))
                i += 1
            layouts.setdefault(name, (size, members, bases))
            continue
        m = VT_RE.match(lines[i])
        if m and not m.group(2):
            name = m.group(1)
            slots = []
            i += 3
            while i < len(lines) and re.match(r"^\s*\d+\s*\|", lines[i]):
                fn = lines[i].split("|", 1)[1].strip().lstrip("&").split()[0] if lines[i].split("|", 1)[1].strip() else ""
                slots.append(fn.split("::")[-1])
                i += 1
            vtables.setdefault(name, slots)
            continue
        i += 1


def pdb_layout(types, ti, deltas):
    u = types.udt(ti)
    fl = types.fields(u["field"]) if u["field"] else []
    size = u["size"]
    members = [(off, name) for k, _a, _t, off, name in fl if k == "member"]
    bases = [(off, types.name(t)) for k, _a, t, off, _n in fl if k == "base"]
    return size, members, bases


def pdb_vtable(types, ti, memo):
    """Slot names of the vtable of class ti (primary base chain)."""
    if ti in memo:
        return memo[ti]
    u = types.udt(ti)
    fl = types.fields(u["field"]) if u["field"] else []
    slots = []
    for k, _a, t, off, _n in fl:
        if k == "base" and off == 0:
            bti = types.resolve(t)
            slots = list(pdb_vtable(types, bti, memo))
            break
    methods = [f for f in fl if f[0] == "method"]
    seen = {}
    for k, a, t, vbo, name in methods:
        if vbo is not None:
            n = seen.get((name, vbo), 0)
            seen[(name, vbo)] = n + 1
            slot = (vbo - 4 * n) // 4
            while len(slots) <= slot:
                slots.append("?")
            slots[slot] = name
    memo[ti] = slots
    return slots


def apply_deltas(name, layout, deltas, types):
    """Shift offsets for classes derived from a class that grew."""
    size, members, bases = layout
    shift_at = []
    for cls, at, nbytes in deltas:
        if cls == name or derives(types, name, cls):
            shift_at.append((at, nbytes))
    for at, nbytes in shift_at:
        members = [(o + nbytes if o >= at else o, n) for o, n in members]
        bases = [(o + nbytes if o > at else o, n) for o, n in bases]
        size += nbytes
    return size, members, bases


def derives(types, name, base, depth=0):
    ti = types.by_name.get(name)
    if not ti or depth > 20:
        return False
    u = types.udt(ti)
    for k, _a, t, _o, _n in (types.fields(u["field"]) if u["field"] else []):
        if k == "base":
            bn = types.name(t)
            if bn == base or derives(types, bn, base, depth + 1):
                return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pdb", default="versions/1.55/cl_dlls/hl.pdb")
    ap.add_argument("--dll", default="hl")
    ap.add_argument("--src", action="append", help="sources to compile (default: all units)")
    ap.add_argument("--delta", action="append", default=[], help="CLASS:OFFSET:BYTES")
    ap.add_argument("--all", action="store_true", help="also print matching classes")
    ap.add_argument("classes", nargs="*")
    args = ap.parse_args()
    cfg = json.load(open(os.path.join(ROOT, "tools", "units.json")))["dlls"][args.dll]
    sources = args.src or [u["source"] for u in cfg["units"] if u["source"].endswith((".cpp", ".c"))]
    layouts, vtables = compile_layouts(args.dll, sources)
    types = pdbtypes.Types(pdbfile.PDB(os.path.join(ROOT, args.pdb)))
    deltas = []
    for d in args.delta:
        c, o, b = d.split(":")
        deltas.append((c, int(o, 0), int(b, 0)))
    memo = {}
    bad = 0
    names = args.classes or sorted(layouts)
    for name in names:
        ti = types.by_name.get(name)
        if name not in layouts:
            print("%-32s not in our headers" % name)
            continue
        if not ti:
            continue
        want = apply_deltas(name, pdb_layout(types, ti, deltas), deltas, types)
        have = layouts[name]
        diffs = []
        if want[0] != have[0]:
            diffs.append("size 0x%X, ours 0x%X" % (want[0], have[0]))
        if [b[1] for b in want[2]] != [b[1] for b in have[2]]:
            diffs.append("bases %s, ours %s" % ([b[1] for b in want[2]], [b[1] for b in have[2]]))
        wm, hm = want[1], have[1]
        if wm != hm:
            wn = [n for _o, n in wm]
            hn = [n for _o, n in hm]
            missing = [n for n in wn if n not in hn]
            extra = [n for n in hn if n not in wn]
            first = next((i for i, (a, b) in enumerate(zip(wm, hm)) if a != b), min(len(wm), len(hm)))
            if missing:
                diffs.append("missing %s" % ", ".join(missing))
            if extra:
                diffs.append("extra %s" % ", ".join(extra))
            if not missing and not extra:
                diffs.append("order/offset differs from member %d (%s)" % (
                    first, wm[first] if first < len(wm) else "end"))
        wv = pdb_vtable(types, ti, memo)
        hv = vtables.get(name)
        if hv is not None and wv and hv != wv:
            first = next((i for i, (a, b) in enumerate(zip(wv, hv)) if a != b), min(len(wv), len(hv)))
            diffs.append("vtable %d slots, ours %d, first difference at slot %d (%s vs %s)" % (
                len(wv), len(hv), first, wv[first] if first < len(wv) else "-",
                hv[first] if first < len(hv) else "-"))
        if diffs:
            bad += 1
            print("%-32s DIFF  %s" % (name, "; ".join(diffs)))
        elif args.all:
            print("%-32s ok" % name)
    print("%d classes differ" % bad)


if __name__ == "__main__":
    main()
