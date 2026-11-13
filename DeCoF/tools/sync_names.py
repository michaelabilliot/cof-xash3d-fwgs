"""Carry the merged retail symbol map into docs/progress.csv and the unit lists.

  python tools/sync_names.py [--dry] [--dll client]

After a name map change (tools/symbols/*.csv, merged into
build/jobs/symbols_hl.csv by the last delink), progress.csv still shows the
old names, and functions that just got a name are in no unit list, so the
delinker does not export them and objdiff never reports them. For every hl
row whose merged name differs from the progress.csv name this:
- updates the name and the demangled column (undname);
- gives a row with no source file (or a "guess:" one) the unit whose
  retail address range contains it, and moves a row whose old name was
  wrong to the unit of its address;
- appends the name to that unit's function list in tools/units/.
Prints what it changes; --dry changes nothing. --since-head first resets
the rows whose name differs from git HEAD, so a run can be repeated.
"""
import bisect
import csv
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import csvio  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UNDNAME = r"I:\Tools\msvc-14.27.29112\bin\Hostx86\x86\undname.exe"


def undecorate(names):
    if not names:
        return {}
    tmp = os.path.join(ROOT, "build", "jobs", "sync_names.txt")
    with open(tmp, "w") as f:
        f.write("\n".join(names) + "\n")
    out = subprocess.run([UNDNAME, tmp], stdout=subprocess.PIPE).stdout.decode("latin-1").splitlines()
    out = [l for l in out if l.strip()]
    return dict(zip(names, out)) if len(out) == len(names) else {}


def is_std(name, full):
    """True when the function itself lives in namespace std (an STL
    instantiation), not when it only takes or returns std types
    (CCareerGame::GetTaskVecForMap returns a std::vector)."""
    if full:
        for cc in ("__thiscall ", "__cdecl ", "__stdcall ", "__fastcall "):
            i = full.find(cc)
            if i >= 0:
                return full[i + len(cc):].startswith("std::")
    return "@std@@" in name


def main():
    dry = "--dry" in sys.argv
    dll = sys.argv[sys.argv.index("--dll") + 1] if "--dll" in sys.argv else "hl"
    path = os.path.join(ROOT, "docs", "progress.csv")
    rows = list(csv.DictReader(open(path, newline="")))
    fields = list(rows[0].keys())
    merged = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll))):
        if r.get("kind", "func") == "func":
            merged[int(r["va"], 16)] = r["name"]
    cfg = json.load(open(os.path.join(ROOT, "tools", "units.json")))
    units = {u["name"]: u for u in cfg["dlls"][dll]["units"]}
    # rows that already belong to a unit, in address order; a few rows are
    # attributed to the wrong unit, so the unit of an address is the
    # majority of its nearest attributed neighbours on both sides
    unit_of_src = {}
    for name, u in units.items():
        unit_of_src[name + ".cpp"] = name
        unit_of_src[name + ".c"] = name
    attributed = sorted((int(r["va"], 16), unit_of_src[r["source_file"]]) for r in rows
                        if r["dll"] == dll and r["class"] != "LIB" and r["source_file"] in unit_of_src)
    avas = [a for a, _u in attributed]

    def unit_at(va, k=4):
        i = bisect.bisect_left(avas, va)
        near = [u for _a, u in attributed[max(0, i - k):i]] + [u for a, u in attributed[i:i + k + 1] if a != va]
        if not near:
            return None
        best = max(set(near), key=near.count)
        # the address must lie inside that unit's span of neighbours
        return best if near.count(best) * 2 > len(near) else None

    byte_unit = {}
    bp = os.path.join(ROOT, "tools", "symbols", "%s_bytes.csv" % dll)
    if os.path.exists(bp):
        for b in csv.DictReader(open(bp)):
            m = re.search(r" in %s/(\S+) " % dll, b["evidence"])
            if m and m.group(1) in units:
                byte_unit[int(b["va"], 16)] = m.group(1)

    # a position name says which object holds the function too
    pp = os.path.join(ROOT, "tools", "symbols", "%s_position.csv" % dll)
    if os.path.exists(pp):
        for b in csv.DictReader(open(pp)):
            m = re.search(r"^position in %s/(\S+) " % dll, b["evidence"])
            if m and m.group(1) in units:
                byte_unit[int(b["va"], 16)] = m.group(1)

    head = {}
    if "--since-head" in sys.argv:
        import io
        text = subprocess.run(["git", "show", "HEAD:docs/progress.csv"], cwd=ROOT, stdout=subprocess.PIPE).stdout.decode()
        head = {(h["dll"], h["va"]): h for h in csv.DictReader(io.StringIO(text))}
    # a curated name replaces the evidence of the name it corrects
    manual = {}
    mp = os.path.join(ROOT, "tools", "symbols", "%s_manual.csv" % dll)
    if os.path.exists(mp):
        manual = {int(m["va"], 16): m["name"] for m in csv.DictReader(open(mp))}
    changes = []
    for r in rows:
        h = head.get((r["dll"], r["va"]))
        if h and r["dll"] == dll and r["class"] != "LIB" and h["name"] != r["name"]:
            # renamed by an earlier run: start again from the committed row
            r["name"], r["source_file"] = h["name"], h["source_file"]
        if r["dll"] != dll or r["class"] == "LIB":
            continue  # toolchain code is not decompiled; its names stay as they are
        va = int(r["va"], 16)
        new = merged.get(va)
        if not new or new == r["name"] or not new.startswith(("?", "_")):
            continue
        # a byte-proven name says which of our objects holds the function
        # (STL instantiations sit after the object that first uses them)
        # (the client has no complete unit layout yet: tools/client_layout.py
        # assigns its files, so only byte-proven units are used there)
        unit = byte_unit.get(va) or (unit_at(va) if dll == "hl" else None)
        changes.append((r, new, unit))
    dem = undecorate(sorted({n for _r, n, _u in changes if n.startswith("?")}))
    lists = {}
    for r, new, unit in changes:
        old_name, old_src = r["name"], r["source_file"]
        r["name"] = new
        if manual.get(int(r["va"], 16)) == new:
            r["evidence"] = "%s_manual.csv" % dll
        if is_std(new, dem.get(new)):
            r["class"] = "LIB"  # STL instantiation from the toolchain headers
        r["demangled"] = dem.get(new, r["demangled"]).split("(")[0].split(" ")[-1] if new in dem else r["demangled"]
        if unit and (not old_src or old_src.startswith("guess:") or old_name):
            src = os.path.splitext(units[unit]["source"])[1]
            r["source_file"] = unit + (".c" if src == ".c" else ".cpp")
            lists.setdefault(unit, []).append(new)
        print("%s  %-55s %s -> %s" % (r["va"], new[:55], old_name[:40] or "unnamed", r["source_file"]))
    if dry:
        print("%d rows (dry run)" % len(changes))
        return
    csvio.write_rows(path, fields, rows)
    for unit, names in lists.items():
        lf = os.path.join(ROOT, units[unit]["functions_file"])
        have = [l.strip() for l in open(lf) if l.strip()] if os.path.exists(lf) else []
        add = [n for n in names if n not in have]
        if add:
            with open(lf, "w") as f:
                f.write("\n".join(have + add) + "\n")
    print("%d rows updated, %d unit lists extended" % (len(changes), len(lists)))


if __name__ == "__main__":
    main()
