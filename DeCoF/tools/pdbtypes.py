"""Type information (TPI stream) and function locals of an MSF 7.00 PDB.

  python tools/pdbtypes.py <pdb> list [substring]
      every class, struct, union and enum with its size
  python tools/pdbtypes.py <pdb> class <name> [<name> ...]
      C++ like declaration: bases, data members with offsets, nested
      types, methods (virtual slots for introducing virtuals)
  python tools/pdbtypes.py <pdb> func <substring>
      signature, frame locals and parameters of matching procedures
  python tools/pdbtypes.py <pdb> header <out.h>
      every user defined type, in TPI order, into one file

The 1.0, 1.4 and 1.55 hl.pdb files carry the full type stream of the
Cry of Fear server DLL as built then (VC 7.1). Layouts changed in 1.6a,
see docs/structs.md.
"""
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdbfile  # noqa: E402

# leaf kinds (cvinfo.h)
LF_VTSHAPE = 0x000A
LF_MODIFIER = 0x1001
LF_POINTER = 0x1002
LF_PROCEDURE = 0x1008
LF_MFUNCTION = 0x1009
LF_ARGLIST = 0x1201
LF_FIELDLIST = 0x1203
LF_BITFIELD = 0x1205
LF_METHODLIST = 0x1206
LF_BCLASS = 0x1400
LF_VBCLASS = 0x1401
LF_IVBCLASS = 0x1402
LF_INDEX = 0x1404
LF_VFUNCTAB = 0x1409
LF_FRIENDCLS = 0x140A
LF_VFUNCOFF = 0x140C
LF_ENUMERATE = 0x1502
LF_ARRAY = 0x1503
LF_CLASS = 0x1504
LF_STRUCTURE = 0x1505
LF_UNION = 0x1506
LF_ENUM = 0x1507
LF_FRIENDFCN = 0x150C
LF_MEMBER = 0x150D
LF_STMEMBER = 0x150E
LF_METHOD = 0x150F
LF_NESTTYPE = 0x1510
LF_ONEMETHOD = 0x1511

PRIMITIVES = {
    0x03: "void", 0x08: "HRESULT", 0x10: "char", 0x11: "short", 0x12: "long",
    0x13: "__int64", 0x20: "unsigned char", 0x21: "unsigned short",
    0x22: "unsigned long", 0x23: "unsigned __int64", 0x30: "bool",
    0x40: "float", 0x41: "double", 0x42: "long double", 0x68: "signed char",
    0x69: "unsigned char", 0x70: "char", 0x71: "wchar_t", 0x72: "short",
    0x73: "unsigned short", 0x74: "int", 0x75: "unsigned int",
    0x76: "__int64", 0x77: "unsigned __int64",
}

CALLCONV = {0x00: "__cdecl", 0x04: "__fastcall", 0x07: "__stdcall",
            0x0B: "__thiscall"}

ACCESS = {1: "private", 2: "protected", 3: "public"}

# symbol kinds for locals
S_BLOCK32 = 0x1103
S_BPREL32 = 0x110B
S_REGREL32 = 0x1111
S_REGISTER = 0x1106
S_LDATA32 = 0x110C
S_LABEL32 = 0x1105

REGS = {17: "eax", 18: "ecx", 19: "edx", 20: "ebx", 21: "esp", 22: "ebp",
        23: "esi", 24: "edi"}


def numeric(d, p):
    """CodeView numeric leaf at d[p]: (value, next offset)."""
    v = struct.unpack_from("<H", d, p)[0]
    if v < 0x8000:
        return v, p + 2
    fmt = {0x8000: "<b", 0x8001: "<h", 0x8002: "<H", 0x8003: "<i",
           0x8004: "<I", 0x8009: "<q", 0x800A: "<Q"}[v]
    return struct.unpack_from(fmt, d, p + 2)[0], p + 2 + struct.calcsize(fmt)


def cstr(d, p):
    end = d.index(b"\x00", p)
    return d[p:end].decode("latin-1"), end + 1


