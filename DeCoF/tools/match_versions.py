"""Carry function names from a build with symbols to a build without.

Usage: python tools/match_versions.py <dll> <from_tag> <to_tag> [--out FILE]
  e.g. python tools/match_versions.py hl 1.55 1.6a

Inputs (produced by earlier steps):
  build/features/<dll>_<tag>.jsonl        tools/ghidra_scripts/ExportFeatures.java
  docs/symbols_<from_tag>.csv            tools/pdb_symbols.py (names of the source build)
  docs/calls/<dll>_<tag>_*.csv           tools/scan_calls.py (exports, cvars, messages)

The two builds use different compilers and optimization levels (VS2003 /Od
against VS2019 /O2), so bytes are useless. Matching is anchor based, in the
spirit of BinDiff:

  1. exports with the same name
  2. vtables of the same RTTI class with the same length, slot by slot
  3. strings referenced by exactly one function on each side
  4. data anchors: globals mapped by name (cvar_t by cvar name, gmsg ids by
     message name, vtables by class) and literals; functions whose set of
     mapped data references is unique on both sides
  5. propagation: callees and callers of matched pairs, accepted only when
     the candidate is unique and its features agree (score >= threshold)

Every accepted pair records the method and a score. One target address can
carry several source names: the release linker folds identical functions
(/OPT:ICF), so the extra names are kept in "folded".
"""
import argparse
import collections
import csv
import json
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class Build(object):
    def __init__(self, dll, tag):
        self.tag = tag
        self.funcs = {}
        self.vtables = {}
        self.labels = {}
        path = os.path.join(ROOT, "build", "features", "%s_%s.jsonl" % (dll, tag))
        for line in open(path):
            r = json.loads(line)
            if r["t"] == "func":
                self.funcs[r["ea"]] = r
            elif r["t"] == "vtable" and r["entries"]:
                cls = r["cls"]
                if len(r["entries"]) > len(self.vtables.get(cls, (0, []))[1]):
                    self.vtables[cls] = (r["ea"], r["entries"])
            elif r["t"] == "label":
                self.labels[r["ea"]] = r["name"]
        self.thunk = {ea: f["thunk"] for ea, f in self.funcs.items() if f["thunk"]}
        # call graph on canonical (non-thunk) addresses
        self.callees = {}
        self.callers = collections.defaultdict(set)
        for ea, f in self.funcs.items():
            if ea in self.thunk:
                continue
            cs = [self.canon(abs(c)) for c in f["callees"] if c > 0]
            cs = [c for c in cs if c in self.funcs]
            self.callees[ea] = cs
            for c in set(cs):
                self.callers[c].add(ea)
        self.excluded = set()
        self.calls_dir = os.path.join(ROOT, "docs", "calls")
        self.dll = dll

    def canon(self, ea):
        seen = 0
        while ea in self.thunk and seen < 4:
            ea = self.thunk[ea]
            seen += 1
        return ea

    def real_funcs(self):
        return [ea for ea in self.funcs if ea not in self.thunk and ea not in self.excluded]

    def csv(self, kind):
        path = os.path.join(self.calls_dir, "%s_%s_%s.csv" % (self.dll, self.tag, kind))
        return list(csv.DictReader(open(path))) if os.path.exists(path) else []


