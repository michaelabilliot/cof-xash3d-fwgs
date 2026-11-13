"""Check the relocation targets of matched functions.

  python tools/relocaudit.py [<unit> ...]

The objdiff report counts a function as 100% when its instructions match,
whatever symbols the relocations point at (the report runs with relocation
names ignored). A call to the wrong function or a load of the wrong global
can hide behind such a 100%. For every function at 100% in that report,
this generates a second report with functionRelocDiffs=name_address and,
for the functions that drop below 100% there, diffs them and looks at the
rows whose relocation target names differ:

- naming: the retail side still has a Ghidra default name (FUN_, DAT_,
  s_..., switchD_, caseD_) or the two names are the same symbol spelled
  differently; the code matches, the retail map lacks a name;
- real: both sides have real names and they differ, or the same global is
  accessed at another offset (another member). Either our source uses
  something else, or the retail name is wrong (check the evidence).

Units default to every hl unit with both objects. tools/update_status.py
uses real_differences(): a function with real differences stays TODO. Prints the real
differences and a count per kind.
"""
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import objdiff_tool  # noqa: E402

ROOT = objdiff_tool.ROOT
OBJDIFF = objdiff_tool.OBJDIFF
DEFAULT_NAME = re.compile(r"^(?:DAT|FUN|LAB|PTR|s|u|thunk_FUN|switchD|caseD|SUB|EXT|OFF|BYTE|WORD|DWORD)_"
                          r"|^switchD_[0-9a-fA-F]+::|^\$LN\d+$|^\.")


def same_symbol(a, b):
    """Equal names, or Ghidra's demangled spelling of a vtable
    (CFoo::vftable) against the decorated one (??_7CFoo@@6B@), or the
    same CRT function under its alias (_stricmp and __stricmp), or an
    undecorated retail name against our decorated one."""
    if a == b or a.lstrip("_") == b.lstrip("_"):
        return True
    if b.startswith("?") and b[1:].startswith(a + "@"):
        return True
    # namespaced and anonymous-namespace vtables: Ghidra writes
    # vgui::CDefaultInputSignal::vftable and _anon_8C64A114::Handler::vftable,
    # MSVC ??_7CDefaultInputSignal@vgui@@6B@ and ??_7Handler@?A0x2730726c@@6B@
    # (the anonymous namespace hash depends on the file path)
    m = re.match(r"^(?:(\w+)::)?(\w+)::vftable$", a)
    if m and b.startswith("??_7%s@" % m.group(2)):
        return True
    # template vtables: std::basic_ifstream<char,std::char_traits<char>_>::vftable
    # against ??_7?$basic_ifstream@DU?$char_traits@D@std@@@std@@6B@
    m = re.match(r"^(?:\w+::)*(\w+)<.*>_?::vftable$", a)
    if m and b.startswith("??_7?$%s@" % m.group(1)):
        return True
    # operator new of a class, and imports by ordinal
    if a.endswith("::operator_new") and b.startswith("??2"):
        return True
    if re.match(r"^[\w.]+\.DLL::Ordinal_\d+$", a, re.I):
        return True
    m = re.match(r"^(\w+)::RTTI_Type_Descriptor$", a)
    if m and b == "??_R0?AV%s@@@8" % m.group(1):
        return True
    # in a namespace: vgui2::HTML::RTTI_Type_Descriptor, ??_R0?AVHTML@vgui2@@@8
    m = re.match(r"^((?:\w+::)+)(\w+)::RTTI_Type_Descriptor$", a)
    if m and b == "??_R0?AV%s@%s@@@8" % (m.group(2), "@".join(reversed(m.group(1).rstrip(":").split("::")))):
        return True
    m = re.match(r"^(\w+)::vftable$", a)
    if m and b == "??_7%s@@6B@" % m.group(1):
        return True
    # an import by name (CGGL.DLL::cgGLEnableProfile) against our C name
    # (_cgGLEnableProfile) or its import pointer (__imp__cgGLEnableProfile);
    # a stdcall import keeps its argument size (_DirectDrawCreate@12)
    m = re.match(r"^[\w.]+\.DLL::(\w+)$", a, re.I)
    if m and re.sub(r"@\d+$", "", b.replace("__imp_", "")).lstrip("_") == m.group(1).lstrip("_"):
        return True
    # the same decorated name with and without back references
    # (...6BPanel@vgui@@@ against ...6BPanel@1@@), or Ghidra's demangled
    # spelling of any vtable (vgui::CListBox::ListBoxSignal::vftable)
    if b.startswith("?"):
        ub = undname(b)
        if a.startswith("?"):
            return ub is not None and ub == undname(a)
        if ub and ub.replace("const ", "").replace("`local vftable'", "vftable").replace("`vftable'", "vftable") == a:
            return True
    return False


