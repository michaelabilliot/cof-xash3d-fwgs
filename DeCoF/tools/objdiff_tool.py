"""Helpers around objdiff-cli one-shot JSON diffs.

  python tools/objdiff_tool.py show <unit> <symbol>
      side by side instructions of one function, differing rows marked
  python tools/objdiff_tool.py harvest <unit> [<unit> ...] [--matched] [--write]
      for every function below 100% (--matched: and at 100%): where both sides have a relocation on
      the same aligned instruction and the retail side points at an unnamed
      symbol (DAT_/FUN_/LAB_ + address), take the name of our side as the
      name of that retail address. String literals and float constants are
      accepted from functions at 80% or more after their value is checked
      against the retail bytes; every other name needs a function at 95% or
      more. The evidence column says which kind it is. Prints the proposals; with --write they
      are added to tools/symbols/<dll>_auto.csv (merged into the symbol map
      by tools/map_names.py and used by the next delink).

Left = retail (delinked target), right = our object (base).
"""
import csv
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OBJDIFF = r"I:\Tools\bin\objdiff-cli.exe"
# Ghidra default labels end with the 8 digit address (DAT_10001234,
# s_some_text_10001234, PTR_FUN_10001234)
UNNAMED = re.compile(r"^(?:DAT|FUN|LAB|PTR|s|u|thunk_FUN)_(?:.*_)?([0-9a-fA-F]{8})$")
SWITCH = re.compile(r"^switchD_[0-9a-fA-F]+::switchdataD_([0-9a-fA-F]{8})$")
LN = re.compile(r"^\$LN\d+$")


def diff_json(unit, symbol):
    out = os.path.join(ROOT, "build", "diffs", "%s__%s.json" % (unit.replace("/", "_"), re.sub(r"[^\w]", "_", symbol)[:80]))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    subprocess.run([OBJDIFF, "diff", "-p", ROOT, "-u", unit, "-o", out, "--format", "json", symbol],
                   cwd=ROOT, capture_output=True)
    return json.load(open(out))


def sym(side, name):
    for s in side["symbols"]:
        if s.get("name") == name:
            return s
    return None


def rows(d, symbol):
    l, r = sym(d["left"], symbol), sym(d["right"], symbol)
    li = (l or {}).get("instructions") or []
    ri = (r or {}).get("instructions") or []
    return l, r, li, ri


def reloc_name(side, ins):
    rel = (ins.get("instruction") or {}).get("relocation")
    if not rel or "target_symbol" not in rel:
        return None
    idx = int(rel["target_symbol"])
    syms = side["symbols"]
    return syms[idx].get("name") if 0 <= idx < len(syms) else None


def reloc_addend(ins):
    rel = (ins.get("instruction") or {}).get("relocation") or {}
    return int(rel.get("addend", "0"))


def show(unit, symbol, full=False, context=3):
    """Retail and ours side by side. By default only the differing rows
    with a few rows of context; runs of identical rows are collapsed
    (--full prints every row)."""
    d = diff_json(unit, symbol)
    l, r, li, ri = rows(d, symbol)
    print("%s  retail %s%%  ours %s%%" % (symbol, (l or {}).get("match_percent"), (r or {}).get("match_percent")))
    pairs = list(zip(li + [{}] * (len(ri) - len(li)), ri + [{}] * (len(li) - len(ri))))
    marks = [bool(a.get("diff_kind") or b.get("diff_kind")) for a, b in pairs]
    keep = set(range(len(pairs))) if full else set()
    if not full:
        for i, m in enumerate(marks):
            if m:
                keep.update(range(max(0, i - context), min(len(pairs), i + context + 1)))
    skipped = 0
    for i, (a, b) in enumerate(pairs):
        if i not in keep:
            skipped += 1
            continue
        if skipped:
            print("  ... %d identical rows ..." % skipped)
            skipped = 0
        fa = (a.get("instruction") or {}).get("formatted", "")
        fb = (b.get("instruction") or {}).get("formatted", "")
        mark = "*" if marks[i] else " "
        print("%s %-48s | %-48s %s" % (mark, fa[:48], fb[:48], a.get("diff_kind", "") or b.get("diff_kind", "")))
    if skipped:
        print("  ... %d identical rows ..." % skipped)
    print("%d rows, %d differ" % (len(pairs), sum(marks)))


