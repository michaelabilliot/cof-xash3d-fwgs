"""Print or merge the retail save table of a class.

  python tools/savetable_src.py <dll> <class> [<class> ...] [--offsets]
  python tools/savetable_src.py <dll> <class> --merge FILE [--write]

Finds <class>::Save in the symbol map, reads the `push <count>; push
<table>` pair it passes to CSave::WriteFields, decodes the TYPEDESCRIPTION
entries and prints the table as source:

    TYPEDESCRIPTION	CFoo::m_SaveData[] =
    {
        DEFINE_FIELD( CFoo, m_flBar, FIELD_FLOAT ),
        DEFINE_ARRAY( CFoo, m_iBaz, FIELD_INTEGER, 4 ),
    };

--offsets adds the offset of every field (the member offsets of the class
in the retail build). --merge rebuilds the table of FILE in retail order:
an entry already written in FILE is kept as written (with its comments and
the lines before it), a missing one is generated, entries that retail does
not have are dropped and listed on stderr. --write saves FILE (keeps its
encoding and line endings).
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
IMAGES = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll"}
FIELDTYPES = ["FIELD_FLOAT", "FIELD_STRING", "FIELD_ENTITY", "FIELD_CLASSPTR", "FIELD_EHANDLE",
              "FIELD_EVARS", "FIELD_EDICT", "FIELD_VECTOR", "FIELD_POSITION_VECTOR", "FIELD_POINTER",
              "FIELD_INTEGER", "FIELD_FUNCTION", "FIELD_BOOLEAN", "FIELD_SHORT", "FIELD_CHARACTER",
              "FIELD_TIME", "FIELD_MODELNAME", "FIELD_SOUNDNAME"]


def load_names(dll):
    names = {}
    for path in [os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll)] + \
            glob.glob(os.path.join(ROOT, "tools", "symbols", "%s_*.csv" % dll)):
        if os.path.exists(path):
            for r in csv.DictReader(open(path)):
                if "name" in r and "va" in r:
                    names.setdefault(r["name"], int(r["va"], 16))
    return names


def table_of(img, base, save_va):
    """(table address, count) from the first `push count; push table` pair."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    pushes = []
    for ins in md.disasm(img[save_va - base:save_va - base + 0x100], save_va):
        if ins.mnemonic == "push" and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
            pushes.append(ins.operands[0].imm & 0xFFFFFFFF)
        if ins.mnemonic == "int3":
            break
    for i in range(len(pushes) - 1):
        n, t = pushes[i], pushes[i + 1]
        if 0 < n < 0x400 and base <= t < base + len(img):
            return t, n
    return None, None


def cstring(img, base, va):
    o = va - base
    return img[o:img.index(b"\0", o)].decode("latin-1")


def retail_table(img, base, names, cls):
    save = None
    for prefix in ("UAEH", "EAEH", "AAEH", "QAEH"):
        save = names.get("?Save@%s@@%sAAVCSave@@@Z" % (cls, prefix))
        if save:
            break
    if not save:
        return None, "no Save in the symbol map"
    t, n = table_of(img, base, save)
    if t is None:
        return None, "no table found in Save at %08X" % save
    fields = []
    for k in range(n):
        ftype, fname, foff, fsize, flags = struct.unpack_from("<IIiHH", img, t - base + 16 * k)
        fields.append((cstring(img, base, fname), FIELDTYPES[ftype] if ftype < len(FIELDTYPES) else str(ftype),
                       foff, fsize, flags))
    return (t, n, save, fields), None


def field_line(cls, name, ft, size, flags):
    if size != 1:
        return "\tDEFINE_%sARRAY( %s, %s, %s, %d )," % ("GLOBAL_" if flags & 1 else "", cls, name, ft, size)
    return "\t%s( %s, %s, %s )," % ("DEFINE_GLOBAL_FIELD" if flags & 1 else "DEFINE_FIELD", cls, name, ft)


def merge(text, cls, fields):
    """Rebuild the m_SaveData initializer of cls in text (LF line endings)."""
    m = re.search(r"TYPEDESCRIPTION\s+%s::m_SaveData\[\]\s*=\s*\{" % re.escape(cls), text)
    if not m:
        raise SystemExit("%s::m_SaveData not found" % cls)
    start = m.end()
    end = text.index("};", start)
    body = text[start:end]
    lines = body.split("\n")
    # group lines: leading lines (comments, blanks, commented-out entries)
    # attach to the next DEFINE entry
    entries = {}
    pending = []
    order = []
    for line in lines[1:] if lines and not lines[0].strip() else lines:
        code = re.sub(r"//.*$", "", line).strip()
        mm = re.match(r"DEFINE_\w+\(\s*\w+\s*,\s*(\w+)", code)
        if mm:
            entries[mm.group(1)] = pending + [line]
            order.append(mm.group(1))
            pending = []
        else:
            pending.append(line)
    tail = pending
    out = []
    used = set()
    for name, ft, _off, size, flags in fields:
        if name in entries:
            out.extend(entries[name])
            used.add(name)
        else:
            out.append(field_line(cls, name, ft, size, flags))
    while tail and not tail[-1].strip():
        tail.pop()
    out.extend(tail)
    dropped = [n for n in order if n not in used]
    new_body = "\n" + "\n".join(out) + "\n"
    return text[:start] + new_body + text[end:], dropped


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    offsets = "--offsets" in sys.argv
    merge_file = None
    if "--merge" in sys.argv:
        merge_file = sys.argv[sys.argv.index("--merge") + 1]
        args.remove(merge_file)
    dll, classes = args[0], args[1:]
    pe = pefile.PE(os.path.join(ROOT, IMAGES[dll]), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    names = load_names(dll)
    for cls in classes:
        tab, err = retail_table(img, base, names, cls)
        if err:
            print("// %s: %s\n" % (cls, err))
            continue
        t, n, save, fields = tab
        if merge_file:
            raw = open(merge_file, "rb").read().decode("latin-1")
            crlf = "\r\n" in raw
            new, dropped = merge(raw.replace("\r\n", "\n"), cls, fields)
            for d in dropped:
                sys.stderr.write("%s: dropped %s (not in the retail table)\n" % (cls, d))
            if "--write" in sys.argv:
                if crlf:
                    new = new.replace("\n", "\r\n")
                open(merge_file, "wb").write(new.encode("latin-1"))
                print("%s: %d fields written to %s" % (cls, n, merge_file))
            else:
                print(new)
            continue
        print("// %s::m_SaveData at %08X, %d fields (Save at %08X)" % (cls, t, n, save))
        print("TYPEDESCRIPTION\t%s::m_SaveData[] = \n{" % cls)
        for name, ft, foff, size, flags in fields:
            line = field_line(cls, name, ft, size, flags)
            if offsets:
                line = "%-72s // 0x%X" % (line, foff)
            print(line)
        print("};\n")


if __name__ == "__main__":
    main()
