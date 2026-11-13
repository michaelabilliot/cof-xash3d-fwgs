"""Rename guess_ prefixed identifiers to the _guess suffix (AGENTS.md, 2026-10-15).

  python tools/rename_guess.py [--write]

The linker orders the functions of an object alphabetically by decorated
name, so a guess_ prefix moved every guessed function out of its retail
position. This turns guess_X into X_guess everywhere a name is spelled:
- src/**/*.c, *.cpp, *.h (identifiers, comments);
- tools/symbols/*.csv, tools/units/*.txt, docs/progress.csv (decorated
  names and the demangled column), tools/*.json;
- the Ghidra labels of the programs named in tools/units.json, through the
  running GhidraMCP server (tools/ghidra_scripts/RenameGuess.java), so the
  delinked objects carry the new names.
File names and the guess: prefix of progress.csv source_file (an inferred
file attribution, not a name) stay as they are. Without --write it only
counts what would change. Run a full delink and check afterwards.
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import name_unit  # noqa: E402

PAT = re.compile(r"(?<![a-z0-9_])(_?)guess_([A-Za-z0-9_]+)")
# names built on top of a guessed name: __CmdFunc_guess_X, __MsgFunc_guess_X,
# and constructors and destructors ??0guess_CFoo / ??1guess_CFoo
PAT2 = re.compile(r"(__CmdFunc_|__MsgFunc_|\?\?[0-9])guess_([A-Za-z0-9_]+)")


def convert(text):
    # guess_X -> X_guess; a C name _guess_X -> _X_guess; inside decorated
    # names a type code letter may come first (...@@3Vguess_CFoo@@A); keep
    # the guess: attribution prefix (no underscore) untouched
    text = PAT2.sub(lambda m: "%s%s_guess" % (m.group(1), m.group(2)), text)
    return PAT.sub(lambda m: "%s%s_guess" % (m.group(1), m.group(2)), text)


def files():
    out = []
    for ext in ("c", "cpp", "h", "C", "CPP", "H"):
        out += glob.glob(os.path.join(ROOT, "src", "**", "*." + ext), recursive=True)
    out += glob.glob(os.path.join(ROOT, "tools", "symbols", "*.csv"))
    out += glob.glob(os.path.join(ROOT, "tools", "units", "*.txt"))
    out += [os.path.join(ROOT, "docs", "progress.csv"), os.path.join(ROOT, "tools", "units.json")]
    return sorted(set(out))


def main():
    write = "--write" in sys.argv
    changed = 0
    names = set()
    if write:
        name_unit.lock()
    try:
        for f in files():
            raw = open(f, "rb").read()
            text = raw.decode("latin-1")
            new = convert(text)
            if new != text:
                changed += 1
                names.update(m.group(0) for m in PAT.finditer(text))
                names.update(m.group(0) for m in PAT2.finditer(text))
                if write:
                    open(f, "wb").write(new.encode("latin-1"))
    finally:
        if write:
            name_unit.unlock()
    print("%d files %s, %d distinct guess_ names" % (changed, "changed" if write else "to change", len(names)))
    if write:
        import build
        cfg = json.load(open(os.path.join(ROOT, "tools", "units.json")))
        script = os.path.join(ROOT, "tools", "ghidra_scripts", "RenameGuess.java")
        for dll, d in cfg["dlls"].items():
            res = build.mcp_call("/run_ghidra_script", body={
                "script_name": script, "args": "",
                "program": d["ghidra_folder"] + "/" + d["ghidra_program"], "timeout_seconds": 1200}, timeout=1300)
            out = res.get("console_output", "") if isinstance(res, dict) else str(res)
            line = [l for l in out.splitlines() if "RenameGuess:" in l]
            print(dll, line[-1] if line else out[-300:])


if __name__ == "__main__":
    main()
