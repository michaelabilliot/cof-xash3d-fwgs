"""List the stretches of a DLL's game code that no progress.csv row covers.

  python tools/gaps.py <dll> [--min N]

Every function of the target should have a row in docs/progress.csv. A
function Ghidra never created (no call reaches it, or analysis stopped at a
data reference) leaves a hole between two rows and is silently missing from
the counts. This walks the rows of one DLL up to the start of the static
library code (docs/units.md) and prints each hole that holds more than
alignment padding (int3 or nop fill), with its first bytes.
"""
import csv
import os
import sys

import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DLLS = {
    "hl": ("versions/1.6a/cryoffear/cl_dlls/hl.dll", 0x10121FE5),
    "client": ("versions/1.6a/cryoffear/cl_dlls/client.dll", 0x100B42A4),
}


def main():
    dll = sys.argv[1]
    minimum = int(sys.argv[sys.argv.index("--min") + 1]) if "--min" in sys.argv else 1
    path, lib_start = DLLS[dll]
    pe = pefile.PE(os.path.join(ROOT, path), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    data = text.get_data()
    start = base + text.VirtualAddress

    def byte_at(va):
        return data[va - start]

    rows = sorted((int(r["va"], 16), int(r["size"])) for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv")))
                  if r["dll"] == dll)
    text_end = start + len(data)

    def kind(e, b):
        # a switch table (code pointers) after its function, maybe behind a
        # 2 or 3 byte nop; import thunks (jmp [iat]); anything else is code
        for skip in (0, 2, 3):
            p = e + skip
            if p + 4 <= b:
                v = int.from_bytes(data[p - start:p - start + 4], "little")
                if start <= v < text_end and (skip == 0 or data[e - start] in (0x66, 0x0F)):
                    return "table"
        if data[e - start:e - start + 2] == b"\xff\x25":
            return "thunks"
        return "code"

    total = {}
    show = sys.argv[sys.argv.index("--kind") + 1] if "--kind" in sys.argv else "code"
    for (a, s), (b, _) in zip(rows, rows[1:]):
        if a >= lib_start:
            break
        e = a + s
        # skip the fill after a function: int3, nop, and the multi-byte nops
        # (66 90, 0F 1F 00, 0F 1F 40 00) MSVC puts before an aligned switch table
        while e < b:
            if byte_at(e) in (0xCC, 0x90):
                e += 1
            elif data[e - start:e - start + 2] == b"\x66\x90":
                e += 2
            elif data[e - start:e - start + 3] == b"\x0f\x1f\x00":
                e += 3
            elif data[e - start:e - start + 4] == b"\x0f\x1f\x40\x00":
                e += 4
            else:
                break
        if b - e >= minimum:
            k = kind(e, b)
            total[k] = total.get(k, 0) + b - e
            if show in (k, "all"):
                print("%08X %5d %-6s %s" % (e, b - e, k, data[e - start:e - start + 12].hex(" ")))
    print("total bytes per kind:", total)


if __name__ == "__main__":
    main()
