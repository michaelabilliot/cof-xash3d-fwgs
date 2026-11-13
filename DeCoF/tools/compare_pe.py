"""Compare a rebuilt PE file with the retail one, section by section.

Usage: python tools/compare_pe.py <rebuilt.dll> <retail.dll>

Reports: file size, header fields, every section (size and whether the
bytes are identical; first differing offsets otherwise), import and export
tables. Fields that always differ between two links (TimeDateStamp,
CheckSum) are reported but not counted as differences.
"""
import sys

import pefile


def first_diffs(a, b, limit=8):
    out = []
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            out.append(i)
            if len(out) >= limit:
                break
    return out


def main():
    new, old = pefile.PE(sys.argv[1]), pefile.PE(sys.argv[2])
    issues = 0
    print("file size      rebuilt %d  retail %d  %s" % (len(new.__data__), len(old.__data__),
          "same" if len(new.__data__) == len(old.__data__) else "DIFFERENT"))
    issues += len(new.__data__) != len(old.__data__)
    for field in ("TimeDateStamp",):
        print("%-14s rebuilt %08X retail %08X (expected to differ)" % (
            field, getattr(new.FILE_HEADER, field), getattr(old.FILE_HEADER, field)))
    for field in ("ImageBase", "SizeOfImage", "AddressOfEntryPoint", "SizeOfCode", "SizeOfInitializedData",
                  "MajorLinkerVersion", "MinorLinkerVersion", "CheckSum"):
        a, b = getattr(new.OPTIONAL_HEADER, field), getattr(old.OPTIONAL_HEADER, field)
        note = "(expected to differ)" if field == "CheckSum" else ("same" if a == b else "DIFFERENT")
        if field != "CheckSum":
            issues += a != b
        print("%-20s rebuilt %08X retail %08X %s" % (field, a, b, note))
    secs_new = {s.Name.rstrip(b"\0").decode(): s for s in new.sections}
    secs_old = {s.Name.rstrip(b"\0").decode(): s for s in old.sections}
    for name in sorted(set(secs_new) | set(secs_old)):
        a, b = secs_new.get(name), secs_old.get(name)
        if not a or not b:
            print("section %-8s only in %s" % (name, "rebuilt" if a else "retail"))
            issues += 1
            continue
        da, db = a.get_data(), b.get_data()
        same = da == db and a.VirtualAddress == b.VirtualAddress
        print("section %-8s va %08X/%08X  size %6d/%6d  %s" % (
            name, a.VirtualAddress, b.VirtualAddress, len(da), len(db),
            "IDENTICAL" if same else "differs at %s" % ["0x%X" % x for x in first_diffs(da, db)]))
        issues += not same
    ea = sorted(e.name.decode() for e in getattr(new, "DIRECTORY_ENTRY_EXPORT", None).symbols if e.name) \
        if hasattr(new, "DIRECTORY_ENTRY_EXPORT") else []
    eb = sorted(e.name.decode() for e in getattr(old, "DIRECTORY_ENTRY_EXPORT", None).symbols if e.name) \
        if hasattr(old, "DIRECTORY_ENTRY_EXPORT") else []
    print("exports        rebuilt %d retail %d %s" % (len(ea), len(eb), "same names" if ea == eb else "DIFFERENT"))
    issues += ea != eb
    ia = [(d.dll.decode().lower(), sorted((i.name or b"").decode() for i in d.imports))
          for d in getattr(new, "DIRECTORY_ENTRY_IMPORT", [])]
    ib = [(d.dll.decode().lower(), sorted((i.name or b"").decode() for i in d.imports))
          for d in getattr(old, "DIRECTORY_ENTRY_IMPORT", [])]
    print("imports        rebuilt %s retail %s %s" % ([x[0] for x in ia], [x[0] for x in ib],
          "same" if ia == ib else "DIFFERENT"))
    issues += ia != ib
    print("RESULT: %s" % ("identical apart from timestamp/checksum" if issues == 0 else "%d difference(s)" % issues))
    return 0 if issues == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
