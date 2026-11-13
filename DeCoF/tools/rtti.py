"""List the RTTI classes of a DLL with their vtables.

  python tools/rtti.py [hl|client|gameui|hw|launcher] [<class name part>]

MSVC /GR puts a type descriptor (".?AVCHUDControl@@") and a complete
object locator before every vtable. This prints, per vtable, its address,
the class name, the offset of that vtable inside the object and the slot
count; with a name part, also every slot whose target is not a library
function (the class's own methods, by address). A client.dll class with no
PDB gets its name from here.
"""
import os
import re
import struct
import sys

import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll", "client": "versions/1.6a/cryoffear/cl_dlls/client.dll",
          "gameui": "versions/1.6a/cryoffear/cl_dlls/GameUI.dll", "hw": "versions/1.6a/hw.dll", "launcher": "versions/1.6a/cof.exe"}


def vtables(dll):
    pe = pefile.PE(os.path.join(ROOT, IMAGES[dll]), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    data = bytes(pe.get_memory_mapped_image())
    tds = {}
    for m in re.finditer(rb"\.\?A[VU][^\0]{1,200}\0", data):
        tds[m.start() - 8 + base] = m.group(0)[:-1].decode("latin-1")
    cols = {}
    for off in range(0, len(data) - 20, 4):
        sig, o, _cd, ptd, pch = struct.unpack_from("<IIIII", data, off)
        if sig == 0 and ptd in tds and base <= pch < base + len(data):
            cols[off + base] = (tds[ptd], o)
    text = [s for s in pe.sections if s.Name.startswith(b".text")][0]
    tlo = base + text.VirtualAddress
    thi = tlo + text.Misc_VirtualSize
    out = []
    for off in range(0, len(data) - 8, 4):
        p = struct.unpack_from("<I", data, off)[0]
        if p not in cols:
            continue
        slots = []
        while True:
            q = struct.unpack_from("<I", data, off + 4 + 4 * len(slots))[0]
            if not tlo <= q < thi:
                break
            slots.append(q)
        if slots:
            name, o = cols[p]
            out.append((off + 4 + base, name, o, slots))
    return sorted(out)


def main():
    args = sys.argv[1:]
    dll = args[0] if args and args[0] in IMAGES else "client"
    part = args[1] if len(args) > 1 else (args[0] if args and args[0] not in IMAGES else None)
    for va, name, o, slots in vtables(dll):
        if part and part not in name:
            continue
        print("%08X %-50s offset %d, %d slots" % (va, name, o, len(slots)))
        if part:
            for i, s in enumerate(slots):
                print("    slot %3d (+0x%x)  %08X" % (i, 4 * i, s))


if __name__ == "__main__":
    main()
