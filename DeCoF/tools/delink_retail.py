"""Write COFF objects made from retail bytes (retail-derived binary, not source).

  python tools/delink_retail.py <dll> <out dir> [<piece> ...]

Third-party code that has no source and no official library (hw.dll: the
Aureal A3D library, the IGA Radial client, the Miles mss32.lib helper, the
DirectX manager of the OpenGL-on-Direct3D wrapper) is linked from the retail
image so the DLL links. The pieces are listed in the "retail_objects" key of
the DLL's link section in tools/units.json:

  {"name": "a3d", "lib": "a3d_retail.lib", "ranges": [["01E14E40", "01E19980"], ...]}

Each range [start, end) of the retail image becomes one section of the
object, named after the PE section it lies in (.text, .rdata, .data; .bss
past the raw data of .data). The section bytes are the retail bytes. The
relocations are rebuilt from the retail image:

- DIR32: every base relocation inside a range.
- REL32: every call, jmp and jcc with a 32-bit displacement (capstone, linear
  sweep restarted at each function start of docs/progress.csv) whose target
  lies outside the section.

A target inside one of the piece's own ranges is section relative. A target
in another piece gets a global symbol there (the retail name, or
retail_<dll>_<VA> when the map has none). Any other target must carry a
name in the merged symbol map (build.py): import slots (__imp_...), import
thunks, engine and CRT functions and data. Floating point and string
constants of other objects (__real@..., ??_C@...) are written into the object
as COMDATs (select any), so the reference resolves whether or not a rebuilt
object still emits them. Every named address inside a piece is defined as a
global symbol, so the rebuilt engine reaches the library by its names.

The objects are written at link time from versions/ and never committed.
Each one holds a .comment section that says it is retail-derived.
"""
import bisect
import csv
import json
import os
import struct
import sys

import capstone
import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build  # noqa: E402

ROOT = build.ROOT
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

DIR32, REL32 = 0x06, 0x14
CODE = 0x60000020
RDATA = 0x40000040
DATA = 0xC0000040
BSS = 0xC0000080
COMDAT = 0x1000
INFO, REMOVE = 0x200, 0x800


def align_flags(va, cap):
    a = 1
    while a < cap and va % (a * 2) == 0:
        a *= 2
    return {1: 0x00100000, 2: 0x00200000, 4: 0x00300000, 8: 0x00400000, 16: 0x00500000}[a]


class Retail:
    def __init__(self, dll):
        cfg = json.load(open(build.CONFIG))["dlls"][dll]
        self.cfg = cfg
        self.pe = pefile.PE(os.path.join(ROOT, cfg["link"]["retail"]))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.img = self.pe.get_memory_mapped_image()
        self.relocs = sorted(self.base + e.rva for b in self.pe.DIRECTORY_ENTRY_BASERELOC
                             for e in b.entries if e.type == 3)
        self.reloc_set = set(self.relocs)
        self.secs = [(self.base + s.VirtualAddress, self.base + s.VirtualAddress + s.Misc_VirtualSize,
                      self.base + s.VirtualAddress + s.SizeOfRawData, s.Name.rstrip(b"\0").decode())
                     for s in self.pe.sections]
        path = build.merged_symbols(cfg, dll, "_retailobj")
        self.name_at = {}
        for r in csv.DictReader(open(path)):
            self.name_at.setdefault(int(r["va"], 16), r["name"])
        self.named = sorted(self.name_at)
        rows = [r for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))) if r["dll"] == dll]
        self.funcs = sorted(int(r["va"], 16) for r in rows)
        # the names Ghidra's function ID gave the static CRT (LIB rows of
        # progress.csv) where the symbol map has none
        taken = set(self.name_at.values())
        for r in rows:
            va = int(r["va"], 16)
            if r["class"] == "LIB" and r["name"] and va not in self.name_at and r["name"] not in taken:
                self.name_at[va] = r["name"]
                taken.add(r["name"])
        self.named = sorted(self.name_at)
        # import slots: __imp_ + the name the import library gives the
        # function (stdcall decoration from the library's symbol list)
        self.iat = {}
        for d in self.pe.DIRECTORY_ENTRY_IMPORT:
            for i in d.imports:
                if i.name:
                    self.iat[i.address] = (d.dll.decode(), i.name.decode())
        self.implib_names = {}

    def import_symbol(self, va, libdirs):
        dll, name = self.iat[va]
        key = os.path.splitext(dll)[0].lower()
        if key not in self.implib_names:
            names = {}
            for d in libdirs:
                for f in os.listdir(d):
                    if os.path.splitext(f)[0].lower() == key and f.lower().endswith(".lib"):
                        for s in lib_symbols(os.path.join(d, f)):
                            if s.startswith("__imp_"):
                                plain = s[6:]
                                bare = plain[1:] if plain.startswith("_") else plain
                                names.setdefault(bare.split("@")[0], s)
                                names.setdefault(plain, s)
                if names:
                    break
            self.implib_names[key] = names
        names = self.implib_names[key]
        bare = name[1:] if name.startswith("_") else name
        return names.get(name) or names.get(bare.split("@")[0])

    def dword(self, va):
        return struct.unpack_from("<I", self.img, va - self.base)[0]

    def bytes(self, va, n):
        return bytes(self.img[va - self.base:va - self.base + n])

    def kind(self, va):
        """(section name of the object, flags) for a retail address"""
        for lo, hi, raw, name in self.secs:
            if lo <= va < hi:
                if name == ".text":
                    return ".text", CODE
                if name == ".rdata":
                    return ".rdata", RDATA
                if name == ".data":
                    return (".data", DATA) if va < raw else (".bss", BSS)
        raise ValueError("%08X is outside the image sections" % va)

    def raw_end(self, va):
        for lo, hi, raw, name in self.secs:
            if lo <= va < hi:
                return raw
        return None


