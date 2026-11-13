"""Scan a game DLL for calls through the engine interface tables.

Usage: python tools/scan_calls.py <hl|client> <dll path> <out dir> [--tag 1.6a]

Linear-sweep disassembly (capstone) of .text, then:
- every call through the engine table copy (g_engfuncs in hl.dll, gEngfuncs
  in client.dll), direct (call [table+x]) or through a register loaded from
  the table, is attributed to the table member;
- for the client, calls through the sub-API pointers stored in gEngfuncs
  (pTriAPI, pEfxAPI, pEventAPI, ...) are attributed to the sub-API member;
- constant arguments of selected members are recovered from the pushes that
  precede the call (strings, integers, code pointers).

Table addresses and sizes come from the export that fills each table
(GiveFnptrsToDll in hl.dll, Initialize in client.dll); member names come
from the headers in src/ (tools/hdrstruct.py). Results are evidence for the
docs, to be confirmed in Ghidra where it matters.
"""
import argparse
import csv
import os
import re
import struct
import sys
from collections import Counter, defaultdict

import capstone
import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hdrstruct import members  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")


class Image(object):
    def __init__(self, path):
        self.pe = pefile.PE(path)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.data = open(path, "rb").read()
        text = next(s for s in self.pe.sections if s.Name.startswith(b".text"))
        self.text_va = self.base + text.VirtualAddress
        self.text = text.get_data()[:text.Misc_VirtualSize]
        self.sections = [(s.Name.rstrip(b"\0").decode(), self.base + s.VirtualAddress,
                          s.Misc_VirtualSize) for s in self.pe.sections]

    def section_of(self, va):
        for name, start, size in self.sections:
            if start <= va < start + size:
                return name
        return None

    def read(self, va, n):
        try:
            return self.pe.get_data(va - self.base, n)
        except Exception:
            return None

    def u32(self, va):
        b = self.read(va, 4)
        return struct.unpack("<I", b)[0] if b and len(b) == 4 else None

    def cstr(self, va, limit=512):
        if self.section_of(va) not in (".rdata", ".data"):
            return None
        b = self.read(va, limit)
        if not b:
            return None
        s = b.split(b"\0")[0]
        if not s or any(c < 0x09 or (0x0D < c < 0x20) or c > 0x7E for c in s):
            return None
        return s.decode("latin-1")

    def exports(self):
        return {e.name.decode(): self.base + e.address
                for e in self.pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}


def sweep(img):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    insns = []
    off = 0
    n = len(img.text)
    while off < n:
        got = False
        for ins in md.disasm_lite(img.text[off:], img.text_va + off):
            insns.append(ins)
            off = ins[0] + ins[1] - img.text_va
            got = True
        if not got or off < n:
            off += 1 if not got else 0
            if not got:
                continue
    return insns


MEM_ABS = re.compile(r"dword ptr \[0x([0-9a-f]+)\]$")
MEM_REG_OFF = re.compile(r"dword ptr \[(e[a-z]{2})(?: \+ (0x[0-9a-f]+|\d+))?\]$")
MOV_REG_ABS = re.compile(r"^(e[a-z]{2}), dword ptr \[0x([0-9a-f]+)\]$")


TABLE_OVERRIDE = [None]


def find_table(img, export, max_scan=0x40):
    """Find 'mov ecx,N ... mov edi,imm32 ... rep movsd' in the export."""
    if TABLE_OVERRIDE[0]:
        return TABLE_OVERRIDE[0]
    va = img.exports()[export]
    code = img.read(va, max_scan)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    count = dest = None
    for a, s, m, o in md.disasm_lite(code, va):
        if m == "mov" and o.startswith("ecx, 0x"):
            count = int(o[5:], 16)
        if m == "mov" and o.startswith("edi, 0x"):
            dest = int(o[5:], 16)
        if m.startswith("rep movs") and count and dest:
            return dest, count * 4
    raise RuntimeError("table copy not found in %s" % export)


