"""Per-function comparison of a linked DLL with the retail one.

Usage: python tools/link_diff.py <dll> [--csv out.csv] [--all]

Needs `python tools/build.py link <dll>` first (build/link/<dll>/<out> and
its .map). Retail names come from the merged symbol map (build.py), retail
function extents from docs/progress.csv. compare_pe.py compares whole
sections; this tool explains why they differ:

- functions: every retail row is paired with a rebuilt function, by name
  (the copy nearest to the local layout shift) or, for unnamed rows, by
  position with the same masked code. Each pair is compared instruction by
  instruction with relocated operands and branch targets replaced by what
  they point at (the paired function, a shared name, or a data object).
  Bytes after the retail extent (jump tables) are compared too.
- functions of game objects that have no retail counterpart (extra code).
- references: every retail address must map to one rebuilt address and back;
  the referenced data objects must have the same contents (relocated words
  masked; the extent is the smaller of the two neighbourhoods).
- exports (names, ordinals, order, targets), imports (order, hints), the
  Rich header, base relocations that straddle a page. An EXE without base
  relocations gets them reconstructed (Image.synth_relocs).
- layout: where the code shift changes inside game code.

Output stays short; --csv writes one row per retail function.
"""
import argparse
import bisect
import collections
import csv
import json
import os
import re
import sys

import capstone
import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build  # noqa: E402

ROOT = build.ROOT
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
SKIP_EXTRA = ("$", "__catch", "__unwind", "__ehhandler", "__tryend", "??__E", "??__F")


class Image:
    def __init__(self, path, symbols):
        self.pe = pefile.PE(path)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.img = self.pe.get_memory_mapped_image()
        self.relocs = set()
        for blk in getattr(self.pe, "DIRECTORY_ENTRY_BASERELOC", []):
            for e in blk.entries:
                if e.type == 3:
                    self.relocs.add(e.rva)
        if not self.relocs:
            self.relocs = self.synth_relocs()
        self.names = collections.defaultdict(set)
        for va, name in symbols:
            self.names[va].add(name)
        for d in self.pe.DIRECTORY_ENTRY_IMPORT:
            for i in d.imports:
                self.names[i.address].add("IAT:" + (i.name.decode() if i.name else "#%d" % i.ordinal))
        self.vas = sorted(self.names)
        text = self.pe.sections[0]
        self.text = (self.base + text.VirtualAddress, self.base + text.VirtualAddress + text.Misc_VirtualSize)

    def synth_relocs(self):
        """An EXE linked without base relocations (cof.exe): the places a
        relocation would have, found by a linear sweep of the code (absolute
        immediates and displacements that point into the image) and by
        scanning the data sections for aligned words that do. Heuristic: a
        jump table decoded as code can hide or invent a few."""
        lo, hi = self.base, self.base + self.pe.OPTIONAL_HEADER.SizeOfImage
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        md.detail = True
        out = set()
        for sec in self.pe.sections:
            start, size = sec.VirtualAddress, min(sec.Misc_VirtualSize, sec.SizeOfRawData)
            data = bytes(self.img[start:start + size])
            if sec.Characteristics & 0x20000000:
                pos = 0
                while pos < size:
                    n = 0
                    for ins in md.disasm(data[pos:], start + pos):
                        n += ins.size
                        # rel32 branch targets are not absolute
                        rel = ins.mnemonic.startswith(("call", "j", "loop"))
                        for off in (ins.disp_offset, 0 if rel else ins.imm_offset):
                            if off and ins.size - off >= 4:
                                v = int.from_bytes(ins.bytes[off:off + 4], "little")
                                if lo <= v < hi:
                                    out.add(ins.address + off)
                    pos += n + 1 if n == 0 else n
            else:
                for o in range(0, size - 3, 4):
                    if lo <= int.from_bytes(data[o:o + 4], "little") < hi:
                        out.add(start + o)
        return out

    def b(self, va, n):
        o = va - self.base
        return self.img[o:o + n]

    def masked(self, va, n):
        b = bytearray(self.b(va, n))
        for p in range(va - 3, va + n):
            if p - self.base in self.relocs:
                for q in range(max(p, va), min(p + 4, va + n)):
                    b[q - va] = 0
        return bytes(b)

    def near(self, v):
        i = bisect.bisect_right(self.vas, v) - 1
        if i < 0:
            return "%08X" % v
        name = sorted(self.names[self.vas[i]])[0]
        return name if v == self.vas[i] else "%s+%X" % (name, v - self.vas[i])

    def section(self, v):
        s = self.pe.get_section_by_rva(v - self.base)
        return s.Name.rstrip(b"\0").decode() if s else "?"


