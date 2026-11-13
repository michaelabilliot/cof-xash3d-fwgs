"""Name the globals of a VC6 engine DLL through the Linux build's relocations.

  python tools/globals_linux.py <dll> <lib.so> [--all] [--write] [--members <symbol> ...]
      (dll: hw, gameui)

The Linux steam_legacy libraries are non-PIC shared objects with text
relocations: every instruction that touches a global keeps a relocation,
R_386_32 against the symbol for exported data (the in-place value is the
member offset) or R_386_RELATIVE with the link-time address for static
data (resolved here against the .symtab objects). The Windows DLL's base
relocations mark every absolute address in its code.

Only the functions named on both sides take part (a progress.csv row whose
name is the Linux name, C names with the leading underscore). The order
inside a function is not used: GCC -O2 and VC6 /Od emit the accesses in
different orders. Instead, for every Linux (symbol, offset) the set of
functions that use it is compared with the set of functions that reference
each Windows address (Jaccard index). A pairing is taken when the index is
at least 0.75, at least two functions share it, and the runner-up scores
under three quarters of it. Then:

- (symbol, 0) pairings name the symbol's address;
- a symbol with only member pairings gets the start they imply when two
  members under 0x40 agree, or when its first dword points to the same
  string on both sides (cvar_t, which starts with the cvar name);
- a pairing is refused when one side is writable and the other is not, or
  when the Linux object has an initial value of 4 or 8 bytes without
  relocations and the Windows bytes differ;
- one address, one name: the higher index wins.

Prints the counts, the first names (--all: every name) and, with --members,
the Linux member offsets of a symbol next to the Windows ones, which shows
where the Windows struct layout differs. --write records the C names in
tools/symbols/<dll>_manual.csv (kind data, the index as evidence) after
removing the rows an earlier run wrote; an address another map row names
already is left alone. C++ names (_Z...) and GCC static locals (name.N) have
no MSVC name to write and are printed only.
"""
import bisect
import collections
import csv
import os
import struct
import sys

import pefile
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import name_unit  # noqa: E402

IMAGES = {"hw": "versions/1.6a/hw.dll", "gameui": "versions/1.6a/cryoffear/cl_dlls/GameUI.dll"}


def linux_refs(lib):
    """linux function name -> [(symbol or None, offset)] in code order.

    R_386_32 relocations name exported data (the in-place value is the
    member offset). R_386_RELATIVE relocations hold the link-time address
    of static data: it is resolved against the .symtab objects, and an
    address inside no object (a string literal or a float constant) gives
    (None, address), a slot that takes no vote."""
    elf = ELFFile(open(lib, "rb"))
    symtab = elf.get_section_by_name(".symtab")
    funcs, objs = [], []
    for s in symtab.iter_symbols():
        t = s["st_info"]["type"]
        if t == "STT_FUNC" and s["st_value"] and s["st_size"]:
            funcs.append((s["st_value"], s["st_size"], s.name))
        elif t == "STT_OBJECT" and s["st_value"]:
            objs.append((s["st_value"], max(s["st_size"], 1), s.name))
    funcs.sort()
    objs.sort()
    starts = [f[0] for f in funcs]
    ostarts = [o[0] for o in objs]
    text = elf.get_section_by_name(".text")
    tdata = text.data()
    tbase = text["sh_addr"]
    refs = collections.defaultdict(list)
    for sec in elf.iter_sections():
        if not isinstance(sec, RelocationSection) or sec.name != ".rel.dyn":
            continue
        st = elf.get_section(sec["sh_link"])
        for rel in sec.iter_relocations():
            off = rel["r_offset"]
            if not tbase <= off < tbase + len(tdata):
                continue
            val = struct.unpack_from("<I", tdata, off - tbase)[0]
            kind = rel["r_info_type"]
            if kind == 1 and rel["r_info_sym"]:  # R_386_32
                sym = st.get_symbol(rel["r_info_sym"])
                if sym["st_info"]["type"] != "STT_OBJECT":
                    continue
                item = (sym.name, val)
            elif kind == 8:  # R_386_RELATIVE
                if tbase <= val < tbase + len(tdata):
                    continue  # code address (jump table, function pointer)
                j = bisect.bisect_right(ostarts, val) - 1
                if j >= 0 and val < objs[j][0] + objs[j][1]:
                    item = (objs[j][2], val - objs[j][0])
                else:
                    item = (None, val)
            else:
                continue
            i = bisect.bisect_right(starts, off) - 1
            if i >= 0 and off < funcs[i][0] + funcs[i][1]:
                refs[funcs[i][2]].append((off, item))
    for n in refs:
        refs[n] = [it for o, it in sorted(refs[n])]
    return refs


