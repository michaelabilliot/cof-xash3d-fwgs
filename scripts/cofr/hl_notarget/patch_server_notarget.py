#!/usr/bin/env python3
"""Link persistent_notarget.c into the CoFR hl.dll server proxy.

hl.dll (proxy -> cofr13_server.dll -> cof_server_original.dll) has no source.
This script compiles persistent_notarget.c to a COFF object, links it into a
new PE section of hl.dll with a minimal linker (DIR32/REL32/DIR32NB, base
relocations for every DIR32) and points the GetEntityAPI/GetEntityAPI2 exports
at the new wrappers. Nothing in the chain pins hl.dll's own hash.

Usage:
  python patch_server_notarget.py <hl.dll in> <hl.dll out> [--cl path\\to\\cl.exe]

Requires pefile. The input must be the known stock proxy (SHA-256 checked).
"""
import argparse
import hashlib
import os
import struct
import subprocess
import sys
import tempfile

import pefile

STOCK_SHA256 = "1bfed32db61a289e089caed0ace98281142c7eff2abfac3db13a2a3d112e7eca"
DEFAULT_CL = r"D:\Dev\DeCoF-Rebase\toolchain\msvc-14.27.29112\bin\Hostx86\x86\cl.exe"
SECTION_NAME = b".cofrnt\0"

# Fixed locations inside the stock hl.dll (RVA), each verified below.
EXTERNALS = {
    "_hl_engfuncs": 0x190F0,       # enginefuncs_t copy (GiveFnptrsToDll: mov edi, 0x100190f0)
    "_hl_gpGlobals": 0x190EC,      # globalvars_t *   (GiveFnptrsToDll: mov [0x100190ec], eax)
    "_hl_GetEntityAPI": 0x1680,
    "_hl_GetEntityAPI2": 0x16D0,
}
EXPORT_HOOKS = {b"GetEntityAPI": "_CofrNt_GetEntityAPI", b"GetEntityAPI2": "_CofrNt_GetEntityAPI2"}

SCN_CNT_CODE = 0x20
SCN_CNT_UNINIT = 0x80
SCN_LNK_INFO = 0x200
SCN_LNK_REMOVE = 0x800
SCN_MEM_DISCARDABLE = 0x02000000
REL_DIR32, REL_DIR32NB, REL_REL32 = 0x06, 0x07, 0x14


def align(v, a):
    return (v + a - 1) & ~(a - 1)


def compile_object(cl, source, obj):
    env = dict(os.environ)
    subprocess.run([cl, "/nologo", "/c", "/O1", "/GS-", "/Zl", "/Oi-", "/TC", "/W4", "/WX",
                    source, "/Fo:" + obj], check=True, env=env)


def parse_coff(data):
    machine, nsec, _, symptr, nsym, optsz, _ = struct.unpack_from("<HHIIIHH", data, 0)
    if machine != 0x14C:
        raise SystemExit("object is not i386")
    strtab = symptr + nsym * 18

    def name_at(raw):
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            end = data.index(b"\0", strtab + off)
            return data[strtab + off:end].decode()
        return raw.rstrip(b"\0").decode()

    sections = []
    for i in range(nsec):
        h = data[20 + optsz + i * 40:20 + optsz + (i + 1) * 40]
        name = h[:8]
        vsize, vaddr, rawsz, rawptr, relptr, _, nrel, _, flags = struct.unpack_from("<IIIIIIHHI", h, 8)
        if name.startswith(b"/"):
            name = name_at(b"\0\0\0\0" + struct.pack("<I", int(name[1:].rstrip(b"\0"))))
        else:
            name = name.rstrip(b"\0").decode()
        raw = b"\0" * rawsz if flags & SCN_CNT_UNINIT else data[rawptr:rawptr + rawsz]
        relocs = [struct.unpack_from("<IIH", data, relptr + r * 10) for r in range(nrel)]
        sections.append({"name": name, "flags": flags, "data": bytearray(raw), "relocs": relocs,
                         "align": 1 << max(((flags >> 20) & 0xF) - 1, 0) if (flags >> 20) & 0xF else 16})

    symbols = {}
    i = 0
    while i < nsym:
        raw = data[symptr + i * 18:symptr + (i + 1) * 18]
        value, secnum, _, sclass, naux = struct.unpack_from("<IhHBB", raw, 8)
        symbols[i] = (name_at(raw[:8]), value, secnum, sclass)
        i += 1 + naux
    return sections, symbols


