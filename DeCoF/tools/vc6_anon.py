"""Give the anonymous namespaces of a VC6 object their retail names.

  python tools/vc6_anon.py <obj> [<obj> ...]

tools/build.py runs it on every VC6 object it compiles.

VC6 names an anonymous namespace ?%<source path><number>@@ where <number>
changes with every compile (it is not derived from the source), so the
decorated names of everything inside such a namespace change on each
build. This rewrites the symbol names of the object so that each
?%<path>/<File>.cpp<number>@@ becomes the name the retail binary's RTTI
records for that file (table below), and leaves the code untouched.

The RTTI type descriptor of a class inside such a namespace holds the
same spelling as a string (.?AVGripPanel@?%<source path><number>@@) in
its own .data COMDAT. Its length follows the number of digits and the
path, so the .data of the linked image moved between two builds of the
same source. Those strings are rewritten to the full retail spelling
(directory included), the section grows or shrinks to fit, and the
linked image gets the retail bytes on every build.
"""
import re
import struct
import sys
import zlib

# file name and number of the anonymous namespace in the retail RTTI type
# descriptors (tools/rtti.py); the directory is dropped because Ghidra symbol
# names cannot hold the spaces of C:\Documents and Settings\...
RETAIL = {
    "frame.cpp": "Frame.cpp20302634",
    "scrollbar.cpp": "ScrollBar.cpp989430212",
    # no RTTI names this namespace in hw.dll (Texture is not polymorphic):
    # a fixed number keeps the template names stable between builds
    "vgui_enginesurfacehw.cpp": "vgui_EngineSurfaceHW.cpp0",
}

# directory of the source file in the retail RTTI strings (GameUI.dll and
# hw.dll spell both files the same way); a file without an entry keeps no
# directory in its strings
RETAIL_DIR = {
    "frame.cpp": "C:\\Documents and Settings\\XPMUser\\Desktop\\GoldSource\\vgui2\\controls\\",
    "scrollbar.cpp": "C:\\Documents and Settings\\XPMUser\\Desktop\\GoldSource\\vgui2\\controls\\",
}

ANON = re.compile(rb"\?%([^@]*?)([^\\/@]+\.cpp)(\d+)@@")
ANON_DATA = re.compile(rb"\?%([^@\0]*?)([^\\/@\0]+\.cpp)(\d+)@")
DYNINIT = re.compile(rb"_\$E(\d+)$")


def fix_name(name):
    def rep(m):
        key = m.group(2).decode("latin1").lower()
        if key not in RETAIL:
            return m.group(0)
        return b"?%" + RETAIL[key].encode("latin1") + b"@@"
    name = ANON.sub(rep, name)
    # the dynamic initializers of an object (_$E1, _$E2, ...): objdiff pairs
    # names that start with "$" by their code instead of their name, and the
    # initializers of several EXPOSE_SINGLE_INTERFACE objects look alike, so
    # both objects spell them _E$1, _E$2, ... (the symbol map, the unit lists
    # and docs/progress.csv keep the compiler spelling _$E1)
    m = DYNINIT.match(name)
    if m:
        name = b"_E$" + m.group(1)
    return name


def fix(path):
    data = bytearray(open(path, "rb").read())
    _m, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    strtab = symptr + nsym * 18
    strsize = struct.unpack_from("<I", data, strtab)[0]
    old = bytes(data[strtab:strtab + strsize])
    names = []
    i = 0
    while i < nsym:
        o = symptr + i * 18
        raw = bytes(data[o:o + 8])
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            e = old.index(b"\0", off)
            names.append((o, old[off:e], True))
        else:
            names.append((o, raw.rstrip(b"\0"), False))
        naux = data[o + 17]
        i += 1 + naux
    changed = 0
    # keep the old table (section headers may point into it) and append
    new = bytearray(old)
    for o, name, is_long in names:
        fixed = fix_name(name)
        if fixed == name:
            continue
        changed += 1
        if len(fixed) > 8:
            struct.pack_into("<II", data, o, 0, len(new))
            new += fixed + b"\0"
        else:
            data[o:o + 8] = fixed.ljust(8, b"\0")
    # the RTTI strings: a rewritten section gets its new raw data placed
    # before the symbol table; relocations and symbols behind a string
    # that changed length move with it, the section symbol's aux record
    # takes the new length and checksum
    extra = bytearray()
    for s in range(nsec):
        h = 20 + opt + s * 40
        size, ptr, prel = struct.unpack_from("<III", data, h + 16)
        nrel = struct.unpack_from("<H", data, h + 32)[0]
        raw = bytes(data[ptr:ptr + size])
        if not ptr or b"?%" not in raw:
            continue
        pieces, moves, pos = [], [], 0
        for m in ANON_DATA.finditer(raw):
            key = m.group(2).decode("latin1").lower()
            if key not in RETAIL:
                continue
            spelled = b"?%" + (RETAIL_DIR.get(key, "") + RETAIL[key]).encode("latin1") + b"@"
            if spelled == m.group(0):
                continue
            pieces += [raw[pos:m.start()], spelled]
            moves.append((m.end(), len(spelled) - len(m.group(0))))
            pos = m.end()
        if not moves:
            continue
        fixed = b"".join(pieces) + raw[pos:]

        def moved(off):
            return off + sum(delta for end, delta in moves if off >= end)

        for k in range(nrel):
            r = prel + k * 10
            struct.pack_into("<I", data, r, moved(struct.unpack_from("<I", data, r)[0]))
        for o, name, _is_long in names:
            value, secnum = struct.unpack_from("<Ih", data, o + 8)
            if secnum != s + 1:
                continue
            struct.pack_into("<I", data, o + 8, moved(value))
            if data[o + 16] == 3 and data[o + 17] and value == 0 and name.startswith(b"."):
                # section definition: Length, ..., CheckSum (the CRC-32 of
                # the raw data without the initial and final inversion)
                old_sum = struct.unpack_from("<I", data, o + 26)[0]
                struct.pack_into("<I", data, o + 18, len(fixed))
                if old_sum == zlib.crc32(raw, 0xFFFFFFFF) ^ 0xFFFFFFFF:
                    struct.pack_into("<I", data, o + 26, zlib.crc32(fixed, 0xFFFFFFFF) ^ 0xFFFFFFFF)
        struct.pack_into("<II", data, h + 16, len(fixed), symptr + len(extra))
        extra += fixed
        changed += 1
    if not changed:
        return 0
    struct.pack_into("<I", new, 0, len(new))
    struct.pack_into("<I", data, 8, symptr + len(extra))
    data = data[:symptr] + extra + data[symptr:strtab] + new
    open(path, "wb").write(data)
    return changed


if __name__ == "__main__":
    for p in sys.argv[1:]:
        print("%s: %d names" % (p, fix(p)))