def push_args(insns, idx, want=4):
    """Constant arguments pushed before insns[idx], nearest push first."""
    args = []
    regs = {}
    j = idx - 1
    while j >= 0 and idx - j <= 24 and len(args) < want:
        a, s, m, o = insns[j]
        if m in ("call", "ret", "jmp") or m.startswith("ret"):
            break
        if m == "push":
            if o.startswith("0x"):
                args.append(int(o, 16))
            elif re.fullmatch(r"-?\d+", o):
                args.append(int(o) & 0xFFFFFFFF)
            else:
                args.append(("reg", o))
        j -= 1
    # resolve register pushes from mov reg, imm in the same window
    for k in range(j, idx):
        a, s, m, o = insns[k]
        mm = re.match(r"^(e[a-z]{2}), 0x([0-9a-f]+)$", o) if m == "mov" else None
        if mm:
            regs[mm.group(1)] = int(mm.group(2), 16)
    out = []
    for x in args:
        if isinstance(x, tuple) and x[1] in regs:
            out.append(regs[x[1]])
        else:
            out.append(x)
    return out


def store_after(insns, idx):
    """'mov dword ptr [abs], eax' right after a call: the global receiving the result."""
    for k in range(idx + 1, min(idx + 4, len(insns))):
        a, s, m, o = insns[k]
        mm = re.match(r"^dword ptr \[0x([0-9a-f]+)\], eax$", o)
        if m == "mov" and mm:
            return int(mm.group(1), 16)
    return None


NO_WRITE = ("cmp", "test", "push", "call", "jmp")
VOLATILE = ("eax", "ecx", "edx")


def written_reg(m, o):
    """The 32-bit register an instruction overwrites, if any (first operand)."""
    if m in NO_WRITE or m.startswith("j") or m.startswith("ret"):
        return None
    if m == "pop":
        return o if re.fullmatch(r"e[a-z]{2}", o) else None
    mm = re.match(r"^(e[a-z]{2})(,|$)", o)
    return mm.group(1) if mm else None


MOV_REG_REGOFF = re.compile(r"^(e[a-z]{2}), dword ptr \[(e[a-z]{2})(?: \+ (0x[0-9a-f]+|\d+))?\]$")


def track(insns, classify):
    """Yield (index, regs, op, is_tail) for every call and jmp.

    classify(addr) maps an absolute address loaded into a register to a
    tracked value (or None). A register loaded from [reg+off] where reg holds
    a tracked string value becomes ("sub", value, off). Registers keep their
    value until overwritten; eax/ecx/edx die at calls; everything resets at
    ret/jmp (a jmp through memory is reported first, as a possible tail call).
    """
    loaded = {}
    for i, (a, s, m, o) in enumerate(insns):
        if m == "call":
            yield i, loaded, o, False
            for r in VOLATILE:
                loaded.pop(r, None)
            continue
        if m == "jmp":
            yield i, loaded, o, True
            loaded.clear()
            continue
        if m.startswith("ret"):
            loaded.clear()
            continue
        if m == "mov":
            mm = MOV_REG_ABS.match(o)
            if mm:
                v = classify(int(mm.group(2), 16))
                if v is not None:
                    loaded[mm.group(1)] = v
                else:
                    loaded.pop(mm.group(1), None)
                continue
            mm = MOV_REG_REGOFF.match(o)
            if mm and isinstance(loaded.get(mm.group(2)), str):
                loaded[mm.group(1)] = ("sub", loaded[mm.group(2)], int(mm.group(3) or "0", 0))
                continue
        r = written_reg(m, o)
        if r:
            loaded.pop(r, None)


def table_calls(insns, base, size):
    """Yield (index, offset) for calls through the table, direct or via register."""
    classify = lambda addr: addr - base if base <= addr < base + size else None
    for i, loaded, o, tail in track(insns, classify):
        mm = MEM_ABS.match(o)
        if mm:
            addr = int(mm.group(1), 16)
            if base <= addr < base + size:
                yield i, addr - base
        elif re.fullmatch(r"e[a-z]{2}", o) and isinstance(loaded.get(o), int):
            yield i, loaded[o]


