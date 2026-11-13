"""Name the gmsg* user message variables of hl.dll 1.6a.

  python tools/name_usermsgs.py [--write]

LinkUserMessages stores every REG_USER_MSG( "Name", size ) result in its
gmsg variable: push size / push "Name" / call [g_engfuncs.pfnRegUserMsg] /
mov [var], eax. The 1.55 build does the same and its variables carry PDB
names, so the message name links the 1.55 variable name to the 1.6a
address. Writes tools/symbols/hl_usermsgs.csv (merged by build.py as a
curated map) with --write.
"""
import csv
import os
import struct
import sys

import capstone
import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import pdbfile  # noqa: E402


def scan(path, func_va, size):
    pe = pefile.PE(path, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    # the optimizer pushes the next message name before storing the
    # previous result, so the name belongs to the call, not to the store
    pending = None
    returned = None
    out = {}
    for ins in md.disasm(img[func_va - base:func_va - base + size], func_va):
        if ins.mnemonic == "push" and ins.operands[0].type == capstone.x86.X86_OP_IMM:
            a = ins.operands[0].imm & 0xFFFFFFFF
            o = a - base
            if 0 <= o < len(img):
                e = img.find(b"\0", o, o + 64)
                s = img[o:e]
                if e > o and all(32 <= c < 127 for c in s):
                    pending = s.decode()
        if ins.mnemonic == "call":
            returned, pending = pending, None
        if ins.mnemonic == "mov" and len(ins.operands) == 2 and ins.operands[0].type == capstone.x86.X86_OP_MEM \
                and ins.operands[0].mem.base == 0 and ins.operands[1].type == capstone.x86.X86_OP_REG \
                and ins.reg_name(ins.operands[1].reg) == "eax" and returned:
            out[returned] = ins.operands[0].mem.disp & 0xFFFFFFFF
            returned = None
    return out


def main():
    pdb = pdbfile.PDB(os.path.join(ROOT, "versions/1.55/cl_dlls/hl.pdb"))
    names55 = {}
    for rva, _f, name in pdb.publics():
        if rva is not None:
            names55[0x10000000 + rva] = name
    lum55 = next(a for a, n in names55.items() if n == "?LinkUserMessages@@YAXXZ")
    s55 = scan(os.path.join(ROOT, "versions/1.55/cl_dlls/hl.dll"), lum55, 0x2000)
    lum6 = None
    for r in csv.DictReader(open(os.path.join(ROOT, "tools", "symbols", "hl_1.6a.csv"))):
        if r["name"] == "?LinkUserMessages@@YAXXZ":
            lum6 = int(r["va"], 16)
    s6 = scan(os.path.join(ROOT, "versions/1.6a/cryoffear/cl_dlls/hl.dll"), lum6, 0x2000)
    rows = []
    for msg, va in sorted(s6.items(), key=lambda x: x[1]):
        var = names55.get(s55.get(msg))
        print("%-20s %08X %s" % (msg, va, var or "(new in 1.6a)"))
        if var:
            rows.append(("%08X" % va, var, "data", "usermsg:%s" % msg))
    print("%d messages, %d named" % (len(s6), len(rows)))
    if "--write" in sys.argv:
        with open(os.path.join(ROOT, "tools", "symbols", "hl_usermsgs.csv"), "w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["va", "name", "kind", "evidence"])
            w.writerows(rows)


if __name__ == "__main__":
    main()
