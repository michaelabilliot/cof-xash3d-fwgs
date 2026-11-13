"""Record the name of one retail function, with its evidence.

  python tools/add_name.py [--dll client] <unit> <va> "<decorated name>" "<evidence>"

For a function that neither tools/bytematch.py nor tools/align_names.py
can name (Cry of Fear changed it and the counts around it differ): after
reading its code, give it the decorated name of the function of our object
that it is (copy the name from `tools/build.py check` or the object), and
say why in the evidence (strings it uses, the callers, the vtable slot, the
reference function it follows). Under the naming lock this appends the row
to tools/symbols/<dll>_manual.csv, delinks the unit, carries the name into
progress.csv and the unit list, and delinks again. The main agent reviews
every entry.
"""
import csv
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import name_unit  # noqa: E402

ROOT = name_unit.ROOT


def main():
    argv = sys.argv[1:]
    dll = "hl"
    if "--dll" in argv:
        i = argv.index("--dll")
        dll = argv[i + 1]
        del argv[i:i + 2]
    if len(argv) != 4:
        sys.exit(__doc__)
    unit, va, name, evidence = argv
    va = va.upper().replace("0X", "")
    if not re.match(r"^[0-9A-F]{8}$", va) or not name.startswith(("?", "_")) or len(evidence) < 10:
        sys.exit("need an 8 digit address, a decorated name and a real evidence text")
    fd = name_unit.lock()
    try:
        path = os.path.join(ROOT, "tools", "symbols", "%s_manual.csv" % dll)
        rows = list(csv.DictReader(open(path))) if os.path.exists(path) else []
        for r in rows:
            # an evidence text with an unquoted comma reads as extra fields
            extra = r.pop(None, None)
            if extra:
                r["evidence"] = ",".join([r["evidence"]] + extra)
        rows = [r for r in rows if r["va"] != va]
        rows.append({"va": va, "name": name, "kind": "func", "evidence": evidence + " (added by an agent)"})
        # write a temporary file and replace it: a failed write must not
        # truncate the curated map
        tmp = path + ".tmp"
        with open(tmp, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["va", "name", "kind", "evidence"], lineterminator="\n")
            w.writeheader()
            w.writerows(rows)
        os.replace(tmp, path)
        tools = os.path.join(ROOT, "tools")
        name_unit.run([os.path.join(tools, "build.py"), "target", unit], quiet=True)
        args = ["--dll", dll] if dll != "hl" else []
        name_unit.run([os.path.join(tools, "sync_names.py")] + args)
        # the unit list may not have the name yet when the row had no unit
        lf = os.path.join(ROOT, "tools", "units", "%s_%s.txt" % (dll, unit.split("/", 1)[1].replace("/", "_")))
        have = [l.strip() for l in open(lf) if l.strip()] if os.path.exists(lf) else []
        if name not in have:
            with open(lf, "w") as f:
                f.write("\n".join(have + [name]) + "\n")
        name_unit.run([os.path.join(tools, "build.py"), "target", unit])
    finally:
        name_unit.unlock()


if __name__ == "__main__":
    main()
