"""Give decorated names to the undecorated C++ names of a symbol map.

  python tools/decorate_names.py <dll> [--write]

The client map (tools/symbols/client_1.6a.csv) holds some names as
"Class::Method": the reference byte match took them from the reference
index, not from an object. objdiff pairs functions by decorated name, so
such a function never meets its source counterpart. For every such row
this looks for the decorated symbols of the reference objects
(build/refobj/<repo>_client, the object named in the evidence first) that
undecorate to the same "Class::Method". Exactly one decorated name is
required (an overloaded method is left alone); the row is then written to
tools/symbols/<dll>_decorated.csv with its evidence, merged by
tools/build.py right after the base map.
"""
import csv
import glob
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UNDNAME = r"I:\Tools\msvc-14.27.29112\bin\Hostx86\x86\undname.exe"
REPOS = ("paranoia", "hlsdk", "spirit")


def coff_symbols(path):
    """Names of the external function symbols defined in a COFF object."""
    data = open(path, "rb").read()
    _m, _nsec, _ts, symptr, nsym, _opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    strtab = symptr + 18 * nsym
    out = []
    i = 0
    while i < nsym:
        o = symptr + 18 * i
        raw = data[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            name = data[strtab + off:data.index(b"\0", strtab + off)].decode("latin-1")
        else:
            name = raw.rstrip(b"\0").decode("latin-1")
        _value, secnum, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        if secnum > 0 and typ == 0x20 and cls == 2 and name.startswith(("?", "_")):
            out.append(name)
        i += 1 + naux
    return out


def undecorate(names):
    tmp = os.path.join(ROOT, "build", "jobs", "decorate_names.txt")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    with open(tmp, "w") as f:
        f.write("\n".join(names) + "\n")
    out = subprocess.run([UNDNAME, tmp], stdout=subprocess.PIPE).stdout.decode("latin-1").splitlines()
    out = [l for l in out if l.strip()]
    return dict(zip(names, out)) if len(out) == len(names) else {}


def short_name(full):
    """"public: virtual int __thiscall CHudTrain::Draw(float)" -> "CHudTrain::Draw"."""
    head = full.split("(", 1)[0]
    m = re.search(r"([\w:~<>]+(?:operator\s*\S+)?)\s*$", head)
    return m.group(1) if m else None


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    dll = sys.argv[1]
    write = "--write" in sys.argv
    base = os.path.join(ROOT, "tools", "symbols", "%s_1.6a.csv" % dll)
    # C++ names without decoration, and C names without the leading
    # underscore of the object symbol (export names such as CL_CreateMove)
    rows = [r for r in csv.DictReader(open(base)) if r["kind"] == "func"
            and not r["name"].startswith(("?", "_"))]
    # decorated names of every reference object of this side
    side = "client" if dll == "client" else "server"
    objs = {}
    for repo in REPOS:
        for p in glob.glob(os.path.join(ROOT, "build", "refobj", "%s_%s" % (repo, side), "*.obj")):
            objs[(repo, os.path.basename(p))] = coff_symbols(p)
    cnames = {n for v in objs.values() for n in v if n.startswith("_")}
    names = sorted({n for v in objs.values() for n in v if n.startswith("?")})
    und = undecorate(names)
    by_short = {}
    for n in names:
        s = short_name(und.get(n, ""))
        if s:
            by_short.setdefault(s, set()).add(n)
    found = []
    skipped = 0
    for r in rows:
        want = r["name"]
        # "CHudMessage::CHudMessage::MessageAdd" (doubled class prefix)
        parts = want.split("::")
        if len(parts) == 3 and parts[0] == parts[1]:
            want = parts[0] + "::" + parts[2]
        if "::" not in want:
            cands = {"_" + want} & cnames
            if len(cands) == 1:
                found.append((r["va"], next(iter(cands)), r["name"], r["evidence"]))
                continue
        cands = by_short.get(want, set())
        # the object named in the evidence ("refbytes:paranoia:cl_dll__train.obj")
        m = re.match(r"refbytes:(\w+):([\w.]+\.obj)", r["evidence"])
        if m and (m.group(1), m.group(2)) in objs:
            local = cands & set(objs[(m.group(1), m.group(2))])
            if local:
                cands = local
        if len(cands) == 1:
            found.append((r["va"], next(iter(cands)), r["name"], r["evidence"]))
        else:
            skipped += 1
    for va, n, old, ev in found:
        print("%s  %-60s %s" % (va, n, old))
    print("%d decorated, %d left (no or several candidates)" % (len(found), skipped))
    if write:
        path = os.path.join(ROOT, "tools", "symbols", "%s_decorated.csv" % dll)
        with open(path, "w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["va", "name", "kind", "evidence"])
            for va, n, old, ev in found:
                why = ("C symbol of the reference objects for the export name %s" % old) if n.startswith("_") else                     ("only decorated symbol of the reference objects that undecorates to %s" % old)
                w.writerow([va, n, "func", "%s; was %s (%s)" % (why, old, ev)])
        print("wrote %s" % os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
