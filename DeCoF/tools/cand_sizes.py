"""For hl units: unnamed TODO retail rows of the unit's source file and our
unmapped functions of similar size (candidates for add_name)."""
import csv
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from align_names import coff_order  # noqa: E402

dll = sys.argv[1] if len(sys.argv) > 1 else "hl"
cfg = {u["name"]: u for u in json.load(open(os.path.join(ROOT, "objdiff.json")))["units"]}
mapped = {r["name"] for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll)))}
rows = [r for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv")))
        if r["dll"] == dll and r["class"] != "LIB" and r["status"] == "TODO" and not r["name"]]
byfile = {}
for r in rows:
    byfile.setdefault(os.path.splitext(r["source_file"].replace("guess:", ""))[0], []).append(r)
SKIP = ("??0", "??1", "??_", "??2", "??3", "??8", "??B", "??D", "??G", "??H", "??R", "?DotProduct", "?ENT", "?FNullEnt", "?FStringNull",
        "?GET_PRIVATE", "?INDEXENT", "?Instance@CBaseEntity", "?Length", "?MESSAGE_BEGIN", "?OFFSET", "?VARS", "?edict@", "?entindex@",
        "?eoffset@", "?abs@", "?fabs@", "?strchr@", "?strstr@", "__v", "_fabs", "_frexp", "?EMIT_SOUND@", "?FClassnameIs@", "?ENTINDEX")
for src, rs in sorted(byfile.items()):
    unit = "%s/%s" % (dll, src)
    if unit not in cfg:
        continue
    ours = [(n, s) for n, s in coff_order(os.path.join(ROOT, cfg[unit]["base_path"]))
            if n not in mapped and "@std@@" not in n and not n.startswith(SKIP)]
    print("==", unit)
    for r in rs:
        size = int(r["size"] or 0)
        cand = sorted(ours, key=lambda ns: abs(ns[1] - size))[:3]
        print("  %s %5d  %s" % (r["va"], size, "  ".join("%s(%d)" % (n[:50], s) for n, s in cand)))
