"""Disassemble retail code with names and strings.

  python tools/disasm.py [--version 1.55] [--outline] <dll> <va|decorated name> [bytes]

Prints the instructions of a function (or <bytes> bytes from <va>) with
the symbol names of call/jump targets and data from the merged symbol map
(build/jobs/symbols_<dll>.csv, else tools/symbols/<dll>_*.csv) and the
text of referenced strings. Stops at the next function start when no
length is given.

--version reads an older hl.dll with a PDB (1.0, 1.4 or 1.55) instead of
1.6a, with the names and sizes of docs/symbols_<version>.csv. These are
debug builds: constants and dead code that the 1.6a release build drops
are still there. When the PDB is on disk (1.0, 1.55), each source
statement starts with a "-- line N" row from its line table, which gives
the original statement order and if/else layout.

--outline prints one row per source statement (or per run up to a call or
jump without a line table) with only the calls, strings, named globals,
immediates, member offsets and jump targets: a fraction of the size of the
full listing.
"""
import csv
import glob
import os
import re
import struct
import sys

import capstone
import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll", "client": "versions/1.6a/cryoffear/cl_dlls/client.dll",
          "hw": "versions/1.6a/hw.dll", "opengl32": "versions/1.6a/opengl32.dll",
          "gameui": "versions/1.6a/cryoffear/cl_dlls/GameUI.dll", "launcher": "versions/1.6a/cof.exe"}
OLD_HL = {"1.0": "versions/1.0/CryOfFear/cl_dlls/hl.dll", "1.4": "versions/hl.dll",
          "1.55": "versions/1.55/cl_dlls/hl.dll"}


def statement_lines(image, va, base):
    """{va: (file, line)} of the statements of the function at va, from the
    PDB next to an older hl.dll (empty when there is none)."""
    pdb_path = os.path.join(ROOT, os.path.splitext(image)[0] + ".pdb")
    if not os.path.exists(pdb_path):
        return {}
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import pdbfile
    pdb = pdbfile.PDB(pdb_path)
    rva = va - base
    for mod, _kind, start, length, _name in pdb.procedures():
        if start <= rva < start + length:
            return {base + r: (os.path.basename(f), ln) for f, r, ln in pdb.module_lines(mod)
                    if start <= r < start + length}
    return {}


def print_outline(insns, lines, names, cs, inside):
    """One row per source statement (1.55 line table) or per run of
    instructions up to a call or jump (no line table): the calls, strings,
    named globals, immediates, member offsets ([reg+0x..], not the frame)
    and jump targets (as line numbers when known). A fraction of the full
    listing, enough to see statement order and if/else layout."""
    starts = sorted(lines)
    import bisect

    def line_of(a):
        i = bisect.bisect_right(starts, a) - 1
        return lines[starts[i]][1] if i >= 0 else None

    rows = []
    cur, items = None, []
    for ins in insns:
        key = lines[ins.address][1] if ins.address in lines else None
        if key is not None and items or (not lines and items and cur == "break"):
            rows.append((cur, items))
            items = []
        if key is not None:
            cur = key
        op = ins.op_str
        if ins.mnemonic == "call":
            t = int(op, 16) if re.match(r"^0x[0-9a-f]+$", op) else None
            items.append("call " + (names.get(t, op) if t else op))
        elif ins.mnemonic.startswith("j"):
            t = int(op, 16) if re.match(r"^0x[0-9a-f]+$", op) else None
            ln = line_of(t) if (t and lines) else None
            items.append("%s %s" % (ins.mnemonic, ("L%d" % ln) if ln else op))
        elif ins.mnemonic.startswith("ret"):
            items.append(ins.mnemonic)
        else:
            for tok in re.findall(r"0x[0-9a-f]+", op):
                a = int(tok, 16)
                n = names.get(a) or (('"%s"' % cs(a)) if cs(a) else None) or inside(a)
                if n:
                    items.append(n)
            m = re.search(r"\[(e[a-z]{2}) \+ (0x[0-9a-f]+)\]", op)
            if m and m.group(1) not in ("ebp", "esp"):
                items.append("+" + m.group(2))
            m = re.match(r"^(push|cmp|mov|and|or|test|sub|add) .*?, (0x[0-9a-f]+|\d+)$", ins.mnemonic + " " + op) or                 re.match(r"^push (0x[0-9a-f]+|\d+)$", ins.mnemonic + " " + op)
            if m and ins.mnemonic in ("push", "cmp", "test", "and", "or"):
                imm = m.group(m.lastindex)
                if int(imm, 0) not in names:
                    items.append("%s %s" % (ins.mnemonic, imm))
        if not lines and ins.mnemonic in ("call",) or not lines and ins.mnemonic.startswith(("j", "ret")):
            cur = "break"
    if items:
        rows.append((cur, items))
    for key, items in rows:
        label = ("L%d" % key) if isinstance(key, int) else ""
        print("%-6s %s" % (label, ", ".join(items)))