def link(obj_data, base_rva, image_base):
    sections, symbols = parse_coff(obj_data)
    keep = [s for s in sections
            if s["data"] or s["flags"] & SCN_CNT_UNINIT
            if not s["flags"] & (SCN_LNK_INFO | SCN_LNK_REMOVE | SCN_MEM_DISCARDABLE)]
    blob = bytearray()
    for s in keep:
        blob += b"\0" * (align(len(blob), s["align"]) - len(blob))
        s["rva"] = base_rva + len(blob)
        blob += s["data"]
    index = {id(s): s for s in keep}
    by_num = {n + 1: s for n, s in enumerate(sections)}

    def resolve(symidx):
        name, value, secnum, _ = symbols[symidx]
        if secnum > 0:
            sec = by_num[secnum]
            if id(sec) not in index:
                raise SystemExit("symbol %s lives in a dropped section %s" % (name, sec["name"]))
            return sec["rva"] + value
        if name in EXTERNALS:
            return EXTERNALS[name]
        raise SystemExit("unresolved external " + name)

    fixups = []
    for s in keep:
        for off, symidx, rtype in s["relocs"]:
            at = s["rva"] - base_rva + off
            addend = struct.unpack_from("<I", blob, at)[0]
            target = resolve(symidx)
            if rtype == REL_DIR32:
                struct.pack_into("<I", blob, at, (image_base + target + addend) & 0xFFFFFFFF)
                fixups.append(base_rva + at)
            elif rtype == REL_DIR32NB:
                struct.pack_into("<I", blob, at, (target + addend) & 0xFFFFFFFF)
            elif rtype == REL_REL32:
                struct.pack_into("<I", blob, at, (target + addend - (base_rva + at + 4)) & 0xFFFFFFFF)
            else:
                raise SystemExit("unsupported relocation type %#x" % rtype)

    exports = {}
    for name, value, secnum, sclass in symbols.values():
        if secnum > 0 and sclass == 2 and id(by_num[secnum]) in index:
            exports[name] = by_num[secnum]["rva"] + value
    return bytes(blob), fixups, exports


def reloc_blocks(rvas):
    out = bytearray()
    pages = {}
    for rva in sorted(rvas):
        pages.setdefault(rva & ~0xFFF, []).append(rva & 0xFFF)
    for page, offs in pages.items():
        entries = [(3 << 12) | o for o in offs]
        if len(entries) % 2:
            entries.append(0)
        out += struct.pack("<II", page, 8 + 2 * len(entries))
        out += struct.pack("<%dH" % len(entries), *entries)
    return bytes(out)


