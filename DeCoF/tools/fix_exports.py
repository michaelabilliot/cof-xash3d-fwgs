"""Add EXPORT to the member declarations that the retail DLL exports.

  python tools/fix_exports.py <dll> <rebuilt.dll> [--write]

HLSDK think, touch and use functions are declared `void EXPORT Name( ... );`
(EXPORT is __declspec(dllexport)), which only puts them in the export
table: the code is the same. For every decorated export of the retail DLL
that the rebuilt DLL lacks, this finds the class declaration in src/ and
inserts EXPORT before the method name, in every copy of a class that
several files declare. Prints what it cannot place.
"""
import glob
import os
import re
import subprocess
import sys

import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RETAIL = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll", "client": "versions/1.6a/cryoffear/cl_dlls/client.dll"}
SRC = {"hl": ["src/dlls", "src/game_shared"], "client": ["src/cl_dll", "src/game_shared"]}
UNDNAME = r"I:\Tools\msvc-14.27.29112\bin\Hostx86\x86\undname.exe"


def exports(path):
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    return {s.name.decode() for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name}


def class_bodies(text, cls):
    for m in re.finditer(r"\b(?:class|struct)\s+%s\b[^;{]*\{" % re.escape(cls), text):
        depth, i = 1, m.end()
        while depth and i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        yield m.end(), i


def main():
    dll, rebuilt = sys.argv[1], sys.argv[2]
    write = "--write" in sys.argv
    missing = sorted(exports(os.path.join(ROOT, RETAIL[dll])) - exports(rebuilt))
    files = []
    for d in SRC[dll]:
        files += glob.glob(os.path.join(ROOT, d, "*.h")) + glob.glob(os.path.join(ROOT, d, "*.cpp"))
    texts = {f: open(f, newline="", encoding="latin-1").read() for f in files}
    changed, fails = set(), []
    for name in missing:
        m = re.match(r"^\?(\w+)@(\w+)@@", name)
        if not m:
            fails.append(name)
            continue
        meth, cls = m.group(1), m.group(2)
        done = False
        for f, t in texts.items():
            for a, b in class_bodies(t, cls):
                body = t[a:b]
                d = re.search(r"(\b(?:void|int|BOOL|bool|float)\s+)(EXPORT\s+)?(%s\s*\()" % re.escape(meth), body)
                if not d:
                    continue
                if not d.group(2):
                    i = a + d.start(3)
                    t = t[:i] + "EXPORT " + t[i:]
                    texts[f] = t
                    changed.add(f)
                # every copy of a class declared in several files
                done = True
        if not done:
            fails.append(name)
    print("%d missing exports, %d files changed, %d not placed" % (len(missing), len(changed), len(fails)))
    for n in fails:
        print("  not placed:", n)
    if write:
        for f in changed:
            open(f, "w", newline="", encoding="latin-1").write(texts[f])


if __name__ == "__main__":
    main()