def main():
    args = sys.argv[1:]
    outline = "--outline" in args
    args = [a for a in args if a != "--outline"]
    version = None
    if args[:1] == ["--version"]:
        version, args = args[1], args[2:]
    dll, what = args[0], args[1]
    length = int(args[2], 0) if len(args) > 2 else None
    if version:
        if dll != "hl" or version not in OLD_HL:
            sys.exit("--version supports hl with %s" % ", ".join(sorted(OLD_HL)))
        image = OLD_HL[version]
    else:
        image = IMAGES[dll]
    pe = pefile.PE(os.path.join(ROOT, image), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    names = {}
    sizes = {}
    data_spans = {}
    if version:
        data = []
        for r in csv.DictReader(open(os.path.join(ROOT, "docs", "symbols_%s.csv" % version))):
            a = int(r["va"], 16)
            name = r["decorated"] or r["name"]
            if name.startswith("??_C@"):
                continue    # print the string text instead
            names.setdefault(a, name)
            if r["kind"] == "func" and r["size"]:
                sizes[a] = int(r["size"])
            elif r["kind"] in ("public_data", "data_static"):
                data.append((a, name))
        # the PDB has no data sizes: a global spans up to the next one
        data.sort()
        for (a, name), (b, _n) in zip(data, data[1:]):
            data_spans[name] = b - a
    else:
        paths = [os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll)]
        if not os.path.exists(paths[0]):
            paths = glob.glob(os.path.join(ROOT, "tools", "symbols", "%s_*.csv" % dll))
        for p in paths:
            for r in csv.DictReader(open(p)):
                if "va" in r and "name" in r:
                    names.setdefault(int(r["va"], 16), r["name"])
        for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))):
            if r["dll"] == dll:
                sizes[int(r["va"], 16)] = int(r["size"] or 0)
                if r["name"]:
                    names.setdefault(int(r["va"], 16), r["name"])
    if re.match(r"^(0x)?[0-9a-fA-F]{8}$", what):
        va = int(what, 16)
    else:
        va = next((a for a, n in names.items() if n == what), None)
        if va is None:
            sys.exit("unknown name %s" % what)
    if length is None:
        length = sizes.get(va) or 0x200

    def cs(a):
        o = a - base
        if 0 <= o < len(img):
            e = img.find(b"\0", o, o + 100)
            s = img[o:e]
            if e > o + 1 and all(32 <= c < 127 for c in s):
                return s.decode()
        return None

    # named globals with a size: references into them print as name+offset
    sizes_by_name = dict(data_spans)
    sp = os.path.join(ROOT, "tools", "symbols", "%s_datasizes.csv" % dll)
    if os.path.exists(sp) and not version:
        sizes_by_name = {r["name"]: int(r["size"]) for r in csv.DictReader(open(sp))}
    spans = sorted((a, a + sizes_by_name[n], n) for a, n in names.items() if n in sizes_by_name)
    starts = [s[0] for s in spans]
    members = {}
    lp = os.path.join(ROOT, "build", "types", "layout_enginefuncs.txt")
    if os.path.exists(lp):
        for line in open(lp):
            p = line.split()
            members[("?g_engfuncs@@3Uenginefuncs_s@@A", int(p[0], 16))] = p[-1]

    def inside(a):
        import bisect
        i = bisect.bisect_right(starts, a) - 1
        if i >= 0 and spans[i][0] < a < spans[i][1]:
            off = a - spans[i][0]
            m = members.get((spans[i][2], off))
            return "%s+0x%X%s" % (spans[i][2], off, " (%s)" % m if m else "")
        return None

    rdata = [s for s in pe.sections if s.Name.startswith(b".rdata")]
    rdata_lo = base + rdata[0].VirtualAddress if rdata else 0
    rdata_hi = rdata_lo + rdata[0].Misc_VirtualSize if rdata else 0
    lines = statement_lines(image, va, base) if version else {}
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    print("%08X %s" % (va, names.get(va, "")))
    insns = list(md.disasm(img[va - base:va - base + length], va))
    if outline:
        print_outline(insns, lines, names, cs, inside)
        return
    for ins in insns:
        if ins.address in lines:
            print("-- line %d (%s)" % (lines[ins.address][1], lines[ins.address][0]))
        notes = []
        for tok in re.findall(r"0x[0-9a-f]+", ins.op_str):
            a = int(tok, 16)
            if a in names and a != va:
                notes.append(names[a])
            else:
                s = cs(a)
                if s:
                    notes.append('"%s"' % s)
                else:
                    n = inside(a)
                    if n:
                        notes.append(n)
                    elif rdata_lo <= a < rdata_hi and ("dword" in ins.op_str or "qword" in ins.op_str):
                        f = struct.unpack_from("<f", img, a - base)[0]
                        d = struct.unpack_from("<d", img, a - base)[0]
                        notes.append(("double %r" % d) if "qword" in ins.op_str else ("float %r" % f))
        print("  %08X  %-40s %s" % (ins.address, (ins.mnemonic + " " + ins.op_str)[:40],
                                     ("; " + ", ".join(notes)) if notes else ""))


if __name__ == "__main__":
    main()
