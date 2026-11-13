"""Check and propose virtual function names of hl.dll 1.6a from vtables.

  python tools/vtable_names.py [--write]

For every class with RTTI in both the 1.55 and the 1.6a hl.dll whose
vtables have the same length, slot i of the 1.6a vtable is the function
the 1.55 PDB names at slot i (the 1.55 vtable points at PDB publics).
Where a 1.6a slot points at a 5-byte `jmp` (a tail call that /Ob1
leaves out of line), the jump target is named after the only function the
1.55 debug body of that slot calls. Addresses that receive different names
(identical code folded by the linker) are skipped.

Prints the names that differ from the current symbol map (tools/symbols/
hl_1.6a.csv + hl_manual.csv) and the names that are new; --write stores
the proposals with their evidence in tools/symbols/hl_vtable.csv, which
tools/build.py merges as a curated map. Addresses named in hl_manual.csv
are never proposed again. The vtables themselves get their decorated
names (??_7Class@@6B@) from the 1.6a RTTI.
"""
import csv
import os
import re
import struct
import sys

import capstone
import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class Image(object):
    def __init__(self, path):
        self.pe = pefile.PE(path, fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.img = self.pe.get_memory_mapped_image()
        text = [s for s in self.pe.sections if s.Name.startswith(b".text")][0]
        self.tlo = self.base + text.VirtualAddress
        self.thi = self.tlo + text.Misc_VirtualSize

    def u32(self, va):
        return struct.unpack_from("<I", self.img, va - self.base)[0]

    def valid(self, va):
        return self.base <= va < self.base + len(self.img) - 4

    def vtables(self):
        """{class name: vtable address} from complete object locators."""
        img, base = self.img, self.base
        out = {}
        for m in re.finditer(rb"\.\?AV(\w+)@@\x00", img):
            td = base + m.start() - 8
            name = m.group(1).decode()
            col = None
            needle = struct.pack("<I", td)
            p = img.find(needle)
            while p >= 0:
                if p >= 12 and p % 4 == 0:
                    sig, off = struct.unpack_from("<II", img, p - 12)
                    if sig == 0 and off == 0:
                        col = base + p - 12
                        break
                p = img.find(needle, p + 1)
            if col is None:
                continue
            ref = img.find(struct.pack("<I", col))
            while ref >= 0 and ref % 4:
                ref = img.find(struct.pack("<I", col), ref + 1)
            if ref >= 0:
                out[name] = base + ref + 4
        return out

    def slots(self, vt):
        out = []
        va = vt
        while self.valid(va):
            f = self.u32(va)
            if not self.tlo <= f < self.thi:
                break
            out.append(f)
            va += 4
        return out


def main():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    old = Image(os.path.join(ROOT, "versions/1.55/cl_dlls/hl.dll"))
    new = Image(os.path.join(ROOT, "versions/1.6a/cryoffear/cl_dlls/hl.dll"))
    names55 = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "docs", "symbols_1.55.csv"))):
        names55[int(r["va"], 16)] = r["decorated"]
    current = {}
    for f in ("hl_1.6a.csv", "hl_manual.csv"):
        for r in csv.DictReader(open(os.path.join(ROOT, "tools", "symbols", f))):
            current[int(r["va"], 16)] = r["name"]
    # 1.55 is a debug build without RTTI: its vtables come from the PDB
    # vftable publics (??_7Class@@6B@)
    vt55 = {}
    for va, n in names55.items():
        m = re.match(r"^\?\?_7(\w+)@@6B@$", n)
        if m:
            vt55[m.group(1)] = va
    vt6 = new.vtables()
    proposals = {}
    conflicts = set()

    def propose(va, name, evidence):
        prev = proposals.get(va)
        if prev and prev[0] != name:
            conflicts.add(va)
            return
        proposals.setdefault(va, (name, evidence))

    def debug_calls(va):
        """Direct call targets in the 1.55 debug body starting at va."""
        out = []
        for ins in md.disasm(old.img[va - old.base:va - old.base + 0x200], va):
            if ins.mnemonic == "call" and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                out.append(ins.operands[0].imm)
            if ins.mnemonic == "ret":
                break
        return out

    compared = 0
    for cls, v6 in sorted(vt6.items()):
        v55 = vt55.get(cls)
        if not v55:
            continue
        s6, s55 = new.slots(v6), old.slots(v55)
        if len(s6) != len(s55):
            continue
        compared += 1
        for i, (a6, a55) in enumerate(zip(s6, s55)):
            n55 = names55.get(a55)
            if not n55:
                continue
            ev = "vtable %s slot %d (+0x%X): 1.55 slot is %s" % (cls, i, 4 * i, n55)
            ins = list(md.disasm(new.img[a6 - new.base:a6 - new.base + 5], a6))
            if len(ins) == 1 and ins[0].mnemonic == "jmp" and ins[0].size == 5:
                propose(a6, n55, ev + "; 1.6a slot is a 5-byte jmp")
                tgt = ins[0].operands[0].imm
                calls = [c for c in debug_calls(a55) if c in names55]
                # the 1.55 body may call small inline helpers (debug build):
                # only a single named callee is taken
                if len(set(calls)) == 1:
                    propose(tgt, names55[calls[0]], ev + "; target of its jmp; the 1.55 body calls only %s" % names55[calls[0]])
            else:
                propose(a6, n55, ev)
    for va in conflicts:
        proposals.pop(va, None)
    # curated names (hl_manual.csv, with their own evidence) are never overridden
    manual = {int(r["va"], 16) for r in csv.DictReader(open(os.path.join(ROOT, "tools", "symbols", "hl_manual.csv")))}
    differ = {va: p for va, p in proposals.items() if va in current and current[va] != p[0] and va not in manual}
    newnames = {va: p for va, p in proposals.items() if va not in current}
    # the vtables themselves: the 1.6a RTTI complete object locator (offset
    # 0) of class X sits right before the primary vtable of X
    vftables = {va: ("??_7%s@@6B@" % cls, "RTTI: the complete object locator of .?AV%s@@ (offset 0) "
                     "precedes this vtable" % cls) for cls, va in vt6.items() if va not in current}
    print("classes compared: %d, proposals: %d (skipped %d folded addresses)" % (compared, len(proposals), len(conflicts)))
    print("names that differ from the current map: %d" % len(differ))
    for va, (n, ev) in sorted(differ.items())[:60]:
        print("  %08X  %s  (now %s)" % (va, n, current[va]))
    print("new names: %d" % len(newnames))
    for va, (n, ev) in sorted(newnames.items())[:20]:
        print("  %08X  %s" % (va, n))
    print("vtable names from RTTI: %d" % len(vftables))
    if "--write" in sys.argv:
        path = os.path.join(ROOT, "tools", "symbols", "hl_vtable.csv")
        rows = {va: (n, "func", ev) for va, (n, ev) in {**differ, **newnames}.items()}
        rows.update({va: (n, "data", ev) for va, (n, ev) in vftables.items()})
        # keep the evidence already recorded (possibly amended by hand) for
        # entries whose name did not change
        if os.path.exists(path):
            for r in csv.DictReader(open(path)):
                va = int(r["va"], 16)
                if va in rows and rows[va][0] == r["name"]:
                    rows[va] = (r["name"], r["kind"], r["evidence"])
        with open(path, "w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["va", "name", "kind", "evidence"])
            for va, (n, kind, ev) in sorted(rows.items()):
                w.writerow(["%08X" % va, n, kind, ev])
        print("wrote %s" % os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