class ImageReader(object):
    """Read memory of a PE or ELF image by virtual address."""

    def __init__(self, path, delta=0):
        data = open(path, "rb").read()
        self.segs = []
        self.overlay = {}
        self.delta = delta  # Ghidra address minus file address (ELF .so: 0x10000)
        if data[:4] == b"ELF":
            from elftools.elf.elffile import ELFFile
            import io
            elf = ELFFile(io.BytesIO(data))
            for seg in elf.iter_segments():
                if seg["p_type"] == "PT_LOAD":
                    self.segs.append((seg["p_vaddr"], seg["p_filesz"], seg["p_offset"]))
            # GOT slots and absolute pointers are filled by relocations at
            # load time: apply R_386_GLOB_DAT / R_386_32 / R_386_JUMP_SLOT
            from elftools.elf.relocation import RelocationSection
            for sec in elf.iter_sections():
                if not isinstance(sec, RelocationSection):
                    continue
                symtab = elf.get_section(sec["sh_link"]) if sec["sh_link"] else None
                for rel in sec.iter_relocations():
                    t = rel["r_info_type"]
                    if t in (1, 6, 7) and symtab is not None and rel["r_info_sym"]:
                        sym = symtab.get_symbol(rel["r_info_sym"])
                        if sym["st_value"]:
                            self.overlay[rel["r_offset"]] = sym["st_value"]
        else:
            import pefile
            pe = pefile.PE(data=data, fast_load=True)
            base = pe.OPTIONAL_HEADER.ImageBase
            for sec in pe.sections:
                self.segs.append((base + sec.VirtualAddress, sec.SizeOfRawData, sec.PointerToRawData))
        self.data = data

    def read(self, va, n):
        for start, size, off in self.segs:
            if start <= va < start + size:
                o = off + (va - start)
                return self.data[o:o + n]
        return b""

    def cstr(self, va):
        b = self.read(va, 96)
        s = b.split(b"\0")[0]
        if len(s) < 3 or len(s) == 96 or any(c < 0x20 or c > 0x7E for c in s):
            return None
        return s.decode("latin-1")

    def ptr_name(self, va, depth=2):
        """Name string pointed to by the first dword at va (cvar_t, command
        tables); one more indirection for GOT slots of PIC code."""
        if va in self.overlay:
            p = self.overlay[va]
        else:
            b = self.read(va, 4)
            if len(b) < 4:
                return None
            p = struct.unpack("<I", b)[0]
        n = self.cstr(p)
        if n is None and depth > 1:
            return self.ptr_name(p, depth - 1)
        return n