def windows_refs(dll, rows):
    pe = pefile.PE(os.path.join(ROOT, IMAGES[dll]))
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    text = [s for s in pe.sections if s.Name.startswith(b".text")][0]
    tlo, thi = base + text.VirtualAddress, base + text.VirtualAddress + text.Misc_VirtualSize
    relocs = []
    for blk in getattr(pe, "DIRECTORY_ENTRY_BASERELOC", []):
        for e in blk.entries:
            if e.type == 3:
                relocs.append(base + e.rva)
    relocs.sort()
    out = {}
    for va, size, name in rows:
        i = bisect.bisect_left(relocs, va)
        lst = []
        while i < len(relocs) and relocs[i] < va + size:
            tgt = struct.unpack_from("<I", img, relocs[i] - base)[0]
            if not tlo <= tgt < thi:
                lst.append(tgt)
            i += 1
        out[name] = lst
    return out


def linux_image(lib):
    """read(addr, n) over the loaded sections, nobits(addr), data relocations by address."""
    elf = ELFFile(open(lib, "rb"))
    secs = []
    for sec in elf.iter_sections():
        if sec["sh_addr"] and sec["sh_flags"] & 2:  # SHF_ALLOC
            nb = sec["sh_type"] == "SHT_NOBITS"
            secs.append((sec["sh_addr"], sec["sh_size"], nb, None if nb else sec.data()))

    def read(addr, n):
        for a, size, nb, data in secs:
            if a <= addr and addr + n <= a + size:
                return b"\0" * n if nb else data[addr - a:addr - a + n]
        return None

    def nobits(addr):
        return any(a <= addr < a + size and nb for a, size, nb, _d in secs)

    relocs = {}
    for sec in elf.iter_sections():
        if isinstance(sec, RelocationSection) and sec.name == ".rel.dyn":
            st = elf.get_section(sec["sh_link"])
            for rel in sec.iter_relocations():
                sym = st.get_symbol(rel["r_info_sym"]) if rel["r_info_sym"] else None
                relocs[rel["r_offset"]] = (rel["r_info_type"], sym["st_value"] if sym else 0)
    return read, nobits, relocs


def cstring(read, addr):
    out = b""
    while len(out) < 256:
        c = read(addr + len(out), 1)
        if not c:
            return None
        if c == b"\0":
            return out
        out += c
    return None