def tokens(img, va, size):
    """[(mnemonic, masked bytes, [(kind, value)])]: absolute operands from the
    relocation table, rel8/rel32 branch targets (inside the function as an
    offset)."""
    code = img.b(va, size)
    out = []
    for a, n, mn, ops in MD.disasm_lite(code, va):
        mn = mn.replace("bnd ", "").replace("notrack ", "")
        raw = bytearray(code[a - va:a - va + n])
        tg = []
        for p in range(a, a + n - 3):
            if p - img.base in img.relocs:
                tg.append(("abs", int.from_bytes(raw[p - a:p - a + 4], "little")))
                raw[p - a:p - a + 4] = b"\0\0\0\0"
        if (mn in ("call", "jmp") or mn.startswith(("j", "loop"))) and ops.startswith("0x"):
            t = int(ops, 16)
            d = 4 if n >= 5 else 1
            raw[n - d:] = b"\0" * d
            tg.append(("in", t - va) if va <= t < va + size else ("rel", t))
        out.append((mn, bytes(raw), tg))
    return out


def same_code(ta, tb):
    return len(ta) == len(tb) and all(x[0] == y[0] and x[1] == y[1] and len(x[2]) == len(y[2])
                                      for x, y in zip(ta, tb))


def tail_equal(ra, va, ob, ova, n):
    """Bytes after a retail extent: jump tables (relocated words) and code
    with calls into the shifted library (rel32 displacements of the branch
    instructions both sides decode at the same offset are ignored)."""
    a, b = bytearray(ra.masked(va, n)), bytearray(ob.masked(ova, n))
    if a == b:
        return True
    ib = {i.address - ova: (i.size, i.mnemonic) for i in MD.disasm(ob.b(ova, n), ova)}
    for i in MD.disasm(ra.b(va, n), va):
        off = i.address - va
        mn = i.mnemonic.replace("bnd ", "")
        if mn in ("call", "jmp") and i.size >= 5 and ib.get(off) == (i.size, i.mnemonic):
            a[off + i.size - 4:off + i.size] = bytes(4)
            b[off + i.size - 4:off + i.size] = bytes(4)
    return a == b