class Matcher(object):
    def __init__(self, a, b):
        self.a, self.b = a, b
        self.ab = {}          # a ea -> b ea
        self.ba = collections.defaultdict(set)  # b ea -> set(a ea)
        self.how = {}         # (a, b) -> (method, score)
        self.datamap = {}     # a data ea -> b data ea

    # ----- bookkeeping -------------------------------------------------
    def accept(self, x, y, method, score=1.0):
        x, y = self.a.canon(x), self.b.canon(y)
        if x not in self.a.funcs or y not in self.b.funcs:
            return False
        if x in self.ab:
            return self.ab[x] == y
        # several source names on one target only with hard evidence (the
        # linker folded identical functions); heuristics must find a free one
        if y in self.ba and method not in ("vtable", "export"):
            return False
        self.ab[x] = y
        self.ba[y].add(x)
        self.how[(x, y)] = (method, round(score, 3))
        return True

    # ----- features ----------------------------------------------------
    def feat(self, side, ea):
        bld = self.a if side == "a" else self.b
        f = bld.funcs[ea]
        strings = set(f["strings"])
        consts = set(f["consts"])
        data = set(f["data"])
        if side == "a":
            data = {self.datamap[d] for d in data if d in self.datamap}
            callees = {self.ab[c] for c in bld.callees.get(ea, []) if c in self.ab}
        else:
            known = set(self.datamap.values())
            data = {d for d in data if d in known}
            callees = {c for c in bld.callees.get(ea, []) if c in self.ba}
        return strings, consts, data, callees

    @staticmethod
    def jac(x, y):
        if not x and not y:
            return None
        return len(x & y) / float(len(x | y))

    def score(self, x, y):
        fa, fb = self.feat("a", x), self.feat("b", y)
        weights = (3.0, 1.0, 2.0, 2.0)
        num = den = 0.0
        for w, p, q in zip(weights, fa, fb):
            j = self.jac(p, q)
            if j is not None:
                num += w * j
                den += w
        # release code is smaller than debug code; a release function much
        # bigger than its debug candidate is suspicious unless it inlines
        sa, sb = self.a.funcs[x]["size"], self.b.funcs[y]["size"]
        if den == 0:
            return 0.0
        s = num / den
        if sb > 3 * sa + 64:
            s *= 0.7
        return s

    # ----- anchors -----------------------------------------------------
    def by_exports(self):
        ea_ = {r["name"]: int(r["va"], 16) for r in self.a.csv("exports")}
        eb_ = {r["name"]: int(r["va"], 16) for r in self.b.csv("exports")}
        n = 0
        for name in set(ea_) & set(eb_):
            n += self.accept(ea_[name], eb_[name], "export")
        return n

    def by_vtables(self):
        votes = collections.Counter()
        for cls, (va, ents_a) in self.a.vtables.items():
            if cls not in self.b.vtables:
                continue
            vb, ents_b = self.b.vtables[cls]
            self.datamap[va] = vb
            if len(ents_a) != len(ents_b):
                continue
            for x, y in zip(ents_a, ents_b):
                votes[(self.a.canon(x), self.b.canon(y))] += 1
        # an a function voted to several b functions is ambiguous: keep the
        # majority only when it is clear
        by_a = collections.defaultdict(list)
        for (x, y), c in votes.items():
            by_a[x].append((c, y))
        n = 0
        for x, lst in by_a.items():
            lst.sort(reverse=True)
            if len(lst) == 1 or lst[0][0] >= 3 * lst[1][0]:
                n += self.accept(x, lst[0][1], "vtable", 1.0)
        return n

    def map_ptr_names(self, ia, ib):
        """Map globals whose first dword points to the same unique name."""
        def keys(bld, img):
            dl = img.delta
            # a reference may point at a field of the struct (cvar.value is
            # at +12): step back over the first fields to find the name
            k = collections.defaultdict(set)
            for f in bld.funcs.values():
                for d in f["data"]:
                    for off in (0, 4, 8, 12, 16):
                        n = img.ptr_name(d - dl - off)
                        if n:
                            k[(n, off)].add(d)
                            break
            return k
        ka, kb = keys(self.a, ia), keys(self.b, ib)
        n0 = len(self.datamap)
        for name, xs in ka.items():
            ys = kb.get(name)
            if len(xs) == 1 and ys and len(ys) == 1:
                self.datamap[next(iter(xs))] = next(iter(ys))
        return len(self.datamap) - n0

    def map_data(self):
        # literals referenced as data are not in "data" (strings are separate);
        # map named globals instead
        n0 = len(self.datamap)
        ca = {r["name"]: int(r["cvar_t"], 16) for r in self.a.csv("cvars_registered") if r.get("cvar_t")}
        cb = {r["name"]: int(r["cvar_t"], 16) for r in self.b.csv("cvars_registered") if r.get("cvar_t")}
        for k in set(ca) & set(cb):
            self.datamap[ca[k]] = cb[k]
        ma = {r["name"]: int(r["gmsg_global"], 16) for r in self.a.csv("usermsgs") if r.get("gmsg_global")}
        mb = {r["name"]: int(r["gmsg_global"], 16) for r in self.b.csv("usermsgs") if r.get("gmsg_global")}
        for k in set(ma) & set(mb):
            self.datamap[ma[k]] = mb[k]
        return len(self.datamap) - n0

    def by_unique(self, keyfn, method, min_score=0.0):
        ka = collections.defaultdict(list)
        kb = collections.defaultdict(list)
        for x in self.a.real_funcs():
            if x in self.ab:
                continue
            for k in keyfn("a", x):
                ka[k].append(x)
        for y in self.b.real_funcs():
            for k in keyfn("b", y):
                kb[k].append(y)
        votes = collections.Counter()
        for k, xs in ka.items():
            ys = kb.get(k)
            if len(xs) == 1 and ys and len(set(ys)) == 1:
                votes[(xs[0], ys[0])] += 1
        # resolve conflicts: one a may vote for several b
        best = {}
        for (x, y), c in votes.items():
            if x not in best or c > best[x][0]:
                best[x] = (c, y)
        n = 0
        for x, (c, y) in best.items():
            s = self.score(x, y)
            if s >= min_score:
                n += self.accept(x, y, method, s)
        return n

    def key_strings(self, side, ea):
        f = (self.a if side == "a" else self.b).funcs[ea]
        return set(s for s in f["strings"] if len(s) >= 4)

    def key_data(self, side, ea):
        f = (self.a if side == "a" else self.b).funcs[ea]
        if side == "a":
            ds = frozenset(self.datamap[d] for d in f["data"] if d in self.datamap)
        else:
            known = set(self.datamap.values())
            ds = frozenset(d for d in f["data"] if d in known)
        return {ds} if len(ds) >= 2 else set()

    def key_data1(self, side, ea):
        """A single mapped global that is specific (vtable, cvar, message id)."""
        f = (self.a if side == "a" else self.b).funcs[ea]
        if side == "a":
            ds = {self.datamap[d] for d in f["data"] if d in self.datamap}
        else:
            known = set(self.datamap.values())
            ds = {d for d in f["data"] if d in known}
        return {("d1", d) for d in ds}

    def key_consts(self, side, ea):
        f = (self.a if side == "a" else self.b).funcs[ea]
        cs = frozenset(c for c in f["consts"] if c not in self.common_consts)
        return {cs} if len(cs) >= 2 else set()

    def init_consts(self):
        cnt = collections.Counter()
        for bld in (self.a, self.b):
            for ea in bld.real_funcs():
                cnt.update(set(bld.funcs[ea]["consts"]))
        total = len(self.a.funcs) + len(self.b.funcs)
        self.common_consts = {c for c, n in cnt.items() if n > 40}

    def propagate_seq(self, min_score):
        """Align callee sequences of matched pairs around matched calls."""
        n = 0
        for x, y in list(self.ab.items()):
            sa = list(dict.fromkeys(self.a.callees.get(x, [])))
            sb = list(dict.fromkeys(self.b.callees.get(y, [])))
            # split both sequences at matched anchors, compare segments
            segs_a, segs_b = [[]], [[]]
            anchors_a = [c for c in sa if c in self.ab and self.ab[c] in sb]
            order = {self.ab[c]: i for i, c in enumerate(anchors_a)}
            for c in sa:
                if c in self.ab and self.ab[c] in order:
                    segs_a.append([])
                elif c not in self.ab:
                    segs_a[-1].append(c)
            for c in sb:
                if c in order:
                    segs_b.append([])
                elif c not in self.ba:
                    segs_b[-1].append(c)
            if len(segs_a) != len(segs_b):
                continue
            for ga, gb in zip(segs_a, segs_b):
                if len(ga) == len(gb) and 0 < len(ga) <= 4:
                    for u, v in zip(ga, gb):
                        if u in self.ab or v in self.ba:
                            continue
                        sc = self.score(u, v)
                        if sc >= min_score:
                            n += self.accept(u, v, "callseq", sc)
        return n

    def propagate_data(self):
        """Matched pairs with exactly one unmapped global each: map them."""
        known_b = set(self.datamap.values())
        n = 0
        for x, y in list(self.ab.items()):
            ua = [d for d in dict.fromkeys(self.a.funcs[x]["data"]) if d not in self.datamap]
            ub = [d for d in dict.fromkeys(self.b.funcs[y]["data"]) if d not in known_b]
            if len(ua) == 1 and len(ub) == 1:
                self.datamap[ua[0]] = ub[0]
                known_b.add(ub[0])
                n += 1
        return n

    def propagate(self, min_score):
        n = 0
        for x, y in list(self.ab.items()):
            ua = [c for c in dict.fromkeys(self.a.callees.get(x, [])) if c not in self.ab]
            ub = [c for c in dict.fromkeys(self.b.callees.get(y, [])) if c not in self.ba]
            if len(ua) == 1 and len(ub) == 1:
                s = self.score(ua[0], ub[0])
                if s >= min_score:
                    n += self.accept(ua[0], ub[0], "callee", s)
            pa = [c for c in self.a.callers.get(x, ()) if c not in self.ab]
            pb = [c for c in self.b.callers.get(y, ()) if c not in self.ba]
            if len(pa) == 1 and len(pb) == 1:
                s = self.score(pa[0], pb[0])
                if s >= min_score:
                    n += self.accept(pa[0], pb[0], "caller", s)
        return n