def report_functions(units):
    # the report of the last `build.py check <unit>` is newer than the
    # project report, which only a full check rewrites
    whole = None
    for unit in units:
        own = os.path.join(ROOT, "build", "check_project", unit.replace("/", "_"), "report.json")
        if os.path.exists(own):
            rep = json.load(open(own))
        else:
            if whole is None:
                whole = json.load(open(os.path.join(ROOT, "build", "report.json")))
            rep = whole
        for u in rep.get("units", []):
            if u["name"] == unit:
                for fn in u.get("functions", []):
                    yield u["name"], fn["name"], fn.get("fuzzy_match_percent", 0.0)


_IMAGE = {}


def retail_bytes(dll, va, n):
    """Bytes of the retail 1.6a image at va (for validating constant names)."""
    import pefile
    paths = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll", "client": "versions/1.6a/cryoffear/cl_dlls/client.dll",
             "opengl32": "versions/1.6a/opengl32.dll", "hw": "versions/1.6a/hw.dll",
             "gameui": "versions/1.6a/cryoffear/cl_dlls/GameUI.dll", "launcher": "versions/1.6a/cof.exe"}
    if dll not in _IMAGE:
        pe = pefile.PE(os.path.join(ROOT, paths[dll]), fast_load=True)
        _IMAGE[dll] = (pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
    base, img = _IMAGE[dll]
    return img[va - base:va - base + n]


_SPECIAL = ",/\\:. \n\t'-"


def msvc_number(text):
    """MSVC mangled number: one digit is 1..10, else hex in A..P ended by @."""
    if text[0].isdigit():
        return int(text[0]) + 1, text[1:]
    end = text.index("@")
    return int("".join("%X" % (ord(c) - ord("A")) for c in text[:end]), 16), text[end + 1:]


def string_prefix(mangled):
    """Decode the visible prefix of a ??_C@_0 string literal name."""
    out = bytearray()
    i = 0
    while i < len(mangled) and mangled[i] != "@":
        c = mangled[i]
        if c != "?":
            out.append(ord(c))
            i += 1
        elif mangled[i + 1] == "$":
            out.append((ord(mangled[i + 2]) - 65) * 16 + ord(mangled[i + 3]) - 65)
            i += 4
        elif mangled[i + 1].isdigit():
            out.append(ord(_SPECIAL[int(mangled[i + 1])]))
            i += 2
        else:
            out.append(ord(mangled[i + 1]) + 0x80)
            i += 2
    return bytes(out)


def constant_ok(dll, va, name):
    """__real@<hex> / __xmm@<hex> and ??_C@_0 string names encode their
    value: check it against the retail bytes."""
    m = re.match(r"^\?\?_C@_0(.*)$", name)
    if m:
        # ??_C@_0<length>@<hash>@<first 32 characters>@: the hash is the
        # JamCRC (CRC-32 without the final inversion) of the whole string
        # with its terminating NUL, so it checks the characters the visible
        # prefix leaves out
        import zlib
        length, rest = msvc_number(m.group(1))
        crc, rest = msvc_number(rest)
        prefix = string_prefix(rest)
        data = retail_bytes(dll, va, length)
        ok = (data.startswith(prefix[:length - 1]) and data[length - 1:length] == b"\0"
              and b"\0" not in data[:length - 1])
        # VC6 (/Gf string pooling) hashes literal names another way; there
        # the length and the visible prefix are all the name tells
        if dll in ("hw", "gameui", "launcher", "opengl32"):
            return ok
        return ok and (~zlib.crc32(data) & 0xFFFFFFFF) == crc
    m = re.match(r"^__(?:real|xmm)@([0-9a-f]+)$", name)
    if not m:
        return True
    hexval = m.group(1)
    want = bytes.fromhex(hexval)[::-1] if name.startswith("__real") else bytes.fromhex(hexval)[::-1]
    return retail_bytes(dll, va, len(want)) == want


def mnemonic(ins):
    for part in (ins.get("instruction") or {}).get("parts", []):
        if "opcode" in part:
            return part["opcode"].get("mnemonic")
    return None


def harvest(units, write, min_pct=80.0, strict_pct=95.0, matched=False):
    found = {}
    conflicts = set()
    for unit, symbol, pct in report_functions(units):
        # the report ignores relocation names, so a function at 100% can
        # still point at unnamed retail symbols: --matched takes those too
        if (pct >= 100.0 and not matched) or pct < min_pct:
            continue
        d = diff_json(unit, symbol)
        l, r, li, ri = rows(d, symbol)
        if len(li) != len(ri):
            continue
        for a, b in zip(li, ri):
            # only rows where the instruction itself matches and an argument
            # (the relocation) differs
            if a.get("diff_kind") not in (None, "DIFF_ARG_MISMATCH") or mnemonic(a) != mnemonic(b):
                continue
            na, nb = reloc_name(d["left"], a), reloc_name(d["right"], b)
            if not na or not nb or na == nb:
                continue
            # jump tables are part of their function (DelinkUnits): their
            # compiler labels ($LN) are local to our object and not harvested
            if SWITCH.match(na) or LN.match(nb):
                continue
            m = UNNAMED.match(na)
            if not m or nb.startswith(("$", ".", "[")) or UNNAMED.match(nb):
                continue
            # retail target = unnamed symbol + its addend; our side is
            # name + addend, so the base of our symbol is target - our addend
            va = int(m.group(1), 16) + reloc_addend(a) - reloc_addend(b)
            kind = "func" if na.startswith(("FUN_", "LAB_", "thunk_FUN")) else "data"
            key = (unit.split("/")[0], va)
            if not constant_ok(key[0], va, nb):
                print("value mismatch at %08X for %s in %s (source difference, not a name)" % (va, nb, symbol))
                continue
            # string literals and float constants are checked against the
            # retail bytes above; any other name is code correspondence only,
            # so it needs a function that is almost fully matched
            validated = nb.startswith(("??_C@", "__real@", "__xmm@"))
            if not validated and pct < strict_pct:
                continue
            prev = found.get(key)
            if prev and prev[0] != nb:
                print("conflict at %08X: %s vs %s (dropped)" % (va, prev[0], nb))
                conflicts.add(key)
                continue
            found[key] = (nb, kind, "%s:objdiff:%s:%s (%.1f%%)" % (
                "bytes" if validated else "aligned reference", unit, symbol, pct))
    for key in conflicts:
        found.pop(key, None)
    # one name, several addresses: the layout behind the name changed (for
    # example a struct accessed at different offsets), so none is trusted
    by_name = {}
    for key, v in found.items():
        if v[1] != "label":  # jump table labels are local to their object
            by_name.setdefault((key[0], v[0]), []).append(key)
    for (dll, name), keys in by_name.items():
        if len(keys) > 1:
            print("%s maps to %d addresses (dropped)" % (name, len(keys)))
            for key in keys:
                found.pop(key)
    for (dll, va), (name, kind, ev) in sorted(found.items()):
        print("%s %08X %-5s %s" % (dll, va, kind, name))
    if write and found:
        by_dll = {}
        for (dll, va), v in found.items():
            by_dll.setdefault(dll, {})[va] = v
        for dll, items in by_dll.items():
            path = os.path.join(ROOT, "tools", "symbols", "%s_auto.csv" % dll)
            old = {}
            if os.path.exists(path):
                for row in csv.DictReader(open(path)):
                    old[int(row["va"], 16)] = (row["name"], row["kind"], row["evidence"])
            old.update(items)
            with open(path, "w", newline="") as f:
                w = csv.writer(f, lineterminator="\n")
                w.writerow(["va", "name", "kind", "evidence"])
                for va in sorted(old):
                    w.writerow(["%08X" % va] + list(old[va]))
            print("wrote %d entries to %s" % (len(old), os.path.relpath(path, ROOT)))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    if sys.argv[1] == "show":
        show(sys.argv[2], sys.argv[3], full="--full" in sys.argv)
    elif sys.argv[1] == "harvest":
        args = [a for a in sys.argv[2:] if not a.startswith("--")]
        harvest(args, "--write" in sys.argv, matched="--matched" in sys.argv)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
