"""Ghidra pseudo C and string literals of an address range of a DLL.

  python tools/range.py <dll> <start hex> <end hex> [<out file>]

Runs tools/ghidra_scripts/RangeDecomp.java through the running GhidraMCP
server on the program of <dll> (tools/units.json) and writes every function
whose entry lies in [start, end) to <out file> (default
build/decomp/<dll>_<start>.c), followed by the exact text of every string
literal the range references. Reference material for writing a source file
from the binary, as the functions of one object are contiguous.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build  # noqa: E402

ROOT = build.ROOT


def main():
    dll, start, end = sys.argv[1], sys.argv[2], sys.argv[3]
    out = sys.argv[4] if len(sys.argv) > 4 else os.path.join(ROOT, "build", "decomp", "%s_%s.c" % (dll, start.upper()))
    out = os.path.abspath(out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    d = json.load(open(os.path.join(ROOT, "tools", "units.json")))["dlls"][dll]
    res = build.mcp_call("/run_ghidra_script", body={
        "script_name": os.path.join(ROOT, "tools", "ghidra_scripts", "RangeDecomp.java"),
        "args": "%s %s %s" % (start, end, out.replace("\\", "/")),
        "program": d["ghidra_folder"] + "/" + d["ghidra_program"], "timeout_seconds": 1800}, timeout=1900)
    con = res.get("console_output", "") if isinstance(res, dict) else str(res)
    line = [l for l in con.splitlines() if "RangeDecomp:" in l]
    print((line[-1].strip() if line else con[-400:]) + " -> " + out)


if __name__ == "__main__":
    main()
