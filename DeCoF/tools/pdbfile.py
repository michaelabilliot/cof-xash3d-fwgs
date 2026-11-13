"""Minimal reader for MSF 7.00 program databases (PDB).

Reads only what this project needs: the PDB info stream, the DBI module
list, section contributions, per-module compiler records and the global
and per-module symbol records. No type information.
"""
import struct

MSF7_MAGIC = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\x00\x00\x00"

# Symbol record kinds (cvinfo.h)
S_COMPILE = 0x0001
S_END = 0x0006
S_OBJNAME = 0x1101
S_CONSTANT = 0x1107
S_UDT = 0x1108
S_LDATA32 = 0x110C
S_GDATA32 = 0x110D
S_PUB32 = 0x110E
S_LPROC32 = 0x110F
S_GPROC32 = 0x1110
S_COMPILE2 = 0x1116
S_PROCREF = 0x1125
S_LPROCREF = 0x1127
S_COMPILE3 = 0x113C
S_ENVBLOCK = 0x113D

CV_LANGUAGES = {0: "C", 1: "C++", 2: "Fortran", 3: "MASM", 4: "Pascal",
                5: "Basic", 6: "COBOL", 7: "Link", 8: "Cvtres", 9: "Cvtpgd",
                10: "C#", 11: "VB", 12: "ILAsm", 13: "Java", 14: "JScript",
                15: "MSIL", 16: "HLSL"}


def _cstr(buf, off):
    end = buf.index(b"\x00", off)
    return buf[off:end].decode("latin-1"), end + 1


class Module(object):
    def __init__(self, index, name, obj_name, sym_stream, sym_size,
                 c11_size, c13_size, contrib_sect, contrib_off, contrib_size):
        self.index = index
        self.name = name
        self.obj_name = obj_name
        self.sym_stream = sym_stream
        self.sym_size = sym_size
        self.c11_size = c11_size
        self.c13_size = c13_size
        self.contrib_sect = contrib_sect
        self.contrib_off = contrib_off
        self.contrib_size = contrib_size