UNDNAME = r"I:\Tools\msvc-14.27.29112\bin\Hostx86\x86\undname.exe"
_UND = {}


def undname(name):
    if name not in _UND:
        try:
            out = subprocess.run([UNDNAME, name], capture_output=True, text=True).stdout
            m = re.search(r'is :- "(.*)"', out)
            _UND[name] = m.group(1) if m else None
        except OSError:
            _UND[name] = None
    return _UND[name]


def unit_report(unit, extra):
    cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
    proj = os.path.join(ROOT, "build", "relocaudit", unit.replace("/", "_"))
    os.makedirs(proj, exist_ok=True)
    keep = []
    for u in cfg["units"]:
        if u["name"] != unit:
            continue
        for key in ("target_path", "base_path"):
            full = os.path.join(ROOT, u[key])
            if not os.path.exists(full):
                return None
            u[key] = os.path.relpath(full, proj).replace("\\", "/")
        keep.append(u)
    cfg["units"] = keep
    with open(os.path.join(proj, "objdiff.json"), "w") as f:
        json.dump(cfg, f)
    out = os.path.join(proj, "r%s.json" % ("_names" if extra else ""))
    cmd = [OBJDIFF, "report", "generate", "-p", proj, "-o", out, "-f", "json"] + extra
    try:
        subprocess.run(cmd, cwd=ROOT, capture_output=True, timeout=300)
    except subprocess.TimeoutExpired:
        print("timeout: %s" % unit)
        return None
    rep = json.load(open(out))
    return {f["name"]: f.get("fuzzy_match_percent", 0.0) for u in rep.get("units", []) for f in u.get("functions", [])}


_MAPS = {}


def retail_map(dll):
    """name -> retail address, address -> class (LIB rows) for one dll."""
    if dll not in _MAPS:
        import csv
        names = {}
        for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll))):
            names.setdefault(r["name"], int(r["va"], 16))
        lib = set()
        for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))):
            if r["dll"] == dll and r["class"] == "LIB":
                lib.add(int(r["va"], 16))
        _MAPS[dll] = (names, lib)
    return _MAPS[dll]


_OURS = {}


def our_function(dll, name):
    """(bytes, masked offsets) of one of our compiled functions, any unit."""
    if dll not in _OURS:
        import glob
        from bytematch import coff_functions
        funcs = {}
        for path in glob.glob(os.path.join(ROOT, "build", "base", dll, "**", "*.obj"), recursive=True):
            for n, f in coff_functions(path).items():
                funcs.setdefault(n, f)
        _OURS[dll] = funcs
    return _OURS[dll].get(name)


def same_place(dll, na, xa, nb, xb):
    """The two targets are one retail place: the same address (an access
    biased from a neighbouring global), a function folded by /OPT:ICF into
    the retail one, or a CRT function the retail map names by its bytes."""
    names, lib = retail_map(dll)
    va = names.get(na)
    if va is None:
        return False
    if va in lib:
        return True
    vb = names.get(nb)
    if vb is not None:
        return va + xa == vb + xb
    f = our_function(dll, nb)
    if f is None or xa or xb:
        return False
    code, masked = f
    ret = objdiff_tool.retail_bytes(dll, va, len(code))
    return len(ret) == len(code) and all(ret[i] == code[i] for i in range(len(code)) if i not in masked)


def real_differences(unit):
    """{function: [(retail target, our target)]} for the functions of the
    unit that are at 100% in the plain report but whose relocations point
    at other symbols (or other offsets of the same symbol); None when the
    unit has no report."""
    plain = unit_report(unit, [])
    if plain is None:
        return None
    named = unit_report(unit, ["-c", "functionRelocDiffs=name_address"])
    if named is None:
        return None
    out = {}
    for fn, pct in sorted(plain.items()):
        if pct < 100.0 or named.get(fn, 0.0) >= 100.0:
            continue
        found = differences(unit, fn)
        if found:
            out[fn] = found
    return out


_DATA = {}