def main():
    args = sys.argv[1:]
    dll, lib = args[0], args[1]
    write = "--write" in args
    members = args[args.index("--members") + 1:] if "--members" in args else []
    lrefs = linux_refs(lib)
    elf = ELFFile(open(lib, "rb"))
    lobj = {s.name: (s["st_value"], s["st_size"]) for s in elf.get_section_by_name(".symtab").iter_symbols()
            if s["st_info"]["type"] == "STT_OBJECT" and s["st_value"]}
    lread, lnobits, lrel = linux_image(lib)
    rows = []
    for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"), encoding="utf-8")):
        if r["dll"] == dll and r["name"] and r["class"] != "LIB":
            n = r["name"][1:] if r["name"].startswith("_") and not r["name"].startswith("__") else r["name"]
            if n in lrefs:
                rows.append((int(r["va"], 16), int(r["size"]), n))
    wrefs = windows_refs(dll, rows)
    pe = pefile.PE(os.path.join(ROOT, IMAGES[dll]))
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()

    def wread(va, n):
        o = va - base
        return img[o:o + n] if 0 <= o and o + n <= len(img) else None

    def wstring(va):
        o = va - base
        if not 0 <= o < len(img):
            return None
        e = img.find(b"\0", o, o + 256)
        return img[o:e] if e >= 0 else None

    F = collections.defaultdict(set)
    for va, size, n in rows:
        for s, a in set(lrefs[n]):
            if s:
                F[(s, a)].add(n)
    G = collections.defaultdict(set)
    byfunc = collections.defaultdict(set)
    for va, size, n in rows:
        for w in set(wrefs.get(n, [])):
            G[w].add(n)
            byfunc[n].add(w)
    pairs = {}  # (symbol, offset) -> (windows address, index, shared, linux uses)
    for key, fs in F.items():
        if len(fs) < 2:
            continue
        cand = collections.Counter()
        for f in fs:
            for w in byfunc[f]:
                cand[w] += 1
        scored = sorted(((inter / len(fs | G[w]), inter, w) for w, inter in cand.items()), reverse=True)
        if not scored:
            continue
        j, inter, w = scored[0]
        j2 = scored[1][0] if len(scored) > 1 else 0
        if j >= 0.75 and inter >= 2 and j2 < 0.75 * j:
            pairs[key] = (w, j, inter, len(fs))

    lwrite = [(sec["sh_addr"], sec["sh_addr"] + sec["sh_size"]) for sec in elf.iter_sections()
              if sec["sh_addr"] and sec["sh_flags"] & 1]  # SHF_WRITE
    wwrite = [(base + sec.VirtualAddress, base + sec.VirtualAddress + sec.Misc_VirtualSize) for sec in pe.sections
              if sec.Characteristics & 0x80000000]  # IMAGE_SCN_MEM_WRITE

    def value_ok(s, w):
        v, size = lobj.get(s, (0, 0))
        if any(a <= v < b for a, b in lwrite) != any(a <= w < b for a, b in wwrite):
            return False  # a variable on one side, a constant on the other
        if size not in (4, 8) or lnobits(v) or any(v + i in lrel for i in range(size)):
            return True
        lv, wv = lread(v, size), wread(w, size)
        return lv is None or wv is None or lv == wv

    named = {}  # symbol -> (address, index, evidence)
    refused = []
    for (s, a), (w, j, inter, n) in pairs.items():
        if a == 0:
            if value_ok(s, w):
                named[s] = (w, j, "%d of its %d paired functions reference this address (Jaccard %.2f)" % (inter, n, j))
            else:
                refused.append((s, w))
    implied = collections.defaultdict(collections.Counter)
    for (s, a), (w, j, inter, n) in pairs.items():
        if a and s not in named:
            implied[s][(w - a) & 0xFFFFFFFF] += 1 if a < 0x40 else 0
    for s, c in implied.items():
        b, k = c.most_common(1)[0]
        v = lobj.get(s, (0, 0))[0]
        why = None
        if k >= 2:
            why = "%d members under 0x40 put its start here" % k
        elif v and lrel.get(v, (0,))[0] in (1, 8):
            t, symval = lrel[v]
            raw = struct.unpack("<I", lread(v, 4))[0]
            ls = cstring(lread, symval + raw if t == 1 else raw)
            for b2 in c:
                wp = wread(b2, 4)
                ws = wstring(struct.unpack("<I", wp)[0]) if wp else None
                if ls and ls == ws:
                    b = b2
                    why = "its first member points to the string \"%s\" on both sides" % ls.decode("latin-1")
                    break
        if why and value_ok(s, b):
            named[s] = (b, 0.0, why + " (member accesses)")
    byaddr = collections.defaultdict(list)
    for s, (w, j, ev) in named.items():
        byaddr[w].append((j, s))
    final = {}
    for w, lst in byaddr.items():
        lst.sort(reverse=True)
        if len(lst) == 1 or lst[0][0] > lst[1][0]:
            final[w] = (lst[0][1], named[lst[0][1]][2])
    print("%d paired functions, %d (symbol, offset) pairings, %d addresses named, %d refused (section or value)"
          % (len(rows), len(pairs), len(final), len(refused)))
    for w, (s, ev) in sorted(final.items())[:None if "--all" in args else 12]:
        print("  %08X %-32s %s" % (w, s, ev))
    for s in members:
        b = named.get(s, (None,))[0]
        print("%s at %s:" % (s, "%08X" % b if b else "?"))
        for (s2, a), (w, j, inter, n) in sorted(pairs.items()):
            if s2 == s:
                print("  linux +0x%X  windows %s  (Jaccard %.2f, %d/%d)"
                      % (a, "+0x%X" % (w - b) if b else "%08X" % w, j, inter, n))
    if not write:
        return
    name_unit.lock()
    try:
        p = os.path.join(ROOT, "tools", "symbols", "%s_manual.csv" % dll)
        old = list(csv.DictReader(open(p))) if os.path.exists(p) else []
        old = [r for r in old if "tools/globals_linux.py" not in r["evidence"]]
        have = {r["va"] for r in old}
        for q in ("%s_1.6a.csv" % dll, "%s_position.csv" % dll):
            qp = os.path.join(ROOT, "tools", "symbols", q)
            if os.path.exists(qp):
                have |= {r["va"] for r in csv.DictReader(open(qp))}
        n = 0
        for w, (s, ev) in sorted(final.items()):
            va = "%08X" % w
            if va in have or s.startswith("_Z") or "." in s:
                continue
            old.append({"va": va, "name": "_" + s, "kind": "data",
                        "evidence": "Linux %s: %s (tools/globals_linux.py)" % (os.path.basename(lib), ev)})
            n += 1
        with open(p, "w", newline="") as f:
            wr = csv.DictWriter(f, fieldnames=["va", "name", "kind", "evidence"], lineterminator="\n")
            wr.writeheader()
            wr.writerows(old)
        print("recorded %d" % n)
    finally:
        name_unit.unlock()


if __name__ == "__main__":
    main()
