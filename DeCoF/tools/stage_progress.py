"""Stage only some rows of docs/progress.csv.

  python tools/stage_progress.py <source_file> [<source_file> ...]
  python tools/stage_progress.py --va <dll>:<va> [...]
  python tools/stage_progress.py --except <source_file> [...]
  python tools/stage_progress.py --dll <dll>

The working copy of docs/progress.csv often carries results of several
pieces of work at once (a source file, a name map change). This stages a
copy of the committed CSV in which only the rows of the given source files
(the source_file column, for example dlls/cof_g43.cpp) or the given
addresses come from the working copy, so each commit carries the rows of
the code it describes. The working copy is left as it is. The pre-commit
hook then regenerates the progress report from the staged CSV.
"""
import csv
import io
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = "docs/progress.csv"


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    files, vas, except_files, dlls = set(), set(), None, set()
    if args[0] == "--except":
        except_files = set(args[1:])
    elif args[0] == "--dll":
        dlls = set(args[1:])
    elif args[0] == "--va":
        for a in args[1:]:
            dll, va = a.split(":")
            vas.add((dll, va.upper()))
    else:
        files = set(args)
    head = subprocess.run(["git", "show", "HEAD:" + PATH], cwd=ROOT, stdout=subprocess.PIPE, check=True).stdout
    old = list(csv.DictReader(io.StringIO(head.decode("utf-8"))))
    fields = list(old[0].keys())
    work = {(r["dll"], r["va"]): r for r in csv.DictReader(open(os.path.join(ROOT, PATH), newline=""))}
    taken = 0
    out = []
    for r in old:
        w = work.get((r["dll"], r["va"]))
        take = w and (w["source_file"] in files or (r["dll"], r["va"]) in vas or r["dll"] in dlls)
        if w and except_files is not None:
            take = w["source_file"] not in except_files and r["source_file"] not in except_files
        if take:
            if w != r:
                taken += 1
            r = w
        out.append(r)
    # rows of the working copy that HEAD lacks: a selected one goes in after
    # the row that precedes it in the working copy
    have = {(r["dll"], r["va"]) for r in old}
    prev = None
    added = []
    for key, w in work.items():
        if key not in have:
            take = w["source_file"] in files or key in vas or w["dll"] in dlls
            if except_files is not None:
                take = w["source_file"] not in except_files
            if take:
                added.append((prev, w))
                have.add(key)
        prev = key
    for after, w in added:
        i = next((n for n, r in enumerate(out) if (r["dll"], r["va"]) == after), len(out) - 1)
        out.insert(i + 1, w)
        taken += 1
    buf = io.StringIO()
    wr = csv.DictWriter(buf, fieldnames=fields, lineterminator="\n")
    wr.writeheader()
    wr.writerows(out)
    blob = subprocess.run(["git", "hash-object", "-w", "--stdin"], cwd=ROOT, input=buf.getvalue().encode("utf-8"),
                          stdout=subprocess.PIPE, check=True).stdout.decode().strip()
    subprocess.run(["git", "update-index", "--add", "--cacheinfo", "100644,%s,%s" % (blob, PATH)], cwd=ROOT, check=True)
    print("staged %s with %d changed rows" % (PATH, taken))


if __name__ == "__main__":
    main()