def parse_map(path):
    pat = re.compile(r"^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})\s+(?:f\s+)?(?:i\s+)?(\S.*)?$")
    out = []
    for line in open(path, encoding="latin-1"):
        m = pat.match(line)
        if m and m.group(1) != "0000":
            # vc6_anon spells the VC6 dynamic initializers _E$1 for objdiff;
            # the symbol map and progress.csv keep the compiler's _$E1
            name = re.sub(r"^_E\$(\d+)$", r"_$E\1", m.group(3))
            out.append((int(m.group(4), 16), name, (m.group(5) or "").strip()))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dll")
    ap.add_argument("--csv")
    ap.add_argument("--all", action="store_true", help="list every finding, not the first ones")
    args = ap.parse_args()
    lim = None if args.all else 12
    cfg = json.load(open(build.CONFIG))["dlls"][args.dll]
    lk = cfg["link"]
    new_path = os.path.join(ROOT, "build", "link", args.dll, lk["out"])
    omap = parse_map(new_path + ".map")
    rsyms = [(int(r["va"], 16), r["name"]) for r in csv.DictReader(open(build.merged_symbols(cfg, args.dll, "_linkdiff")))]
    R = Image(os.path.join(ROOT, lk["retail"]), rsyms)
    O = Image(new_path, [(va, n) for va, n, _o in omap])
    oobj = {}
    for va, n, obj in omap:
        oobj.setdefault(va, obj)
    rows = [r for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))) if r["dll"] == args.dll]
    rows.sort(key=lambda r: int(r["va"], 16))
    rname = {va: sorted(n)[0] for va, n in R.names.items()}
    rvas = [int(r["va"], 16) for r in rows]
    otext = [v for v in O.vas if O.text[0] <= v < O.text[1]]
    byname = collections.defaultdict(list)
    for v in otext:
        for n in O.names[v]:
            byname[n].append(v)

    def oext(v):
        i = bisect.bisect_right(otext, v)
        e = otext[i] if i < len(otext) else O.text[1]
        return len(O.b(v, min(e - v, 0x100000)).rstrip(b"\xcc"))

    def rext(v):
        i = bisect.bisect_right(rvas, v)
        e = rvas[i] if i < len(rvas) else R.text[1]
        return len(R.b(v, min(e - v, 0x100000)).rstrip(b"\xcc"))

    # 1. pairing: game rows by name (the copy nearest to the local shift);
    # library rows (names from byte matching, often one of several identical
    # template copies) by position with the same code first, then by name
    game_end = max(int(r["va"], 16) for r in rows if r["class"] != "LIB")
    pair = {}
    shift = 0
    for r in rows:
        va = int(r["va"], 16)
        cands = byname.get(r["name"] or rname.get(va, ""), [])
        if cands and r["class"] != "LIB":
            pair[va] = min(cands, key=lambda c: abs(c - va - shift))
            shift = pair[va] - va
    for r in rows:
        va = int(r["va"], 16)
        if va in pair:
            continue
        paired = sorted(pair.items())
        pk = [p[0] for p in paired]
        i = bisect.bisect_right(pk, va) - 1
        size = int(r["size"])
        for j in (i, i + 1):
            if 0 <= j < len(paired):
                c = va + paired[j][1] - paired[j][0]
                if same_code(tokens(R, va, size), tokens(O, c, size)):
                    pair[va] = c
                    break
        else:
            cands = byname.get(r["name"] or rname.get(va, ""), [])
            if cands:
                d = paired[i][1] - paired[i][0] if i >= 0 else 0
                pair[va] = min(cands, key=lambda c: abs(c - va - d))
    used = set(pair.values())

    # 2. compare
    dpairs = collections.defaultdict(set)   # data: retail -> rebuilt
    results = []
    for r in rows:
        va = int(r["va"], 16)
        size = int(r["size"])
        if va not in pair:
            results.append((r, None, "no counterpart"))
            continue
        ova = pair[va]
        ta, tb = tokens(R, va, size), tokens(O, ova, size)
        st = "identical"
        if not same_code(ta, tb):
            k = next(i for i in range(min(len(ta), len(tb)) + 1)
                     if i == len(ta) or i == len(tb) or ta[i][:2] != tb[i][:2])
            st = "code differs at instruction %d of %d" % (k, len(ta))
        else:
            for (m1, b1, t1), (m2, b2, t2) in zip(ta, tb):
                for (k1, v1), (k2, v2) in zip(t1, t2):
                    iat1 = {n for n in R.names.get(v1, ()) if n.startswith("IAT:")}
                    iat2 = {n for n in O.names.get(v2, ()) if n.startswith("IAT:")}
                    if k1 == "in" or k2 == "in":
                        if (k1, v1) != (k2, v2):
                            st = "branch target differs"
                    elif iat1 or iat2:
                        # an import slot: the imported function decides, not
                        # a label of the symbol map (which can be wrong)
                        if not iat1 & iat2:
                            st = "imports %s, retail %s" % ("/".join(sorted(iat2)) or O.near(v2),
                                                            "/".join(sorted(iat1)) or R.near(v1))
                    elif R.names.get(v1, set()) & O.names.get(v2, set()):
                        pass
                    elif v1 in pair:
                        if pair[v1] != v2:
                            st = "calls %s, retail %s" % (O.near(v2), R.near(v1))
                    elif r["class"] != "LIB":
                        dpairs[v1].add(v2)
            # tail (jump tables) up to the next retail row
            n = rext(va)
            if st == "identical" and n > size:
                # jump tables compare as masked bytes, code (calls into the
                # shifted library) as masked instructions
                if not tail_equal(R, va + size, O, ova + size, n - size):
                    st = "tail differs"
            if st == "identical" and r["class"] != "LIB" and oext(ova) > max(n, size):
                st = "rebuilt longer (%d vs %d bytes)" % (oext(ova), max(n, size))
        results.append((r, ova, st))

    def show(title, items):
        print("%s: %d" % (title, len(items)))
        for x in items[:lim]:
            print("    " + x)
        if lim and len(items) > lim:
            print("    ... %d more (--all)" % (len(items) - lim))

    count = collections.Counter()
    for r, ova, st in results:
        cls = "LIB" if r["class"] == "LIB" else "game"
        count[(cls, "moved" if ova and ova != int(r["va"], 16) else "same address" if ova else "-", st.split(" ")[0])] += 1
    print("retail rows %d, paired %d" % (len(rows), len(pair)))
    for k in sorted(count):
        print("    %-5s %-13s %-10s %d" % (k[0], k[1], k[2], count[k]))
    show("game functions that differ", ["%s %s %s (%s %s)" % (r["va"], (r["name"] or "?")[:70], st, r["status"], r["match_percent"])
                                       for r, ova, st in results
                                       if r["class"] != "LIB" and st not in ("identical", "no counterpart")])
    show("game rows without counterpart", ["%s %s" % (r["va"], r["name"]) for r, ova, st in results
                                          if r["class"] != "LIB" and st == "no counterpart"])
    show("LIB rows in game code that differ", ["%s %s %s" % (r["va"], r["name"] or "(unnamed)", st)
                                              for r, ova, st in results
                                              if r["class"] == "LIB" and ova and int(r["va"], 16) < game_end
                                              and st.startswith(("code", "tail", "rebuilt"))])
    show("LIB rows after game code (static CRT) that differ or moved out of order",
         ["%s %s %s" % (r["va"], r["name"] or "(unnamed)", st) for r, ova, st in results
          if r["class"] == "LIB" and ova and int(r["va"], 16) > game_end and st.startswith(("code", "tail", "rebuilt"))])
    # 3. extra functions of game objects
    extra, norow = [], []
    rev_pair = sorted((o, r) for r, o in pair.items())
    rk = [x[0] for x in rev_pair]
    for v in otext:
        if v in used or ".lib" in oobj.get(v, "").lower() or ":" in oobj.get(v, ""):
            continue
        names = [n for n in O.names[v] if not n.startswith(SKIP_EXTRA)]
        n = oext(v)
        if not names or n <= 0:
            continue
        # the same code near the retail position: a retail function without
        # a progress row (an inventory gap), not extra code
        i = bisect.bisect_right(rk, v) - 1
        guess = v - (rev_pair[i][0] - rev_pair[i][1]) if i >= 0 else v
        tb = tokens(O, v, n)
        hit = next((guess + d for d in range(-0x40, 0x41) if same_code(tokens(R, guess + d, n), tb)), None)
        line = "%08X %-24s %s" % (v, oobj.get(v, ""), sorted(names)[0][:80])
        if hit:
            norow.append(line + " (retail %08X)" % hit)
        else:
            extra.append(line)
    show("rebuilt functions of game objects with no retail counterpart (extra code)", extra)
    show("rebuilt functions found in retail by code but without a progress.csv row", norow)
    # 4. references and data
    rev = collections.defaultdict(set)
    for v1, s in dpairs.items():
        for v2 in s:
            rev[v2].add(v1)
    multi = ["retail %08X -> %s" % (v1, ", ".join(O.near(x) for x in sorted(s)[:3]))
             for v1, s in dpairs.items() if len(s) > 1]
    rmulti = ["%s <- retail %s" % (O.near(v2), ", ".join("%08X" % x for x in sorted(s)[:3]))
              for v2, s in rev.items() if len(s) > 1]
    show("retail addresses used as several rebuilt ones (often a loop end pointer)", multi)
    show("rebuilt addresses used as several retail ones", rmulti)
    rref = sorted(dpairs)
    bad = []
    nobj = 0
    seen = set()
    for v1, s in sorted(dpairs.items()):
        if len(s) != 1:
            continue
        v2 = next(iter(s))
        if O.section(v2) not in (".rdata", ".data") or any(n.startswith("IAT:") for n in O.names.get(v2, ())):
            continue
        i = bisect.bisect_right(O.vas, v2) - 1
        start = O.vas[i]
        if (start, v1 - (v2 - start)) in seen:
            continue
        seen.add((start, v1 - (v2 - start)))
        r0 = v1 - (v2 - start)
        oend = O.vas[i + 1] if i + 1 < len(O.vas) else start + 16
        j = bisect.bisect_right(rref, v1)
        rend = rref[j] if j < len(rref) else v1 + 16
        n = max(1, min(oend - start, rend - r0, 0x10000))
        nm = sorted(O.names[start])[0]
        if nm.startswith("??_C@"):
            n = min(n, O.b(start, n).find(b"\0") + 1 or n)
        nobj += 1
        if R.masked(r0, n) != O.masked(start, n):
            k = next(x for x in range(n) if R.masked(r0, n)[x] != O.masked(start, n)[x])
            bad.append("%s (retail %08X, +%X of %d)" % (nm[:70], r0, k, n))
    print("referenced data objects compared: %d" % nobj)
    show("data objects whose contents differ", bad)
    # 5. exports, imports, rich header, relocations
    def exports(img):
        d = img.pe.DIRECTORY_ENTRY_EXPORT
        return [(e.name.decode() if e.name else "", e.ordinal, img.base + e.address) for e in d.symbols]
    ea, eb = exports(R), exports(O)
    tgt_bad = [n for (n, o, v), (_n2, _o2, v2) in zip(ea, eb) if v2 != v and pair.get(v, dpairs.get(v) and next(iter(dpairs[v]))) != v2]
    print("exports: retail %d, rebuilt %d, names %s, ordinals %s, order %s, %d targets not the paired function"
          % (len(ea), len(eb), "same" if sorted(x[0] for x in ea) == sorted(x[0] for x in eb) else "DIFFERENT",
             "same" if {x[0]: x[1] for x in ea} == {x[0]: x[1] for x in eb} else "DIFFERENT",
             "same" if [x[0] for x in ea] == [x[0] for x in eb] else "DIFFERENT", len(tgt_bad)))
    def imports(img):
        return [(d.dll.decode().lower(), [((i.name or b"").decode(), i.hint) for i in d.imports])
                for d in img.pe.DIRECTORY_ENTRY_IMPORT]
    ia, ib = imports(R), imports(O)
    for (d1, l1), (d2, l2) in zip(ia, ib):
        order = [x[0] for x in l1] == [x[0] for x in l2]
        names = sorted(x[0] for x in l1) == sorted(x[0] for x in l2)
        hints = sum(1 for x, y in zip(l1, l2) if x[1] != y[1])
        print("imports %-14s %s: names %s, order %s, %d hints differ" % (d1, "" if d1 == d2 else "vs " + d2,
              "same" if names else "DIFFERENT", "same" if order else "DIFFERENT", hints if order else -1))
    ra, rb = R.pe.parse_rich_header(), O.pe.parse_rich_header()
    if ra and rb:
        va_, vb_ = ra["values"], rb["values"]
        ca = {va_[i]: va_[i + 1] for i in range(0, len(va_), 2)}
        cb = {vb_[i]: vb_[i + 1] for i in range(0, len(vb_), 2)}
        diff = ["prod %d build %d: retail %s, rebuilt %s" % (k >> 16, k & 0xFFFF, ca.get(k), cb.get(k))
                for k in sorted(set(ca) | set(cb)) if ca.get(k) != cb.get(k)]
        show("Rich header entries that differ", diff)
    for img, nm in ((R, "retail"), (O, "rebuilt")):
        st = sum(1 for r in img.relocs if (r & 0xFFF) > 0xFFC)
        print("%s: %d base relocations straddle a page" % (nm, st))
    # 6. layout: shift changes along game code
    seq = [(int(r["va"], 16), ova - int(r["va"], 16), r["name"]) for r, ova, st in results
           if ova and r["class"] != "LIB"]
    moved = set()
    for i in range(1, len(seq) - 1):
        if abs(seq[i][1] - seq[i - 1][1]) >= 0x100 and abs(seq[i][1] - seq[i + 1][1]) >= 0x100:
            moved.add(i)
    show("game functions placed out of retail order (a name that sorts elsewhere)",
         ["%08X %s (rebuilt %+d)" % (seq[i][0], seq[i][2][:70], seq[i][1]) for i in sorted(moved)])
    trans = []
    prev = None
    for i, (va, d, name) in enumerate(seq):
        if i in moved:
            continue
        if prev and d != prev[1]:
            trans.append("%08X %+5d -> %+5d  after %s" % (va, prev[1], d, prev[2][:60]))
        prev = (va, d, name)
    show("code shift changes along game code (EQUIVALENT sizes, page-straddle padding, function order)", trans)
    print("entry point: retail %08X, rebuilt %08X" % (R.pe.OPTIONAL_HEADER.AddressOfEntryPoint, O.pe.OPTIONAL_HEADER.AddressOfEntryPoint))
    if args.csv:
        with open(args.csv, "w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["va", "rebuilt", "size", "class", "status", "result", "name"])
            for r, ova, st in results:
                w.writerow([r["va"], "%08X" % ova if ova else "", r["size"], r["class"], r["status"], st, r["name"]])


if __name__ == "__main__":
    main()