class Types(object):
    def __init__(self, pdb):
        s = pdb.stream(2)
        _ver, hdr, self.tmin, self.tmax, size = struct.unpack_from("<5I", s, 0)
        self.recs = {}
        ti = self.tmin
        p = hdr
        while p < hdr + size:
            length, kind = struct.unpack_from("<HH", s, p)
            self.recs[ti] = (kind, s[p + 4:p + 2 + length])
            ti += 1
            p += 2 + length
        self.by_name = {}
        for ti, (kind, d) in self.recs.items():
            if kind in (LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM):
                u = self.udt(ti)
                if not u["fwdref"]:
                    self.by_name.setdefault(u["name"], ti)

    # -- records -------------------------------------------------------
    def udt(self, ti):
        kind, d = self.recs[ti]
        if kind in (LF_CLASS, LF_STRUCTURE):
            count, prop, field, derived, vshape = struct.unpack_from("<HHIII", d, 0)
            size, p = numeric(d, 16)
        elif kind == LF_UNION:
            count, prop, field = struct.unpack_from("<HHI", d, 0)
            size, p = numeric(d, 8)
        elif kind == LF_ENUM:
            count, prop, utype, field = struct.unpack_from("<HHII", d, 0)
            size, p = None, 12
        else:
            raise ValueError("not a UDT: 0x%x" % ti)
        name, _ = cstr(d, p)
        return {"kind": kind, "count": count, "prop": prop, "field": field,
                "size": size, "name": name, "fwdref": bool(prop & 0x80),
                "utype": utype if kind == LF_ENUM else None}

    def resolve(self, ti):
        """Forward reference to definition by name."""
        if ti in self.recs and self.recs[ti][0] in (LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM):
            u = self.udt(ti)
            if u["fwdref"]:
                return self.by_name.get(u["name"], ti)
        return ti

    def fields(self, ti):
        """Decode an LF_FIELDLIST (following LF_INDEX continuations)."""
        out = []
        while ti:
            kind, d = self.recs[ti]
            assert kind == LF_FIELDLIST, hex(kind)
            p = 0
            ti = 0
            while p < len(d):
                leaf = struct.unpack_from("<H", d, p)[0]
                if leaf == LF_MEMBER:
                    attr, t = struct.unpack_from("<HI", d, p + 2)
                    off, q = numeric(d, p + 8)
                    name, q = cstr(d, q)
                    out.append(("member", attr, t, off, name))
                elif leaf == LF_STMEMBER:
                    attr, t = struct.unpack_from("<HI", d, p + 2)
                    name, q = cstr(d, p + 8)
                    out.append(("static", attr, t, None, name))
                elif leaf == LF_BCLASS:
                    attr, t = struct.unpack_from("<HI", d, p + 2)
                    off, q = numeric(d, p + 8)
                    out.append(("base", attr, t, off, None))
                elif leaf in (LF_VBCLASS, LF_IVBCLASS):
                    attr, t, vbp = struct.unpack_from("<HII", d, p + 2)
                    _a, q = numeric(d, p + 12)
                    _b, q = numeric(d, q)
                    out.append(("vbase", attr, t, None, None))
                elif leaf == LF_VFUNCTAB:
                    t = struct.unpack_from("<I", d, p + 4)[0]
                    q = p + 8
                    out.append(("vfptr", 0, t, 0, None))
                elif leaf == LF_ENUMERATE:
                    attr = struct.unpack_from("<H", d, p + 2)[0]
                    val, q = numeric(d, p + 4)
                    name, q = cstr(d, q)
                    out.append(("enumerate", attr, None, val, name))
                elif leaf == LF_METHOD:
                    count, mlist = struct.unpack_from("<HI", d, p + 2)
                    name, q = cstr(d, p + 8)
                    # method lists are stored in reverse declaration order
                    for attr, t, vbo in reversed(list(self.methodlist(mlist))):
                        out.append(("method", attr, t, vbo, name))
                elif leaf == LF_ONEMETHOD:
                    attr, t = struct.unpack_from("<HI", d, p + 2)
                    q = p + 8
                    vbo = None
                    if (attr >> 2) & 7 in (4, 6):
                        vbo = struct.unpack_from("<I", d, q)[0]
                        q += 4
                    name, q = cstr(d, q)
                    out.append(("method", attr, t, vbo, name))
                elif leaf == LF_NESTTYPE:
                    t = struct.unpack_from("<I", d, p + 4)[0]
                    name, q = cstr(d, p + 8)
                    out.append(("nested", 0, t, None, name))
                elif leaf == LF_FRIENDCLS:
                    t = struct.unpack_from("<I", d, p + 4)[0]
                    q = p + 8
                    out.append(("friendcls", 0, t, None, None))
                elif leaf == LF_FRIENDFCN:
                    t = struct.unpack_from("<I", d, p + 4)[0]
                    name, q = cstr(d, p + 8)
                    out.append(("friendfcn", 0, t, None, name))
                elif leaf == LF_VFUNCOFF:
                    q = p + 12
                elif leaf == LF_INDEX:
                    ti = struct.unpack_from("<I", d, p + 4)[0]
                    q = p + 8
                else:
                    raise ValueError("field leaf 0x%x" % leaf)
                p = q
                while p < len(d) and d[p] >= 0xF0:
                    p += d[p] & 0x0F
        return out

    def methodlist(self, ti):
        kind, d = self.recs[ti]
        assert kind == LF_METHODLIST
        p = 0
        while p < len(d):
            attr, _pad, t = struct.unpack_from("<HHI", d, p)
            p += 8
            vbo = None
            if (attr >> 2) & 7 in (4, 6):
                vbo = struct.unpack_from("<I", d, p)[0]
                p += 4
            yield attr, t, vbo

    def args(self, ti):
        kind, d = self.recs[ti]
        n = struct.unpack_from("<I", d, 0)[0]
        return list(struct.unpack_from("<%dI" % n, d, 4))

    # -- type names ----------------------------------------------------
    def name(self, ti, inner=""):
        """C declaration of type ti around declarator `inner`."""
        sp = (" " + inner) if inner else ""
        if ti < 0x1000:
            base = PRIMITIVES.get(ti & 0xFF, "prim_%x" % ti)
            if ti & 0xF00:
                return "%s *%s" % (base, inner)
            return base + sp
        kind, d = self.recs[ti]
        if kind in (LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM):
            return self.udt(ti)["name"] + sp
        if kind == LF_MODIFIER:
            t, attr = struct.unpack_from("<IH", d, 0)
            q = ("const " if attr & 1 else "") + ("volatile " if attr & 2 else "")
            return q + self.name(t, inner)
        if kind == LF_POINTER:
            t, attr = struct.unpack_from("<II", d, 0)
            mode = (attr >> 5) & 7
            if mode == 1:
                star = "&"
            elif mode in (2, 3):
                cls = struct.unpack_from("<I", d, 8)[0]
                star = self.name(cls) + "::*"
            else:
                star = "*"
            if attr & 0x400:
                star += "const "
            tk = self.recs[t][0] if t >= 0x1000 else None
            if tk in (LF_PROCEDURE, LF_MFUNCTION, LF_ARRAY):
                return self.name(t, "(%s%s)" % (star, inner))
            return self.name(t, star + inner)
        if kind == LF_ARRAY:
            et, _it = struct.unpack_from("<II", d, 0)
            size, _ = numeric(d, 8)
            esize = self.size(et) or 1
            if size == 0:
                return self.name(et, "%s[]" % inner)
            return self.name(et, "%s[%d]" % (inner, size // esize))
        if kind == LF_PROCEDURE:
            rv, cc, _fa, _n, al = struct.unpack_from("<IBBHI", d, 0)
            a = ", ".join(self.name(x) for x in self.args(al)) or "void"
            return self.name(rv, "%s %s(%s)" % (CALLCONV.get(cc, "cc%d" % cc), inner, a))
        if kind == LF_MFUNCTION:
            rv, cls, this, cc, _fa, _n, al = struct.unpack_from("<IIIBBHI", d, 0)
            a = ", ".join(self.name(x) for x in self.args(al)) or "void"
            return self.name(rv, "%s(%s)" % (inner, a))
        if kind == LF_BITFIELD:
            t, length, pos = struct.unpack_from("<IBB", d, 0)
            return "%s%s : %d" % (self.name(t), sp, length)
        return "type_%x%s" % (ti, sp)

    def size(self, ti):
        if ti < 0x1000:
            if ti & 0xF00:
                return 4
            return {0x03: 0, 0x10: 1, 0x20: 1, 0x30: 1, 0x68: 1, 0x69: 1, 0x70: 1,
                    0x11: 2, 0x21: 2, 0x71: 2, 0x72: 2, 0x73: 2,
                    0x12: 4, 0x22: 4, 0x40: 4, 0x74: 4, 0x75: 4, 0x08: 4,
                    0x13: 8, 0x23: 8, 0x41: 8, 0x76: 8, 0x77: 8, 0x42: 10}.get(ti & 0xFF)
        ti = self.resolve(ti)
        kind, d = self.recs[ti]
        if kind in (LF_CLASS, LF_STRUCTURE, LF_UNION):
            return self.udt(ti)["size"]
        if kind == LF_ENUM:
            return 4
        if kind == LF_MODIFIER:
            return self.size(struct.unpack_from("<I", d, 0)[0])
        if kind == LF_POINTER:
            return 4
        if kind == LF_ARRAY:
            return numeric(d, 8)[0]
        return None

    # -- declarations ----------------------------------------------------
    def declare_raw(self, ti):
        """Members and methods interleaved, in field list (declaration)
        order, with access labels."""
        ti = self.resolve(ti)
        u = self.udt(ti)
        fl = self.fields(u["field"]) if u["field"] else []
        out = ["// %s, size 0x%X" % (u["name"], u["size"] or 0)]
        access = None
        seen = {}
        for k, a, t, off, name in fl:
            acc = ACCESS.get(a & 3)
            if k in ("member", "static", "method") and acc != access:
                out.append("%s:" % acc)
                access = acc
            if k == "base":
                out.append("\t// base %s %s at 0x%X" % (acc, self.name(t), off))
            elif k == "member":
                out.append("\t%-56s // 0x%X" % (self.name(t, name) + ";", off))
            elif k == "static":
                out.append("\tstatic %s;" % self.name(t, name))
            elif k == "nested":
                out.append("\t// nested %s" % self.name(t))
            elif k == "method":
                mprop = (a >> 2) & 7
                pre = {1: "virtual ", 2: "static ", 4: "virtual ", 5: "virtual ", 6: "virtual "}.get(mprop, "")
                post = " = 0" if mprop in (5, 6) else ""
                note = ""
                if off is not None:
                    n = seen.get((name, off), 0)
                    seen[(name, off)] = n + 1
                    note = "  // new slot +0x%X" % (off - 4 * n)
                if a & 0x100:
                    note += "  // compiler generated"
                out.append("\t%s%s%s;%s" % (pre, self.name(t, name), post, note))
        return "\n".join(out) + "\n"

    def declare(self, ti):
        ti = self.resolve(ti)
        u = self.udt(ti)
        kw = {LF_CLASS: "class", LF_STRUCTURE: "struct", LF_UNION: "union",
              LF_ENUM: "enum"}[u["kind"]]
        if u["fwdref"]:
            return "%s %s;  // no definition in this PDB\n" % (kw, u["name"])
        fl = self.fields(u["field"]) if u["field"] else []
        if u["kind"] == LF_ENUM:
            lines = ["enum %s\n{" % u["name"]]
            for _k, _a, _t, val, name in fl:
                lines.append("\t%s = %d," % (name, val))
            return "\n".join(lines) + "\n};\n"
        bases = ["%s %s" % (ACCESS.get(a & 3, ""), self.name(t)) for k, a, t, _o, _n in fl if k == "base"]
        head = "%s %s%s" % (kw, u["name"], (" : " + ", ".join(bases)) if bases else "")
        lines = ["// size 0x%X" % u["size"], head, "{"]
        for k, a, t, off, name in fl:
            if k == "base":
                lines.append("\t// base %s at 0x%X" % (self.name(t), off))
            elif k == "vfptr":
                lines.append("\t// vftable pointer at 0x0")
            elif k == "member":
                lines.append("\t%-56s // 0x%X" % (self.name(t, name) + ";", off))
            elif k == "static":
                lines.append("\tstatic %s;" % self.name(t, name))
            elif k == "nested":
                lines.append("\t// nested %s = %s" % (name, self.name(t)))
        methods = [f for f in fl if f[0] == "method"]
        # overloaded introducing virtuals all carry the offset of the last
        # slot of their group; MSVC fills the group in reverse declaration
        # order, so the k-th overload sits 4*k below it
        seen = {}
        for i, (k, a, t, vbo, name) in enumerate(methods):
            if vbo is not None:
                n = seen.get((name, vbo), 0)
                seen[(name, vbo)] = n + 1
                methods[i] = (k, a, t, vbo - 4 * n, name)
        if methods:
            lines.append("")
            for k, a, t, vbo, name in methods:
                mprop = (a >> 2) & 7
                pre = {1: "virtual ", 2: "static ", 4: "virtual ", 5: "virtual ", 6: "virtual "}.get(mprop, "")
                post = " = 0" if mprop in (5, 6) else ""
                slot = "  // vtable +0x%X" % vbo if vbo is not None else ""
                comp = "  // compiler generated" if a & 0x100 else ""
                lines.append("\t%s%s%s;%s%s" % (pre, self.name(t, name), post, slot, comp))
        return "\n".join(lines) + "\n};\n"

    def udts(self):
        for ti, (kind, _d) in sorted(self.recs.items()):
            if kind in (LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM):
                u = self.udt(ti)
                if not u["fwdref"]:
                    yield ti, u


def proc_locals(pdb, types, needle):
    """Yield text blocks for procedures whose name contains needle."""
    for mod in pdb.modules:
        recs = list(pdb.module_symbols(mod))
        i = 0
        while i < len(recs):
            _p, kind, d = recs[i]
            if kind in (pdbfile.S_GPROC32, pdbfile.S_LPROC32):
                (_parent, _end, _next, length, _ds, _de, ti, off, seg,
                 _flags) = struct.unpack_from("<IIIIIIIIHB", d, 0)
                name = cstr(d, 35)[0]
                if needle in name:
                    out = ["%s  (%s, rva 0x%X, %d bytes)" % (name, mod.name, pdb.rva(seg, off) or 0, length),
                           "  type: " + types.name(ti, "f")]
                    depth = 1
                    j = i + 1
                    while j < len(recs) and depth:
                        _p2, k2, d2 = recs[j]
                        ind = "  " * (depth + 1)
                        if k2 in (pdbfile.S_GPROC32, pdbfile.S_LPROC32, S_BLOCK32):
                            depth += 1
                            if k2 == S_BLOCK32:
                                out.append(ind + "{")
                        elif k2 == pdbfile.S_END:
                            depth -= 1
                            if depth:
                                out.append("  " * (depth + 1) + "}")
                        elif k2 == S_BPREL32:
                            boff, t = struct.unpack_from("<iI", d2, 0)
                            out.append(ind + "[ebp%+d] %s" % (boff, types.name(t, cstr(d2, 8)[0])))
                        elif k2 == S_REGREL32:
                            roff, t, reg = struct.unpack_from("<IIH", d2, 0)
                            out.append(ind + "[%s%+d] %s" % (REGS.get(reg, "r%d" % reg),
                                                             struct.unpack("<i", struct.pack("<I", roff))[0],
                                                             types.name(t, cstr(d2, 10)[0])))
                        elif k2 == S_REGISTER:
                            t, reg = struct.unpack_from("<IH", d2, 0)
                            out.append(ind + "<%s> %s" % (REGS.get(reg, "r%d" % reg), types.name(t, cstr(d2, 6)[0])))
                        elif k2 == S_LDATA32:
                            t, o2, s2 = struct.unpack_from("<IIH", d2, 0)
                            out.append(ind + "static %s" % types.name(t, cstr(d2, 10)[0]))
                        elif k2 == S_LABEL32:
                            out.append(ind + "label " + cstr(d2, 7)[0])
                        j += 1
                    yield "\n".join(out)
            i += 1


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    pdb = pdbfile.PDB(sys.argv[1])
    types = Types(pdb)
    cmd, rest = sys.argv[2], sys.argv[3:]
    if cmd == "list":
        for ti, u in types.udts():
            if not rest or rest[0].lower() in u["name"].lower():
                print("%05X  %-6s %s" % (ti, "enum" if u["size"] is None else "0x%X" % u["size"], u["name"]))
    elif cmd == "class":
        for n in rest:
            ti = types.by_name.get(n)
            print(types.declare(ti) if ti else "// %s: not found\n" % n)
    elif cmd == "raw":
        for n in rest:
            ti = types.by_name.get(n)
            print(types.declare_raw(ti) if ti else "// %s: not found\n" % n)
    elif cmd == "func":
        for block in proc_locals(pdb, types, rest[0]):
            print(block + "\n")
    elif cmd == "header":
        with open(rest[0], "w") as f:
            f.write("// generated by tools/pdbtypes.py from %s\n\n" % sys.argv[1])
            for ti, _u in types.udts():
                f.write(types.declare(ti) + "\n")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