class PDB(object):
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        if not self.data.startswith(MSF7_MAGIC):
            raise ValueError("%s: not an MSF 7.00 file" % path)
        (self.block_size, _free_map, self.num_blocks, dir_bytes,
         _unknown, block_map_addr) = struct.unpack_from("<6I", self.data, 32)
        bs = self.block_size
        n_dir_blocks = (dir_bytes + bs - 1) // bs
        dir_blocks = struct.unpack_from("<%dI" % n_dir_blocks, self.data,
                                        block_map_addr * bs)
        directory = b"".join(self.data[b * bs:(b + 1) * bs]
                             for b in dir_blocks)[:dir_bytes]
        num_streams = struct.unpack_from("<I", directory, 0)[0]
        sizes = struct.unpack_from("<%dI" % num_streams, directory, 4)
        off = 4 + 4 * num_streams
        self._streams = []
        for size in sizes:
            if size == 0xFFFFFFFF:
                self._streams.append(None)
                continue
            nb = (size + bs - 1) // bs
            blocks = struct.unpack_from("<%dI" % nb, directory, off)
            off += 4 * nb
            self._streams.append((size, blocks))
        self._read_info()
        self._read_dbi()

    def stream(self, idx):
        if idx >= len(self._streams) or self._streams[idx] is None:
            return b""
        size, blocks = self._streams[idx]
        bs = self.block_size
        return b"".join(self.data[b * bs:(b + 1) * bs] for b in blocks)[:size]

    def _read_info(self):
        s = self.stream(1)
        self.info_version, self.signature, self.age = struct.unpack_from("<3I", s, 0)
        g = s[12:28]
        d1, d2, d3 = struct.unpack_from("<IHH", g, 0)
        self.guid = "%08X%04X%04X%s" % (d1, d2, d3, g[8:].hex().upper())

    def _read_dbi(self):
        s = self.stream(3)
        (self.dbi_sig, self.dbi_version, self.dbi_age, self.gsi_stream,
         self.build_number, self.psi_stream, self.pdb_dll_version,
         self.sym_record_stream, self.pdb_dll_rbld, modi_size, sc_size,
         secmap_size, srcinfo_size, tsmap_size, _mfc, dbghdr_size, ec_size,
         self.dbi_flags, self.machine, _pad) = struct.unpack_from(
            "<iIIHHHHHHiiiiiIiiHHI", s, 0)
        self.toolchain = "%d.%02d" % ((self.build_number >> 8) & 0x7F,
                                      self.build_number & 0xFF)
        off = 64
        end = off + modi_size
        self.modules = []
        while off < end:
            (_u1, c_sect, _p1, c_off, c_size, _c_chr, _c_mod, _p2, _c_dcrc,
             _c_rcrc, _flags, sym_stream, sym_size, c11, c13, _nsrc, _p3,
             _u2, _src_ni, _pdb_ni) = struct.unpack_from(
                "<IHHiiIHHIIHHIIIHHIII", s, off)
            name, p = _cstr(s, off + 64)
            obj, p = _cstr(s, p)
            self.modules.append(Module(len(self.modules), name, obj,
                                       sym_stream, sym_size, c11, c13,
                                       c_sect, c_off, c_size))
            off = (p + 3) & ~3
        # File info substream: source files (including headers) per module
        fi_off = 64 + modi_size + sc_size + secmap_size
        self.module_files = [[] for _ in self.modules]
        if srcinfo_size >= 4:
            fi = s[fi_off:fi_off + srcinfo_size]
            nmod = struct.unpack_from("<H", fi, 0)[0]
            counts = struct.unpack_from("<%dH" % nmod, fi, 4 + 2 * nmod)
            total = sum(counts)
            offs_at = 4 + 4 * nmod
            offsets = struct.unpack_from("<%dI" % total, fi, offs_at)
            names_at = offs_at + 4 * total
            k = 0
            for m in range(min(nmod, len(self.modules))):
                for _ in range(counts[m]):
                    self.module_files[m].append(_cstr(fi, names_at + offsets[k])[0])
                    k += 1
        # Section contributions
        sc = s[end:end + sc_size]
        self.contribs = []
        if sc:
            ver = struct.unpack_from("<I", sc, 0)[0]
            esize = 32 if ver == 0xEFFE0000 + 20140516 else 28
            p = 4
            while p + esize <= len(sc):
                sect, _p, c_off, c_size, chars, mod = struct.unpack_from(
                    "<HHiiIH", sc, p)
                self.contribs.append((sect, c_off, c_size, chars, mod))
                p += esize
        # Optional debug header: stream index of the section headers
        dbg_off = (64 + modi_size + sc_size + secmap_size + srcinfo_size +
                   tsmap_size + ec_size)
        n = dbghdr_size // 2
        self.dbg_streams = struct.unpack_from("<%dH" % n, s, dbg_off) if n else ()
        self.sections = []
        if len(self.dbg_streams) > 5 and self.dbg_streams[5] != 0xFFFF:
            sh = self.stream(self.dbg_streams[5])
            for i in range(len(sh) // 40):
                name = sh[i * 40:i * 40 + 8].rstrip(b"\x00").decode("latin-1")
                vsize, va = struct.unpack_from("<II", sh, i * 40 + 8)
                self.sections.append((name, va, vsize))

    def rva(self, seg, off):
        if 1 <= seg <= len(self.sections):
            return self.sections[seg - 1][1] + off
        return None

    @staticmethod
    def iter_records(buf, start=0, end=None):
        p = start
        end = len(buf) if end is None else end
        while p + 4 <= end:
            reclen, kind = struct.unpack_from("<HH", buf, p)
            if reclen < 2:
                break
            yield p, kind, buf[p + 4:p + 2 + reclen]
            p += 2 + reclen

    def module_symbols(self, mod):
        if mod.sym_stream == 0xFFFF:
            return
        s = self.stream(mod.sym_stream)
        # The first 4 bytes are the CV signature.
        for rec in self.iter_records(s, 4, mod.sym_size):
            yield rec

    def module_lines(self, mod):
        """C11 line numbers of a module (VC 7.x debug builds).

        Returns a list of (file name, rva, line) sorted by rva.
        """
        if mod.sym_stream == 0xFFFF or not mod.c11_size:
            return []
        s = self.stream(mod.sym_stream)[mod.sym_size:mod.sym_size + mod.c11_size]
        c_file, _c_seg = struct.unpack_from("<HH", s, 0)
        file_offs = struct.unpack_from("<%dI" % c_file, s, 4)
        out = []
        for fo in file_offs:
            c_seg = struct.unpack_from("<H", s, fo)[0]
            ln_offs = struct.unpack_from("<%dI" % c_seg, s, fo + 4)
            name_at = fo + 4 + 4 * c_seg + 8 * c_seg
            name = _cstr(s, name_at)[0]
            for lo in ln_offs:
                seg, n = struct.unpack_from("<HH", s, lo)
                offs = struct.unpack_from("<%dI" % n, s, lo + 4)
                lines = struct.unpack_from("<%dH" % n, s, lo + 4 + 4 * n)
                for off, line in zip(offs, lines):
                    out.append((name, self.rva(seg, off), line))
        out.sort(key=lambda x: x[1])
        return out

    def global_symbols(self):
        s = self.stream(self.sym_record_stream)
        for rec in self.iter_records(s):
            yield rec

    def module_compile_info(self, mod):
        """Return a dict with the compiler record and env block of a module."""
        info = {}
        for _p, kind, d in self.module_symbols(mod):
            if kind == S_OBJNAME:
                info["objname"] = _cstr(d, 4)[0]
            elif kind == S_COMPILE3:
                flags, machine = struct.unpack_from("<IH", d, 0)
                v = struct.unpack_from("<8H", d, 6)
                info["kind"] = "S_COMPILE3"
                info["language"] = CV_LANGUAGES.get(flags & 0xFF, str(flags & 0xFF))
                info["flags"] = flags
                info["machine"] = machine
                info["fe_version"] = "%d.%d.%d.%d" % v[0:4]
                info["be_version"] = "%d.%d.%d.%d" % v[4:8]
                info["version_string"] = _cstr(d, 22)[0]
            elif kind == S_COMPILE2:
                flags, machine = struct.unpack_from("<IH", d, 0)
                v = struct.unpack_from("<6H", d, 6)
                info["kind"] = "S_COMPILE2"
                info["language"] = CV_LANGUAGES.get(flags & 0xFF, str(flags & 0xFF))
                info["flags"] = flags
                info["machine"] = machine
                info["fe_version"] = "%d.%d.%d" % v[0:3]
                info["be_version"] = "%d.%d.%d" % v[3:6]
                info["version_string"] = _cstr(d, 18)[0]
            elif kind == S_ENVBLOCK:
                p = 1
                env = {}
                while p < len(d) and d[p] != 0:
                    k, p = _cstr(d, p)
                    v, p = _cstr(d, p)
                    env[k] = v
                info["env"] = env
            elif kind in (S_GPROC32, S_LPROC32):
                break
        return info

    def procedures(self):
        """Yield (module, kind, rva, length, name) for every procedure."""
        for mod in self.modules:
            for _p, kind, d in self.module_symbols(mod):
                if kind in (S_GPROC32, S_LPROC32):
                    (_parent, _end, _next, length, _ds, _de, _ti, off,
                     seg, _flags) = struct.unpack_from("<IIIIIIIIHB", d, 0)
                    name = _cstr(d, 35)[0]
                    yield mod, kind, self.rva(seg, off), length, name

    def publics(self):
        """Yield (rva, flags, name) for every S_PUB32 record."""
        for _p, kind, d in self.global_symbols():
            if kind == S_PUB32:
                flags, off, seg = struct.unpack_from("<IIH", d, 0)
                yield self.rva(seg, off), flags, _cstr(d, 10)[0]
