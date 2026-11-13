"""Find the retail address of our globals that the client map does not name.

For every client unit, pair the functions present in both the retail
(target) and our (base) object; inside a pair whose code bytes are the same
length, pair the DIR32 relocations by offset. When the retail side points
at a default Ghidra label (DAT_1018A0C0) and ours at a named global plus an
addend, vote base = label address + retail addend - our addend.
Prints, per global: the winning base, its votes, and the disagreeing votes.
"""
import collections
import csv
import json
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEF = re.compile(r"^(?:DAT|PTR|BYTE|WORD|DWORD|s|u|OFF)_([0-9a-fA-F]{8})")


def coff(path):
    d = open(path, "rb").read()
    _m, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", d, 0)
    strtab = symptr + 18 * nsym
    syms = [None] * nsym
    funcs = {}
    i = 0
    while i < nsym:
        o = symptr + 18 * i
        raw = d[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            n = d[strtab + off:d.index(b"\0", strtab + off)].decode("latin-1")
        else:
            n = raw.rstrip(b"\0").decode("latin-1")
        val, sec, typ, cls, naux = struct.unpack_from("<IhHBB", d, o + 8)
        syms[i] = (n, sec, val)
        if sec > 0 and typ == 0x20 and cls in (2, 3):
            funcs[n] = (sec, val)
        i += 1 + naux
    secs = []
    for s in range(nsec):
        o = 20 + opt + 40 * s
        size, praw, prel = struct.unpack_from("<III", d, o + 16)
        nrel = struct.unpack_from("<H", d, o + 32)[0]
        rel = []
        for k in range(nrel):
            va, si, ty = struct.unpack_from("<IIH", d, prel + 10 * k)
            if ty == 6:
                add = struct.unpack_from("<i", d, praw + va)[0]
                rel.append((va, syms[si][0], add))
        secs.append((size, rel))
    return funcs, secs


def ranges(funcs, secs):
    by = collections.defaultdict(list)
    for n, (sec, val) in funcs.items():
        by[sec].append((val, n))
    out = {}
    for sec, lst in by.items():
        lst.sort()
        for i, (val, n) in enumerate(lst):
            end = lst[i + 1][0] if i + 1 < len(lst) else secs[sec - 1][0]
            out[n] = (sec, val, end)
    return out


def main():
    cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
    mapped = {r["name"] for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_client.csv")))}
    votes = collections.defaultdict(collections.Counter)
    for u in cfg["units"]:
        if not u["name"].startswith("client/"):
            continue
        tp, bp = os.path.join(ROOT, u["target_path"]), os.path.join(ROOT, u["base_path"])
        if not (os.path.exists(tp) and os.path.exists(bp)):
            continue
        tf, ts = coff(tp)
        bf, bs = coff(bp)
        tr_by = ranges(tf, ts)
        br_by = ranges(bf, bs)
        for name, (bsec, bstart, bend) in br_by.items():
            if name not in tr_by:
                continue
            tsec, tstart, tend = tr_by[name]
            if bend - bstart != tend - tstart:
                continue
            tr = {va - tstart: (n, a) for va, n, a in ts[tsec - 1][1] if tstart <= va < tend}
            for va, n, a in bs[bsec - 1][1]:
                if not bstart <= va < bend:
                    continue
                if n in mapped or (va - bstart) not in tr:
                    continue
                rn, ra = tr[va - bstart]
                m = DEF.match(rn)
                if not m:
                    continue
                votes[n][int(m.group(1), 16) + ra - a] += 1
    rows = sorted(votes.items(), key=lambda kv: -sum(kv[1].values()))
    for n, c in rows:
        (base, k), = c.most_common(1)
        other = sum(c.values()) - k
        print("%08X %5d %4d  %s" % (base, k, other, n))


if __name__ == "__main__":
    main()
