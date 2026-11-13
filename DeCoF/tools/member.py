"""Member at an offset of a class, from the compiler's layout of our headers.

  python tools/member.py <class> [offset ...]
      no offset: print the flattened layout (base members included)
  python tools/member.py --src src/dlls/cof_glock.cpp <class> 0x170

Compiles one source file syntax only with /d1reportSingleClassLayout<class>
and flattens the nested base classes, so an offset seen in the decompiler
(this + 0x170) maps to the member name to write (custom_w_model). The
layout is the one we build, which matches 1.6a for every class that
tools/layoutcheck.py reports as equal.
"""
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import msvc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def layout(cls, src):
    cfg = json.load(open(os.path.join(ROOT, "tools", "units.json")))["dlls"]["hl"]
    cmd = [msvc.cl_path(), "/nologo", "/c", "/Zs", "/d1reportSingleClassLayout" + cls, "/EHsc", "/GR"]
    cmd += ["/D" + x for x in cfg["defines"]] + ["/I" + os.path.join(ROOT, x) for x in cfg["includes"]]
    cmd.append(os.path.join(ROOT, src))
    out = subprocess.run(cmd, env=msvc.env(), capture_output=True, text=True, errors="replace").stdout
    lines = out.splitlines()
    rows = []
    start = None
    for i, l in enumerate(lines):
        if re.match(r"^class %s\s+size\(" % re.escape(cls), l) or re.match(r"^struct %s\s+size\(" % re.escape(cls), l):
            start = i
            break
    if start is None:
        sys.exit("layout of %s not found (does %s include its header?)" % (cls, src))
    owners = [cls]
    for l in lines[start + 2:]:
        if l.strip().startswith("+---") and not re.search(r"\|", l):
            break
        m = re.match(r"^\s*(\d+)?\s*\|(.*)$", l)
        if not m:
            continue
        rest = m.group(2)
        depth = 0
        while rest.startswith(" |"):
            rest = rest[2:]
            depth += 1
        rest = rest.strip()
        bm = re.match(r"^\+--- \(base class (.*)\)$", rest)
        if bm:
            owners = owners[:depth + 1] + [bm.group(1)]
            continue
        if rest.startswith("+---") or not m.group(1):
            continue
        owners = owners[:depth + 1]
        if rest.startswith("<alignment"):
            continue
        rows.append((int(m.group(1)), owners[-1], rest))
    return rows


def vtable(cls, src):
    cfg = json.load(open(os.path.join(ROOT, "tools", "units.json")))["dlls"]["hl"]
    cmd = [msvc.cl_path(), "/nologo", "/c", "/Zs", "/d1reportSingleClassLayout" + cls, "/EHsc", "/GR"]
    cmd += ["/D" + x for x in cfg["defines"]] + ["/I" + os.path.join(ROOT, x) for x in cfg["includes"]]
    cmd.append(os.path.join(ROOT, src))
    out = subprocess.run(cmd, env=msvc.env(), capture_output=True, text=True, errors="replace").stdout
    lines = out.splitlines()
    for i, l in enumerate(lines):
        if l.strip() == "%s::$vftable@:" % cls:
            slots = []
            for l2 in lines[i + 3:]:
                m = re.match(r"^\s*(\d+)\s*\|\s*&?(.*)$", l2)
                if not m:
                    break
                slots.append(m.group(2).strip())
            return slots
    sys.exit("vftable of %s not found" % cls)


def main():
    args = sys.argv[1:]
    src = "src/dlls/items.cpp"
    if args and args[0] == "--src":
        src = args[1]
        args = args[2:]
    if args and args[0] == "--vt":
        slots = vtable(args[1], src)
        offs = [int(x, 16) for x in args[2:]]
        for i, s in enumerate(slots):
            if not offs or i * 4 in offs:
                print("+0x%03X %3d  %s" % (i * 4, i, s))
        return
    cls, offs = args[0], [int(x, 16) for x in args[1:]]
    rows = layout(cls, src)
    if not offs:
        for off, owner, name in rows:
            print("0x%04X  %-24s %s" % (off, owner, name))
        return
    for o in offs:
        best = None
        for off, owner, name in rows:
            if off <= o:
                best = (off, owner, name)
        print("0x%X -> %s::%s%s" % (o, best[1], best[2], "" if best[0] == o else " + 0x%X" % (o - best[0])))


if __name__ == "__main__":
    main()
