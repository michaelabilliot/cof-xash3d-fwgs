"""Name retail functions by their position inside an object.

  python tools/align_names.py [--dll client] [--write] <unit> [<unit> ...]

A function that Cry of Fear changed matches no reference bytes, so it has
no name, and objdiff cannot pair it with our source. Inside one object the
linker keeps the compiler's function order, which is either the decorated
name order (most objects) or the source order (some objects, for example
dlls/cbase.cpp). For each unit this:
- takes the retail functions of the unit's window (from the last function
  of the file before it to the first function of the file after it);
- takes our compiled functions in both orders and keeps the order in which
  the already named retail functions (anchors) appear in address order;
- between two consecutive anchors, pairs the unnamed retail functions with
  our functions that have no retail address, in order, when both counts
  are equal.
Gaps where the counts differ are listed (a function missing on one side:
not written yet, inlined, or emitted by another object) and left alone.
--write stores the pairs in tools/symbols/<dll>_position.csv, merged by
tools/build.py after <dll>_bytes.csv. A position name is weaker evidence
than a byte match: the function must still reach 100% in objdiff.
"""
import bisect
import csv
import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decorate_names import short_name, undecorate  # noqa: E402


def coff_order(path):
    """Our functions in object order: [(name, size)] by COMDAT section number."""
    data = open(path, "rb").read()
    _m, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    sizes = []
    for i in range(nsec):
        o = 20 + opt + 40 * i
        size, = struct.unpack_from("<I", data, o + 16)
        chars, = struct.unpack_from("<I", data, o + 36)
        sizes.append((size, chars))
    strtab = symptr + 18 * nsym
    out = []
    i = 0
    while i < nsym:
        o = symptr + 18 * i
        raw = data[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            name = data[strtab + off:data.index(b"\0", strtab + off)].decode("latin-1")
        else:
            name = raw.rstrip(b"\0").decode("latin-1")
        value, secnum, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        if secnum > 0 and typ == 0x20 and cls == 2 and value == 0 and sizes[secnum - 1][1] & 0x20:
            out.append((secnum, name, sizes[secnum - 1][0]))
        i += 1 + naux
    return [(n, s) for _k, n, s in sorted(out)]


def main():
    argv = sys.argv[1:]
    dll = "hl"
    if "--dll" in argv:
        i = argv.index("--dll")
        dll = argv[i + 1]
        del argv[i:i + 2]
    write = "--write" in argv
    units = [a for a in argv if not a.startswith("--")]
    if not units:
        sys.exit(__doc__)
    cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
    paths = {u["name"]: u for u in cfg["units"]}
    rows = sorted((r for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv")))
                   if r["dll"] == dll and r["class"] != "LIB"), key=lambda r: int(r["va"], 16))
    names = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll))):
        if r["kind"] == "func":
            names[int(r["va"], 16)] = r["name"]
    retail_names = set(names.values())
    index = json.load(open(os.path.join(ROOT, "build", "ref_index.json")))
    side = "client" if dll == "client" else "server"
    defined_in = {k: v for k, v in index.items() if k.endswith("/" + side)}
    proposals = []
    for unit in units:
        src = unit.split("/", 1)[1]
        # library templates (STL, VGUI1 Dar) live with whichever object
        # first used them, not with this one
        ours = [(n, s) for n, s in coff_order(os.path.join(ROOT, paths[unit]["base_path"])) if "@std@@" not in n and "@vgui@@" not in n]
        # so do inline functions of headers (cl_util.h ConsolePrint): the
        # reference index says where each function is defined
        short = undecorate([n for n, _s in ours if n.startswith("?")])
        ours = [(n, s) for n, s in ours
                if not any(v.get(short_name(short.get(n, "")) or n, "").endswith(".h") for v in defined_in.values())]
        mine = [r for r in rows if os.path.splitext(r["source_file"].replace("guess:", ""))[0] == src]
        if not mine:
            print("%s: no rows" % unit)
            continue
        lo, hi = int(mine[0]["va"], 16), int(mine[-1]["va"], 16)
        others = [int(r["va"], 16) for r in rows if r["source_file"]
                  and os.path.splitext(r["source_file"].replace("guess:", ""))[0] != src]
        k = bisect.bisect_left(others, lo)
        lo = others[k - 1] + 1 if k else lo
        k = bisect.bisect_right(others, hi)
        hi = others[k] - 1 if k < len(others) else hi
        window = [r for r in rows if lo <= int(r["va"], 16) <= hi]
        ournames = {n for n, _s in ours}
        # pick the order in which the anchors are increasing
        chosen = None
        for label, order in (("name", sorted(ours)), ("source", ours)):
            pos = {n: i for i, (n, _s) in enumerate(order)}
            seq = [pos[names[int(r["va"], 16)]] for r in window if names.get(int(r["va"], 16)) in pos]
            if all(a < b for a, b in zip(seq, seq[1:])):
                chosen = (label, order, pos)
                break
        if not chosen:
            print("%s: anchors fit neither order" % unit)
            continue
        label, order, pos = chosen
        # walk the gaps between anchors
        marks = [(i, pos[names[int(r["va"], 16)]]) for i, r in enumerate(window) if names.get(int(r["va"], 16)) in pos]
        bounds = [(-1, -1)] + marks + [(len(window), len(order))]
        for (ra, oa), (rb, ob) in zip(bounds, bounds[1:]):
            gap_r = [r for r in window[ra + 1:rb] if not names.get(int(r["va"], 16)) or names[int(r["va"], 16)].startswith(("FUN_", "thunk_"))]
            gap_o = [(n, s) for n, s in order[oa + 1:ob] if n not in retail_names]
            # the window edges may hold functions of the neighbouring files
            edge = ra == -1 or rb == len(window)
            if not gap_r and not gap_o:
                continue
            before = names.get(int(window[ra]["va"], 16), "start") if ra >= 0 else "start"
            if len(gap_r) == len(gap_o) and not edge:
                # a Cry of Fear change makes the sizes differ, but not by more
                # than this; CRT helpers (_fabsl, __vsnprintf_l) are not ours
                for r, (n, s) in zip(gap_r, gap_o):
                    rs = int(r["size"] or 0)
                    crt = n.startswith("__") or (n.startswith("_") and n[1:2].islower())
                    if crt or not rs or not 0.4 <= s / rs <= 2.5:
                        continue
                    proposals.append((r["va"], n, r["size"], s, unit, label, before))
            elif edge and gap_r and gap_o and 0 <= ra or edge and gap_r and gap_o and rb < len(window):
                # at a window edge the retail rows next to the anchor are the
                # first (or last) functions of this object; pair outward from
                # the anchor while the sizes stay close, stop at the first
                # pair that does not fit (a function of the neighbouring file)
                pairs = list(zip(gap_r[::-1], gap_o[::-1])) if ra == -1 else list(zip(gap_r, gap_o))
                taken = 0
                for r, (n, s) in pairs:
                    rs = int(r["size"] or 0)
                    if not rs or not 0.75 <= s / rs <= 1.33:
                        break
                    proposals.append((r["va"], n, r["size"], s, unit, label, before if ra >= 0 else "edge before the first anchor"))
                    taken += 1
                print("%s: window edge after %s: %d retail, %d ours, %d paired by size" % (
                    unit, before[:40], len(gap_r), len(gap_o), taken))
            if (gap_r or gap_o) and not (edge and gap_r and gap_o) and len(gap_r) != len(gap_o):
                print("%s: gap after %s: %d retail, %d ours%s" % (
                    unit, names.get(int(window[ra]["va"], 16), "?")[:40] if ra >= 0 else "window start",
                    len(gap_r), len(gap_o), " (window edge)" if edge else ""))
    for va, n, rs, s, unit, label, before in proposals:
        print("%s  %-60s retail %5s ours %5d  %s (%s order)" % (va, n[:60], rs, s, unit, label))
    print("%d proposals" % len(proposals))
    if write and proposals:
        path = os.path.join(ROOT, "tools", "symbols", "%s_position.csv" % dll)
        old = {}
        if os.path.exists(path):
            for r in csv.DictReader(open(path)):
                old[r["va"]] = (r["name"], r["kind"], r["evidence"])
        for va, n, rs, s, unit, label, before in proposals:
            old[va] = (n, "func", "position in %s (%s order), after %s; sizes retail %s, ours %d" % (unit, label, before, rs, s))
        with open(path, "w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["va", "name", "kind", "evidence"])
            for va in sorted(old):
                w.writerow([va] + list(old[va]))
        print("wrote %s (%d entries)" % (os.path.relpath(path, ROOT), len(old)))


if __name__ == "__main__":
    main()
