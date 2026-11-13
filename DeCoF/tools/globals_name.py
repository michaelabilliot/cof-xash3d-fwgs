"""Turn the votes of globals_vote.py into map rows and extents.

Writes the proposed rows to stdout (dry run) or, with --write, appends them
to tools/symbols/client_manual.csv (kind data) and their sizes to
tools/symbols/client_datasizes.csv. The extent is our object size, capped
by the distance to the next retail data address that is named or voted.
"""
import collections
import csv
import glob
import importlib.util
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import data_sizes  # noqa: E402
import name_unit  # noqa: E402

spec = importlib.util.spec_from_file_location("gv", os.path.join(HERE, "globals_vote.py"))
gv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gv)

SKIP = ("__real@", "__xmm@", "??_C@", "___security_cookie", "?_OptionsStorage", "__TI", "__CT", "__CTA",
        "?_Stinit", "?_Id_cnt", "?id@", "?_Storage", "?_Iostream_error", "??_R", "??_7", "??_8", "___", "?_Fac")
FIXED = {  # disagreeing votes resolved by hand (see the session notes)
    "?guess_vecIronsightOfs@@3VVector@@A": 0x105594F0,
    "?guess_iTelescopeFlag@@3HA": 0x105436F8,
    "?guess_flUnknown105595A0@@3MA": 0x105595A0,
    "?guess_flUnknown105595A4@@3MA": 0x105595A4,
    "?g_iUser1@@3HA": 0x105593B8,
    "?g_HeadShieldEnt@@3Ucl_entity_s@@A": 0x10550348,
}


def votes():
    cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
    mapped = {r["name"] for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_client.csv")))}
    v = collections.defaultdict(collections.Counter)
    for u in cfg["units"]:
        if not u["name"].startswith("client/"):
            continue
        tp, bp = os.path.join(ROOT, u["target_path"]), os.path.join(ROOT, u["base_path"])
        if not (os.path.exists(tp) and os.path.exists(bp)):
            continue
        tf, ts = gv.coff(tp)
        bf, bs = gv.coff(bp)
        tr_by, br_by = gv.ranges(tf, ts), gv.ranges(bf, bs)
        for name, (bsec, bstart, bend) in br_by.items():
            if name not in tr_by:
                continue
            tsec, tstart, tend = tr_by[name]
            if bend - bstart != tend - tstart:
                continue
            tr = {va - tstart: (n, a) for va, n, a in ts[tsec - 1][1] if tstart <= va < tend}
            for va, n, a in bs[bsec - 1][1]:
                if not bstart <= va < bend or n in mapped or (va - bstart) not in tr:
                    continue
                rn, ra = tr[va - bstart]
                m = gv.DEF.match(rn)
                if m:
                    v[n][int(m.group(1), 16) + ra - a] += 1
    return v


def main():
    write = "--write" in sys.argv
    v = votes()
    sizes = {}
    for obj in glob.glob(os.path.join(ROOT, "build", "base", "client", "**", "*.obj"), recursive=True):
        for name, size in data_sizes.coff_data_sizes(obj).items():
            sizes[name] = max(size, sizes.get(name, 0))
    known = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_client.csv"))):
        if r["kind"] != "func":
            known[int(r["va"], 16)] = r["name"]
    rows = []
    for n, c in v.items():
        if n.startswith(SKIP):
            continue
        (base, k), = c.most_common(1)
        bad = sum(c.values()) - k
        if n in FIXED:
            base = FIXED[n]
        elif bad:
            print("SKIP disagreeing", n, dict(c))
            continue
        if base in known and known[base] != n:
            print("SKIP address already named", n, hex(base), known[base])
            continue
        rows.append((base, n, k))
    addrs = sorted(set(list(known) + [b for b, _n, _k in rows]))
    out = []
    for base, n, k in sorted(rows):
        i = addrs.index(base)
        gap = addrs[i + 1] - base if i + 1 < len(addrs) else 0
        size = sizes.get(n, 0)
        if gap and (not size or size > gap):
            size = gap
        out.append((base, n, k, size))
        print("%08X %6d %5d  %s" % (base, size, k, n))
    print(len(out), "rows")
    if not write:
        return
    name_unit.lock()
    try:
        p = os.path.join(ROOT, "tools", "symbols", "client_manual.csv")
        man = list(csv.DictReader(open(p)))
        for r in man:
            extra = r.pop(None, None)
            if extra:
                r["evidence"] = ",".join([r["evidence"]] + extra)
        have = {r["va"] for r in man}
        for base, n, k, size in out:
            va = "%08X" % base
            if va in have:
                continue
            man.append({"va": va, "name": n, "kind": "data",
                        "evidence": "address from %d paired references in functions of identical size (our global plus addend against the retail address)" % k})
        tmp = p + ".tmp"
        with open(tmp, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["va", "name", "kind", "evidence"], lineterminator="\n")
            w.writeheader()
            w.writerows(man)
        os.replace(tmp, p)
        p = os.path.join(ROOT, "tools", "symbols", "client_datasizes.csv")
        ds = list(csv.DictReader(open(p)))
        hv = {r["name"] for r in ds}
        for base, n, k, size in out:
            if n not in hv and size:
                ds.append({"name": n, "size": str(size), "source": "our object, capped at the next retail symbol"})
        with open(p, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["name", "size", "source"], lineterminator="\n")
            w.writeheader()
            w.writerows(ds)
    finally:
        name_unit.unlock()


if __name__ == "__main__":
    main()
