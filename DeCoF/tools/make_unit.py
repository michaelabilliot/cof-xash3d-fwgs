"""Register a source file as a build unit from docs/progress.csv.

Usage: python tools/make_unit.py <dll> <source_file> [--src PATH]
  e.g. python tools/make_unit.py hl pm_shared/pm_math.c

Collects the decorated names of every function of <dll> whose source_file is
<source_file> (LIB rows excluded), writes them to
tools/units/<dll>_<path with / replaced by _>.txt and adds or updates the
unit <dll>/<source_file without extension> in tools/units.json. The unit
compiles src/<source_file> unless --src says otherwise.
"""
import argparse
import csv
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dll")
    ap.add_argument("source_file")
    ap.add_argument("--src")
    args = ap.parse_args()
    names = []
    for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))):
        # "guess:" rows lie between two functions of the file (tools/client_layout.py)
        if r["dll"] == args.dll and r["source_file"].lower().replace("guess:", "") == args.source_file.lower() \
                and r["class"] != "LIB" and r["name"]:
            names.append(r["name"])
    if not names:
        raise SystemExit("no named functions attributed to %s in %s" % (args.source_file, args.dll))
    stem = os.path.splitext(args.source_file)[0]
    listfile = "tools/units/%s_%s.txt" % (args.dll, stem.replace("/", "_"))
    with open(os.path.join(ROOT, listfile), "w") as f:
        f.write("\n".join(names) + "\n")
    cfg_path = os.path.join(ROOT, "tools", "units.json")
    cfg = json.load(open(cfg_path))
    units = cfg["dlls"][args.dll]["units"]
    unit = {"name": stem, "source": args.src or "src/" + args.source_file, "functions_file": listfile}
    for i, u in enumerate(units):
        if u["name"] == stem:
            if not args.src:
                unit["source"] = u["source"]  # keep an existing path (original file name case)
            units[i] = unit
            break
    else:
        units.append(unit)
    with open(cfg_path, "w") as f:
        json.dump(cfg, f, indent=2)
        f.write("\n")
    print("%s/%s: %d functions -> %s" % (args.dll, stem, len(names), listfile))


if __name__ == "__main__":
    main()
