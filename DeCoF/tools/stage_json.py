"""Stage tools/units.json with only some of its working-copy changes.

  python tools/stage_json.py <dll>[.<key>] [...]

units.json often carries the work of several agents at once. This stages
HEAD's version in which only the named parts come from the working copy:
a whole dll entry (hw) or one key of it (client.link, launcher.units).
The file keeps its formatting (json.dumps with indent 2, the line endings
of HEAD), so every part not named stays byte-identical to HEAD.
"""
import collections
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = "tools/units.json"


def main():
    head = subprocess.run(["git", "show", "HEAD:" + PATH], cwd=ROOT, capture_output=True).stdout.decode("utf-8")
    nl = "\r\n" if "\r\n" in head else "\n"
    head = head.replace("\r\n", "\n")
    work = open(os.path.join(ROOT, PATH), encoding="utf-8").read()
    h = json.loads(head, object_pairs_hook=collections.OrderedDict)
    w = json.loads(work, object_pairs_hook=collections.OrderedDict)
    if json.dumps(h, indent=2) + "\n" != head:
        sys.exit("HEAD's units.json is not in json.dumps(indent=2) form, stage it by hand")
    for part in sys.argv[1:]:
        dll, _, key = part.partition(".")
        if not key:
            h["dlls"][dll] = w["dlls"][dll]
        elif key in w["dlls"][dll]:
            h["dlls"][dll][key] = w["dlls"][dll][key]
        else:
            h["dlls"][dll].pop(key, None)
    text = json.dumps(h, indent=2) + "\n"
    blob = subprocess.run(["git", "hash-object", "-w", "--stdin", "--no-filters"], cwd=ROOT,
                          input=text.replace("\n", nl).encode("utf-8"), capture_output=True).stdout.decode().strip()
    subprocess.check_call(["git", "update-index", "--cacheinfo", "100644,%s,%s" % (blob, PATH)], cwd=ROOT)
    print("staged %s with %s from the working copy" % (PATH, ", ".join(sys.argv[1:])))


if __name__ == "__main__":
    main()
