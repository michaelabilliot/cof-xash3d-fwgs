"""Check retail function names against the stack bytes their code pops.

  python tools/sigcheck.py [dll]

A decorated C++ name fixes the calling convention and the parameter list.
For __thiscall and __stdcall the callee pops the parameters, so every
`ret N` of the function must pop exactly their size; a __cdecl function
must end in a plain `ret`. A name carried from an older build (1.55 PDB)
whose signature changed in 1.6a fails this test: that is evidence that the
name (the decorated signature, not the function) is wrong.

Names come from the merged symbol map of the last delink
(build/jobs/symbols_<dll>.csv), function sizes from docs/progress.csv.
Parameters passed by value whose size is not known (classes other than
Vector) make the function skipped. Prints the mismatches.
"""
import csv
import os
import re
import subprocess
import sys

import capstone
import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UNDNAME = r"I:\Tools\msvc-14.27.29112\bin\Hostx86\x86\undname.exe"
IMAGES = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll", "client": "versions/1.6a/cryoffear/cl_dlls/client.dll"}
BY_VALUE = {"class Vector": 12}


def undecorate(names):
    """{decorated: undecorated} through undname (file mode, in chunks)."""
    out = {}
    tmp = os.path.join(ROOT, "build", "jobs", "sigcheck_names.txt")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    for i in range(0, len(names), 2000):
        chunk = names[i:i + 2000]
        with open(tmp, "w") as f:
            f.write("\n".join(chunk) + "\n")
        res = subprocess.run([UNDNAME, tmp], stdout=subprocess.PIPE).stdout.decode("latin-1").splitlines()
        res = [l for l in res if l.strip()]
        if len(res) != len(chunk):
            sys.exit("undname returned %d lines for %d names" % (len(res), len(chunk)))
        out.update(zip(chunk, res))
    return out


def split_params(s):
    out, depth, cur = [], 0, ""
    for c in s:
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        if c == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def param_bytes(p):
    """Stack bytes of one undecorated parameter, None if unknown."""
    if p in ("void", "..."):
        return 0 if p == "void" else None
    if "*" in p or "&" in p or "(" in p:
        return 4
    if re.search(r"\bdouble\b|__int64", p):
        return 8
    if p.startswith("enum "):
        return 4
    if p.startswith(("class ", "struct ", "union ")):
        return BY_VALUE.get(p.replace(" const", ""))
    return 4


def expected_pop(und):
    """(convention, bytes the callee pops) from an undecorated signature."""
    m = re.search(r"^(.*?)__(thiscall|stdcall|cdecl|fastcall) .*?\((.*)\)( const)?$", und)
    if not m:
        return None, None
    ret, conv, params = m.group(1), m.group(2), split_params(m.group(3))
    if conv == "cdecl":
        return conv, 0
    sizes = [param_bytes(p) for p in params]
    if None in sizes:
        return conv, None
    # a member function returns a class by value through a hidden pointer
    # that the callee pops as well (as do __stdcall functions returning a
    # class larger than 8 bytes, e.g. Vector)
    ret = re.sub(r"^(public|protected|private): |virtual |static ", "", ret).strip()
    if re.match(r"^(class|struct|union) ", ret) and "*" not in ret and "&" not in ret:
        if "::" in und.split("(")[0] or ret.replace(" const", "") in BY_VALUE:
            sizes.append(4)
    if conv == "fastcall":
        # the first two dword-or-smaller parameters go in ecx and edx
        regs = 0
        rest = 0
        for s in sizes:
            if s and s <= 4 and regs < 2:
                regs += 1
            else:
                rest += s
        return conv, rest
    return conv, sum(sizes)


def main():
    dll = sys.argv[1] if len(sys.argv) > 1 else "hl"
    pe = pefile.PE(os.path.join(ROOT, IMAGES[dll]), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    sizes = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))):
        if r["dll"] == dll and r["size"]:
            sizes[int(r["va"], 16)] = int(r["size"])
    names = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll))):
        if r.get("kind", "func") == "func" and r["name"].startswith("?") and not r["name"].startswith("??_"):
            names[int(r["va"], 16)] = r["name"]
    und = undecorate(sorted(set(names.values())))
    # local classes of CRT functions (`...'::`2'::...) are not checked
    names = {va: n for va, n in names.items() if "`" not in und.get(n, "`")}
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    checked = skipped = 0
    bad = []
    for va, name in sorted(names.items()):
        conv, want = expected_pop(und.get(name, ""))
        size = sizes.get(va)
        if want is None or not size:
            skipped += 1
            continue
        pops = set()
        for ins in md.disasm(img[va - base:va - base + size], va):
            if ins.mnemonic == "ret":
                pops.add(int(ins.op_str, 0) if ins.op_str else 0)
        if len(pops) != 1:
            # no ret (tail call), or rets of different sizes: the size in
            # progress.csv runs into the next function
            skipped += 1
            continue
        checked += 1
        if pops != {want}:
            bad.append((va, name, conv, want, sorted(pops)))
    print("checked %d, skipped %d (unknown by-value parameter, no single ret size, no size), mismatches %d"
          % (checked, skipped, len(bad)))
    for va, name, conv, want, pops in bad:
        print("  %08X  %s  %s expects ret %d, code pops %s" % (va, name, conv, want, "/".join(map(str, pops))))
        print("            %s" % und[name])


if __name__ == "__main__":
    main()
