"""symeq with if-conversion: a conditional branch whose two arms are short
runs of register/stack-only code that rejoin is executed as one block, the
differing values merged as ('ite', cond, taken, fallthrough). cmov results
are normalized the same way, so a branch diamond on one side and a cmov on
the other compare equal.

  python tools/symeq_ifc.py <unit> <symbol> [--verbose] [--maxarm=N] [--byteflags]

--byteflags widens 1-byte constant stores to stack slots to 4 bytes (upper
bytes 0), so byte flags set on some paths keep one slot size (used for
CHudAmmo::Draw, docs/sessions.md 2026-10-25).
"""
import os, sys, importlib.util
spec = importlib.util.spec_from_file_location('symeq', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'symeq.py'))
se = importlib.util.module_from_spec(spec)
sys.modules['symeq'] = se
spec.loader.exec_module(se)

MAXARM = 12
for x in sys.argv:
    if x.startswith('--maxarm='):
        MAXARM = int(x[9:])


def ite(c, t, f):
    if t == f:
        return t
    nc = se.neg_cond(c)
    if nc[0] != 'not' and repr(nc) < repr(c):
        return ('ite', nc, f, t)
    return ('ite', c, t, f)


def cpy(st):
    s = st.copy()
    s.written = set(st.written)
    s.lahf = getattr(st, 'lahf', None)
    return s


def path(fn, a):
    """straight-line addresses from a, following direct jmps, until a
    conditional branch, call, ret or indirect jump (exclusive)"""
    out = []
    while a in fn.ins and len(out) < MAXARM:
        n = fn.ins[a]
        if a in out:
            break
        out.append(a)
        if n.mn == 'jmp':
            if n.target is None:
                return out, False
            a = n.target
            continue
        if n.mn.startswith('j') or n.mn == 'call' or n.mn.startswith('ret') or n.mn == 'leave':
            return out, False
        a = n.next
    return out, True


orig_step = se.Exec.step


def step(self, st, n):
    r = orig_step(self, st, n)
    if n.mn.startswith('cmov') and r is None:
        o = n.ops[0]
        v = self.getr(st, o[1])
        if isinstance(v, tuple) and v[0] == 'ite':
            self.setr(st, o[1], ite(v[1], v[2], v[3]))
    return r


se.Exec.step = step


def try_ifc(self, st, c, t, f):
    pt, okt = path(self.fn, t)
    pf, okf = path(self.fn, f)
    # join: first address of one arm that the other arm reaches
    J = None
    if t in pf:
        J = t; pf = pf[:pf.index(t)]; pt = []
    elif f in pt:
        J = f; pt = pt[:pt.index(f)]; pf = []
    else:
        for i, a in enumerate(pf):
            if a in pt:
                J = a; pf = pf[:i]; pt = pt[:pt.index(a)]
                break
    if J is None:
        return None
    res = []
    for arm in (pt, pf):
        s = cpy(st)
        n0 = len(s.eff)
        for a in arm:
            n = self.fn.ins[a]
            if n.mn == 'jmp':
                continue
            r = orig_step(self, s, n)
            if n.mn.startswith('cmov'):
                o = n.ops[0]
                v = self.getr(s, o[1])
                if isinstance(v, tuple) and v[0] == 'ite':
                    self.setr(s, o[1], ite(v[1], v[2], v[3]))
            if r is not None:
                return None
        if len(s.eff) != n0:
            return None
        res.append(s)
    sT, sF = res
    if len(sT.fp) != len(sF.fp):
        return None
    out = cpy(st)
    for k in set(sT.r) | set(sF.r):
        out.r[k] = ite(c, sT.r.get(k, ('undef', self.side, k)), sF.r.get(k, ('undef', self.side, k)))
    for k in set(sT.x) | set(sF.x):
        out.x[k] = ite(c, sT.x.get(k, ('undef', self.side, k)), sF.x.get(k, ('undef', self.side, k)))
    out.fp = [ite(c, a, b) for a, b in zip(sT.fp, sF.fp)]
    stk = {}
    for k in set(sT.stk) | set(sF.stk):
        a, b = sT.stk.get(k), sF.stk.get(k)
        if a is None or b is None or a[0] != b[0]:
            if a == b:
                continue
            # slot written on one arm only with a different size: give up
            return None
        stk[k] = (a[0], ite(c, a[1], b[1]))
    out.stk = stk
    out.written = sT.written | sF.written
    out.loads = sT.loads + [x for x in sF.loads if x not in sT.loads]
    out.warn = sT.warn + [w for w in sF.warn if w not in sT.warn]
    out.flags = None
    return out, J, pt + pf


def run(self, st, addr, maxsteps=6000):
    seen = set()
    steps = 0
    while True:
        if addr not in self.fn.ins:
            raise se.Unhandled('no insn at %x' % addr)
        if addr in seen:
            return ('loop', addr)
        seen.add(addr)
        steps += 1
        if steps > maxsteps:
            raise se.Unhandled('too many steps')
        n = self.fn.ins[addr]
        r = self.step(st, n)
        if r is None:
            addr = n.next
            continue
        if r[0] == 'goto':
            addr = r[1]
            continue
        if r[0] == 'br' and r[2] is not None and r[3] is not None:
            got = try_ifc(self, st, r[1], r[2], r[3])
            if got is not None:
                out, J, arms = got
                st.__dict__.update(out.__dict__)
                seen.update(arms)
                addr = J
                continue
        return r


se.Exec.run = run

if "--byteflags" in sys.argv:
    orig_store = se.Exec.store

    def store(self, st, a, size, v):
        if size == 1 and isinstance(v, int) and se.slot_of(a) and not self.in_memory(a):
            return orig_store(self, st, a, 4, v)
        return orig_store(self, st, a, size, v)
    se.Exec.store = store

if __name__ == "__main__":
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    c = se.Checker(a[0], a[1], None, '--verbose' in sys.argv)
    c.run()
