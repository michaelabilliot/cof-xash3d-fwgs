"""Name the LINK_ENTITY_TO_CLASS functions of hl.dll 1.6a correctly.

  python tools/fix_link_entities.py [--write]

In 1.6a every entity export (weapon_glock, ...) is a 5-byte tail call,
`jmp GetClassPtr<CClass>`, because GetClassPtr is not inlined and the
export returns void. The 1.55 name carry put the export names on the
GetClassPtr bodies. This script:

- names each export thunk `_<export>` (kind func; extern "C" cdecl),
- names each jump target `??$GetClassPtr@V<C>@@@@YAPAV<C>@@PAV0@@Z`, the
  class read from the vtable the body stores (RTTI type descriptor),
- fixes tools/symbols/hl_1.6a.csv and docs/progress.csv (renames the
  wrong rows, adds rows for the thunks with the source file of the class).
"""
import csv
import os
import re
import struct
import sys

import capstone
import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    write = "--write" in sys.argv
    pe = pefile.PE(os.path.join(ROOT, "versions/1.6a/cryoffear/cl_dlls/hl.dll"))
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    def vtable_class(vt):
        if not base + 4 <= vt < base + len(img):
            return None
        col = struct.unpack_from("<I", img, vt - 4 - base)[0]
        if not base <= col < base + len(img):
            return None
        td = struct.unpack_from("<I", img, col + 12 - base)[0]
        if not base <= td < base + len(img):
            return None
        name = img[td + 8 - base:img.index(b"\0", td + 8 - base)].decode("latin-1")
        m = re.match(r"^\.\?A[VU](\w+)@@$", name)
        return m.group(1) if m else None

    def getclassptr_class(fn):
        for ins in md.disasm(img[fn - base:fn - base + 0x80], fn):
            if ins.mnemonic == "mov" and len(ins.operands) == 2 and ins.operands[0].type == capstone.x86.X86_OP_MEM \
                    and ins.operands[1].type == capstone.x86.X86_OP_IMM:
                c = vtable_class(ins.operands[1].imm & 0xFFFFFFFF)
                if c:
                    return c
            if ins.mnemonic == "ret":
                break
        return None

    exports = {}
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name:
            exports[base + e.address] = e.name.decode()
    thunks = {}
    targets = {}
    for va, name in exports.items():
        ins = list(md.disasm(img[va - base:va - base + 5], va))
        if ins and ins[0].mnemonic == "jmp" and ins[0].operands[0].type == capstone.x86.X86_OP_IMM:
            tgt = ins[0].operands[0].imm
            cls = getclassptr_class(tgt)
            if cls:
                thunks[va] = (name, cls)
                targets[tgt] = cls
    print("%d entity export thunks, %d GetClassPtr bodies" % (len(thunks), len(targets)))

    def gcp(cls):
        return "??$GetClassPtr@V%s@@@@YAPAV%s@@PAV0@@Z" % (cls, cls)

    # symbol map
    sp = os.path.join(ROOT, "tools", "symbols", "hl_1.6a.csv")
    rows = list(csv.DictReader(open(sp)))
    fields = list(rows[0].keys())
    by_va = {int(r["va"], 16): r for r in rows}
    for va, (name, cls) in thunks.items():
        r = by_va.get(va)
        if r is None:
            r = {"va": "%08X" % va, "name": "", "kind": "", "evidence": ""}
            rows.append(r)
            by_va[va] = r
        r["name"], r["kind"], r["evidence"] = "_" + name, "func", "exports+jmp_thunk"
    for va, cls in targets.items():
        r = by_va.get(va)
        if r is None:
            r = {"va": "%08X" % va, "name": "", "kind": "", "evidence": ""}
            rows.append(r)
            by_va[va] = r
        r["name"], r["kind"], r["evidence"] = gcp(cls), "func", "jmp_target_of_export+rtti"
    rows.sort(key=lambda r: int(r["va"], 16))

    # progress
    pp = os.path.join(ROOT, "docs", "progress.csv")
    prow = list(csv.DictReader(open(pp)))
    pfields = list(prow[0].keys())
    pby = {(r["dll"], int(r["va"], 16)): r for r in prow if r["dll"] == "hl"}
    cls_file = {}
    for r in prow:
        if r["dll"] == "hl" and r["source_file"] and not r["source_file"].startswith("guess:"):
            m = re.match(r"^\?\w+@(\w+)@@", r["name"])
            if m:
                cls_file.setdefault(m.group(1), r["source_file"])
    added = renamed = 0
    for va, cls in targets.items():
        r = pby.get(("hl", va))
        if r is not None and r["name"] != gcp(cls):
            r["name"], r["demangled"] = gcp(cls), "GetClassPtr<%s>" % cls
            r["evidence"] = "jmp_target_of_export+rtti"
            r["class"] = "HLSDK"
            if cls in cls_file:
                r["source_file"] = cls_file[cls]
            renamed += 1
    for va, (name, cls) in thunks.items():
        r = pby.get(("hl", va))
        if r is None:
            r = {k: "" for k in pfields}
            r.update({"dll": "hl", "va": "%08X" % va, "size": "5", "name": "_" + name,
                      "demangled": name, "class": "HLSDK", "source_file": cls_file.get(cls, ""),
                      "status": "TODO", "evidence": "exports+jmp_thunk",
                      "notes": "LINK_ENTITY_TO_CLASS( %s, %s )" % (name, cls)})
            prow.append(r)
            added += 1
        else:
            r["name"], r["evidence"] = "_" + name, "exports+jmp_thunk"
            r["source_file"] = r["source_file"] or cls_file.get(cls, "")
    prow.sort(key=lambda r: (r["dll"], int(r["va"], 16)))
    print("progress: %d GetClassPtr rows renamed, %d thunk rows added" % (renamed, added))
    if write:
        with open(sp, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
            w.writeheader()
            w.writerows(rows)
        with open(pp, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=pfields, lineterminator="\n")
            w.writeheader()
            w.writerows(prow)
        print("written")


if __name__ == "__main__":
    main()