class Obj:
    """a COFF object under construction"""

    def __init__(self):
        self.sections = []   # dict(name, flags, data(bytes or None), size, relocs[(off, sym, type)])
        self.symbols = []    # dict(name, value, sec, type, cls, aux(bytes or None))
        self.sym_index = {}

    def add_section(self, name, flags, data, size):
        self.sections.append(dict(name=name, flags=flags, data=data, size=size, relocs=[]))
        return len(self.sections)  # 1-based section number

    def symbol(self, name, value=0, sec=0, typ=0, cls=2, aux=None):
        if cls == 2 and name in self.sym_index:
            s = self.symbols[self.sym_index[name]]
            if sec and not s["sec"]:
                s.update(value=value, sec=sec, typ=typ)
            return self.sym_index[name]
        self.symbols.append(dict(name=name, value=value, sec=sec, typ=typ, cls=cls, aux=aux))
        idx = len(self.symbols) - 1
        if cls == 2:
            self.sym_index[name] = idx
        return idx

    def write(self, path):
        # symbol table indices count aux records
        index, n = [], 0
        for s in self.symbols:
            index.append(n)
            n += 1 + (1 if s["aux"] is not None else 0)
        strtab = bytearray(b"\0\0\0\0")
        head = 20 + 40 * len(self.sections)
        body = bytearray()
        hdrs = []
        for s in self.sections:
            raw_ptr = 0
            if s["data"] is not None:
                raw_ptr = head + len(body)
                body += s["data"]
            rel_ptr = 0
            if s["relocs"]:
                rel_ptr = head + len(body)
                for off, sym, typ in s["relocs"]:
                    body += struct.pack("<IIH", off, index[sym], typ)
            if len(s["relocs"]) > 0xFFFF:
                raise ValueError("too many relocations in one section")
            name = s["name"].encode()
            hdrs.append(struct.pack("<8sIIIIIIHHI", name, 0, 0, s["size"], raw_ptr, rel_ptr, 0,
                                    len(s["relocs"]), 0, s["flags"]))
        symtab = bytearray()
        for s in self.symbols:
            nm = s["name"].encode("latin-1")
            if len(nm) <= 8:
                field = nm.ljust(8, b"\0")
            else:
                field = struct.pack("<II", 0, len(strtab))
                strtab += nm + b"\0"
            symtab += field + struct.pack("<IhHBB", s["value"], s["sec"], s["typ"], s["cls"],
                                          1 if s["aux"] is not None else 0)
            if s["aux"] is not None:
                symtab += s["aux"]
        struct.pack_into("<I", strtab, 0, len(strtab))
        out = struct.pack("<HHIIIHH", 0x14C, len(self.sections), 0, head + len(body), n, 0, 0)
        with open(path, "wb") as f:
            f.write(out + b"".join(hdrs) + body + symtab + strtab)


