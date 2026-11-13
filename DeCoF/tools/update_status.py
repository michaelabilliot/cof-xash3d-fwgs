"""Write objdiff results back into docs/progress.csv.

Usage: python tools/update_status.py [--report build/report.json] (<unit> ... | --all)

For every function in the objdiff report: 100% -> status MATCHED and
match_percent 100 (an "EQUIVALENT: ..." note is cleared); below 100 ->
match_percent updated, MATCHED goes back to TODO, other statuses are left as
is (TODO or EQUIVALENT are decided by hand). Functions are located by their
decorated name through tools/symbols/<dll>_1.6a.csv. Unit names in the report
are "<dll>/<path>". With unit names, only those units are written back:
the report covers every unit of objdiff.json, and the objects of a unit an
agent is working on can be stale or half edited. With unit names and no
--report, the report of the last `build.py check` of exactly those units
(build/check_project/<units>/report.json) is used.
"""
import argparse
import csv
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import csvio  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report")
    ap.add_argument("--no-relocs", action="store_true")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("units", nargs="*")
    args = ap.parse_args()
    # a whole-project report holds stale or missing objects of other DLLs
    # (one such run set 1880 hl.dll rows to TODO 0%): write back every unit
    # only when asked
    if not args.units and not args.all:
        ap.error("name the units to write back, or pass --all for every unit of the report")
    if not args.report:
        if args.units:
            # same directory name as tools/build.py check
            tag = "_".join(sorted(n.replace("/", "_") for n in args.units))[:120]
            args.report = os.path.join(ROOT, "build", "check_project", tag, "report.json")
        else:
            args.report = os.path.join(ROOT, "build", "report.json")
    print("report %s" % os.path.relpath(args.report, ROOT))
    rep = json.load(open(args.report))
    path = os.path.join(ROOT, "docs", "progress.csv")
    # a function at 100% whose relocations point at other symbols (or at
    # another member of the same global) is not matched: tools/relocaudit.py
    real = {}
    if not args.no_relocs:
        import relocaudit
        for unit in rep.get("units", []):
            if args.units and unit["name"] not in args.units:
                continue
            if any(fn.get("fuzzy_match_percent", 0.0) >= 100.0 for fn in unit.get("functions", [])):
                real[unit["name"]] = relocaudit.real_differences(unit["name"]) or {}
    args.real = real
    # progress.csv is also written by tools/name_unit.py and add_name.py:
    # read and write it under their lock
    import name_unit
    name_unit.lock()
    try:
        changed = update(rep, args, path)
    finally:
        name_unit.unlock()
    print("updated %d rows" % changed)


def update(rep, args, path):
    rows = list(csv.DictReader(open(path)))
    fields = list(rows[0].keys())
    index = {(r["dll"], r["va"]): r for r in rows}
    symbols = {}
    changed = 0
    for unit in rep.get("units", []):
        if args.units and unit["name"] not in args.units:
            continue
        dll = unit["name"].split("/")[0]
        if dll not in symbols:
            # the merged map used by the last delink (1.6a + manual + auto
            # names), else the base map
            sp = os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll)
            if not os.path.exists(sp):
                sp = os.path.join(ROOT, "tools", "symbols", "%s_1.6a.csv" % dll)
            symbols[dll] = {}
            if os.path.exists(sp):
                for r in csv.DictReader(open(sp)):
                    symbols[dll].setdefault(r["name"], []).append(r["va"])
        # a name local to its object (the VC6 dynamic initializers _$E1, ...,
        # the file statics of tools/symbols/<dll>_local.csv) labels one
        # address per unit: take the row of this unit's file
        stem = unit["name"].split("/", 1)[1] + "."
        for fn in unit.get("functions", []):
            # the objects spell the VC6 dynamic initializers _E$1 (tools/vc6_anon.py)
            name = "_$E" + fn["name"][3:] if fn["name"].startswith("_E$") else fn["name"]
            vas = symbols[dll].get(name, [])
            if len(vas) > 1:
                own = [v for v in vas if index.get((dll, v), {}).get("source_file", "").replace("guess:", "").startswith(stem)]
                vas = own or vas
            va = vas[0] if vas else None
            row = index.get((dll, va)) if va else None
            if row is None:
                continue
            pct = fn.get("fuzzy_match_percent", 0.0)
            wrong = args.real.get(unit["name"], {}).get(fn["name"])
            if wrong and pct >= 100.0:
                pct = 99.99
            # a MATCHED function that no longer matches goes back to TODO
            if pct >= 100.0:
                new_status = "MATCHED"
            elif row["status"] == "MATCHED":
                new_status = "TODO"
            else:
                new_status = row["status"]
            # floored: 99.995 must not read as 100
            new_pct = "100" if pct >= 100.0 else "%.2f" % (math.floor(pct * 100) / 100)
            # the remaining-difference note of an EQUIVALENT function is
            # obsolete once it matches
            new_notes = row["notes"]
            if wrong:
                new_notes = "relocation targets differ (instructions identical): " + "; ".join(
                    "retail %s, ours %s" % p for p in wrong[:3])
            elif new_notes.startswith("relocation targets differ"):
                new_notes = ""
            if new_status == "MATCHED" and new_notes.startswith("EQUIVALENT:"):
                new_notes = ""
            if row["status"] != new_status or row["match_percent"] != new_pct or row["notes"] != new_notes:
                row["status"], row["match_percent"], row["notes"] = new_status, new_pct, new_notes
                changed += 1
    csvio.write_rows(path, fields, rows)
    return changed


if __name__ == "__main__":
    main()
