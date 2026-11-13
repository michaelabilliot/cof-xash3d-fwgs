"""Reference-guided naming: match retail functions against objects compiled
from reference source with the same compiler, check relocation targets, and
propagate names through relocations.

Usage:
  python tools/refmatch.py <dll> <features.jsonl> <out.csv> --boundary HEX
         --seed CSV[:va_col:name_col] ... --ref TAG=DIR ...

1. Every function of the reference objects is indexed with its bytes and its
   relocations (offset, type, target symbol, implicit addend). String
   literal symbols (??_C@...) carry their contents.
2. A retail function is a byte candidate of a reference function when the
   sizes are equal and all bytes outside relocation fields are equal.
3. Each candidate is scored on its relocations, using what is already known
   about the retail image: a string literal must have the same contents at
   the retail target (+1 / -3), a target with a known name must carry the
   reference symbol's name (+1 / -3), unknown targets count 0.
4. The best candidate is accepted when its score is >= 0, it beats every
   candidate with another name, and either the score is positive or the
   bytes are unique and at least 32 bytes long.
5. Accepted functions name the retail targets of their relocations
   (callees, globals, vtables) when those are still unknown. Steps 3-5
   repeat until nothing changes.

Seeds: CSVs of already known names (exports, 1.55 matches, CRT matches).
Only functions below --boundary (the start of the static CRT) are matched.
"""
import argparse
import collections
import csv
import ctypes
import json
import os
import struct
import sys

import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objmatch import iter_archive  # noqa: E402

REL_DIR32, REL_REL32 = 0x06, 0x14
_und = ctypes.windll.dbghelp.UnDecorateSymbolName


def short(name):
    """Comparable short form: undecorated qualified name without signature."""
    if name.startswith("?"):
        b = ctypes.create_string_buffer(2048)
        if _und(name.encode("latin-1"), b, 2048, 0x1000):
            return b.value.decode("latin-1")
        return name
    n = name[1:] if name.startswith("_") else name
    return n.split("@")[0]


class RefFunc(object):
    __slots__ = ("name", "data", "relocs", "src")

    def __init__(self, name, data, relocs, src):
        self.name, self.data, self.relocs, self.src = name, data, relocs, src