def our_data(unit, name, n):
    """n bytes of one of our data symbols (a VC6 $SG string literal) in the
    unit's base object, or None."""
    if unit not in _DATA:
        import struct
        cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
        path = next((os.path.join(ROOT, u["base_path"]) for u in cfg["units"] if u["name"] == unit), None)
        syms = {}
        if path and os.path.exists(path):
            data = open(path, "rb").read()
            _m, nsec, _ts, symptr, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
            secs = []
            for i in range(nsec):
                size, praw = struct.unpack_from("<II", data, 20 + opt + 40 * i + 16)
                secs.append(data[praw:praw + size] if praw else b"\0" * size)
            strtab = symptr + 18 * nsym
            i = 0
            while i < nsym:
                o = symptr + 18 * i
                raw = data[o:o + 8]
                if raw[:4] == b"\0\0\0\0":
                    off = struct.unpack_from("<I", raw, 4)[0]
                    sname = data[strtab + off:data.index(b"\0", strtab + off)].decode("latin-1")
                else:
                    sname = raw.rstrip(b"\0").decode("latin-1")
                value, secnum, _typ, _cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
                if sname.startswith("$SG") and secnum > 0:
                    syms[sname] = (secs[secnum - 1], value)
                i += 1 + naux
        _DATA[unit] = syms
    s = _DATA[unit].get(name)
    return s[0][s[1]:s[1] + n] if s else None


def literal_differs(unit, va, name):
    """Our VC6 string literal ($SGnnn, no /GF) against the retail bytes at
    va: compare up to and including our terminating NUL."""
    ours = our_data(unit, name, 4096)
    if ours is None or b"\0" not in ours:
        return False
    ours = ours[:ours.index(b"\0") + 1]
    return objdiff_tool.retail_bytes(unit.split("/")[0], va, len(ours)) != ours


def differences(unit, fn):
    d = objdiff_tool.diff_json(unit, fn)
    _l, _r, li, ri = objdiff_tool.rows(d, fn)
    found = []
    for a, b in zip(li, ri):
        na = objdiff_tool.reloc_name(d["left"], a)
        nb = objdiff_tool.reloc_name(d["right"], b)
        # a string literal or float constant of ours against an unnamed
        # retail address: the name encodes the value, check it against the
        # retail bytes (the plain report ignores what a relocation points at)
        m = objdiff_tool.UNNAMED.match(na or "")
        if m and nb and nb.startswith(("??_C@_0", "__real@", "__xmm@")):
            va = int(m.group(1), 16) + objdiff_tool.reloc_addend(a) - objdiff_tool.reloc_addend(b)
            if not objdiff_tool.constant_ok(unit.split("/")[0], va, nb):
                found.append((na, nb))
            continue
        if m and nb and nb.startswith("$SG"):
            va = int(m.group(1), 16) + objdiff_tool.reloc_addend(a) - objdiff_tool.reloc_addend(b)
            if literal_differs(unit, va, nb):
                found.append((na, nb))
            continue
        if not na or not nb or DEFAULT_NAME.search(na) or DEFAULT_NAME.search(nb):
            continue
        # two STL instantiations, or a CRT internal (common_strtime) against
        # the public function: toolchain code, named by bytes on the retail side
        if ("@std@@" in na and "@std@@" in nb) or (na.startswith("??$common_") and nb.startswith("_")):
            continue
        # an import thunk spelled by Ghidra (VGUI.DLL::vgui::Color::Color)
        # against our decorated name
        if ".DLL::" in na.upper() and nb.startswith("?"):
            continue
        if same_symbol(na, nb) or na.startswith("FID_conflict:"):
            # the same global at another offset is another member
            xa, xb = objdiff_tool.reloc_addend(a), objdiff_tool.reloc_addend(b)
            if xa != xb and na == nb:
                found.append(("%s+0x%x" % (na, xa), "%s+0x%x" % (nb, xb)))
            continue
        if same_place(unit.split("/")[0], na, objdiff_tool.reloc_addend(a), nb, objdiff_tool.reloc_addend(b)):
            continue
        found.append((na, nb))
    return sorted(set(found))


def main():
    units = sys.argv[1:]
    if not units:
        cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
        units = [u["name"] for u in cfg["units"] if u["name"].startswith("hl/")]
    counts = {"clean": 0, "naming": 0, "real": 0}
    real = []
    for unit in units:
        plain = unit_report(unit, [])
        if plain is None:
            continue
        named = unit_report(unit, ["-c", "functionRelocDiffs=name_address"])
        if named is None:
            continue
        for fn, pct in sorted(plain.items()):
            if pct < 100.0:
                continue
            if named.get(fn, 0.0) >= 100.0:
                counts["clean"] += 1
                continue
            found = differences(unit, fn)
            if found:
                counts["real"] += 1
                real.append((unit, fn, sorted(set(found))))
            else:
                counts["naming"] += 1
    for unit, fn, found in real:
        print("%s  %s" % (unit, fn))
        for na, nb in found:
            print("    retail %s" % na)
            print("    ours   %s" % nb)
    print("matched functions: %d clean, %d naming only, %d with real name differences"
          % (counts["clean"], counts["naming"], counts["real"]))


if __name__ == "__main__":
    main()