def lib_symbols(path):
    """public symbol names of a library (its first linker member)"""
    data = open(path, "rb").read()
    if data[:8] != b"!<arch>\n":
        return []
    size = int(data[8 + 48:8 + 58].decode().strip())
    body = data[68:68 + size]
    n = struct.unpack_from(">I", body, 0)[0]
    return [s.decode("latin-1") for s in body[4 + 4 * n:].split(b"\0")[:n]]


def section_aux(length, nrel, selection=0, number=0):
    return struct.pack("<IHHIHB3x", length, nrel, 0, 0, number, selection)


def make(rt, dll, pieces, piece, out):
    """write one piece; returns the list of problems"""
    problems = []
    ranges = [(int(a, 16), int(b, 16)) for a, b in piece["ranges"]]
    # a range that crosses the raw end of .data continues as .bss
    split = []
    for lo, hi in ranges:
        raw = rt.raw_end(lo)
        if raw and lo < raw < hi:
            split += [(lo, raw), (raw, hi)]
        else:
            split.append((lo, hi))
    ranges = split
    obj = Obj()
    note = ("retail-derived binary, not source: %s bytes %s of the retail %s (tools/delink_retail.py)"
            % (piece["name"], " ".join("%08X-%08X" % r for r in ranges), dll)).encode() + b"\0"
    obj.add_section(".comment", INFO | REMOVE | 0x00100000, note, len(note))
    obj.symbol(".comment", 0, 1, 0, 3, section_aux(len(note), 0))
    secs = []  # (lo, hi, secno, symidx, data bytearray, name)
    for lo, hi in ranges:
        name, flags = rt.kind(lo)
        cap = 16 if name == ".text" else 8
        data = None if name == ".bss" else bytearray(rt.bytes(lo, hi - lo))
        no = obj.add_section(name, flags | align_flags(lo, cap), data, hi - lo)
        sym = obj.symbol(name, 0, no, 0, 3, section_aux(hi - lo, 0))
        secs.append((lo, hi, no, sym, data, name))

    def own(va):
        for s in secs:
            if s[0] <= va < s[1]:
                return s
        return None

    def other_piece(va):
        for p in pieces:
            if p is piece:
                continue
            for a, b in p["ranges"]:
                if int(a, 16) <= va < int(b, 16):
                    return p
        return None

    # global symbols for every named address inside the piece, and for
    # addresses other pieces reference
    for lo, hi, no, _sym, _data, name in secs:
        k = bisect.bisect_left(rt.named, lo)
        while k < len(rt.named) and rt.named[k] < hi:
            va = rt.named[k]
            obj.symbol(rt.name_at[va], va - lo, no, 0x20 if name == ".text" else 0, 2)
            k += 1
    # "aliases": {"<va>": "<name>"}, a second name for a named address (two
    # rebuilt files that call one library function by different _guess names)
    for va, name in piece.get("aliases", {}).items():
        s = own(int(va, 16))
        if s is None:
            problems.append("alias %s at %s lies outside the piece" % (name, va))
            continue
        obj.symbol(name, int(va, 16) - s[0], s[2], 0x20 if s[5] == ".text" else 0, 2)
    for va in piece.get("_exported", ()):
        s = own(va)
        if s and va not in rt.name_at:
            obj.symbol("_retail_%s_%08X" % (dll, va), va - s[0], s[2], 0, 2)

    comdats = {}

    def target_symbol(t, site):
        """(symbol index, base va) for a reference to t"""
        s = own(t)
        if s:
            return s[3], s[0]
        slot = t
        thunk = rt.kind(t)[0] == ".text" and rt.bytes(t, 2) == b"\xff\x25"
        if thunk:
            slot = rt.dword(t + 2)  # an import thunk: jmp [slot]
        if slot in rt.iat:
            name = rt.import_symbol(slot, rt.libdirs)
            if name:
                return obj.symbol(name[6:] if thunk else name), t
            problems.append("%08X: import %s!%s has no import library symbol" % ((site,) + rt.iat[slot]))
            return None, t
        if t in rt.name_at:
            name = rt.name_at[t]
            if (name.startswith("__real@") or name.startswith("??_C@")) and other_piece(t) is None:
                if name not in comdats:
                    size = next_boundary(t) - t
                    kname, flags = rt.kind(t)
                    if kname == ".bss":
                        flags, data = flags, None
                    else:
                        data = rt.bytes(t, size)
                    no = obj.add_section(kname, flags | COMDAT | align_flags(t, 8), data, size)
                    obj.symbol(kname, 0, no, 0, 3, section_aux(size, 0, selection=2))
                    comdats[name] = obj.symbol(name, 0, no, 0, 2)
                return comdats[name], t
            return obj.symbol(name), t
        p = other_piece(t)
        if p is not None:
            p.setdefault("_exported", set()).add(t)
            return obj.symbol("_retail_%s_%08X" % (dll, t)), t
        problems.append("%08X: reference to %08X has no name" % (site, t))
        return None, t

    def next_boundary(t):
        k = bisect.bisect_right(rt.named, t)
        nxt = rt.named[k] if k < len(rt.named) else t + 8
        return min(nxt, t + 64)

    # DIR32 from the base relocations
    for lo, hi, no, _sym, data, name in secs:
        if data is None:
            continue
        k = bisect.bisect_left(rt.relocs, lo)
        while k < len(rt.relocs) and rt.relocs[k] < hi:
            site = rt.relocs[k]
            k += 1
            if site + 4 > hi:
                problems.append("%08X: relocation crosses the end of the range" % site)
                continue
            t = rt.dword(site)
            sym, base = target_symbol(t, site)
            if sym is None:
                continue
            struct.pack_into("<I", data, site - lo, t - base)
            obj.sections[no - 1]["relocs"].append((site - lo, sym, DIR32))
    # REL32 from a linear sweep of the code, restarted at function starts
    for lo, hi, no, _sym, data, name in secs:
        if name != ".text":
            continue
        starts = [f for f in rt.funcs if lo <= f < hi]
        code = bytes(data)
        pos = lo
        while pos < hi:
            if pos in rt.reloc_set:
                pos += 4  # a jump table entry
                continue
            k = bisect.bisect_right(starts, pos)
            stop = starts[k] if k < len(starts) else hi
            ins = next(MD.disasm(code[pos - lo:min(stop, pos + 16) - lo], pos, 1), None)
            if ins is None:
                pos += 1
                continue
            b0 = ins.bytes[0]
            if (b0 in (0xE8, 0xE9) and ins.size == 5) or (b0 == 0x0F and 0x80 <= ins.bytes[1] <= 0x8F
                                                          and ins.size == 6):
                site = ins.address + ins.size - 4
                t = (site + 4 + struct.unpack_from("<i", ins.bytes, ins.size - 4)[0]) & 0xFFFFFFFF
                if not (lo <= t < hi):
                    sym, base = target_symbol(t, ins.address)
                    if sym is not None:
                        struct.pack_into("<i", data, site - lo, t - base)
                        obj.sections[no - 1]["relocs"].append((site - lo, sym, REL32))
            pos += ins.size
    for s in obj.sections:
        s["relocs"].sort()
    # section symbol aux records carry the relocation counts
    for sym in obj.symbols:
        if sym["cls"] == 3 and sym["aux"] is not None:
            sec = obj.sections[sym["sec"] - 1]
            length, _n, _l, _c, num, sel = struct.unpack("<IHHIHB3x", sym["aux"])
            sym["aux"] = section_aux(length, len(sec["relocs"]), sel, num)
    obj.write(out)
    return problems


