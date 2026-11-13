"""Symbolic equivalence check of one function: retail (delinked target obj)
against ours (base obj).

  python tools/symeq.py <unit> <symbol> [--ret void|eax|al|st0] [--verbose]

Both functions are executed symbolically per superblock (runs up to a
conditional branch, a switch or a ret, following direct jumps). The two
CFGs are walked in lockstep; at each pair of superblocks the entry values
are abstracted into equivalence classes that hold on every incoming edge
(fixpoint), so register and stack slot choice does not matter. Compared:
calls (target, stack args, this, xmm0/st0 inputs) in order, stores to non
stack memory (multiset per call segment, plus order of possibly aliasing
stores), branch conditions (with inversion), switch dispatch, return value
and callee saved registers. Loads of non stack memory are listed as
warnings when the sets differ per segment (dead or moved loads).
"""
import struct, sys, os, re, json, csv
import capstone
from capstone import x86 as X

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import objdiff_tool  # noqa

M32 = 0xffffffff
UNNAMED = re.compile(r"^(?:DAT|FUN|LAB|PTR|s|u|thunk_FUN)_(?:.*_)?([0-9a-fA-F]{8})$")

# ---------------------------------------------------------------- COFF
class Coff:
    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        mach, nsec, ts, psym, nsym, opt, ch = struct.unpack_from('<HHIIIHH', d, 0)
        self.bigobj = False
        if mach == 0 and nsec == 0xffff:
            self.bigobj = True
            # ANON_OBJECT_HEADER_BIGOBJ
            nsec, psym, nsym = struct.unpack_from('<III', d, 44)
            shoff = 56
            symsz = 20
        else:
            shoff = 20 + opt
            symsz = 18
        self.strtab_off = psym + nsym * symsz
        self.secs = []
        for i in range(nsec):
            o = shoff + 40 * i
            name = d[o:o + 8].rstrip(b'\0').decode('latin1')
            vs, va, rsz, praw, prel, pln, nrel, nln, chars = struct.unpack_from('<IIIIIIHHI', d, o + 8)
            if name.startswith('/'):
                name = self.cstr(self.strtab_off + int(name[1:]))
            if nrel == 0xffff and chars & 0x01000000:
                nrel = struct.unpack_from('<I', d, prel)[0]
                prel += 10; nrel -= 1
            relocs = {}
            for k in range(nrel):
                ra, si, ty = struct.unpack_from('<IIH', d, prel + 10 * k)
                relocs[ra] = (si, ty)
            self.secs.append(dict(name=name, data=d[praw:praw + rsz] if praw else b'\0' * rsz,
                                  relocs=relocs, size=rsz))
        self.syms = {}
        self.byname = {}
        i = 0
        while i < nsym:
            o = psym + i * symsz
            if d[o:o + 4] == b'\0\0\0\0':
                name = self.cstr(self.strtab_off + struct.unpack_from('<I', d, o + 4)[0])
            else:
                name = d[o:o + 8].rstrip(b'\0').decode('latin1')
            if self.bigobj:
                val, sec, typ, sc, naux = struct.unpack_from('<IiHBB', d, o + 8)
            else:
                val, sec, typ, sc, naux = struct.unpack_from('<IhHBB', d, o + 8)
            self.syms[i] = (name, val, sec, sc)
            if sec > 0 and name not in self.byname and sc in (2, 3, 6) and not name.startswith('$'):
                self.byname.setdefault(name, (sec - 1, val))
            if sec > 0 and sc == 2:
                self.byname[name] = (sec - 1, val)
            i += 1 + naux

    def cstr(self, o):
        e = self.d.index(b'\0', o)
        return self.d[o:e].decode('latin1')

    def sym_at(self, sec, off):
        best = None
        for i, (n, v, s, sc) in self.syms.items():
            if s - 1 == sec and v == off and sc in (2, 3, 6) and not n.startswith('$') and not n.startswith('.'):
                if best is None or sc == 2:
                    best = n
        return best