def load_names(tag, build=None):
    path = os.path.join(ROOT, "docs", "symbols_%s.csv" % tag)
    out = {}
    if not os.path.exists(path) and build is not None:
        # no PDB export: use the names Ghidra has for the source build
        # (ELF symbol tables of the Linux reference binaries)
        for ea, f in build.funcs.items():
            if not f["name"].startswith(("FUN_", "thunk_FUN_")):
                out[ea] = {"name": f["name"], "decorated": "", "object": "", "kind": "func"}
        return out
    for r in csv.DictReader(open(path)):
        if r["kind"].startswith("func") or r["kind"] == "public_code":
            out[int(r["va"], 16)] = r
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dll")
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--out")
    ap.add_argument("--min-score", type=float, default=0.35)
    ap.add_argument("--data-propagation", action="store_true",
                    help="map globals through matched functions (cross-platform builds)")
    ap.add_argument("--src-image", help="binary of the source build (PE or ELF), enables pointer-to-name data anchors")
    ap.add_argument("--dst-image", help="binary of the target build")
    ap.add_argument("--seed", help="CSV (va,name) of names already known in the target build; "
                    "pairs with the source function of the same name are accepted first")
    args = ap.parse_args()
    a, b = Build(args.dll, args.src), Build(args.dll, args.dst)
    names = load_names(args.src, a)
    # the source build's static CRT is a different CRT version: never match it
    a.excluded = {ea for ea, r in names.items() if r["object"].lower().endswith(".lib")}
    m = Matcher(a, b)
    m.init_consts()
    print("functions: %s %d, %s %d" % (a.tag, len(a.real_funcs()), b.tag, len(b.real_funcs())))
    if args.seed:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        from refmatch import short
        by_short = collections.defaultdict(list)
        for ea, r in names.items():
            if ea in a.funcs and ea not in a.excluded:
                by_short[short(r["decorated"] or r["name"])].append(ea)
        n = 0
        for r in csv.DictReader(open(args.seed)):
            if r.get("kind", "func") != "func" or r.get("evidence", "").startswith(("hl_1.55", "%s_1.6a_crt" % args.dll)):
                continue
            xs = by_short.get(short(r["name"]), [])
            if len(xs) == 1:
                n += m.accept(xs[0], int(r["va"], 16), "seed:" + r.get("evidence", "").split(":")[0], 1.0)
        print("seeds     +%d" % n)
    print("exports   +%d" % m.by_exports())
    print("vtables   +%d" % m.by_vtables())
    print("data map  +%d globals" % m.map_data())
    if args.src_image and args.dst_image:
        ia, ib = ImageReader(args.src_image), ImageReader(args.dst_image)
        for img, bld in ((ia, a), (ib, b)):
            if img.data[:4] == b"ELF":
                # Ghidra rebases shared objects; derive the delta from a symbol
                from elftools.elf.elffile import ELFFile
                import io
                elf = ELFFile(io.BytesIO(img.data))
                syms = {x.name: x["st_value"] for x in elf.get_section_by_name(".symtab").iter_symbols()
                        if x["st_info"]["type"] == "STT_FUNC" and x["st_value"]}
                deltas = collections.Counter(ea - syms[f["name"]] for ea, f in bld.funcs.items()
                                             if f["name"] in syms)
                img.delta = deltas.most_common(1)[0][0] if deltas else 0
                print("ELF base delta 0x%X" % img.delta)
        print("ptr names +%d globals" % m.map_ptr_names(ia, ib))
    print("strings   +%d" % m.by_unique(m.key_strings, "string", 0.25))
    for rnd in range(12):
        n = m.by_unique(m.key_data, "data", args.min_score)
        n += m.by_unique(m.key_data1, "data1", args.min_score)
        n += m.by_unique(m.key_strings, "string", 0.25)
        n += m.by_unique(m.key_consts, "consts", 0.5)
        n += m.propagate(args.min_score)
        n += m.propagate_seq(args.min_score)
        if args.data_propagation:
            m.propagate_data()
        print("round %d   +%d (total %d)" % (rnd, n, len(m.ab)))
        if n == 0:
            break

    out = args.out or os.path.join(ROOT, "build", "match", "%s_%s_to_%s.csv" % (args.dll, args.src, args.dst))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    rows = []
    for y, xs in sorted(m.ba.items()):
        xs = sorted(xs)
        x = xs[0]
        meth, sc = m.how[(x, y)]
        info = names.get(x, {})
        folded = [names.get(z, {}).get("decorated") or names.get(z, {}).get("name", "%08X" % z) for z in xs[1:]]
        rows.append({
            "to_va": "%08X" % y, "to_size": b.funcs[y]["size"],
            "from_va": "%08X" % x, "from_size": a.funcs[x]["size"],
            "name": info.get("decorated") or "", "demangled": info.get("name") or a.funcs[x]["name"],
            "object": info.get("object", ""), "method": meth, "score": sc,
            "folded": ";".join(folded),
        })
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()), lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    meth = collections.Counter(r["method"] for r in rows)
    print("matched %d target functions (%d source functions) -> %s" % (len(rows), len(m.ab), out))
    print("by method:", dict(meth))


if __name__ == "__main__":
    main()
