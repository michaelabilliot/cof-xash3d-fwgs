"""Replace wrong Ghidra signature names (MFC, ATL, "~<>") by our names through matched functions.

  python tools/fid_fix.py <unit> [<unit> ...] [--write]

Ghidra's function ID database gives some small engine functions the name of
an MFC or ATL function with the same bytes (ATL::CAtlBaseModule::~CAtlBaseModule,
CSliderCtrl::SetRange, "~<>"). In a function that matches 100%, every call
or address of such a function lines up with a call or address of ours, so
the retail address gets our symbol's name. Only pairings where every use
agrees are taken. --write records them in tools/symbols/<dll>_manual.csv and
lists the old labels in <dll>_forget.csv.
"""
import collections
import csv
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import name_unit  # noqa: E402
import objdiff_tool  # noqa: E402
import relocaudit  # noqa: E402

ROOT = objdiff_tool.ROOT
BAD = re.compile(r"^(?:~<>|ATL::|\?\?[0-9_][A-Z]?C(?:Atl|Slider|Wnd|Ole|Cmd)\w*@|.*\bC(?:Slider|Atl|Wnd|String|Ole|Cmd|Edit|List|Combo|Button|Static|Dialog|Window)\w*::|\?\w+@C(?:Slider|Atl|Wnd|Ole|Cmd|Edit|List|Combo|Button|Static|Dialog|Window)\w*@)")


def main():
    args = [a for a in sys.argv[1:] if a != "--write"]
    write = "--write" in sys.argv
    dll = args[0].split("/")[0]
    names = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll))):
        names.setdefault(r["name"], int(r["va"], 16))
    votes = collections.defaultdict(collections.Counter)
    users = collections.defaultdict(set)
    for unit in args:
        plain = relocaudit.unit_report(unit, [])
        if not plain:
            continue
        for fn, pct in plain.items():
            if pct < 100.0:
                continue
            d = objdiff_tool.diff_json(unit, fn)
            _l, _r, li, ri = objdiff_tool.rows(d, fn)
            for a, b in zip(li, ri):
                na = objdiff_tool.reloc_name(d["left"], a)
                nb = objdiff_tool.reloc_name(d["right"], b)
                if na and nb and BAD.match(na) and not BAD.match(nb):
                    votes[na][nb] += 1
                    users[(na, nb)].add(fn)
    found = []
    for bad, c in votes.items():
        if len(c) == 1:
            good = next(iter(c))
            found.append((bad, good))
    for bad, good in found:
        print("  %-50s -> %s" % (bad[:50], good))
    print("%d labels paired" % len(found))
    if not write or not found:
        return
    # addresses: the rows that carry the label, narrowed to the ones the
    # matched functions call (direct calls read from the retail bytes)
    rows = list(csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"), encoding="utf-8")))
    size = {int(r["va"], 16): int(r["size"] or 0) for r in rows if r["dll"] == dll}
    import struct

    def callees(fn):
        va = names.get(fn)
        if va is None:
            return set()
        code = objdiff_tool.retail_bytes(dll, va, size.get(va, 0))
        return {(va + k + 5 + struct.unpack_from("<i", code, k + 1)[0]) & 0xFFFFFFFF
                for k in range(len(code) - 4) if code[k] == 0xE8}
    name_unit.lock()
    try:
        m = os.path.join(ROOT, "tools", "symbols", "%s_manual.csv" % dll)
        mrows = list(csv.DictReader(open(m, encoding="latin-1")))
        fields = list(mrows[0].keys())
        have = {r["va"] for r in mrows}
        forget = []
        for bad, good in found:
            m_addr = re.search(r"FUN_([0-9a-fA-F]{8})", bad)
            m_cls = re.search(r"C[A-Z][A-Za-z]+", bad)
            if m_addr:
                cands = {int(m_addr.group(1), 16)}
            else:
                cands = {int(r["va"], 16) for r in rows if r["dll"] == dll and (
                    r["demangled"] == bad or r["name"] == bad
                    or (m_cls and m_cls.group(0) in r["demangled"] and not r["name"]))}
            called = set()
            for fn in users[(bad, good)]:
                called |= callees(fn)
            cands = sorted("%08X" % v for v in cands & called)
            if len(cands) != 1:
                print("  %s: %d rows carry that label, skipped" % (bad, len(cands)))
                continue
            va = cands[0]
            if va not in have:
                mrows.append({"va": va, "name": good, "kind": "func",
                              "evidence": "Ghidra signature name %s; every use in 100%% functions pairs with our %s (tools/fid_fix.py)" % (bad, good)})
            forget.append((va, bad))
        with open(m, "w", newline="", encoding="latin-1") as f:
            w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
            w.writeheader()
            w.writerows(mrows)
        fp = os.path.join(ROOT, "tools", "symbols", "%s_forget.csv" % dll)
        t = open(fp, newline="").read()
        if t and not t.endswith("\n"):
            t += "\n"
        for va, bad in forget:
            t += '%s,"%s","Ghidra signature name; the address is %s (tools/fid_fix.py)"\n' % (va, bad, dict(found)[bad])
        open(fp, "w", newline="").write(t)
        print("recorded %d" % len(forget))
    finally:
        name_unit.unlock()


if __name__ == "__main__":
    main()