def subapi_calls(insns, base, ptr_offsets):
    """Calls 'call [reg+off]' where reg was loaded from gEngfuncs.pXxxAPI."""
    classify = lambda addr: ptr_offsets.get(addr - base)
    for i, loaded, o, tail in track(insns, classify):
        mm = MEM_REG_OFF.match(o)
        if mm and isinstance(loaded.get(mm.group(1)), str):
            yield i, loaded[mm.group(1)], int(mm.group(2) or "0", 0)
        elif re.fullmatch(r"e[a-z]{2}", o) and isinstance(loaded.get(o), tuple):
            yield i, loaded[o][1], loaded[o][2]


def fmt_arg(img, x):
    if isinstance(x, tuple):
        return "<%s>" % x[1]
    s = img.cstr(x)
    if s is not None:
        return s.replace("\n", "\\n")
    if img.section_of(x) == ".text":
        return "code:%08X" % x
    if img.section_of(x) in (".data", ".rdata"):
        return "data:%08X" % x
    return str(x if x < 0x80000000 else x - 0x100000000)


def write_csv(path, header, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(header)
        w.writerows(rows)
    print("  wrote %s (%d rows)" % (os.path.relpath(path, ROOT), len(rows)))


def scan_imports_exports(img, out, prefix):
    rows = []
    for e in getattr(img.pe, "DIRECTORY_ENTRY_IMPORT", []):
        for imp in e.imports:
            rows.append([e.dll.decode(), imp.name.decode() if imp.name else "#%d" % imp.ordinal,
                         "%08X" % imp.address])
    write_csv(os.path.join(out, prefix + "_imports.csv"), ["dll", "function", "iat_va"], rows)
    rows = sorted([[name, "%08X" % va] for name, va in img.exports().items()], key=lambda r: r[1])
    write_csv(os.path.join(out, prefix + "_exports.csv"), ["name", "va"], rows)


def scan_hl(img, insns, out, prefix):
    base, size = find_table(img, "GiveFnptrsToDll")
    names = members(os.path.join(SRC, "engine", "eiface.h"), "} enginefuncs_t;")
    print("  g_engfuncs at %08X, %d slots (header has %d)" % (base, size // 4, len(names)))
    usage = Counter()
    sites = defaultdict(list)
    for i, off in table_calls(insns, base, size):
        usage[off // 4] += 1
        sites[names[off // 4] if off // 4 < len(names) else "slot%d" % (off // 4)].append(i)
    write_csv(os.path.join(out, prefix + "_engfuncs.csv"), ["slot", "offset", "member", "call_sites"],
              [[k, "0x%03X" % (4 * k), names[k] if k < len(names) else "?", usage[k]]
               for k in range(size // 4)])

    def collect(member, nargs):
        for i in sites.get(member, []):
            yield i, insns[i][0], push_args(insns, i, nargs)

    rows = []
    for i, va, a in collect("pfnRegUserMsg", 2):
        g = store_after(insns, i)
        rows.append([fmt_arg(img, a[0]) if a else "", fmt_arg(img, a[1]) if len(a) > 1 else "",
                     "%08X" % g if g else "", "%08X" % va])
    write_csv(os.path.join(out, prefix + "_usermsgs.csv"), ["name", "size", "gmsg_global", "call_site"], rows)

    rows = []
    for i, va, a in collect("pfnCVarRegister", 1):
        if a and not isinstance(a[0], tuple):
            name, value, flags, fval = (img.u32(a[0]), img.u32(a[0] + 4), img.u32(a[0] + 8),
                                        img.read(a[0] + 12, 4))
            rows.append(["%08X" % a[0], img.cstr(name) if name else "", img.cstr(value) if value else "",
                         flags, struct.unpack("<f", fval)[0] if fval else "", "%08X" % va])
        else:
            rows.append(["", "", "", "", "", "%08X" % va])
    write_csv(os.path.join(out, prefix + "_cvars_registered.csv"),
              ["cvar_t", "name", "default", "flags", "value", "call_site"], rows)

    rows = []
    for member in ("pfnCVarGetFloat", "pfnCVarGetString", "pfnCVarSetFloat", "pfnCVarSetString",
                   "pfnCvar_DirectSet"):
        for i, va, a in collect(member, 2):
            rows.append([member, fmt_arg(img, a[0]) if a else "", fmt_arg(img, a[1]) if len(a) > 1 else "",
                         "%08X" % va])
    write_csv(os.path.join(out, prefix + "_cvars_used.csv"), ["member", "arg0", "arg1", "call_site"], rows)

    rows = []
    for i, va, a in collect("pfnAddServerCommand", 2):
        rows.append([fmt_arg(img, a[0]) if a else "", fmt_arg(img, a[1]) if len(a) > 1 else "", "%08X" % va])
    write_csv(os.path.join(out, prefix + "_commands.csv"), ["name", "handler", "call_site"], rows)

    rows = []
    for member, nargs in (("pfnServerCommand", 1), ("pfnClientCommand", 3), ("pfnServerPrint", 1),
                          ("pfnPrecacheModel", 1), ("pfnPrecacheSound", 1), ("pfnPrecacheGeneric", 1),
                          ("pfnPrecacheEvent", 2), ("pfnAlertMessage", 2), ("pfnCreateNamedEntity", 1),
                          ("pfnSetModel", 2), ("pfnEmitSound", 5), ("pfnCvar_DirectSet", 2)):
        for i, va, a in collect(member, nargs):
            rows.append([member] + [fmt_arg(img, x) for x in a] + [""] * (nargs - len(a)) +
                        ["%08X" % va])
    write_csv(os.path.join(out, prefix + "_engine_calls_args.csv"),
              ["member", "arg0", "arg1", "arg2", "arg3", "arg4", "call_site"],
              [r[:-1] + [""] * (7 - len(r)) + [r[-1]] if len(r) < 7 else r for r in rows])


def scan_client(img, insns, out, prefix):
    base, size = find_table(img, "Initialize")
    names = members(os.path.join(SRC, "engine", "cdll_int.h"), "} cl_enginefunc_t;")
    print("  gEngfuncs at %08X, %d slots (header has %d)" % (base, size // 4, len(names)))
    usage = Counter()
    sites = defaultdict(list)
    for i, off in table_calls(insns, base, size):
        usage[off // 4] += 1
        sites[names[off // 4] if off // 4 < len(names) else "slot%d" % (off // 4)].append(i)

    subapis = {
        "pTriAPI": ("common/triangleapi.h", "} triangleapi_t;"),
        "pEfxAPI": ("common/r_efx.h", "struct efx_api_s"),
        "pEventAPI": ("common/event_api.h", "} event_api_t;"),
        "pDemoAPI": ("common/demo_api.h", "} demo_api_t;"),
        "pNetAPI": ("common/net_api.h", "} net_api_t;"),
        "pVoiceTweak": ("common/ivoicetweak.h", "} IVoiceTweak;"),
    }
    ptr_offsets = {}
    sub_names = {}
    for member, (hdr, end) in subapis.items():
        if member in names:
            ptr_offsets[4 * names.index(member)] = member
            # Prefer the src/ (Paranoia) header; fall back to the Steam-era
            # HLSDK header for members the older header does not have.
            try:
                own = members(os.path.join(SRC, hdr), end)
            except (ValueError, OSError):
                own = []
            try:
                sdk = members(os.path.join(ROOT, "refs", "halflife", hdr), end)
            except (ValueError, OSError):
                sdk = []
            sub_names[member] = own + ["%s [HLSDK]" % n for n in sdk[len(own):]]
    sub_usage = Counter()
    for i, member, off in subapi_calls(insns, base, ptr_offsets):
        sn = sub_names.get(member, [])
        if off // 4 >= len(sn) + 4:
            continue  # register reused for something else
        sub_usage[(member, off // 4, sn[off // 4] if off // 4 < len(sn) else "?")] += 1

    rows = [[k, "0x%03X" % (4 * k), names[k] if k < len(names) else "?", usage[k]] for k in range(size // 4)]
    for (member, slot, name), n in sorted(sub_usage.items()):
        rows.append(["%s[%d]" % (member, slot), "0x%03X" % (4 * slot), "%s->%s" % (member, name), n])
    write_csv(os.path.join(out, prefix + "_engfuncs.csv"), ["slot", "offset", "member", "call_sites"], rows)

    def collect(member, nargs):
        for i in sites.get(member, []):
            yield i, insns[i][0], push_args(insns, i, nargs)

    rows = []
    for i, va, a in collect("pfnHookUserMsg", 2):
        rows.append([fmt_arg(img, a[0]) if a else "", fmt_arg(img, a[1]) if len(a) > 1 else "", "%08X" % va])
    write_csv(os.path.join(out, prefix + "_usermsgs.csv"), ["name", "handler", "call_site"], rows)

    rows = []
    for i, va, a in collect("pfnRegisterVariable", 3):
        g = store_after(insns, i)
        rows.append([fmt_arg(img, a[0]) if a else "", fmt_arg(img, a[1]) if len(a) > 1 else "",
                     fmt_arg(img, a[2]) if len(a) > 2 else "", "%08X" % g if g else "", "%08X" % va])
    write_csv(os.path.join(out, prefix + "_cvars_registered.csv"),
              ["name", "default", "flags", "cvar_ptr_global", "call_site"], rows)

    rows = []
    for member in ("pfnGetCvarPointer", "pfnGetCvarFloat", "pfnGetCvarString", "Cvar_Set"):
        for i, va, a in collect(member, 2):
            g = store_after(insns, i) if member == "pfnGetCvarPointer" else None
            rows.append([member, fmt_arg(img, a[0]) if a else "", fmt_arg(img, a[1]) if len(a) > 1 else "",
                         "%08X" % g if g else "", "%08X" % va])
    write_csv(os.path.join(out, prefix + "_cvars_used.csv"), ["member", "arg0", "arg1", "stored_to", "call_site"], rows)

    rows = []
    for i, va, a in collect("pfnAddCommand", 2):
        rows.append([fmt_arg(img, a[0]) if a else "", fmt_arg(img, a[1]) if len(a) > 1 else "", "%08X" % va])
    write_csv(os.path.join(out, prefix + "_commands.csv"), ["name", "handler", "call_site"], rows)

    rows = []
    for member, nargs in (("pfnClientCmd", 1), ("pfnServerCmd", 1), ("pfnServerCmdUnreliable", 1),
                          ("pfnPlaySoundByName", 2), ("pfnPlaySoundByNameAtLocation", 3),
                          ("pfnSPR_Load", 1), ("pfnConsolePrint", 1), ("Con_Printf", 1),
                          ("Con_DPrintf", 1), ("pfnCenterPrint", 1), ("COM_LoadFile", 3),
                          ("pfnGetLevelName", 0)):
        for i, va, a in collect(member, nargs):
            rows.append([member] + [fmt_arg(img, x) for x in a] + [""] * (3 - len(a)) + ["%08X" % va])
    write_csv(os.path.join(out, prefix + "_engine_calls_args.csv"),
              ["member", "arg0", "arg1", "arg2", "call_site"], rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("kind", choices=["hl", "client"])
    ap.add_argument("dll")
    ap.add_argument("out")
    ap.add_argument("--tag", default="1.6a")
    ap.add_argument("--table", help="engine table VA:size (hex), when the filling export has no rep movsd")
    args = ap.parse_args()
    if args.table:
        va, size = args.table.split(":")
        TABLE_OVERRIDE[0] = (int(va, 16), int(size, 16))
    os.makedirs(args.out, exist_ok=True)
    img = Image(args.dll)
    insns = sweep(img)
    print("%s: %d instructions" % (args.dll, len(insns)))
    prefix = "%s_%s" % (args.kind, args.tag)
    scan_imports_exports(img, args.out, prefix)
    if args.kind == "hl":
        scan_hl(img, insns, args.out, prefix)
    else:
        scan_client(img, insns, args.out, prefix)


if __name__ == "__main__":
    main()