# ---------------------------------------------------------------- retail DLL
class Retail:
    def __init__(self):
        import pefile
        self.pe = pefile.PE(os.path.join(ROOT, r'versions\1.6a\cryoffear\cl_dlls\client.dll'), fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.names = {}
        for p in (r'build\jobs\symbols_client.csv', r'tools\symbols\client_1.6a.csv'):
            fp = os.path.join(ROOT, p)
            if os.path.exists(fp):
                for r in csv.DictReader(open(fp, newline='', encoding='utf-8')):
                    self.names.setdefault(r['name'], int(r['va'], 16))
        self.popcache = {}

    def read(self, va, n):
        return self.pe.get_data(va - self.base, n)

    def pop_bytes(self, name):
        if name in self.popcache:
            return self.popcache[name]
        r = None
        m = re.match(r'^(?:__imp_)?_\w+@(\d+)$', name)
        if m:
            r = int(m.group(1))
        elif re.match(r'^(?:__imp_)?_[A-Za-z_]\w*$', name):
            r = 0
        else:
            va = self.names.get(name)
            mm = UNNAMED.match(name)
            if va is None and mm:
                va = int(mm.group(1), 16)
            if va is not None:
                md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
                try:
                    code = self.read(va, 0x6000)
                except Exception:
                    code = b''
                for ins in md.disasm(code, va):
                    if ins.mnemonic in ('ret', 'retn'):
                        r = int(ins.op_str, 0) if ins.op_str else 0
                        break
                    if ins.mnemonic == 'jmp' and ins.address == va:
                        break  # thunk
        self.popcache[name] = r
        return r


RET = None
OBJCAP = 0x40
IDIOMS = False
STDCALL_RANGES = []
STDCALL_NAMES = []
DECLARED = {}
PTR_STDCALL = False
OBJSIZE = {}
NORETURN = ('_Xlength_error', '_Xout_of_range', '_Xbad_alloc', '_Xinvalid_argument', 'CxxThrowException',
            '_invalid_parameter_noinfo_noreturn', '__std_terminate', '_abort', '_exit@', '__report_rangecheckfailure',
            '_Xbad_function_call', '_Xruntime_error', '_Xoverflow_error')
MUTATE = {}


def mangled_cc(name):
    """'cdecl', 'thiscall', 'stdcall', 'fastcall' or None"""
    if UNNAMED.match(name) or name.startswith('F:') or name.startswith('sub_'):
        return None
    if name.startswith('?'):
        m = re.search(r'@@([A-Z])', name)
        if not m:
            return None
        rest = name[m.end(1) - 1:]
        c = rest[0]
        if c == 'Y':
            cc = rest[1]
        elif c in 'ABIJQRCDKLSTEFMNUV':
            if c in 'CDKLST':
                cc = rest[1]
            else:
                cc = rest[2]
        else:
            return None
        return {'A': 'cdecl', 'B': 'cdecl', 'E': 'thiscall', 'G': 'stdcall', 'I': 'fastcall'}.get(cc)
    if re.match(r'^(?:__imp_)?_\w+@\d+$', name):
        return 'stdcall'
    return 'cdecl'


# ---------------------------------------------------------------- expressions
def sgn(c):
    c &= M32
    return c - (1 << 32) if c & 0x80000000 else c


def as_lin(e):
    if isinstance(e, int):
        return e & M32, {}
    if isinstance(e, tuple) and e and e[0] == 'lin':
        return e[1], dict(e[2])
    return 0, {e: 1}


def mklin(c, terms):
    terms = {t: sgn(k) for t, k in terms.items() if k & M32}
    c &= M32
    if not terms:
        return c
    if len(terms) == 1 and c == 0:
        (t, k), = terms.items()
        if k == 1:
            return t
    return ('lin', c, tuple(sorted(terms.items(), key=lambda kv: repr(kv[0]))))


def eadd(a, b):
    ca, ta = as_lin(a)
    cb, tb = as_lin(b)
    for t, k in tb.items():
        ta[t] = ta.get(t, 0) + k
    return mklin(ca + cb, ta)


def escale(a, k):
    c, t = as_lin(a)
    return mklin(c * k, {x: v * k for x, v in t.items()})


def esub(a, b):
    return eadd(a, escale(b, -1))


def slot_of(addr):
    c, t = as_lin(addr)
    if len(t) == 1:
        (k, v), = t.items()
        if v == 1 and isinstance(k, tuple) and k[0] == 'base':
            return (k[1], sgn(c))
    return None


def is_stable(e):
    if isinstance(e, tuple) and e and e[0] in ('init', 'arg', 'k', 'kv', 'imp', 'retaddr'):
        return True
    c, t = as_lin(e)
    return all(isinstance(k, tuple) and k[0] in ('base', 'sym') for k in t)


COMM = {'and', 'or', 'xor', 'mul', 'fadd', 'fmul', 'dadd', 'dmul', 'test', 'imul', 'fmax', 'fmin'}


def op(name, *args):
    if name in COMM:
        args = tuple(sorted(args, key=repr))
    if name == 'and' and all(isinstance(a, int) for a in args):
        return args[0] & args[1]
    if name == 'or' and all(isinstance(a, int) for a in args):
        return args[0] | args[1]
    if name == 'xor' and all(isinstance(a, int) for a in args):
        return args[0] ^ args[1]
    if name == 'and' and any(a == M32 for a in args):
        return [a for a in args if a != M32][0] if len(args) == 2 and not (args[0] == args[1] == M32) else args[0]
    return (name,) + tuple(args)


def lo8(v):
    if isinstance(v, int):
        return v & 0xff
    if isinstance(v, tuple) and v[0] in ('zx8', 'sx8'):
        return v[1]
    if isinstance(v, tuple) and v[0] == 'setlo8':
        return v[2]
    return ('lo8', v)


def setlo8(old, v):
    if isinstance(old, int) and old == 0:
        return zx8(v)
    if isinstance(old, int) and isinstance(v, int):
        return (old & ~0xff | v) & M32
    return ('setlo8', old, v)


def zx8(v):
    if isinstance(v, int):
        return v & 0xff
    if isinstance(v, tuple) and v[0] == 'set':
        return v
    if isinstance(v, tuple) and v[0] == 'lo8' and isinstance(v[1], tuple) and v[1][0] in ('zx8', 'set'):
        return v[1]
    return ('zx8', v)


# conditions ------------------------------------------------------------
NEG = {'eq': 'ne', 'slt': 'sge', 'sle': 'sgt', 'ult': 'uge', 'ule': 'ugt',
       'OGT': 'ULE', 'OGE': 'ULT', 'OLT': 'UGE', 'OLE': 'UGT', 'OEQ': 'UNE', 'UEQ': 'ONE', 'UNO': 'ORD',
       'sign': 'nsign', 'tz': 'tnz', 'cf': 'ncf', 'pf': 'npf', 'of': 'nof', 'gen': 'ngen'}
for k, v in list(NEG.items()):
    NEG[v] = k
SWAP = {'eq': 'eq', 'ne': 'ne', 'slt': 'sgt', 'sgt': 'slt', 'sle': 'sge', 'sge': 'sle', 'ult': 'ugt', 'ugt': 'ult',
        'ule': 'uge', 'uge': 'ule', 'OGT': 'OLT', 'OLT': 'OGT', 'OGE': 'OLE', 'OLE': 'OGE', 'ULT': 'UGT', 'UGT': 'ULT',
        'ULE': 'UGE', 'UGE': 'ULE', 'OEQ': 'OEQ', 'UNE': 'UNE', 'UEQ': 'UEQ', 'ONE': 'ONE', 'UNO': 'UNO', 'ORD': 'ORD'}
ICC = {'e': 'eq', 'z': 'eq', 'ne': 'ne', 'nz': 'ne', 'l': 'slt', 'nge': 'slt', 'le': 'sle', 'ng': 'sle', 'g': 'sgt',
       'nle': 'sgt', 'ge': 'sge', 'nl': 'sge', 'b': 'ult', 'c': 'ult', 'nae': 'ult', 'be': 'ule', 'na': 'ule',
       'a': 'ugt', 'nbe': 'ugt', 'ae': 'uge', 'nb': 'uge', 'nc': 'uge'}
FCC = {'a': 'OGT', 'nbe': 'OGT', 'ae': 'OGE', 'nb': 'OGE', 'nc': 'OGE', 'b': 'ULT', 'c': 'ULT', 'nae': 'ULT',
       'be': 'ULE', 'na': 'ULE', 'e': 'UEQ', 'z': 'UEQ', 'ne': 'ONE', 'nz': 'ONE', 'p': 'UNO', 'pe': 'UNO',
       'np': 'ORD', 'po': 'ORD'}


def canon_cond(pred, a, b):
    if pred in SWAP and repr(a) > repr(b):
        return (SWAP[pred], b, a)
    return (pred, a, b)


def make_cond(cc, flags):
    if flags is None:
        return ('gen', cc, None)
    k = flags[0]
    if k == 'cmp':
        a, b = flags[1], flags[2]
        if cc in ICC:
            return canon_cond(ICC[cc], a, b)
        if cc in ('s', 'ns'):
            return ('sign' if cc == 's' else 'nsign', esub(a, b), 0)
    if k in ('test', 'res'):
        if k == 'test':
            a, b = flags[1], flags[2]
            x = a if a == b else op('and', a, b)
        else:
            x = flags[1]
        if cc in ('e', 'z', 'ne', 'nz'):
            return canon_cond(ICC[cc], x, 0)
        if cc in ('s', 'l'):
            return ('slt', x, 0)
        if cc in ('ns', 'ge'):
            return ('sge', x, 0)
        if cc == 'le':
            return ('sle', x, 0)
        if cc == 'g':
            return ('sgt', x, 0)
    if k == 'fcmp':
        a, b = flags[2], flags[3]
        if cc in FCC:
            p = FCC[cc]
            p = p if flags[1] == 'u' else p + '_s'
            if p in SWAP:
                return canon_cond(p, a, b)
            return (p, a, b)
    if k == 'lahf':
        mask, f = flags[1], flags[2]
        if f[0] == 'fcmp' and cc in ('p', 'pe', 'np', 'po'):
            jp = cc in ('p', 'pe')
            tab = {0x44: ('UNE', 'OEQ'), 0x05: ('UGE', 'OLT'), 0x41: ('UGT', 'OLE')}
            if mask in tab:
                p = tab[mask][0 if jp else 1]
                if f[1] != 'u':
                    p += '_s'
                if p in SWAP:
                    return canon_cond(p, f[2], f[3])
                return (p, f[2], f[3])
    return ('gen', cc, flags)


def neg_cond(c):
    p = c[0]
    base = p[:-2] if p.endswith('_s') else p
    suf = '_s' if p.endswith('_s') else ''
    if base in NEG:
        q = NEG[base] + suf
        if c[0] == 'gen':
            return ('ngen', c[1], c[2])
        return canon_cond(q, c[1], c[2]) if q in SWAP else (q, c[1], c[2])
    if p == 'ngen':
        return ('gen', c[1], c[2])
    return ('not', c)


# ---------------------------------------------------------------- function loading
REG32 = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
SUB = {}
for r in ('a', 'b', 'c', 'd'):
    SUB[r + 'l'] = ('e' + r + 'x', 0, 1)
    SUB[r + 'h'] = ('e' + r + 'x', 1, 1)
    SUB[r + 'x'] = ('e' + r + 'x', 0, 2)
for r in ('si', 'di', 'bp', 'sp'):
    SUB[r] = ('e' + r, 0, 2)


class Insn:
    pass


class Func:
    def __init__(self, coff, name, size, side, alias):
        self.side = side
        sec, off = coff.byname[name]
        self.sec = sec
        self.coff = coff
        s = coff.secs[sec]
        self.start, self.end = off, off + size
        for mo, mb in MUTATE.get(side, []):
            d0 = s['data']
            s['data'] = d0[:off + mo] + mb + d0[off + mo + len(mb):]
        code = s['data'][off:off + size]
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        md.detail = True
        self.ins = {}
        self.order = []
        for i in md.disasm(code, off):
            n = Insn()
            n.addr, n.size, n.mn, n.ops_str = i.address, i.size, i.mnemonic, i.op_str
            n.next = i.address + i.size
            n.ops = []
            relsym = {}
            for ra, (si, ty) in s['relocs'].items():
                if i.address <= ra < i.address + i.size:
                    nm = coff.syms[si][0]
                    tsec = coff.syms[si][2] - 1
                    relsym[ra - i.address] = (nm, ty, tsec, coff.syms[si][1], coff.syms[si][3])
            n.relsym = relsym
            for o in i.operands:
                if o.type == X.X86_OP_REG:
                    n.ops.append(('reg', i.reg_name(o.reg), o.size))
                elif o.type == X.X86_OP_IMM:
                    sym = relsym.get(i.imm_offset) if relsym else None
                    n.ops.append(('imm', o.imm & M32, o.size, self.symname(sym, alias)))
                elif o.type == X.X86_OP_MEM:
                    m = o.mem
                    sym = relsym.get(i.disp_offset) if relsym else None
                    n.ops.append(('mem', i.reg_name(m.base) if m.base else None,
                                  i.reg_name(m.index) if m.index else None, m.scale, m.disp & M32,
                                  o.size, self.symname(sym, alias),
                                  i.reg_name(m.segment) if m.segment else None))
            # rel32 branch / call to external
            n.target = None
            n.tsym = None
            if n.mn == 'call' or n.mn.startswith('j'):
                if n.ops and n.ops[0][0] == 'imm':
                    if relsym:
                        nm = list(relsym.values())[0]
                        n.tsym = self.symname(nm, alias)
                        if nm[2] == sec and nm[4] in (3, 6) and n.mn != 'call':
                            n.target = nm[3] + ((n.ops[0][1]) & 0)  # label in own section
                            n.tsym = None
                    else:
                        t = n.ops[0][1]
                        if self.start <= t < self.end and n.mn != 'call':
                            n.target = t
                        else:
                            n.tsym = coff.sym_at(sec, t) or ('sub_%x' % t)
                            n.tsym = alias.get(n.tsym, n.tsym) if side == 'L' else n.tsym
            self.ins[n.addr] = n
            self.order.append(n.addr)

    def idioms(self):
        """comiss a, b / jb x / movaps a, b / x:  ->  a = minss(b, a)
        (equal to the branch for every input except a == b == +-0)"""
        self.idiom_sites = []
        for a in list(self.order):
            n1 = self.ins[a]
            if n1.mn not in ('comiss', 'ucomiss') or len(n1.ops) != 2 or n1.ops[0][0] != 'reg' or n1.ops[1][0] != 'reg':
                continue
            n2 = self.ins.get(n1.next)
            if not n2 or n2.mn != 'jb' or n2.target is None:
                continue
            n3 = self.ins.get(n2.next)
            if not n3 or n3.mn != 'movaps' or n3.ops != n1.ops or n3.next != n2.target:
                continue
            n1.mn = 'minidiom'
            n1.next = n2.target
            self.idiom_sites.append(a)

    def symname(self, sym, alias):
        if not sym:
            return None
        nm = sym[0]
        if self.side == 'L':
            nm = alias.get(nm, nm)
        return nm


# ---------------------------------------------------------------- state
class Unhandled(Exception):
    pass


class State:
    def __init__(self):
        self.r = {}
        self.x = {}
        self.fp = []
        self.flags = None
        self.stk = {}       # (base, off) -> (size, val)
        self.eff = []       # ('call', ...) / ('st', addr, size, val)
        self.loads = []
        self.epoch = None
        self.ncall = 0
        self.floor = None
        self.written = set()
        self.warn = []

    def copy(self):
        s = State()
        s.r = dict(self.r); s.x = dict(self.x); s.fp = list(self.fp); s.flags = self.flags
        s.stk = dict(self.stk); s.eff = list(self.eff); s.loads = list(self.loads); s.epoch = self.epoch
        s.ncall = self.ncall; s.floor = self.floor; s.warn = list(self.warn)
        return s


class Exec:
    def __init__(self, fn, retail, ctx):
        self.fn = fn
        self.retail = retail
        self.ctx = ctx      # shared: escaped objects etc.
        self.side = fn.side

    # registers
    def getr(self, st, name):
        if name in st.r or name in REG32:
            return st.r.get(name, ('undef', self.side, name))
        if name in SUB:
            full, byte, size = SUB[name]
            v = st.r.get(full, ('undef', self.side, full))
            if size == 1 and byte == 0:
                return lo8(v)
            if size == 1:
                if isinstance(v, int):
                    return (v >> 8) & 0xff
                if isinstance(v, tuple) and v[0] == 'sethi8':
                    return v[2]
                return ('hi8', v)
            if isinstance(v, int):
                return v & 0xffff
            return ('lo16', v)
        if name.startswith('xmm'):
            return st.x.get(name, ('undef', self.side, name))
        raise Unhandled('reg ' + name)

    def setr(self, st, name, v):
        if name in REG32:
            st.r[name] = v
            return
        if name in SUB:
            full, byte, size = SUB[name]
            old = st.r.get(full, ('undef', self.side, full))
            if size == 1 and byte == 0:
                st.r[full] = setlo8(old, v)
            elif size == 1:
                st.r[full] = ('sethi8', old, v)
            else:
                st.r[full] = ('setlo16', old, v)
            return
        if name.startswith('xmm'):
            st.x[name] = v
            return
        raise Unhandled('reg ' + name)

    def addr(self, st, o):
        _, base, index, scale, disp, size, sym, seg = o
        a = disp if not sym else eadd(('sym', sym), disp)
        if base:
            a = eadd(a, self.getr(st, base))
        if index:
            a = eadd(a, escale(self.getr(st, index), scale))
        if seg == 'fs':
            a = ('fs', a)
        return a

    def stack_base(self, a):
        """(base, const) when a is a stack address plus other terms"""
        c, t = as_lin(a)
        bs = [k for k, v in t.items() if isinstance(k, tuple) and k[0] == 'base' and v == 1]
        if len(bs) == 1:
            return (bs[0][1], sgn(c)), len(t) > 1
        return None, False

    def in_memory(self, a):
        sb, computed = self.stack_base(a)
        if sb is None:
            return True
        if computed:
            self.ctx.add_escape(self.side, sb, 'computed')
            return True
        if sb[0] == 'S0' and sb[1] >= 0:
            return False
        if self.ctx.cookie.get(self.side) == sb:
            return False
        return self.ctx.escaped_obj(self.side, *sb) is not None

    def load(self, st, a, size):
        sl = slot_of(a)
        if sl and not self.in_memory(a):
            return self.stk_load(st, sl, size)
        if isinstance(a, tuple) and a[0] == 'fs':
            return ('fsld', a, size)
        c0, t0 = as_lin(a)
        if len(t0) == 1:
            (k0, v0), = t0.items()
            if v0 == 1 and isinstance(k0, tuple) and k0[0] == 'sym' and k0[1].startswith(('__real@', '__xmm@')):
                raw = bytes.fromhex(k0[1].split('@')[1])[::-1]
                return kval(raw[sgn(c0):sgn(c0) + size])
            if v0 == 1 and c0 == 0 and isinstance(k0, tuple) and k0[0] == 'sym' and k0[1].startswith('__imp_') and size == 4:
                return ('imp', k0[1])
            mm = UNNAMED.match(k0[1]) if isinstance(k0, tuple) and k0[0] == 'sym' else None
            if v0 == 1 and mm and self.side == 'L' and 0x1013A000 <= int(mm.group(1), 16) < 0x10189B52 and k0[1].startswith('DAT_'):
                return kval(self.retail.read(int(mm.group(1), 16) + sgn(c0), size))
            if v0 == 1 and isinstance(k0, tuple) and k0[0] == 'sym' and k0[1].startswith(('??_C@', '__mask@', 'STR:')):
                return ('k', k0[1], sgn(c0), size)
        if isinstance(a, int) and self.side == 'L' and 0x1013A000 <= a < 0x10189B52:
            # retail .rdata constant without a relocation in the delinked object
            return kval(self.retail.read(a, size))
        # forward from stores in this superblock
        deps = []
        for e in reversed(st.eff):
            if e[0] == 'call':
                break
            if e[0] == 'st':
                if e[1] == a and e[2] == size:
                    st.loads.append((a, size, 'fwd'))
                    return e[3]
                if may_alias(e[1], e[2], a, size):
                    deps.append(('dep', e[1], e[2]))
        st.loads.append((a, size, st.epoch))
        return ('ld', size, a, st.epoch, tuple(deps))

    def stk_load(self, st, sl, size):
        b, off = sl
        if (b, off) in st.stk:
            s2, v = st.stk[(b, off)]
            if s2 == size:
                return v
            if s2 > size:
                return ('part', 0, size, v)
        for (b2, o2), (s2, v) in st.stk.items():
            if b2 == b and o2 <= off and off + size <= o2 + s2 and (o2, s2) != (off, size):
                return ('part', off - o2, size, v)
        if b == 'S0' and off == 0 and size == 4:
            return ('retaddr',)
        if b == 'S0' and off >= 4:
            return ('arg', off, size)
        return ('uninit', self.side, b, off, size)

    def store(self, st, a, size, v):
        sl = slot_of(a)
        cva = self.ctx.cookie_va
        if sl and cva is not None and isinstance(v, tuple) and v[0] == 'xor' and any(
                isinstance(x, tuple) and x[:1] == ('ld',) and x[2] == cva for x in v[1:]):
            if self.ctx.cookie.get(self.side) != sl:
                self.ctx.cookie[self.side] = sl
                self.ctx.changed = True
                self.ctx.resize()
        mem = (not sl) or self.in_memory(a)
        if mem and not isinstance(v, int):
            vb, _ = self.stack_base(v)
            if vb is not None and not (vb[0] == 'S0' and vb[1] >= 0):
                self.ctx.add_escape(self.side, vb, 'stored')
        if not mem:
            b, off = sl
            for k in list(st.stk):
                b2, o2 = k
                s2 = st.stk[k][0]
                if b2 == b and o2 < off + size and off < o2 + s2:
                    del st.stk[k]
            st.stk[(b, off)] = (size, v)
            st.written.add((b, off))
            return
        st.eff.append(('st', a, size, v))

    def val(self, st, o, size=None):
        t = o[0]
        if t == 'reg':
            return self.getr(st, o[1])
        if t == 'imm':
            if o[3]:
                return eadd(('sym', o[3]), o[1])
            return o[1]
        if t == 'mem':
            return self.load(st, self.addr(st, o), o[5])
        raise Unhandled(t)

    def put(self, st, o, v):
        if o[0] == 'reg':
            self.setr(st, o[1], v)
        elif o[0] == 'mem':
            self.store(st, self.addr(st, o), o[5], v)
        else:
            raise Unhandled('put imm')

    def push(self, st, v, size=4):
        st.r['esp'] = eadd(st.r['esp'], -size)
        self.store(st, st.r['esp'], size, v)

    def pop(self, st, size=4):
        v = self.load(st, st.r['esp'], size)
        st.r['esp'] = eadd(st.r['esp'], size)
        return v

    # x87
    def fpush(self, st, v):
        st.fp.append(v)

    def fst(self, st, i=0):
        if len(st.fp) > i:
            return st.fp[-1 - i]
        return ('fpundef', self.side, i - len(st.fp))

    def fpop(self, st):
        if st.fp:
            return st.fp.pop()
        return ('fpundef', self.side, 0)

    def fset(self, st, i, v):
        while len(st.fp) <= i:
            st.fp.insert(0, ('fpundef', self.side, 99))
        st.fp[-1 - i] = v

    def call(self, st, n):
        # target
        if n.tsym:
            tgt = ('sym', n.tsym)
            tname = n.treal or n.tsym
        else:
            o = n.ops[0]
            tgt = self.val(st, o)
            tname = None
            if o[0] == 'mem' and o[6] and o[6].startswith('__imp_'):
                tname = o[6]
                tgt = ('sym', tname)
            elif isinstance(tgt, tuple) and tgt[0] == 'imp':
                tname = tgt[1]
                tgt = ('sym', tname)
        esp = st.r['esp']
        pending = 0
        while True:
            sl = slot_of(eadd(esp, 4 * pending))
            if not sl or sl not in st.written:
                break
            v = st.stk.get(sl, (0, None))[1]
            if isinstance(v, tuple) and v[:1] == ('init',) and v[1] in ('ebx', 'esi', 'edi', 'ebp'):
                break
            pending += 1
        cc = mangled_cc(tname) if tname else None
        pop = None
        nargs = None
        if tname:
            pop = self.retail.pop_bytes(tname)
            if pop is None and n.tsym and n.tsym.startswith('F:'):
                pop = self.retail.pop_bytes('FUN_' + n.tsym[2:])
            if pop is not None and pop > 0:
                nargs = pop // 4
                if cc not in ('stdcall', 'thiscall'):
                    cc = 'thiscall' if tname.startswith('?') else 'stdcall'
            elif pop == 0:
                cc = cc if cc == 'thiscall' else 'cdecl'
            elif cc == 'cdecl':
                pop = 0
        this = None
        vt = False
        if tname is None:
            # vtable call: target = ld(ld(ecx)+k)
            ecx = st.r.get('ecx')
            if isinstance(tgt, tuple) and tgt[0] == 'ld':
                inner = as_lin(tgt[2])
                for t in inner[1]:
                    if isinstance(t, tuple) and t[0] == 'ld' and strip_epoch(t[2]) == strip_epoch(ecx):
                        vt = True
            symaddr = isinstance(tgt, tuple) and tgt[0] == 'ld' and all(
                isinstance(k, tuple) and k[0] == 'sym' for k in as_lin(tgt[2])[1])
            gaddr = as_lin(tgt[2])[0] if isinstance(tgt, tuple) and tgt[0] == 'ld' and not as_lin(tgt[2])[1] else None
            if vt:
                cc = 'thiscall'
                pop = pending * 4
            elif gaddr is not None and any(lo <= gaddr < hi for lo, hi in STDCALL_RANGES):
                cc = 'stdcall'
                pop = pending * 4
            elif PTR_STDCALL and isinstance(tgt, tuple) and tgt[0] == 'ld' and not symaddr:
                cc = 'stdcall'
                pop = pending * 4
                st.warn.append('%x: pointer call, stdcall assumed (--ptr-stdcall)' % n.addr)
            else:
                pop = 0
                st.warn.append('%x: indirect call, cdecl assumed' % n.addr)
        if cc == 'thiscall' or (cc is None and tname):
            this = st.r.get('ecx')
        if pop is None:
            pop = pending * 4 if cc in ('stdcall', 'thiscall') else 0
            st.warn.append('%x: pop of %s unknown, %d assumed' % (n.addr, tname, pop))
        cnt = nargs if nargs is not None else pending
        args = []
        for k in range(cnt):
            args.append(self.load(st, eadd(esp, 4 * k), 4))
        extra = []
        if tname and ('libm_sse2' in tname or tname.startswith('__libm')):
            extra.append(('xmm0', st.x.get('xmm0')))
            if any(s in tname for s in ('pow', 'atan2', 'fmod')):
                extra.append(('xmm1', st.x.get('xmm1')))
        if tname and (tname.startswith('__ftol') or tname.startswith('__CI') or tname.startswith('__dtol')
                      or tname.startswith('__ftoui') or tname.startswith('__dtoui')):
            extra.append(('st0', self.fst(st)))
            if tname.startswith('__ftol') or tname.startswith('__dtol') or tname.startswith('__ftoui') or tname.startswith('__dtoui'):
                if 'sse' not in tname:
                    self.fpop(st)
        snaps = []
        for v in list(args) + [this]:
            sl = slot_of(v) if v is not None else None
            snap = {}
            if sl and not (sl[0] == 'S0' and sl[1] >= 0):
                for (b2, o2), (sz2, v2) in st.stk.items():
                    if b2 == sl[0] and 0 <= o2 - sl[1] < 0x40:
                        snap[o2 - sl[1]] = (sz2, v2)
            snaps.append(snap)
        idx = st.ncall
        st.eff.append(('call', tgt, tuple(args), this, tuple(extra), n.addr, cnt, pending, snaps))
        st.ncall += 1
        tag = (st.epoch, idx)
        st.epoch = ('C', tag)   # the callee may write any memory
        st.r['eax'] = ('ret', tag, 'eax')
        st.r['ecx'] = ('clob', tag, 'ecx')
        st.r['edx'] = ('ret', tag, 'edx')
        for k in range(8):
            st.x['xmm%d' % k] = ('clob', tag, 'xmm%d' % k)
        st.x['xmm0'] = ('ret', tag, 'xmm0')
        st.flags = None
        if tname and (tname.startswith('__CI')):
            if st.fp:
                st.fp.pop()
        st.fp = [('ret', tag, 'st0')] if not (tname and tname.startswith('__ftol') and 'sse' in tname) else st.fp
        if tname and tname.startswith('__ftol') and 'sse' in tname:
            pass
        st.r['esp'] = eadd(esp, pop)
        self.ctx.note_escapes(self.side, args, this, st)
        st.floor = st.r['esp']
        st.written = set()

    def run(self, st, addr, maxsteps=6000):
        """execute from addr to a terminator; returns (kind, data)"""
        seen = set()
        steps = 0
        while True:
            if addr not in self.fn.ins:
                raise Unhandled('no insn at %x' % addr)
            if addr in seen:
                return ('loop', addr)
            seen.add(addr)
            steps += 1
            if steps > maxsteps:
                raise Unhandled('too many steps')
            n = self.fn.ins[addr]
            r = self.step(st, n)
            if r is None:
                addr = n.next
                continue
            if r[0] == 'goto':
                addr = r[1]
                continue
            return r

    def step(self, st, n):
        mn = n.mn
        ops = n.ops
        g = lambda i: self.val(st, ops[i])
        if mn in ('nop', 'int3', 'fwait', 'wait', 'cld'):
            return None
        if mn == 'mov':
            self.put(st, ops[0], g(1)); return None
        if mn in ('movzx', 'movsx'):
            v = g(1)
            sz = ops[1][2] if ops[1][0] == 'reg' else ops[1][5]
            if sz == 1:
                v = zx8(v) if mn == 'movzx' else ('sx8', v)
            else:
                v = ('zx16', v) if mn == 'movzx' else ('sx16', v)
            self.put(st, ops[0], v); return None
        if mn == 'lea':
            self.put(st, ops[0], self.addr(st, ops[1])); return None
        if mn == 'push':
            if ops[0][0] == 'imm' and ops[0][1] & 0x80000000 == 0 and ops[0][2] == 1:
                pass
            v = g(0)
            self.push(st, v)
            if isinstance(v, tuple) and v[0] == 'init':
                st.floor = st.r['esp']
            return None
        if mn == 'pop':
            v = self.pop(st)
            self.put(st, ops[0], v); return None
        if mn == 'xchg':
            a, b = g(0), g(1)
            self.put(st, ops[0], b); self.put(st, ops[1], a); return None
        if mn in ('add', 'sub'):
            a, b = g(0), g(1)
            r = eadd(a, b) if mn == 'add' else esub(a, b)
            self.put(st, ops[0], r)
            st.flags = ('cmp', a, b) if mn == 'sub' else ('res', r)
            if ops[0][0] == 'reg' and ops[0][1] == 'esp':
                if mn == 'add':
                    st.floor = st.r['esp']
                else:
                    nx = self.fn.ins.get(n.next)
                    argspace = nx is not None and nx.ops and nx.ops[0][0] == 'mem' and nx.ops[0][1] == 'esp' and nx.mn != 'push'
                    if not argspace:
                        st.floor = st.r['esp']
            return None
        if mn in ('inc', 'dec'):
            a = g(0)
            r = eadd(a, 1 if mn == 'inc' else -1)
            self.put(st, ops[0], r); st.flags = ('res', r); return None
        if mn == 'neg':
            a = g(0)
            r = escale(a, -1)
            self.put(st, ops[0], r); st.flags = ('neg', a); return None
        if mn == 'not':
            self.put(st, ops[0], op('not', g(0))); return None
        if mn in ('and', 'or', 'xor'):
            a, b = g(0), g(1)
            if mn == 'xor' and ops[0] == ops[1]:
                r = 0
            elif mn == 'and' and ops[0][0] == 'reg' and ops[0][1] == 'esp':
                r = ('base', ('A', repr(a), b))
                st.floor = None
            else:
                r = op(mn, a, b)
            self.put(st, ops[0], r)
            st.flags = ('res', r)
            return None
        if mn in ('shl', 'sal', 'shr', 'sar', 'rol', 'ror'):
            a = g(0)
            b = g(1) if len(ops) > 1 else 1
            if mn in ('shl', 'sal') and isinstance(b, int):
                r = escale(a, 1 << b)
            else:
                r = op(mn, a, b)
            self.put(st, ops[0], r); st.flags = ('res', r); return None
        if mn == 'imul':
            if len(ops) == 1:
                a = self.getr(st, 'eax'); b = g(0)
                self.setr(st, 'eax', op('imul', a, b)); self.setr(st, 'edx', op('imulhi', a, b)); return None
            if len(ops) == 2:
                a, b = g(0), g(1)
            else:
                a, b = g(1), g(2)
            if isinstance(b, int):
                r = escale(a, b)
            elif isinstance(a, int):
                r = escale(b, a)
            else:
                r = op('imul', a, b)
            self.put(st, ops[0], r); st.flags = ('res', r); return None
        if mn in ('mul',):
            a = self.getr(st, 'eax'); b = g(0)
            self.setr(st, 'eax', op('umul', a, b)); self.setr(st, 'edx', op('umulhi', a, b)); return None
        if mn in ('idiv', 'div'):
            a = self.getr(st, 'eax'); d = self.getr(st, 'edx'); b = g(0)
            self.setr(st, 'eax', (mn, 'q', d, a, b)); self.setr(st, 'edx', (mn, 'r', d, a, b)); return None
        if mn == 'cdq':
            self.setr(st, 'edx', ('sext_hi', self.getr(st, 'eax'))); return None
        if mn in ('cmp',):
            st.flags = ('cmp', g(0), g(1)); return None
        if mn == 'test':
            st.flags = ('test',) + tuple(sorted((g(0), g(1)), key=repr)); return None
        if mn in ('sbb', 'adc'):
            a, b = g(0), g(1)
            if mn == 'sbb' and ops[0] == ops[1]:
                self.put(st, ops[0], ('sbbmask', make_cond('b', st.flags))); st.flags = None; return None
            self.put(st, ops[0], (mn, a, b, st.flags)); st.flags = None; return None
        if mn.startswith('set'):
            self.put(st, ops[0], ('set', make_cond(mn[3:], st.flags))); return None
        if mn.startswith('cmov'):
            c = make_cond(mn[4:], st.flags)
            a, b = g(0), g(1)
            self.put(st, ops[0], ('ite', c, b, a)); return None
        if mn == 'lahf':
            self.setr(st, 'ah', ('lahf', st.flags)); st.lahf = st.flags; return None
        if mn == 'sahf':
            st.flags = ('sahf', self.getr(st, 'ah')); return None
        if mn == 'fnstsw':
            self.setr(st, 'ax', ('fnstsw', st.fpflags if hasattr(st, 'fpflags') else None)); return None
        if mn == 'call':
            self.call(st, n)
            t = getattr(n, 'treal', None) or n.tsym or (n.ops[0][6] if n.ops and n.ops[0][0] == 'mem' else None) or ''
            if any(x in t for x in NORETURN):
                return ('noreturn', t)
            return None
        if mn in ('ret', 'retn'):
            return ('ret', ops[0][1] if ops else 0)
        if mn == 'leave':
            st.r['esp'] = st.r['ebp']
            st.r['ebp'] = self.pop(st); return None
        if mn == 'jmp':
            if n.target is not None:
                return ('goto', n.target)
            if n.tsym:
                return ('tailcall', n.tsym)
            o = ops[0]
            if o[0] == 'mem' and o[6]:
                return ('switch', o, self.addr(st, o))
            raise Unhandled('indirect jmp at %x' % n.addr)
        if mn.startswith('j'):
            cc = mn[1:]
            if mn in ('jecxz',):
                c = canon_cond('eq', self.getr(st, 'ecx'), 0)
            elif st.flags is not None and st.flags[0] == 'test' and isinstance(st.flags[1], tuple) and st.flags[1][0] == 'lahf':
                pass
            c = self.cond_from_flags(st, cc)
            return ('br', c, n.target, n.next)
        if mn == 'minidiom':
            a, b = g(0), g(1)
            self.put(st, ops[0], ('fmin_ord', b, a))
            st.flags = None
            st.warn.append('%x: comiss/jb/movaps read as minss (b must not be a zero)' % n.addr)
            return None
        # SSE
        r = self.sse(st, n)
        if r is not False:
            return r
        r = self.x87(st, n)
        if r is not False:
            return r
        if mn in ('rep movsd', 'rep stosd', 'rep movsb', 'rep stosb', 'movsd', 'stosd', 'movsb', 'stosb'):
            rep = mn.startswith('rep')
            sz = 4 if mn.endswith('d') else 1
            cnt = self.getr(st, 'ecx') if rep else 1
            if not isinstance(cnt, int) or cnt > 16:
                edi = self.getr(st, 'edi')
                esi = self.getr(st, 'esi')
                nbytes = escale(cnt, sz)
                if 'movs' in mn:
                    deps = tuple(('dep', e[1], e[2]) for e in st.eff if e[0] == 'st' and may_alias(e[1], e[2], esi, None))
                    self.in_memory(esi)
                    st.eff.append(('st', edi, ('n', nbytes), ('blk', esi, nbytes, st.epoch, deps)))
                    self.setr(st, 'esi', eadd(esi, nbytes))
                else:
                    st.eff.append(('st', edi, ('n', nbytes), ('fill', self.getr(st, 'eax') if sz == 4 else self.getr(st, 'al'))))
                self.in_memory(edi)
                self.setr(st, 'edi', eadd(edi, nbytes))
                self.setr(st, 'ecx', 0)
                return None
            edi = self.getr(st, 'edi')
            esi = self.getr(st, 'esi')
            for k in range(cnt):
                if 'movs' in mn:
                    v = self.load(st, eadd(esi, sz * k), sz)
                else:
                    v = self.getr(st, 'eax') if sz == 4 else self.getr(st, 'al')
                self.store(st, eadd(edi, sz * k), sz, v)
            self.setr(st, 'edi', eadd(edi, sz * cnt))
            if 'movs' in mn:
                self.setr(st, 'esi', eadd(esi, sz * cnt))
            if rep:
                self.setr(st, 'ecx', 0)
            return None
        raise Unhandled('%x: %s %s' % (n.addr, mn, n.ops_str))

    def cond_from_flags(self, st, cc):
        f = st.flags
        if f and f[0] == 'test':
            a, b = f[1], f[2]
            for x, y in ((a, b), (b, a)):
                if isinstance(x, tuple) and x[0] == 'hi8' and isinstance(x[1], tuple) and x[1][0] == 'setlo8':
                    pass
                if isinstance(y, int) and isinstance(x, tuple) and x[0] in ('hi8',) and isinstance(x[1], tuple):
                    pass
                if isinstance(y, int) and isinstance(x, tuple) and x[0] == 'lahf':
                    return make_cond(cc, ('lahf', y, x[1]))
                if isinstance(y, int) and isinstance(x, tuple) and x[0] == 'hi8' and isinstance(x[1], tuple) and x[1][0] == 'sethi8' and isinstance(x[1][2], tuple) and x[1][2][0] == 'lahf':
                    return make_cond(cc, ('lahf', y, x[1][2][1]))
                if isinstance(y, int) and isinstance(x, tuple) and x[0] == 'hi8' and isinstance(x[1], tuple) and x[1][0] == 'setlo16':
                    pass
        return make_cond(cc, f)

    def sse(self, st, n):
        mn, ops = n.mn, n.ops
        if (mn.endswith('ss') or mn.endswith('sd')) and mn not in ('movsd',) and len(ops) == 2 and ops[1][0] == 'mem':
            want = 4 if mn.endswith('ss') else 8
            if mn in ('cvtss2sd', 'cvtss2si', 'cvttss2si'):
                want = 4
            if mn in ('cvtsd2ss', 'cvtsd2si', 'cvttsd2si'):
                want = 8
            if ops[1][5] != want:
                ops = [ops[0], ops[1][:5] + (want,) + ops[1][6:]]
        g = lambda i: self.val(st, ops[i])
        if mn in ('movss', 'movsd', 'movd', 'movq', 'movaps', 'movups', 'movapd', 'movdqa', 'movdqu', 'movlps', 'movhps', 'movlpd'):
            if mn == 'movsd' and (not ops or (ops[0][0] == 'mem' and ops[1][0] == 'mem')):
                return False
            v = g(1)
            if mn in ('movhps',):
                v = ('movhps', v, g(0))
            self.put(st, ops[0], v); return None
        if mn in ('xorps', 'xorpd', 'pxor'):
            if ops[0] == ops[1]:
                self.put(st, ops[0], 0); return None
            a, b = g(0), g(1)
            self.put(st, ops[0], op('fxor', a, b) if not isinstance(b, tuple) or b[0] != 'ld' else ('fneg', a, b)); return None
        if mn in ('andps', 'andpd', 'andnps', 'orps'):
            self.put(st, ops[0], (mn, g(0), g(1))); return None
        fop = {'addss': 'fadd', 'mulss': 'fmul', 'subss': 'fsub', 'divss': 'fdiv', 'minss': 'fmin', 'maxss': 'fmax',
               'addsd': 'dadd', 'mulsd': 'dmul', 'subsd': 'dsub', 'divsd': 'ddiv', 'addps': 'fadd', 'mulps': 'fmul',
               'subps': 'fsub', 'divps': 'fdiv', 'addpd': 'dadd', 'mulpd': 'dmul', 'subpd': 'dsub', 'divpd': 'ddiv',
               'minps': 'fmin', 'maxps': 'fmax', 'minsd': 'dmin', 'maxsd': 'dmax'}
        if mn in fop:
            k = fop[mn]
            if k in ('fmin', 'fmax', 'dmin', 'dmax'):
                self.put(st, ops[0], (k + '_ord', g(0), g(1))); return None
            self.put(st, ops[0], op(k, g(0), g(1))); return None
        cv = {'cvtss2sd': 'f2d', 'cvtps2pd': 'f2d', 'cvtsd2ss': 'd2f', 'cvtpd2ps': 'd2f', 'cvtsi2ss': 'i2f',
              'cvtsi2sd': 'i2d', 'cvttss2si': 'f2i_t', 'cvttsd2si': 'd2i_t', 'cvtss2si': 'f2i', 'cvtsd2si': 'd2i',
              'cvtdq2ps': 'i2f', 'cvttps2dq': 'f2i_t', 'cvtdq2pd': 'i2d', 'sqrtss': 'fsqrt', 'sqrtsd': 'dsqrt',
              'sqrtps': 'fsqrt', 'rsqrtss': 'frsqrt', 'rcpss': 'frcp'}
        if mn in cv:
            v = g(1)
            if isinstance(v, tuple) and v[0] == 'unpck':
                v = v[1]
            self.put(st, ops[0], (cv[mn], v)); return None
        if mn in ('unpcklps', 'unpcklpd'):
            a, b = g(0), g(1)
            self.put(st, ops[0], a if a == b else ('unpck', a, b)); return None
        if mn in ('shufps', 'pshufd', 'movlhps', 'movhlps', 'unpckhps', 'cmpltss', 'cmpless', 'cmpss', 'cmpeqss',
                  'cmpnless', 'cmpnltss', 'shufpd'):
            self.put(st, ops[0], (mn,) + tuple(g(i) for i in range(len(ops)))); return None
        if mn in ('ucomiss', 'comiss', 'ucomisd', 'comisd'):
            st.flags = ('fcmp', 'u' if mn.startswith('u') else 's', g(0), g(1)); return None
        return False

    def x87(self, st, n):
        mn, ops = n.mn, n.ops
        if not mn.startswith('f'):
            return False
        def src(i):
            o = ops[i]
            if o[0] == 'reg' and o[1].startswith('st'):
                k = int(o[1][3]) if '(' in o[1] else 0
                return self.fst(st, k)
            v = self.val(st, o)
            return v
        def stidx(o):
            if o[0] == 'reg' and o[1].startswith('st'):
                return int(o[1][3]) if '(' in o[1] else 0
            return None
        if mn in ('fld',):
            v = src(0)
            if ops[0][0] == 'mem':
                v = ('x87ld', ops[0][5], v)
            self.fpush(st, v); return None
        if mn == 'fild':
            self.fpush(st, ('fild', ops[0][5], src(0))); return None
        if mn in ('fld1', 'fldz'):
            self.fpush(st, mn); return None
        if mn in ('fstp', 'fst', 'fistp', 'fist', 'fisttp'):
            v = self.fst(st)
            o = ops[0]
            k = stidx(o)
            if k is not None:
                self.fset(st, k, v)
            else:
                tag = 'x87st' if mn in ('fstp', 'fst') else 'x87ist'
                self.put(st, o, (tag, o[5], v))
            if mn.endswith('p'):
                self.fpop(st)
            return None
        if mn == 'fxch':
            k = stidx(ops[0]) if ops else 1
            a, b = self.fst(st), self.fst(st, k)
            self.fset(st, 0, b); self.fset(st, k, a); return None
        if mn in ('fchs', 'fabs', 'fsqrt', 'fsin', 'fcos', 'frndint'):
            self.fset(st, 0, (mn, self.fst(st))); return None
        base = {'fadd': 'fadd', 'faddp': 'fadd', 'fmul': 'fmul', 'fmulp': 'fmul', 'fsub': 'fsub', 'fsubp': 'fsub',
                'fsubr': 'fsubr', 'fsubrp': 'fsubr', 'fdiv': 'fdiv', 'fdivp': 'fdiv', 'fdivr': 'fdivr',
                'fdivrp': 'fdivr', 'fiadd': 'fadd', 'fimul': 'fmul', 'fisub': 'fsub', 'fidiv': 'fdiv'}
        if mn in base:
            k = base[mn]
            def comb(a, b, k):
                if k == 'fsubr':
                    return ('xsub', b, a)
                if k == 'fdivr':
                    return ('xdiv', b, a)
                if k in ('fadd', 'fmul'):
                    return op('x' + k[1:], a, b)
                return ('x' + k[1:], a, b)
            if len(ops) == 1 and ops[0][0] == 'mem':
                self.fset(st, 0, comb(self.fst(st), ('x87ld', ops[0][5], src(0)), k)); return None
            if len(ops) == 2:
                i, j = stidx(ops[0]), stidx(ops[1])
                r = comb(self.fst(st, i), self.fst(st, j), k)
                self.fset(st, i, r)
                if mn.endswith('p'):
                    self.fpop(st)
                return None
            if len(ops) == 0:
                r = comb(self.fst(st, 1), self.fst(st, 0), k)
                self.fset(st, 1, r); self.fpop(st); return None
        if mn in ('fcomp', 'fcom', 'fucomp', 'fucom', 'fcompp', 'fucompp', 'fcomip', 'fucomip', 'fcomi', 'fucomi'):
            a = self.fst(st)
            b = src(len(ops) - 1) if ops else self.fst(st, 1)
            if ops and ops[-1][0] == 'mem':
                b = ('x87ld', ops[-1][5], b)
            f = ('fcmp', 'u' if 'u' in mn else 's', a, b)
            if mn in ('fcomip', 'fucomip', 'fcomi', 'fucomi'):
                st.flags = f
            else:
                st.fpflags = f
            npop = 2 if mn.endswith('pp') else (1 if mn.endswith('p') else 0)
            for _ in range(npop):
                self.fpop(st)
            return None
        if mn == 'fnstsw':
            self.setr(st, 'ax', ('fnstsw', getattr(st, 'fpflags', None))); return None
        return False


def kval(raw):
    if len(raw) <= 4:
        return int.from_bytes(raw, 'little')
    return ('kv', len(raw), raw.hex())


def strip_epoch(e):
    if isinstance(e, tuple) and e:
        if e[0] == 'ld':
            return ('ld', e[1], strip_epoch(e[2]))
        return tuple(strip_epoch(x) for x in e)
    return e


def esp_diff(a, b):
    ca, ta = as_lin(a)
    cb, tb = as_lin(b)
    if ta != tb:
        return None
    return (ca - cb) & M32


def may_alias(a1, s1, a2, s2):
    c1, t1 = as_lin(a1)
    c2, t2 = as_lin(a2)
    if not isinstance(s1, int) or not isinstance(s2, int):
        s1 = s2 = 1 << 30
    if t1 == t2:
        d = sgn(c2 - c1)
        return not (d >= s1 or -d >= s2)
    def kind(t):
        if any(isinstance(k, tuple) and k[0] == 'base' for k in t):
            return 'stack'
        if t and all(isinstance(k, tuple) and k[0] == 'sym' for k in t):
            return 'global'
        return 'other'
    k1, k2 = kind(t1), kind(t2)
    if (k1 == 'stack') != (k2 == 'stack'):
        # a frame object never aliases memory reached through a pointer
        # loaded from memory or returned by a call (valid C)
        return False
    # two different globals
    if all(isinstance(k, tuple) and k[0] == 'sym' for k in t1) and all(isinstance(k, tuple) and k[0] == 'sym' for k in t2) and t1 and t2:
        return False
    return True


# ---------------------------------------------------------------- lockstep walk
class Ctx:
    def __init__(self):
        self.changed = False
        self.weakpair = set()
        self.cookie = {}
        self.cookie_va = None
        self.strong = {'L': set(), 'R': set()}
        self.weak = {'L': set(), 'R': set()}
        self.esc = {'L': {}, 'R': {}}   # side -> {(base, off): size_guess}
        self.pairs = {}                 # ('L', base, off) -> pid
        self.conflicts = []

    def add_escape(self, side, sl, why):
        if sl[0] == 'S0' and sl[1] >= 0:
            return
        strong = why == 'arg'
        for (doff, dsz) in DECLARED.get(side, []):
            if doff <= sl[1] < doff + dsz:
                sl = (sl[0], doff)
                strong = True
                break
        cur = self.esc[side]
        if sl in self.strong[side] or (not strong and sl in self.weak[side]):
            return
        if strong:
            self.strong[side].add(sl)
        else:
            self.weak[side].add(sl)
        self.changed = True
        self.resize()

    def resize(self):
        for side2 in ('L', 'R'):
            st_ = sorted(self.strong[side2])
            objs = {}
            for k in st_:
                objs[k] = True
            # weak bases inside a strong object's range are not objects
            for k in sorted(self.weak[side2]):
                inside = False
                for b in st_:
                    if b[0] == k[0] and b[1] <= k[1] < b[1] + 0x40:
                        nxt = [x for x in st_ if x[0] == b[0] and x[1] > b[1]]
                        lim = b[1] + 0x40 if not nxt else min(b[1] + 0x40, nxt[0][1])
                        if k[1] < lim:
                            inside = True
                if not inside:
                    # also not inside an earlier weak object
                    for b in list(objs):
                        if b[0] == k[0] and b[1] < k[1] < b[1] + 0x40 and not objs[b]:
                            inside = True
                    if not inside:
                        objs[k] = False
            decl = DECLARED.get(side2, [])
            if decl:
                for k in list(objs):
                    for (doff, dsz) in decl:
                        if doff < k[1] < doff + dsz or (doff <= k[1] < doff + dsz and k[1] != doff):
                            del objs[k]
                            break
            ks = sorted(objs)
            res = {}
            for i, k in enumerate(ks):
                nxt = [k2 for k2 in ks[i + 1:] if k2[0] == k[0]]
                dd = [dsz for (doff, dsz) in decl if doff == k[1]]
                if dd:
                    res[k] = dd[0]
                    continue
                lim = OBJSIZE.get((side2,) + k, OBJCAP)
                if nxt:
                    lim = min(lim, nxt[0][1] - k[1])
                if k[0] == 'S0' and k[1] < 0:
                    lim = min(lim, max(4, -k[1] - 4))
                elif k[0] != 'S0' and k[1] < 0:
                    # aligned frame: the saved ebp sits at the aligned base
                    lim = min(lim, max(4, -k[1]))
                ck = self.cookie.get(side2)
                if ck and ck[0] == k[0] and ck[1] > k[1]:
                    lim = min(lim, ck[1] - k[1])
                res[k] = lim
            self.esc[side2] = res

    def escaped_obj(self, side, b, off):
        best = None
        for (b2, o2), sz in self.esc[side].items():
            if b2 == b and o2 <= off < o2 + sz:
                if best is None or o2 > best[1]:
                    best = (b2, o2)
        return best

    def note_escapes(self, side, args, this, st):
        vals = list(args) + ([this] if this is not None else [])
        for v in vals:
            sl = slot_of(v) if v is not None else None
            if sl and not (sl[0] == 'S0' and sl[1] >= 0):
                self.add_escape(side, sl, 'arg')


OKLEAF = ('sym', 'base', 'k', 'kv', 'init', 'arg', 'imp', 'retaddr')


def derive(c, shared, defs_of=None, depth=0):
    """c rewritten with ('cref', x) for every maximal subterm x in shared;
    None when a side specific leaf remains"""
    if c in shared:
        return ('cref', c)
    if isinstance(c, tuple) and c and c[0] == 'v' and defs_of and depth < 8:
        d = defs_of(c)
        if d is not None:
            return derive(d, shared, defs_of, depth + 1)
        return None
    if isinstance(c, (int, str)) or c is None:
        return c
    if not c:
        return c
    if c[0] in OKLEAF:
        return c
    if c[0] in ('v', 'only', 'ret', 'clob', 'uninit', 'esc', 'undef', 'fpundef', 'objaddr', 'stkaddr', 'side', 'lost'):
        return None
    out = []
    for x in c:
        if isinstance(x, tuple):
            d = derive(x, shared, defs_of, depth)
            if d is None:
                return None
            out.append(d)
        else:
            out.append(x)
    return tuple(out)


def zip_der(a, b):
    """same shape with class references (x, y) -> ('cref', (('cls', x), ('cls', y)))"""
    if isinstance(a, tuple) and a and a[0] == 'cref':
        if isinstance(b, tuple) and b and b[0] == 'cref':
            return ('cref', (('cls', a[1]), ('cls', b[1])))
        return None
    if type(a) != type(b):
        return None
    if isinstance(a, tuple):
        if len(a) != len(b):
            return None
        out = []
        for x, y in zip(a, b):
            if isinstance(x, tuple) or isinstance(y, tuple):
                z = zip_der(x, y)
                if z is None:
                    return None
                out.append(z)
            elif x == y:
                out.append(x)
            else:
                return None
        return tuple(out)
    return a if a == b else None


def remap_der(e, m, both):
    if isinstance(e, tuple) and e and e[0] == 'cref':
        k = e[1]
        if k not in m or m[k] not in both:
            return None
        return ('cref', m[k])
    if isinstance(e, tuple):
        out = []
        for x in e:
            if isinstance(x, tuple):
                y = remap_der(x, m, both)
                if y is None:
                    return None
                out.append(y)
            else:
                out.append(x)
        return tuple(out)
    return e


def subst_der(e, key):
    if isinstance(e, tuple) and e and e[0] == 'cref':
        return ('v', key, e[1])
    if isinstance(e, tuple):
        return tuple(subst_der(x, key) if isinstance(x, tuple) else x for x in e)
    return e


def resubst(e, f):
    """rebuild e with every ('cref', y) replaced by f(y), renormalized"""
    if isinstance(e, tuple) and e and e[0] == 'cref':
        return f(e[1])
    if isinstance(e, tuple) and e and e[0] == 'lin':
        r = e[1]
        for t, k in e[2]:
            r = eadd(r, escale(resubst(t, f), k))
        return r
    if isinstance(e, tuple) and e:
        args = tuple(resubst(x, f) if isinstance(x, tuple) else x for x in e[1:])
        if isinstance(e[0], str) and e[0] in COMM:
            return op(e[0], *args)
        return (e[0],) + args
    return e


def crefs_of(e, out):
    if isinstance(e, tuple) and e and e[0] == 'cref':
        if e[1] not in out:
            out.append(e[1])
    elif isinstance(e, tuple):
        for x in e:
            crefs_of(x, out)
    return out


def comp_expr(c, tag, defs):
    """value of one side component of a merged class key, as an expression"""
    if c[0] == 'cv':
        return c[1]
    if c[0] == 'cls':
        return ('cref', (tag, c[1]))
    if c[0] == 'der':
        return resubst(c[1], lambda y: ('cref', (tag, y)))
    return ('cref', (tag, 'opaque', c))


def generalize(src, other, by_side, side, olddefs, newdefs):
    """src is a value of one edge set ('der' structure over that side's
    classes); other the value of the same location on the other edge set.
    Find merged classes for the references of src such that src evaluated
    with the other edge set's components equals other. side: 1 when src is
    the new edge (references are new class keys), 0 when old."""
    import itertools
    if src[0] == 'der':
        struct = src[1]
    elif src[0] == 'cls':
        struct = ('cref', src[1])
    else:
        return None
    otag = 'o'
    target = comp_expr(other, otag, None)
    refs = crefs_of(struct, [])
    choices = []
    for y in refs:
        c = by_side.get(('cls', y), [])
        if not c:
            return None
        choices.append(c)
    n = 0
    for combo in itertools.product(*choices):
        n += 1
        if n > 64:
            break
        mp = dict(zip(refs, combo))
        val = resubst(struct, lambda y: comp_expr(mp[y][1 - side], otag, None))
        if val == target:
            return resubst(struct, lambda y: ('cref', mp[y]))
        # the other component may be a class with a definition
        if other[0] == 'cls':
            defs = olddefs if side == 1 else newdefs
            if other[1] in defs:
                t2 = resubst(defs[other[1]], lambda y: ('cref', (otag, y)))
                if val == t2:
                    return resubst(struct, lambda y: ('cref', mp[y]))
    return None


class Checker:
    def __init__(self, unit, symbol, retkind=None, verbose=False):
        self.unit, self.symbol, self.verbose = unit, symbol, verbose
        d = objdiff_tool.diff_json(unit, symbol)
        l, r, li, ri = objdiff_tool.rows(d, symbol)
        self.pct = (l or {}).get('match_percent')
        self.alias, self.alias_notes = build_alias(d, li, ri)
        def _ad(x):
            i = (x or {}).get('instruction') or {}
            return int(i['address']) if 'address' in i else None
        self.rows = [(_ad(a), _ad(b)) for a, b in zip(li, ri)]
        u = [x for x in json.load(open(os.path.join(ROOT, 'objdiff.json')))['units'] if x['name'] == unit][0]
        self.retail = Retail()
        global RET
        RET = self.retail
        self.L = Func(Coff(os.path.join(ROOT, u['target_path'])), symbol, int(l['size']), 'L', {})
        self.R = Func(Coff(os.path.join(ROOT, u['base_path'])), symbol, int(r['size']), 'R', {})
        self.rbase = self.r_bases()
        self.strnames()
        if IDIOMS:
            self.L.idioms()
            self.R.idioms()
        self.retkind = retkind or ret_kind(symbol)
        self.ctx = Ctx()
        self.ctx.cookie_va = self.retail.names.get('___security_cookie')
        for nm, sz in STDCALL_NAMES:
            va = self.retail.names.get(nm)
            if va is not None:
                STDCALL_RANGES.append((va, va + sz))
        self.problems = {}
        self.warnings = {}
        self.switch_info = {}

    def strname(self, name):
        if not name:
            return name
        if name.startswith('??_C@'):
            co = self.R.coff
            loc = co.byname.get(name)
            if loc is None:
                for i, (n, v, sn, sc) in co.syms.items():
                    if n == name and sn > 0:
                        loc = (sn - 1, v)
                        break
            if loc is None:
                return name
            d = co.secs[loc[0]]['data'][loc[1]:]
            wide = name.startswith('??_C@_1')
            if wide:
                k = 0
                while k + 1 < len(d) and d[k:k + 2] != b'\0\0':
                    k += 2
            else:
                k = d.index(b'\0') if b'\0' in d else len(d)
            return 'STR:' + ('w' if wide else '') + d[:k].hex()
        m = UNNAMED.match(name)
        if m and (name.startswith('s_') or name.startswith('u_')):
            va = int(m.group(1), 16)
            d = self.retail.read(va, 1024)
            if name.startswith('u_'):
                k = 0
                while k + 1 < len(d) and d[k:k + 2] != b'\0\0':
                    k += 2
                return 'STR:w' + d[:k].hex()
            k = d.index(b'\0') if b'\0' in d else len(d)
            return 'STR:' + d[:k].hex()
        return name

    def gname(self, side, name):
        """('va', address) for a global known by its retail address, else
        ('name', canonical name)"""
        if name.startswith(('__real@', '__xmm@', '__imp_')):
            return ('name', name)
        if name.startswith('??_C@'):
            return ('name', self.strname(name))
        m = UNNAMED.match(name)
        if side == 'L' and m and name.startswith(('s_', 'u_')):
            return ('name', self.strname(name))
        if side == 'L' and name.startswith('PTR_') and self.alias.get(name, '').startswith('__imp_'):
            return ('name', self.alias[name])
        if side == 'L' and name.startswith('PTR_') and m:
            core = name[4:-9]
            if not hasattr(self, 'impnames'):
                self.impnames = {}
                for i, (n2, v2, sn2, sc2) in self.R.coff.syms.items():
                    if n2.startswith('__imp_'):
                        c2 = re.sub(r'@\d+$', '', n2[6:]).lstrip('_')
                        self.impnames.setdefault(c2, n2)
            if core in self.impnames:
                return ('name', self.impnames[core])
        if side == 'L' and m and self.alias.get(name, '').startswith('??_C@'):
            return ('name', self.strname(self.alias[name]))
        if side == 'L' and m:
            return ('va', int(m.group(1), 16))
        mi = re.match(r'^IMAGE_DOS_HEADER_([0-9a-fA-F]{8})$', name)
        if side == 'L' and mi:
            return ('va', int(mi.group(1), 16))
        if name in self.retail.names and not name.startswith(('$', 'switchD')):
            return ('va', self.retail.names[name])
        if side == 'L' and name in self.alias:
            return self.gname('R', self.alias[name])
        if side == 'R' and name in getattr(self, 'rbase', {}):
            return ('va', self.rbase[name])
        if side == 'R':
            inv = [a for a, b in self.alias.items() if b == name and UNNAMED.match(a)]
            if len(inv) == 1 and not inv[0].startswith(('s_', 'u_', 'PTR_')):
                return ('va', int(UNNAMED.match(inv[0]).group(1), 16))
        return ('name', name)

    def r_bases(self):
        """retail address of R symbols that the retail map does not name, from
        aligned instructions: L address + L addend - R addend"""
        cand = {}
        for la, ra in self.rows:
            if la is None or ra is None:
                continue
            a, b = self.L.ins.get(la), self.R.ins.get(ra)
            if not a or not b or len(a.ops) != len(b.ops) or a.mn.startswith('j') or a.mn == 'call':
                continue
            for x, y in zip(a.ops, b.ops):
                if x[0] != y[0] or x[0] not in ('imm', 'mem'):
                    continue
                ln, rn = (x[3], y[3]) if x[0] == 'imm' else (x[6], y[6])
                if not ln or not rn or rn in self.retail.names:
                    continue
                m = UNNAMED.match(ln)
                lva = int(m.group(1), 16) if m and not ln.startswith(('s_', 'u_', 'PTR_')) else self.retail.names.get(ln)
                if lva is None:
                    continue
                la_ = x[1] if x[0] == 'imm' else x[4]
                ra_ = y[1] if y[0] == 'imm' else y[4]
                cand.setdefault(rn, []).append((lva + la_ - ra_) & M32)
        out = {}
        self.rbase_notes = []
        for k, v in cand.items():
            best = max(set(v), key=v.count)
            if v.count(best) * 2 > len(v):
                out[k] = best
                if len(set(v)) > 1:
                    self.rbase_notes.append('%s: retail base %08x by %d of %d aligned uses, others %s' % (
                        k, best, v.count(best), len(v), sorted('%08x' % x for x in set(v) - {best})))
        return out

    def strnames(self):
        self.va_notes = set()
        for fn in (self.L, self.R):
            side = fn.side
            for n in fn.ins.values():
                ops = []
                for o in n.ops:
                    if o[0] == 'imm' and o[3]:
                        g = self.gname(side, o[3])
                        if g[0] == 'va':
                            o = ('imm', (g[1] + o[1]) & M32, o[2], None)
                        else:
                            o = o[:3] + (g[1],) + o[4:]
                    elif o[0] == 'mem' and o[6]:
                        g = self.gname(side, o[6])
                        if g[0] == 'va' and not (n.mn == 'jmp'):
                            o = o[:4] + ((g[1] + o[4]) & M32, o[5], None) + o[7:]
                        elif g[0] == 'name':
                            o = o[:6] + (g[1],) + o[7:]
                    ops.append(o)
                n.ops = ops
                if n.tsym:
                    real = n.tsym
                    if side == 'L' and real in self.alias:
                        real = self.alias[real]
                    n.treal = real
                    g = self.gname(side, n.tsym)
                    n.tsym = ('F:%08x' % g[1]) if g[0] == 'va' else g[1]
                else:
                    n.treal = None

    def initial(self, side):
        st = State()
        for r in REG32:
            st.r[r] = ('init', r)
        st.r['esp'] = ('base', 'S0')
        st.epoch = ('M', 'entry', 0)
        st.floor = st.r['esp']
        return st

    def canon(self, side, e):
        """replace side specific stack object names by pair ids"""
        if isinstance(e, int) or e is None or isinstance(e, str) or not e:
            return e
        if e[0] == 'esc':
            obj = e[2]
            pid = self.ctx.pairs.get((side,) + obj, ('unpaired', side) + obj)
            return ('esc', pid, e[3], e[4], e[5])
        if e[0] == 'uninit':
            return e
        if e[0] == 'ld' and len(e) == 5 and e[4]:
            deps = tuple(sorted(set(self.canon(side, d) for d in e[4]), key=repr))
            return ('ld', e[1], self.canon(side, e[2]), self.canon(side, e[3]), deps)
        if e[0] == 'lin':
            c, t = as_lin(e)
            bs = [k for k, v in t.items() if isinstance(k, tuple) and k[0] == 'base' and v == 1]
            if len(bs) == 1:
                b = bs[0][1]
                off = sgn(c)
                rest = tuple(sorted(((self.canon(side, k), v) for k, v in t.items() if k != bs[0]), key=repr))
                if b == 'S0' and off >= 0:
                    head = ('argaddr', off)
                else:
                    obj = self.ctx.escaped_obj(side, b, off)
                    if obj is None and rest:
                        near = [k for k in self.ctx.esc[side] if k[0] == b and off < k[1] <= off + 0x40]
                        if near:
                            obj = min(near, key=lambda k: k[1])
                    if obj is not None:
                        pid = self.ctx.pairs.get((side,) + obj, ('unpaired', side) + obj)
                        head = ('objaddr', pid, off - obj[1])
                    else:
                        head = ('stkaddr', (b, off))
                return head if not rest else ('lin+', head, rest)
        if e[0] == 'base':
            return ('argaddr', 0) if e[1] == 'S0' else ('stkaddr', (e[1], 0))
        return tuple(self.canon(side, x) if isinstance(x, tuple) else x for x in e)

    def eq(self, a, b):
        return self.canon('L', a) == self.canon('R', b)

    # entry state classes ------------------------------------------------
    def abstract(self, stL, stR):
        """location -> value for both sides: stable values concrete, values
        present on both sides by class, other values derived from classes
        when they can be written in terms of them"""
        locsL = self.locs(stL)
        locsR = self.locs(stR)
        out = ({}, {})
        cv = ({}, {})
        wr = (locsL.pop(('written',)), locsR.pop(('written',)))
        ep = (locsL.pop(('epoch',))[1], locsR.pop(('epoch',))[1])
        if ep[0] != ep[1]:
            ep = (None, None)
        for i, side, locs in ((0, 'L', locsL), (1, 'R', locsR)):
            for loc, v in locs.items():
                cv[i][loc] = (self.canon(side, v), is_stable(v))
        self.pair_by_values(cv)
        raw = (locsL, locsR)
        shared = {c for c, stb in cv[0].values() if not stb} & {c for c, stb in cv[1].values() if not stb}
        other = ({c for c, stb in cv[0].values() if not stb}, {c for c, stb in cv[1].values() if not stb})
        self.halfref = set()
        for i in (0, 1):
            for loc, (c, stb) in cv[i].items():
                if stb:
                    out[i][loc] = ('cv', raw[i][loc])
                elif c in shared:
                    out[i][loc] = ('cls', c)
                else:
                    d = derive(c, shared, self.defs_of)
                    if d is None:
                        d = derive(c, shared | other[1 - i], self.defs_of)
                        if d is not None:
                            for x in crefs_of(d, []):
                                if x not in shared:
                                    self.halfref.add(x)
                    out[i][loc] = ('der', d) if d is not None else ('cls', ('side', i, c))
        for i in (0, 1):
            for loc, v in out[i].items():
                if v[0] == 'cls' and isinstance(v[1], tuple) and v[1][:1] == ('side',) and v[1][2] in self.halfref:
                    out[i][loc] = ('cls', v[1][2])
            out[i][('written',)] = ('wr', wr[i][1])
            out[i][('epoch',)] = ('ep', ep[i])
        defs = {}
        for c in shared:
            d = derive(c, shared - {c}, self.defs_of)
            if d is not None and d != ('cref', c):
                defs[c] = d
        return (out[0], out[1], defs)

    def defs_of(self, v):
        """definition of a class variable ('v', key, n) in terms of other class
        variables of the same key, or None"""
        ent = self.entries.get(v[1])
        if not ent:
            return None
        d = ent[2].get(v[2])
        if d is None:
            return None
        return subst_der(d, v[1])

    def locs(self, st):
        d = {}
        for k, v in st.r.items():
            d[('r', k)] = v
        for k, v in st.x.items():
            d[('x', k)] = v
        for i, v in enumerate(reversed(st.fp)):
            d[('fp', i)] = v
        for k, (sz, v) in st.stk.items():
            d[('s', k, sz)] = v
        d[('flags',)] = st.flags
        d[('written',)] = ('W', frozenset(st.written))
        d[('epoch',)] = ('EP', st.epoch)
        return d

    def merge(self, old, new):
        if old is None:
            return self.renumber(new)
        res = ({}, {}, {})
        pend = []
        for i in (0, 1):
            for loc in set(old[i]) & set(new[i]):
                a, b = old[i][loc], new[i][loc]
                if loc == ('written',):
                    res[i][loc] = ('wr', a[1] & b[1])
                elif loc == ('epoch',):
                    res[i][loc] = a if a == b else ('ep', None)
                elif a == b and a[0] == 'cv':
                    res[i][loc] = a
                elif a[0] == 'der' or b[0] == 'der':
                    pend.append((i, loc, a, b))
                else:
                    res[i][loc] = ('cls', (a, b))
                    for x in (a, b):
                        if x[0] == 'cv' and isinstance(x[1], tuple):
                            cc_, tt_ = as_lin(x[1])
                            bs_ = [k for k, v in tt_.items() if isinstance(k, tuple) and k[0] == 'base' and v == 1]
                            if len(bs_) == 1 and loc != ('r', 'esp') and loc != ('r', 'ebp'):
                                self.ctx.add_escape('LR'[i], (bs_[0][1], sgn(cc_)), 'merged')
        # merged classes by their old and by their new component
        keys = {v[1] for i in (0, 1) for v in res[i].values() if v[0] == 'cls'}
        by_new, by_old = {}, {}
        for k in keys:
            by_new.setdefault(k[1], []).append(k)
            by_old.setdefault(k[0], []).append(k)
        for i, loc, a, b in pend:
            e = generalize(b, a, by_new, 1, old[2], new[2])
            if e is None:
                e = generalize(a, b, by_old, 0, old[2], new[2])
            res[i][loc] = ('der', e) if e is not None else ('cls', (a, b))
        # definitions of merged classes
        for k in keys:
            a, b = k
            e = None
            if b[0] == 'cls' and b[1] in new[2]:
                e = generalize(('der', new[2][b[1]]), a, by_new, 1, old[2], new[2])
            if e is None and a[0] == 'cls' and a[1] in old[2]:
                e = generalize(('der', old[2][a[1]]), b, by_old, 0, old[2], new[2])
            if e is not None and e != ('cref', k):
                res[2][k] = e
        return self.renumber(res)

    def renumber(self, ent):
        m = {}
        res = ({}, {})
        for i in (0, 1):
            for loc in sorted(ent[i], key=repr):
                v = ent[i][loc]
                if v[0] in ('cv', 'wr', 'ep'):
                    res[i][loc] = v
                elif v[0] == 'cls':
                    if v[1] not in m:
                        m[v[1]] = len(m)
                    res[i][loc] = ('cls', m[v[1]])
        # classes present on one side only become side specific, unless a
        # derived value of the other side refers to them
        cntL = {v[1] for v in res[0].values() if v[0] == 'cls'}
        cntR = {v[1] for v in res[1].values() if v[0] == 'cls'}
        refd = set()
        for i in (0, 1):
            for v in ent[i].values():
                if v[0] == 'der':
                    for x in crefs_of(v[1], []):
                        if x in m:
                            refd.add(m[x])
        cntL |= refd & cntR | (refd - cntL - cntR)
        both = (cntL & cntR) | (refd & (cntL | cntR))
        cntL = cntL | (refd & cntR)
        cntR = cntR | (refd & cntL)
        for i, other in ((0, cntR), (1, cntL)):
            for loc, v in res[i].items():
                if v[0] == 'cls' and v[1] not in other:
                    res[i][loc] = ('only', i, v[1])
        defs = {}
        for k, d in ent[2].items():
            if k in m and m[k] in both:
                e = remap_der(d, m, both)
                if e is not None:
                    defs[m[k]] = e
        res = (res[0], res[1], defs)
        # derived values: class references renumbered, dropped when a class is gone
        for i in (0, 1):
            for loc in sorted(ent[i], key=repr):
                v = ent[i][loc]
                if v[0] != 'der':
                    continue
                e = remap_der(v[1], m, both)
                if e is None:
                    k = ('lostder', i, loc)
                    m[k] = len(m)
                    res[i][loc] = ('only', i, m[k])
                else:
                    res[i][loc] = ('der', e)
        return res

    def materialize(self, ent, i, key):
        st = State()
        side = 'LR'[i]
        st.epoch = ('M', key, 0)
        fps = {}
        for loc, v in ent[i].items():
            if v[0] == 'wr':
                st.written = set(v[1])
                continue
            if v[0] == 'ep':
                if v[1] is not None:
                    st.epoch = v[1]
                continue
            if v[0] == 'cv':
                val = v[1]
            elif v[0] == 'cls':
                val = ('v', key, v[1])
            elif v[0] == 'der':
                val = subst_der(v[1], key)
            else:
                val = ('only', side, key, v[2])
            if loc[0] == 'r':
                st.r[loc[1]] = val
            elif loc[0] == 'x':
                st.x[loc[1]] = val
            elif loc[0] == 'fp':
                fps[loc[1]] = val
            elif loc[0] == 's':
                st.stk[loc[1]] = (loc[2], val)
            elif loc[0] == 'flags':
                st.flags = val if v[0] != 'cv' or v[1] is not None else None
            elif loc[0] == 'written':
                st.written = set(v[1])
        st.fp = [fps[k] for k in sorted(fps, reverse=True)]
        st.floor = st.r.get('esp')
        return st

    def decanon(self, side, c):
        # stable values contain only base/sym terms and are not changed by canon except stack addresses
        if isinstance(c, tuple) and c and c[0] == 'objaddr':
            pid, rel = c[1], c[2]
            for k, p in self.ctx.pairs.items():
                if p == pid and k[0] == side:
                    return mklin(k[2] + rel, {('base', k[1]): 1})
            return ('lost', c)
        if isinstance(c, tuple) and c and c[0] == 'stkaddr':
            b, off = c[1]
            return mklin(off, {('base', b): 1})
        return c

    # main ---------------------------------------------------------------
    def run(self):
        for npass in range(6):
            self.ctx.changed = False
            self.problems, self.warnings, self.switch_info = {}, {}, {}
            self.run1()
            if not self.ctx.changed:
                break
        self.passes = npass + 1
        return self.report()

    def run1(self):
        start = (self.L.start, self.R.start)
        ent0 = self.abstract(self.initial('L'), self.initial('R'))
        # the entry: every init value is shared
        self.entries = {start: self.renumber(ent0)}
        work = [start]
        self.done = set()
        iters = 0
        while work:
            iters += 1
            if iters > 20000:
                print('too many iterations')
                break
            key = work.pop()
            ent = self.entries[key]
            succ = self.process(key, ent)
            news = [(k2, self.abstract(stL, stR)) for (k2, stL, stR) in succ]
            for (k2, new) in news:
                self.cur_k2 = (key, k2)
                old = self.entries.get(k2)
                merged = self.merge(old, new)
                if old is None or merged != old:
                    self.entries[k2] = merged
                    if k2 not in work:
                        work.append(k2)

    def process(self, key, ent):
        ka, kb = key
        stL = self.materialize(ent, 0, key) if key != (self.L.start, self.R.start) else self.initial('L')
        stR = self.materialize(ent, 1, key) if key != (self.L.start, self.R.start) else self.initial('R')
        if key == (self.L.start, self.R.start):
            stL.epoch = stR.epoch = ('M', key, 0)
        exL, exR = Exec(self.L, self.retail, self.ctx), Exec(self.R, self.retail, self.ctx)
        probs = []
        warns = []
        try:
            rL = exL.run(stL, ka)
        except Unhandled as e:
            self.problems[key] = ['L unhandled: %s' % e]
            return []
        try:
            rR = exR.run(stR, kb)
        except Unhandled as e:
            self.problems[key] = ['R unhandled: %s' % e]
            return []
        warns += ['L ' + w for w in stL.warn] + ['R ' + w for w in stR.warn]
        self.compare_effects(stL, stR, probs, warns)
        # memory epoch at the exit: unchanged when the superblock stored nothing
        for st_ in (stL, stR):
            if any(e[0] == 'st' for e in st_.eff):
                st_.epoch = ('X', key)
        succ = []
        if rL[0] != rR[0] and not ({rL[0], rR[0]} <= {'loop'}):
            probs.append('terminator differs: L %s, R %s' % (fmt(rL[:2]), fmt(rR[:2])))
        elif rL[0] == 'br':
            cL, cR = self.canon('L', rL[1]), self.canon('R', rR[1])
            if cL == cR:
                succ += [((rL[2], rR[2]), stL, stR), ((rL[3], rR[3]), stL, stR)]
            elif cL == self.canon('R', neg_cond(rR[1])):
                succ += [((rL[2], rR[3]), stL, stR), ((rL[3], rR[2]), stL, stR)]
            else:
                probs.append('branch differs:\n      L %s\n      R %s' % (fmt(cL), fmt(cR)))
                succ += [((rL[2], rR[2]), stL, stR), ((rL[3], rR[3]), stL, stR)]
        elif rL[0] == 'loop':
            succ.append(((rL[1], rR[1]), stL, stR))
        elif rL[0] == 'ret':
            self.check_ret(stL, stR, rL, rR, probs)
        elif rL[0] == 'noreturn':
            if rL[1] != rR[1]:
                probs.append('noreturn call differs')
        elif rL[0] == 'tailcall':
            if rL[1] != rR[1]:
                probs.append('tailcall differs')
        elif rL[0] == 'switch':
            mL = self.switch_map(self.L, rL[1], rL[2])
            mR = self.switch_map(self.R, rR[1], rR[2])
            if mL is None or mR is None:
                probs.append('switch not decoded L %s R %s' % (mL is not None, mR is not None))
            else:
                (xL, dL), (xR, dR) = mL, mR
                if self.canon('L', xL) != self.canon('R', xR):
                    probs.append('switch index differs:\n      L %s\n      R %s' % (fmt(self.canon('L', xL)), fmt(self.canon('R', xR))))
                if set(dL) != set(dR):
                    probs.append('switch index ranges differ: L %s R %s' % (sorted(set(dL) - set(dR))[:8], sorted(set(dR) - set(dL))[:8]))
                pairs = sorted({(dL[k], dR[k]) for k in dL if k in dR}, key=repr)
                bad = [k for k in dL if k in dR and (isinstance(dL[k], tuple) or isinstance(dR[k], tuple))]
                if bad:
                    probs.append("switch entries outside the table at %s: %s" % (bad[:5], [(dL[k], dR[k]) for k in bad[:5]]))
                    pairs = [pq for pq in pairs if not isinstance(pq[0], tuple) and not isinstance(pq[1], tuple)]
                self.switch_info[key] = pairs
                for a, b in pairs:
                    succ.append(((a, b), stL, stR))
        if probs:
            self.problems[key] = probs
        else:
            self.problems.pop(key, None)
        if warns:
            self.warnings[key] = warns
        else:
            self.warnings.pop(key, None)
        return succ

    def sym_loc(self, fn, name):
        if fn.side == 'L':
            inv = {v: k for k, v in self.alias.items()}
            name = inv.get(name, name)
        co = fn.coff
        for i, (n, v, sn, sc) in co.syms.items():
            if n == name and sn > 0:
                return sn - 1, v
        return None

    def sec_limit(self, fn, sec, off):
        """end of the data that starts at off: next symbol in the section or its end"""
        co = fn.coff
        lim = co.secs[sec]['size']
        for i, (n, v, sn, sc) in co.syms.items():
            if sn - 1 == sec and off < v < lim and sc in (2, 3, 6):
                lim = v
        return lim

    def switch_map(self, fn, o, addr):
        loc = self.sym_loc(fn, o[6])
        if loc is None:
            return None
        sec, off = loc
        s = fn.coff.secs[sec]
        c, t = as_lin(addr)
        t = {k: v for k, v in t.items() if not (isinstance(k, tuple) and k[0] == 'sym')}
        if len(t) != 1 or list(t.values())[0] != 4:
            return None
        x = list(t)[0]
        base = off + sgn(c)
        entries = []
        k = off
        while k in s['relocs']:
            si, ty = s['relocs'][k]
            nm, val, tsec, sc = fn.coff.syms[si]
            if tsec - 1 != fn.sec:
                break
            entries.append(val + struct.unpack_from('<I', s['data'], k)[0])
            k += 4
        first = (base - off) // 4     # entry index for x == 0
        if isinstance(x, tuple) and x[0] == 'zx8' and isinstance(x[1], tuple) and x[1][0] == 'ld' and x[1][1] == 1:
            a = x[1][2]
            cb, tb = as_lin(a)
            syms = [k2 for k2 in tb if isinstance(k2, tuple) and k2[0] == 'sym']
            if len(syms) != 1:
                return None
            bl = self.sym_loc(fn, syms[0][1])
            if bl is None:
                return None
            bsec, boff = bl
            y = mklin(0, {k2: v for k2, v in tb.items() if k2 not in syms})
            lim = self.sec_limit(fn, bsec, boff)
            data = fn.coff.secs[bsec]['data'][boff:lim]
            d = {}
            for i, b in enumerate(data):
                if not (0 <= b + first < len(entries)):
                    break
                if 0 <= b + first < len(entries):
                    d[i - sgn(cb)] = entries[b + first]
                else:
                    d[i - sgn(cb)] = ('bad', b)
            return y, d
        return x, {i - first: e for i, e in enumerate(entries)}

    def compare_effects(self, stL, stR, probs, warns):
        def segs(st):
            out = [[]]
            calls = []
            for e in st.eff:
                if e[0] == 'call':
                    calls.append(e)
                    out.append([])
                else:
                    out[-1].append(e)
            return out, calls
        sL, cL = segs(stL)
        sR, cR = segs(stR)
        if len(cL) != len(cR):
            probs.append('call count differs: L %d (%s), R %d (%s)' % (
                len(cL), ', '.join(fmt(c[1]) for c in cL), len(cR), ', '.join(fmt(c[1]) for c in cR)))
        for i in range(min(len(cL), len(cR)) + 1):
            if i < len(sL) and i < len(sR):
                a = sorted(repr(self.canon('L', ('st',) + e[1:])) for e in sL[i])
                b = sorted(repr(self.canon('R', ('st',) + e[1:])) for e in sR[i])
                if a != b and self.pair_by_stores(sL[i], sR[i]):
                    pass
                if a != b:
                    probs.append('stores differ before call %d:\n      L only %s\n      R only %s' % (
                        i, [x for x in a if x not in b], [x for x in b if x not in a]))
                else:
                    oa = [repr(self.canon('L', ('st',) + e[1:])) for e in sL[i]]
                    ob = [repr(self.canon('R', ('st',) + e[1:])) for e in sR[i]]
                    for p in range(len(sL[i])):
                        for q in range(p + 1, len(sL[i])):
                            e1, e2 = sL[i][p], sL[i][q]
                            if may_alias(e1[1], e1[2], e2[1], e2[2]) and oa[p] != oa[q]:
                                if ob.index(oa[p]) > ob.index(oa[q]):
                                    probs.append('order of possibly aliasing stores differs: %s / %s' % (oa[p][:200], oa[q][:200]))
            if i < len(cL) and i < len(cR):
                x, y = cL[i], cR[i]
                # pair stack object args
                n = min(len(x[2]), len(y[2]))
                for k in range(n):
                    self.pair_obj(x[2][k], y[2][k])
                if x[3] is not None and y[3] is not None:
                    self.pair_obj(x[3], y[3])
                bad = []
                if self.canon('L', x[1]) != self.canon('R', y[1]):
                    bad.append('target L %s R %s' % (fmt(self.canon('L', x[1])), fmt(self.canon('R', y[1]))))
                if len(x[2]) != len(y[2]):
                    bad.append('arg count L %d R %d (extra L %s R %s)' % (len(x[2]), len(y[2]), fmt(x[2][n:]), fmt(y[2][n:])))
                for k in range(n):
                    if self.canon('L', x[2][k]) != self.canon('R', y[2][k]):
                        bad.append('arg %d:\n        L %s\n        R %s' % (k, fmt(self.canon('L', x[2][k])), fmt(self.canon('R', y[2][k]))))
                if self.canon('L', x[3]) != self.canon('R', y[3]):
                    bad.append('this L %s R %s' % (fmt(self.canon('L', x[3])), fmt(self.canon('R', y[3]))))
                if self.canon('L', x[4]) != self.canon('R', y[4]):
                    bad.append('extra in L %s R %s' % (fmt(self.canon('L', x[4])), fmt(self.canon('R', y[4]))))
                # contents of passed stack objects
                for k in range(n):
                    self.cmp_obj(x[2][k], y[2][k], x[8][k], y[8][k], bad)
                if x[3] is not None and y[3] is not None:
                    self.cmp_obj(x[3], y[3], x[8][-1], y[8][-1], bad)
                if bad:
                    probs.append('call %d (L %x, R %x) %s: %s' % (i, x[5], y[5], fmt(self.canon('L', x[1])), '; '.join(bad)))
        # load sets per segment
        def lsegs(st):
            pass
        la = {repr(self.canon('L', (a, s, ep))) for (a, s, ep) in stL.loads if ep != 'fwd'}
        lb = {repr(self.canon('R', (a, s, ep))) for (a, s, ep) in stR.loads if ep != 'fwd'}
        if la != lb:
            warns.append('load sets differ: L only %s; R only %s' % (sorted(la - lb), sorted(lb - la)))

    def pair_by_values(self, cv):
        """pair unpaired stack objects that appear at the same place of equal
        values on both sides (for the next pass)"""
        def objs(e, out):
            if isinstance(e, tuple) and e:
                if e[0] == 'unpaired':
                    out.add(e)
                    return
                for x in e:
                    objs(x, out)
            return out

        def mask(e):
            if isinstance(e, tuple) and e:
                if e[0] == 'unpaired':
                    return 'P'
                return tuple(mask(x) for x in e)
            return e
        L = {}
        for loc, (c, stb) in cv[0].items():
            o = objs(c, set())
            if len(o) == 1:
                L.setdefault(mask(c), set()).add(next(iter(o)))
        for loc, (c, stb) in cv[1].items():
            o = objs(c, set())
            if len(o) != 1:
                continue
            m = mask(c)
            if m in L and len(L[m]) == 1:
                a = next(iter(L[m]))
                b = next(iter(o))
                ka, kb = ('L',) + a[2:], ('R',) + b[2:]
                if ka not in self.ctx.pairs and kb not in self.ctx.pairs:
                    pid = ('obj', ka[2], kb[2])
                    self.ctx.pairs[ka] = pid
                    self.ctx.pairs[kb] = pid
                    self.ctx.weakpair |= {ka, kb}
                    self.ctx.changed = True

    def pair_by_stores(self, LL, RR):
        """pair unpaired stack objects whose stores agree (same offset in the
        object, size and value)"""
        def head(side, e):
            c = self.canon(side, e[1])
            if isinstance(c, tuple) and c and c[0] == 'objaddr' and c[1][0] == 'unpaired':
                return c
            return None
        got = False
        for x in LL:
            hx = head('L', x)
            if not hx:
                continue
            for y in RR:
                hy = head('R', y)
                if not hy or hx[2] != hy[2] or x[2] != y[2]:
                    continue
                if self.canon('L', x[3]) != self.canon('R', y[3]):
                    continue
                ka = ('L',) + hx[1][2:]
                kb = ('R',) + hy[1][2:]
                if ka in self.ctx.pairs or kb in self.ctx.pairs:
                    continue
                pid = ('obj', ka[2], kb[2])
                self.ctx.pairs[ka] = pid
                self.ctx.pairs[kb] = pid
                self.ctx.weakpair |= {ka, kb}
                self.ctx.changed = True
                got = True
                break
        return got

    def pair_obj(self, a, b):
        sa, sb = slot_of(a) if a is not None else None, slot_of(b) if b is not None else None
        if not sa or not sb or (sa[0] == 'S0' and sa[1] >= 0):
            return
        oa = self.ctx.escaped_obj('L', *sa)
        ob = self.ctx.escaped_obj('R', *sb)
        if oa is None or ob is None:
            return
        ka, kb = ('L',) + oa, ('R',) + ob
        if (sa[1] - oa[1]) != (sb[1] - ob[1]):
            return
        pa, pb = self.ctx.pairs.get(ka), self.ctx.pairs.get(kb)
        if pa is None and pb is None:
            pid = ('obj', oa[1], ob[1])
            self.ctx.pairs[ka] = pid
            self.ctx.pairs[kb] = pid
            self.ctx.changed = True
        elif pa != pb:
            if (pa is None or ka in self.ctx.weakpair) and (pb is None or kb in self.ctx.weakpair):
                for k in list(self.ctx.pairs):
                    if self.ctx.pairs[k] in (pa, pb) and self.ctx.pairs[k] is not None:
                        del self.ctx.pairs[k]
                        self.ctx.weakpair.discard(k)
                pid = ('obj', oa[1], ob[1])
                self.ctx.pairs[ka] = pid
                self.ctx.pairs[kb] = pid
                self.ctx.changed = True
            else:
                self.ctx.conflicts.append((ka, kb, pa, pb))

    def cmp_obj(self, a, b, snapL, snapR, bad):
        sa, sb = slot_of(a) if a is not None else None, slot_of(b) if b is not None else None
        if not sa or not sb or (sa[1] >= 0 and sa[0] == 'S0'):
            return
        oa = self.ctx.escaped_obj('L', *sa)
        ob = self.ctx.escaped_obj('R', *sb)
        if oa is None or ob is None:
            bad.append('stack object not tracked')
            return
        n = min(self.ctx.esc['L'][oa] - (sa[1] - oa[1]), self.ctx.esc['R'][ob] - (sb[1] - ob[1]))
        A = {r: (z, repr(self.canon('L', v))) for r, (z, v) in snapL.items() if r < n}
        B = {r: (z, repr(self.canon('R', v))) for r, (z, v) in snapR.items() if r < n}
        if A != B:
            diff = sorted(set(A) | set(B))
            bad.append('object contents (size %d) differ: %s' % (n, '; '.join(
                '+%x L %s R %s' % (r, A.get(r), B.get(r)) for r in diff if A.get(r) != B.get(r))[:1200]))

    def check_ret(self, stL, stR, rL, rR, probs):
        if rL[1] != rR[1]:
            probs.append('ret imm differs')
        for r in ('ebx', 'esi', 'edi', 'ebp'):
            for side, st in (('L', stL), ('R', stR)):
                if st.r.get(r) != ('init', r) and not (isinstance(st.r.get(r), tuple) and st.r.get(r)[0] == 'v'):
                    probs.append('%s: %s not restored: %s' % (side, r, fmt(st.r.get(r))))
            if not self.eq(stL.r.get(r), stR.r.get(r)):
                probs.append('%s differs at ret' % r)
        if not self.eq(stL.r['esp'], stR.r['esp']):
            probs.append('esp differs at ret')
        k = self.retkind
        if k == 'eax':
            a, b = stL.r['eax'], stR.r['eax']
        elif k == 'al':
            a, b = lo8(stL.r['eax']), lo8(stR.r['eax'])
        elif k == 'st0':
            a, b = (stL.fp[-1] if stL.fp else None), (stR.fp[-1] if stR.fp else None)
        else:
            a = b = None
        if not self.eq(a, b):
            probs.append('return value differs:\n      L %s\n      R %s' % (fmt(self.canon('L', a)), fmt(self.canon('R', b))))

    def report(self):
        print('%s  %s%%  pairs %d' % (self.symbol, self.pct, len(self.entries)))
        if self.alias_notes:
            for n in self.alias_notes:
                print('  alias', n)
        for n in getattr(self, 'rbase_notes', []):
            print('  NOTE', n)
        if self.ctx.conflicts:
            print('  object pairing conflicts:', self.ctx.conflicts[:5])
        nprob = 0
        for key in sorted(self.problems):
            for p in self.problems[key]:
                nprob += 1
                print('  PROBLEM at L %x / R %x: %s' % (key[0], key[1], p))
        nw = 0
        for key in sorted(self.warnings):
            for w in self.warnings[key]:
                nw += 1
                if self.verbose or 'load sets' in w:
                    print('  warn at L %x / R %x: %s' % (key[0], key[1], w[:1500]))
        print('  RESULT: %s (%d problems, %d warnings)' % ('SAME' if nprob == 0 else 'DIFFERENT', nprob, nw))
        return nprob


def fmt(e, depth=0):
    s = repr(e)
    s = s.replace("('base', 'S0')", 'S0')
    return s if len(s) < 600 else s[:600] + '...'


def ret_kind(name):
    m = re.search(r'@@[A-Z]+?([A-Z]|_N|PA|PB|\?A)', name)
    if name.startswith('?'):
        mm = re.search(r'@@(?:Y[AGE]|[QAIEUMCKSTBJRFVNDL][AB]?[AEGI])(_N|[A-Z]|\?A)', name)
        if mm:
            t = mm.group(1)
            if t == 'X':
                return 'void'
            if t in ('M', 'N'):
                return 'st0'
            if t in ('_N', 'D', 'E', 'C'):
                return 'al'
            return 'eax'
    return 'void'


def build_alias(d, li, ri):
    alias = {}
    notes = []
    conflict = set()
    for a, b in zip(li, ri):
        na = objdiff_tool.reloc_name(d['left'], a) if a else None
        nb = objdiff_tool.reloc_name(d['right'], b) if b else None
        if na and nb and na != nb:
            if na in alias and alias[na] != nb:
                conflict.add(na)
            alias[na] = nb
    out = {}
    retail = Retail()
    for na, nb in alias.items():
        if na in conflict:
            notes.append('conflicting alias %s' % na)
            continue
        m = UNNAMED.match(na)
        ok = None
        if m and (nb.startswith('__real@') or nb.startswith('__xmm@')):
            hx = nb.split('@')[1]
            raw = bytes.fromhex(hx)[::-1]
            try:
                got = retail.read(int(m.group(1), 16), len(raw))
            except Exception:
                got = None
            ok = got == raw
            if not ok:
                notes.append('CONSTANT MISMATCH %s=%s vs %s' % (na, got.hex() if got else None, nb))
                continue
        elif na.startswith('PTR_') and nb.startswith('__imp_'):
            core = re.sub(r'^PTR_|_[0-9a-fA-F]{8}$', '', na)
            if core not in nb:
                notes.append('import alias %s -> %s' % (na, nb))
        elif m:
            notes.append('unnamed alias %s -> %s' % (na, nb))
        else:
            notes.append('NAMED ALIAS %s -> %s' % (na, nb))
        out[na] = nb
    return out, notes


if __name__ == '__main__':
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    rk = None
    if '--ret' in sys.argv:
        rk = sys.argv[sys.argv.index('--ret') + 1]
        a = [x for x in a if x != rk]
    for x in sys.argv:
        if x.startswith('--stdcall-global='):
            for item in x[17:].split(','):
                nm, _, sz = item.partition(':')
                STDCALL_NAMES.append((nm, int(sz, 0) if sz else 0x400))
        if x == '--idioms':
            IDIOMS = True
        if x == '--ptr-stdcall':
            PTR_STDCALL = True
        if x.startswith('--obj='):
            for item in x[6:].split(','):
                sd, off, sz = item.split(':')
                DECLARED.setdefault(sd, []).append((int(off, 0), int(sz, 0)))
        if x.startswith('--objsize='):
            OBJCAP = int(x[10:], 0)
        if x.startswith('--objat='):
            for item in x[8:].split(','):
                sd, off, sz = item.split(':')
                OBJSIZE[(sd, 'S0', int(off, 0))] = int(sz, 0)
        if x.startswith('--mutate='):
            o, h = x[9:].split(':')
            MUTATE.setdefault('R', []).append((int(o, 16), bytes.fromhex(h)))
    c = Checker(a[0], a[1], rk, '--verbose' in sys.argv)
    c.run()