def parse_obj(data, src):
    """Yield RefFunc for every function; relocs = [(off, type, sym, addend, strbytes)]."""
    if len(data) < 20:
        return
    machine, nsec, _ts, symoff, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    if machine != 0x14C or symoff == 0:
        return
    secs = []
    for i in range(nsec):
        o = 20 + opt + 40 * i
        vsize, va, rawsize, rawptr, relptr, _ln, nrel, _nln, chars = struct.unpack_from(
            "<IIIIIIHHI", data, o + 8)
        secs.append((data[o:o + 8].rstrip(b"\0"), rawsize, rawptr, relptr, nrel, chars))
    strtab = symoff + 18 * nsym
    syms = []
    i = 0
    while i < nsym:
        p = symoff + 18 * i
        raw = data[p:p + 8]
        if raw[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", raw, 4)[0]
            name = data[strtab + so:data.index(b"\0", strtab + so)].decode("latin-1")
        else:
            name = raw.rstrip(b"\0").decode("latin-1")
        value, secnum, typ, sclass, naux = struct.unpack_from("<IhHBB", data, p + 8)
        syms.append((name, value, secnum, typ, sclass))
        for _ in range(naux):
            syms.append(None)
        i += 1 + naux

    def sec_bytes(secnum):
        _n, rawsize, rawptr, _r, _nr, _c = secs[secnum - 1]
        return data[rawptr:rawptr + rawsize] if rawptr else b""

    per_sec = collections.defaultdict(list)
    for s in syms:
        if s and s[2] > 0 and (s[3] & 0xF0) == 0x20 and s[4] in (2, 3):
            per_sec[s[2]].append((s[1], s[0]))
    for secnum, fns in per_sec.items():
        name, rawsize, rawptr, relptr, nrel, chars = secs[secnum - 1]
        if not chars & 0x20 or not rawptr:
            continue
        body = data[rawptr:rawptr + rawsize]
        relocs = []
        for r in range(nrel):
            off, symidx, rtype = struct.unpack_from("<IIH", data, relptr + 10 * r)
            s = syms[symidx]
            if s is None:
                continue
            sname, svalue, ssec, _t, ssc = s
            strb = None
            if sname.startswith("??_C@") and ssec > 0:
                sb = sec_bytes(ssec)[svalue:]
                strb = sb.split(b"\0")[0]
            elif ssc == 3 and ssec > 0 and svalue == 0 and not secs[ssec - 1][5] & 0x20:
                # section symbol of a data section: local static data
                sname = None
            relocs.append((off, rtype, sname, struct.unpack_from("<i", body, off)[0] if off + 4 <= len(body) else 0, strb))
        fns.sort()
        for k, (start, fname) in enumerate(fns):
            end = fns[k + 1][0] if k + 1 < len(fns) else rawsize
            fb = body[start:end]
            while len(fb) > 1 and fb[-1:] == b"\xcc":
                fb = fb[:-1]
            rel = [(o - start, t, s, a, sb) for (o, t, s, a, sb) in relocs if start <= o < start + len(fb)]
            yield RefFunc(fname, fb, rel, src)


def load_refs(specs):
    index = collections.defaultdict(list)
    n = 0
    for spec in specs:
        tag, path = spec.split("=", 1)
        files = []
        for dp, _dn, fn in os.walk(path):
            files += [os.path.join(dp, f) for f in fn if f.lower().endswith((".obj", ".lib"))]
        for f in files:
            if f.lower().endswith(".lib"):
                members = [("%s(%s)" % (os.path.basename(f), m), d) for m, d in iter_archive(f)]
            else:
                members = [("%s:%s" % (tag, os.path.basename(f)), open(f, "rb").read())]
            for src, data in members:
                for rf in parse_obj(data, src):
                    if len(rf.data) >= 2:
                        index[len(rf.data)].append(rf)
                        n += 1
    return index, n


class Image(object):
    def __init__(self, path):
        pe = pefile.PE(path, fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.img = pe.get_memory_mapped_image()

    def bytes(self, va, n):
        o = va - self.base
        return self.img[o:o + n]

    def u32(self, va):
        b = self.bytes(va, 4)
        return struct.unpack("<I", b)[0] if len(b) == 4 else None

    def i32(self, va):
        b = self.bytes(va, 4)
        return struct.unpack("<i", b)[0] if len(b) == 4 else None


def masked_equal(t, rf):
    if len(t) < len(rf.data):
        return False
    mask = bytearray(len(rf.data))
    for off, *_ in rf.relocs:
        for k in range(off, min(off + 4, len(mask))):
            mask[k] = 1
    d = rf.data
    for k in range(len(d)):
        if not mask[k] and t[k] != d[k]:
            return False
    return True


def reloc_target(img, ea, off, rtype, addend):
    if rtype == REL_DIR32:
        v = img.u32(ea + off)
        return None if v is None else v - addend
    if rtype == REL_REL32:
        v = img.i32(ea + off)
        return None if v is None else ea + off + 4 + v - addend
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dll")
    ap.add_argument("features")
    ap.add_argument("out")
    ap.add_argument("--boundary", required=True)
    ap.add_argument("--seed", action="append", default=[])
    ap.add_argument("--ref", action="append", default=[])
    ap.add_argument("--similarity", action="store_true",
                    help="also name modified functions by strings and callees")
    args = ap.parse_args()
    boundary = int(args.boundary, 16)
    img = Image(args.dll)
    funcs = {}
    for l in open(args.features):
        r = json.loads(l)
        if r["t"] == "func" and not r["thunk"]:
            funcs[r["ea"]] = r
    thunks = {}
    for l in open(args.features):
        r = json.loads(l)
        if r["t"] == "func" and r["thunk"]:
            thunks[r["ea"]] = r["thunk"]

    # known: retail address -> (name, provenance)
    known = {}
    for spec in args.seed:
        parts = spec.split(":")
        path = parts[0] if len(parts) < 3 or len(parts[0]) > 1 else parts[0] + ":" + parts[1]
        rest = spec[len(path) + 1:].split(":") if len(spec) > len(path) else []
        va_col = rest[0] if len(rest) > 0 else "va"
        name_col = rest[1] if len(rest) > 1 else "name"
        for r in csv.DictReader(open(path)):
            n = r.get(name_col) or ""
            if n:
                known.setdefault(int(r[va_col], 16), (n, os.path.basename(path)))
    print("seeded %d names" % len(known))
    index, nref = load_refs(args.ref)
    print("indexed %d reference functions" % nref)

    cands = {}
    for ea, f in funcs.items():
        if ea >= boundary:
            continue
        t = img.bytes(ea, f["size"] + 16)
        cs = []
        for size in range(max(2, f["size"] - 15), f["size"] + 1):
            for rf in index.get(size, ()):
                if masked_equal(t, rf):
                    cs.append(rf)
        if cs:
            best = max(len(c.data) for c in cs)
            cands[ea] = [c for c in cs if len(c.data) == best]
    print("functions with byte candidates: %d" % len(cands))

    def score(ea, rf):
        s = 0
        for off, rtype, sym, addend, strb in rf.relocs:
            tgt = reloc_target(img, ea, off, rtype, addend)
            if tgt is None:
                continue
            tgt = thunks.get(tgt, tgt)
            if strb is not None:
                got = img.bytes(tgt, len(strb) + 1)
                s += 1 if got == strb + b"\0" else -3
            elif sym and tgt in known:
                s += 1 if short(known[tgt][0]) == short(sym) else -3
        return s

    accepted = {}
    for rnd in range(30):
        changed = 0
        for ea, cs in cands.items():
            if ea in accepted:
                continue
            scored = sorted(((score(ea, c), c) for c in cs), key=lambda x: -x[0])
            top, best = scored[0]
            others = [s for s, c in scored if short(c.name) != short(best.name)]
            if top < 0 or (others and others[0] >= top):
                continue
            uniq_bytes = len(set(short(c.name) for c in cs)) == 1
            if not (top > 0 or (uniq_bytes and len(best.data) >= 32)):
                continue
            if ea in known and short(known[ea][0]) != short(best.name):
                continue
            accepted[ea] = (best, top)
            known.setdefault(ea, (best.name, "refbytes"))
            changed += 1
            for off, rtype, sym, addend, strb in best.relocs:
                if not sym or strb is not None or sym.startswith(("$", ".")):
                    continue
                tgt = reloc_target(img, ea, off, rtype, addend)
                if tgt is None:
                    continue
                tgt = thunks.get(tgt, tgt)
                if tgt not in known and (tgt in funcs or rtype == REL_DIR32):
                    known[tgt] = (sym, "reloc:%08X" % ea)
                    changed += 1
        print("round %d: +%d (accepted %d, known %d)" % (rnd, changed, len(accepted), len(known)))
        if not changed:
            break

    # Similarity phase: name functions CoF modified (bytes differ) by the
    # strings they reference and the names of their callees.
    if args.similarity:
        ref_feats = {}
        for size, rfs in index.items():
            for rf in rfs:
                strs = frozenset(sb.decode("latin-1") for _o, _t, _s, _a, sb in rf.relocs if sb)
                calls = frozenset(short(s) for _o, t, s, _a, sb in rf.relocs
                                  if t == REL_REL32 and s and sb is None)
                if strs:
                    key = short(rf.name)
                    old = ref_feats.get(key)
                    if old is None or len(strs) > len(old[0]):
                        ref_feats[key] = (strs, calls, rf.name)
        by_string = collections.defaultdict(set)
        for key, (strs, calls, _n) in ref_feats.items():
            for s in strs:
                by_string[s].add(key)
        used = {short(n) for n, _p in known.values()}
        added = 0
        for ea, f in sorted(funcs.items()):
            if ea >= boundary or ea in known or not f["strings"]:
                continue
            strs = set(f["strings"])
            calls = {short(known[c][0]) for c in f["callees"] if c > 0 and c in known}
            cand = collections.Counter()
            for s in strs:
                for key in by_string.get(s, ()):
                    cand[key] += 1
            scored = []
            for key, shared in cand.items():
                if shared < 2 and len(strs) > 1:
                    continue
                rs, rc, rname = ref_feats[key]
                js = len(strs & rs) / float(len(strs | rs))
                jc = len(calls & rc) / float(len(calls | rc)) if (calls or rc) else 0.0
                scored.append((0.65 * js + 0.35 * jc, key, rname))
            if not scored:
                continue
            scored.sort(reverse=True)
            top, key, rname = scored[0]
            second = scored[1][0] if len(scored) > 1 else 0.0
            if top >= 0.45 and top - second >= 0.15 and key not in used:
                known[ea] = (rname, "similar:%.2f" % top)
                used.add(key)
                added += 1
        print("similarity phase: +%d" % added)

    rows = []
    for ea, (name, prov) in sorted(known.items()):
        if ea >= boundary and prov.startswith(("reloc", "refbytes")):
            continue
        is_func = ea in funcs
        acc = accepted.get(ea)
        rows.append({"va": "%08X" % ea, "name": name, "kind": "func" if is_func else "data",
                     "evidence": prov if not acc else "refbytes:%s:score%d" % (acc[0].src, acc[1])})
    with open(args.out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=["va", "name", "kind", "evidence"], lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    prov = collections.Counter(r["evidence"].split(":")[0] for r in rows)
    print("wrote %d names -> %s  %s" % (len(rows), args.out, dict(prov)))


if __name__ == "__main__":
    main()
