"""Write the coflaunchapp rows of docs/progress.csv from compare.py results.

Runs the method by method comparison of build/coflaunchapp/CoFLaunchApp.exe
against retail (build it first with build.py), then replaces every row whose
dll column is "coflaunchapp" with one row per retail method. All other rows
are written back unchanged and in the same order.

Columns:
  va             metadata token of the retail method (06xxxxxx)
  size           IL code size in bytes
  name           Namespace.Type::Method as ildasm prints it
  demangled      the ildasm method header (flags, signature)
  class          COF (the launcher is Cry of Fear code)
  source_file    file under src/ that holds the method
  first_version  first build whose launcher has a method of that name
  changed        whether the IL of that method changed between builds
  status         MATCHED when ildasm prints identical IL (header, locals,
                 instructions), EQUIVALENT when it differs, TODO if missing
  match_percent  100 for MATCHED; for a method that differs only in
                 .maxstack, the share of identical bytes in the method body
                 (12 byte fat header plus code)

Usage: python tools/coflaunchapp/progress.py
"""

import contextlib
import csv
import io
import math
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))

import compare  # noqa: E402
import csvio  # noqa: E402

ROOT = compare.ROOT
PROGRESS = os.path.join(ROOT, "docs", "progress.csv")
DLL = "coflaunchapp"
OLDER = [
    ("1.0", os.path.join(ROOT, "versions", "1.0", "CryOfFear", "CoFLaunchApp.exe")),
    ("1.55", os.path.join(ROOT, "versions", "1.55", "CoFLaunchApp.exe")),
]


def source_file(key):
    cls, method = key.split("::")
    method = method.split("#")[0]
    if cls == "CoFLaunchApp.Form1":
        if method in ("Dispose", "InitializeComponent"):
            return "coflaunchapp/Form1.Designer.cs"
        return "coflaunchapp/Form1.cs"
    if cls == "CoFLaunchApp.Program":
        return "coflaunchapp/Program.cs"
    if cls == "CoFLaunchApp.Properties.Resources":
        return "coflaunchapp/Properties/Resources.Designer.cs"
    if cls == "CoFLaunchApp.Properties.Settings":
        return "coflaunchapp/Properties/Settings.Designer.cs"
    return ""


def older_builds():
    tmp = tempfile.mkdtemp(prefix="coflaunchapp_old_")
    try:
        builds = [(v, compare.parse(compare.normalize(compare.ildasm(p, os.path.join(tmp, v)))))
                  for v, p in OLDER]
        builds.append(("1.6a", compare.parse(compare.normalize(compare.ildasm(compare.RETAIL, os.path.join(tmp, "t"))))))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return builds


def history(key, builds):
    present = [(v, b["methods"][key]) for v, b in builds if key in b["methods"]]
    first = present[0][0]
    steps = []
    for (va, ma), (vb, mb) in zip(present, present[1:]):
        same = ma["header"] == mb["header"] and ma["body"] == mb["body"]
        steps.append("%s->%s %s" % (va, vb, "same" if same else "changed"))
    return first, "; ".join(steps)


def percent(entry):
    if entry["status"] == "identical":
        return "100"
    if entry.get("maxstack_only"):
        a, b = entry["maxstack"]
        differing = sum(1 for x, y in zip(a.to_bytes(2, "little"), b.to_bytes(2, "little")) if x != y)
        total = 12 + entry["size"]
        return "%.2f" % (math.floor(10000.0 * (total - differing) / total) / 100)
    return ""


def notes(entry):
    if entry["status"] == "identical":
        return ""
    if entry.get("maxstack_only"):
        a, b = entry["maxstack"]
        return ("instructions and locals identical; .maxstack retail %d, rebuilt %d. The C# 5 csc (4.5 to 4.8) "
                "emits the exact depth, csc 4.0 (the retail compiler) one more after a newobj" % (a, b))
    return entry.get("diff", "")


def main():
    if not os.path.isfile(compare.REBUILT):
        sys.exit("build first: python tools/coflaunchapp/build.py")
    with contextlib.redirect_stdout(io.StringIO()):
        results = compare.compare()
    builds = older_builds()

    new_rows = []
    for e in results:
        status = {"identical": "MATCHED", "different": "EQUIVALENT"}.get(e["status"], "TODO")
        first, changed = history(e["key"], builds)
        header = re.sub(r"^\.method\s+", "", e["header"])
        header = re.sub(r"\s+cil managed$", "", header)
        new_rows.append({
            "dll": DLL,
            "va": e["token"],
            "size": str(e["size"]),
            "name": e["key"],
            "demangled": header,
            "class": "COF",
            "source_file": source_file(e["key"]),
            "first_version": first,
            "changed": changed,
            "status": status,
            "match_percent": percent(e) if status != "TODO" else "",
            "evidence": "ildasm_il_compare",
            "notes": notes(e),
        })

    with open(PROGRESS, newline="") as f:
        reader = csv.DictReader(f)
        fields = reader.fieldnames
        rows = list(reader)
    out = []
    inserted = False
    for row in rows:
        if row["dll"] == DLL:
            if not inserted:
                out.extend(new_rows)
                inserted = True
            continue
        out.append(row)
    if not inserted:
        out.extend(new_rows)
    csvio.write_rows(PROGRESS, fields, out)

    counts = {}
    for r in new_rows:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    print("%s: %d rows (%s)" % (DLL, len(new_rows), ", ".join("%s %d" % kv for kv in sorted(counts.items()))))


if __name__ == "__main__":
    main()