def main():
    dll, outdir = sys.argv[1], sys.argv[2]
    want = set(sys.argv[3:])
    rt = Retail(dll)
    # import libraries: the ones build.py made for the link (out dir), then
    # the toolchain's
    rt.libdirs = [outdir, os.path.join(build.toolchain(rt.cfg).root if hasattr(build.toolchain(rt.cfg), "root")
                                       else "", "LIB")]
    pieces = rt.cfg["link"].get("retail_objects", [])
    os.makedirs(outdir, exist_ok=True)
    bad = 0
    # two passes: the first finds the addresses one piece takes from another
    for final in (False, True):
        for p in pieces:
            if want and p["name"] not in want:
                continue
            path = os.path.join(outdir, "retail_%s.obj" % p["name"])
            problems = make(rt, dll, pieces, p, path)
            if final:
                # one line per unnamed target: first site and count
                seen = {}
                for msg in problems:
                    key = msg.split("reference to ")[-1]
                    seen.setdefault(key, [msg, 0])[1] += 1
                for msg, count in list(seen.values())[:60]:
                    print("  %s: %s%s" % (p["name"], msg, " (%d sites)" % count if count > 1 else ""))
                if len(seen) > 60:
                    print("  %s: ... %d more targets" % (p["name"], len(seen) - 60))
                bad += len(problems)
                print("%-8s %s (%d problems)" % (p["name"], os.path.relpath(path, ROOT), len(problems)))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
