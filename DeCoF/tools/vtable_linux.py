"""Pair the virtual methods of a VC6 DLL with the Linux build through vtables.

  python tools/vtable_linux.py <dll> <lib.so>        (dll: gameui, hw)

Windows side: the MSVC RTTI vtables (tools/rtti.py), named by class.
Linux side: the _ZTV<class> vtable symbols of the unstripped .so (two
header words, then the function pointers, read from the relocated data).
A class present on both sides has its slots in the same declaration order,
except that the Itanium ABI gives a virtual destructor two entries
(complete D1, deleting D0) where MSVC has one (the scalar deleting
destructor): the walk merges them. Only primary vtables (offset 0) are
compared, and only while both sides stay in step (a pure virtual or
unknown entry on either side is skipped as a pair).

Writes build/match/<dll>_vtable_linux.csv: windows va, linux va, linux
symbol, class, slot. tools/align_linux.py uses these pairs as anchors.
"""
import collections
import csv
import os
import struct
import sys

from elftools.elf.elffile import ELFFile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rtti  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def parse_name(s, i):
    """Itanium <name> at s[i]: a nested N...E name or a <source-name>."""
    parts = []
    if s[i] == "N":
        i += 1
        while s[i] != "E":
            if s[i].isdigit():
                j = i
                while s[j].isdigit():
                    j += 1
                n = int(s[i:j])
                parts.append(s[j:j + n])
                i = j + n
            else:
                return None, i
        return parts, i + 1
    j = i
    while j < len(s) and s[j].isdigit():
        j += 1
    if j == i:
        return None, i
    n = int(s[i:j])
    return [s[j:j + n]], j + n


def msvc_class(name):
    # .?AVPanel@Controls@vgui@@ -> ['vgui', 'Controls', 'Panel']
    core = name[4:]
    if core.endswith("@@"):
        core = core[:-2]
    return list(reversed([p for p in core.split("@") if p]))


def main():
    dll, lib = sys.argv[1], sys.argv[2]
    elf = ELFFile(open(lib, "rb"))
    segs = [s for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]

    def read32(va):
        for sg in segs:
            if sg["p_vaddr"] <= va < sg["p_vaddr"] + sg["p_filesz"]:
                sg.stream.seek(sg["p_offset"] + va - sg["p_vaddr"])
                return struct.unpack("<I", sg.stream.read(4))[0]
        return None

    # vtable entries of preemptible functions are R_386_32 relocations
    # against the symbol (0 in the file); local ones are R_386_RELATIVE
    relsym = {}
    dynsym = elf.get_section_by_name(".dynsym")
    for sec in elf.iter_sections():
        if sec.header["sh_type"] != "SHT_REL":
            continue
        for rel in sec.iter_relocations():
            if rel["r_info_type"] == 1 and rel["r_info_sym"]:  # R_386_32
                sym = dynsym.get_symbol(rel["r_info_sym"])
                relsym[rel["r_offset"]] = sym.name
    funcs = {}
    byname = {}
    vts = {}
    for s in elf.get_section_by_name(".symtab").iter_symbols():
        t = s["st_info"]["type"]
        if t == "STT_FUNC" and s["st_value"]:
            funcs.setdefault(s["st_value"], s.name)
            byname.setdefault(s.name, s["st_value"])
        elif t == "STT_OBJECT" and s.name.startswith("_ZTV") and s["st_size"]:
            parts, _ = parse_name(s.name, 4)
            if parts:
                vts[tuple(parts)] = (s["st_value"], s["st_size"])
    out = []
    stats = collections.Counter()
    for wva, wname, off, wslots in rtti.vtables(dll):
        if off != 0:
            continue
        key = tuple(msvc_class(wname))
        if key not in vts:
            stats["class not in Linux"] += 1
            continue
        lva, lsize = vts[key]
        lslots = []
        for k in range(2, lsize // 4):
            at = lva + 4 * k
            if at in relsym:
                v = byname.get(relsym[at], 0)
            else:
                v = read32(at)
            if v is None:
                break
            lslots.append(v)
        # merge D1/D0 destructor pairs into one slot
        merged = []
        i = 0
        while i < len(lslots):
            n = funcs.get(lslots[i], "")
            if n.endswith(("D1Ev", "D2Ev")) and i + 1 < len(lslots) and funcs.get(lslots[i + 1], "").endswith("D0Ev"):
                merged.append(lslots[i + 1])
                i += 2
                continue
            # the end of the primary vtable: offset-to-top / typeinfo of a secondary one
            if lslots[i] not in funcs and lslots[i] != 0:
                break
            merged.append(lslots[i])
            i += 1
        stats["classes"] += 1
        for slot, (w, l) in enumerate(zip(wslots, merged)):
            ln = funcs.get(l)
            if not ln or "pure_virtual" in ln:
                continue
            out.append((w, l, ln, ".".join(key), slot))
    # a windows function shared by several classes (inherited) keeps one pair
    seen = {}
    for w, l, ln, c, slot in out:
        seen.setdefault(w, (w, l, ln, c, slot))
    path = os.path.join(ROOT, "build", "match", "%s_vtable_linux.csv" % dll)
    with open(path, "w", newline="") as f:
        wr = csv.writer(f, lineterminator="\n")
        wr.writerow(["windows_va", "linux_va", "linux_name", "class", "slot"])
        for w, l, ln, c, slot in sorted(seen.values()):
            wr.writerow(["%08X" % w, "%08X" % l, ln, c, slot])
    print("%s: %d classes compared, %d windows functions paired (%s) -> %s"
          % (dll, stats["classes"], len(seen), dict(stats), path))


if __name__ == "__main__":
    main()