def verify_stock(pe, data):
    base = pe.OPTIONAL_HEADER.ImageBase

    def code_at(rva, expected):
        off = pe.get_offset_from_rva(rva)
        if data[off:off + len(expected)] != expected:
            raise SystemExit("unexpected code at RVA %#x" % rva)

    code_at(0x1749, b"\xbf" + struct.pack("<I", base + EXTERNALS["_hl_engfuncs"]))
    code_at(0x1761, b"\xa3" + struct.pack("<I", base + EXTERNALS["_hl_gpGlobals"]))
    code_at(0x16B9, b"\xc7\x43\x4c" + struct.pack("<I", base + 0x1000))   # ClientCommand slot
    names = {e.name: e for e in pe.DIRECTORY_ENTRY_EXPORT.symbols}
    if names[b"GetEntityAPI"].address != EXTERNALS["_hl_GetEntityAPI"] or \
       names[b"GetEntityAPI2"].address != EXTERNALS["_hl_GetEntityAPI2"]:
        raise SystemExit("unexpected GetEntityAPI export addresses")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--cl", default=DEFAULT_CL)
    args = ap.parse_args()

    data = bytearray(open(args.input, "rb").read())
    digest = hashlib.sha256(data).hexdigest()
    if digest != STOCK_SHA256:
        raise SystemExit("input hl.dll is not the stock proxy (sha256 %s)" % digest)

    pe = pefile.PE(data=bytes(data))
    verify_stock(pe, data)
    opt = pe.OPTIONAL_HEADER
    salign, falign, image_base = opt.SectionAlignment, opt.FileAlignment, opt.ImageBase

    last = pe.sections[-1]
    if not last.Name.startswith(b".reloc") or opt.DATA_DIRECTORY[5].VirtualAddress != last.VirtualAddress:
        raise SystemExit("expected .reloc to be the last section")
    if len(data) != last.PointerToRawData + last.SizeOfRawData:
        raise SystemExit("unexpected overlay data after the last section")

    new_rva = align(last.VirtualAddress + last.Misc_VirtualSize, salign)
    source = os.path.join(os.path.dirname(os.path.abspath(__file__)), "persistent_notarget.c")
    with tempfile.TemporaryDirectory() as tmp:
        obj = os.path.join(tmp, "persistent_notarget.obj")
        compile_object(args.cl, source, obj)
        blob, fixups, symbols = link(open(obj, "rb").read(), new_rva, image_base)

    # Grow .reloc with the new section's HIGHLOW entries.
    old_relocs = data[last.PointerToRawData:last.PointerToRawData + opt.DATA_DIRECTORY[5].Size]
    relocs = bytes(old_relocs) + reloc_blocks(fixups)
    if last.VirtualAddress + len(relocs) > new_rva:
        raise SystemExit(".reloc would overlap the new section")
    reloc_raw = align(len(relocs), falign)
    new_raw_ptr = last.PointerToRawData + reloc_raw
    new_raw_size = align(len(blob), falign)

    out = bytearray(data[:last.PointerToRawData])
    out += relocs + b"\0" * (reloc_raw - len(relocs))
    out += blob + b"\0" * (new_raw_size - len(blob))

    # Section header for the new code.
    nt = pe.DOS_HEADER.e_lfanew
    sec_table = nt + 24 + pe.FILE_HEADER.SizeOfOptionalHeader
    nsec = pe.FILE_HEADER.NumberOfSections
    hdr_at = sec_table + nsec * 40
    if hdr_at + 40 > pe.sections[0].PointerToRawData or any(out[hdr_at:hdr_at + 40]):
        raise SystemExit("no room for another section header")
    flags = 0x20 | 0x20000000 | 0x40000000 | 0x80000000   # code, execute, read, write
    out[hdr_at:hdr_at + 40] = SECTION_NAME + struct.pack("<IIIIIIHHI", len(blob), new_rva, new_raw_size,
                                                        new_raw_ptr, 0, 0, 0, 0, flags)
    struct.pack_into("<H", out, nt + 6, nsec + 1)

    # .reloc header: VirtualSize / SizeOfRawData; optional header fields.
    reloc_hdr = sec_table + (nsec - 1) * 40
    struct.pack_into("<I", out, reloc_hdr + 8, len(relocs))
    struct.pack_into("<I", out, reloc_hdr + 16, reloc_raw)
    opt_at = nt + 24
    struct.pack_into("<I", out, opt_at + 4, opt.SizeOfCode + new_raw_size)
    struct.pack_into("<I", out, opt_at + 56, align(new_rva + len(blob), salign))     # SizeOfImage
    struct.pack_into("<I", out, opt_at + 96 + 5 * 8 + 4, len(relocs))                # BASERELOC size

    # Redirect the exports.
    exp = pe.DIRECTORY_ENTRY_EXPORT
    funcs_off = pe.get_offset_from_rva(exp.struct.AddressOfFunctions)
    for sym in exp.symbols:
        if sym.name in EXPORT_HOOKS:
            struct.pack_into("<I", out, funcs_off + 4 * (sym.ordinal - exp.struct.Base),
                             symbols[EXPORT_HOOKS[sym.name]])

    patched = pefile.PE(data=bytes(out))
    patched.OPTIONAL_HEADER.CheckSum = patched.generate_checksum()
    final = patched.write()
    open(args.output, "wb").write(final)
    print("wrote %s (%d bytes, %d base relocations, section RVA %#x)" % (args.output, len(final), len(fixups), new_rva))
    print("sha256 %s" % hashlib.sha256(final).hexdigest())


if __name__ == "__main__":
    sys.exit(main())
