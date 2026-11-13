"""Find TYPEDESCRIPTION save tables in a game DLL.

  python tools/savetables.py <dll> [field substring]

A TYPEDESCRIPTION is { FIELDTYPE fieldType; char *fieldName; int
fieldOffset; short fieldSize; short flags; } (16 bytes). The script scans
the data sections for runs of entries whose name pointer hits a string and
prints each run (one run per class m_SaveData). The offsets are the real
member offsets of the class in that build, which is how layout changes
between versions are confirmed.
"""
import struct
import sys

import pefile

FIELDTYPES = ["FLOAT", "STRING", "ENTITY", "CLASSPTR", "EHANDLE", "EVARS",
              "EDICT", "VECTOR", "POSITION_VECTOR", "POINTER", "INTEGER",
              "FUNCTION", "BOOLEAN", "SHORT", "CHARACTER", "TIME",
              "MODELNAME", "SOUNDNAME", "RANGE", "INT64", "DOUBLE"]


def cstring(img, base, va):
    o = va - base
    if o < 0 or o >= len(img):
        return None
    end = img.find(b"\x00", o, o + 128)
    if end <= o:
        return None
    s = img[o:end]
    if not all(32 <= c < 127 for c in s):
        return None
    return s.decode()


def tables(path):
    pe = pefile.PE(path, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    for sec in pe.sections:
        name = sec.Name.rstrip(b"\x00").decode()
        if name not in (".data", ".rdata"):
            continue
        start = sec.VirtualAddress
        end = start + sec.Misc_VirtualSize
        p = start
        run = []
        while p + 16 <= end:
            ftype, fname, foff, fsize, flags = struct.unpack_from("<IIiHH", img, p)
            s = cstring(img, base, fname) if ftype < len(FIELDTYPES) else None
            if s is not None and 0 <= foff < 0x10000 and 0 < fsize < 0x1000 and flags < 0x10:
                run.append((base + p, FIELDTYPES[ftype], s, foff, fsize, flags))
                p += 16
                continue
            if len(run) >= 2:
                yield run
            run = []
            p += 4
        if len(run) >= 2:
            yield run


def main():
    needle = sys.argv[2] if len(sys.argv) > 2 else None
    for run in tables(sys.argv[1]):
        if needle and not any(needle in e[2] for e in run):
            continue
        print("table at %08X (%d fields)" % (run[0][0], len(run)))
        for va, ft, s, off, size, flags in run:
            print("  0x%04X  %-16s %-32s size %d%s" % (off, ft, s, size, " flags %d" % flags if flags else ""))


if __name__ == "__main__":
    main()
