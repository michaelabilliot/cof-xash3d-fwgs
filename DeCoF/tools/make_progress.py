"""Generate docs/progress.csv: every function of the 1.6a client.dll and hl.dll.

Usage: python tools/make_progress.py

Inputs: build/features/*_1.6a.jsonl (all functions), build/match/*_refnames.csv
(names and evidence), build/match/hl_1.55_to_1.6a.csv (1.55 objects),
build/ref_index.json (ancestry), docs/symbols_{1.0,1.4,1.55}.csv and the
older DLLs (first version, changes between the 2012 builds). Rows already
present in docs/progress.csv keep their status, match_percent and notes.

Classes: HLSDK, SPIRIT, PARANOIA, COF (by where the function name is defined:
HLSDK first, then Spirit 1.7, then Paranoia 1.2, otherwise CoF), LIB (static
CRT, STL instantiations and compiler-generated helpers: not decompiled, they
come from the toolchain), UNKNOWN (not named yet). Thunks and exception
handling funclets are left out (compiler generated, part of other functions).
"""
import collections
import csv
import json
import os
import re
import struct
import sys

import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from refmatch import short  # noqa: E402
from pdbfile import PDB  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DLLS = {
    "hl": {"boundary": 0x10121FE5, "side": "server"},
    "client": {"boundary": 0x100B42A4, "side": "client"},
}
OLD_HL = {
    "1.0": ("versions/1.0/CryOfFear/cl_dlls/hl.dll", "docs/symbols_1.0.csv"),
    "1.4": ("versions/hl.dll", "docs/symbols_1.4.csv"),
    "1.55": ("versions/1.55/cl_dlls/hl.dll", "docs/symbols_1.55.csv"),
}
FIELDS = ["dll", "va", "size", "name", "demangled", "class", "source_file", "first_version",
          "changed", "status", "match_percent", "evidence", "notes"]
COMPILER_HELPERS = ("`vector constructor iterator'", "`vector destructor iterator'",
                    "`eh vector constructor iterator'", "`eh vector destructor iterator'",
                    "`vector deleting destructor'", "`scalar deleting destructor'")


# UCRT stdio functions defined inline in the SDK headers (emitted into game objects)
CRT_INLINE = {"sprintf", "_snprintf", "snprintf", "fprintf", "printf", "sscanf", "vsprintf",
              "_vsnprintf", "vsnprintf", "_vsprintf_l", "_vsnprintf_l", "_vfprintf_l", "_sscanf_l",
              "_vsscanf_l", "swprintf", "_swprintf", "__local_stdio_printf_options",
              "__local_stdio_scanf_options"}


def lookup(dem, names):
    """Name lookup tolerant of Windows A/W macro renames and anonymous namespaces."""
    if dem in names:
        return True
    d = dem.replace("`anonymous namespace'::", "")
    if d in names:
        return True
    return d.endswith("A") and d[:-1] in names


def p(*parts):
    return os.path.join(ROOT, *parts)


def load_features(dll):
    funcs = {}
    for line in open(p("build", "features", "%s_1.6a.jsonl" % dll)):
        r = json.loads(line)
        if r["t"] == "func":
            funcs[r["ea"]] = r
    return funcs


def pdb_sources(pdb_path):
    """object name (lowercase basename without .obj) -> source path relative to the source root"""
    pdb = PDB(pdb_path)
    out = {}
    for m in pdb.modules:
        base = os.path.basename(m.name).lower()
        if not base.endswith(".obj"):
            continue
        cpp = [f for f in pdb.module_files[m.index] if f.lower().endswith((".cpp", ".c"))]
        if cpp:
            path = cpp[0].replace("\\", "/")
            path = re.sub(r"^.*?/sourcecode/", "", path, flags=re.I)
            out[base[:-4]] = path
    return out


def ref_source(evidence):
    # refbytes:paranoia:dlls__cbase.obj:score3 -> dlls/cbase.cpp
    parts = evidence.split(":")
    if parts[0] == "refbytes" and len(parts) >= 3:
        obj = parts[2]
        stem = obj[:-4] if obj.lower().endswith(".obj") else obj
        rel = stem.replace("__", "/")
        ext = ".c" if rel.startswith("pm_shared/") else ".cpp"
        return rel + ext
    return ""


def masked(data, base, va, size, relocs):
    b = bytearray(data[va - base:va - base + size])
    for k in range(len(b)):
        if va + k in relocs:
            b[k:k + 4] = b"\0\0\0\0"
    k = 0
    while k < len(b) - 4:
        if b[k] in (0xE8, 0xE9):
            b[k + 1:k + 5] = b"\0\0\0\0"
            k += 5
        else:
            k += 1
    return bytes(b)


def old_builds():
    """name -> {version: masked bytes} for the 2012 hl.dll builds."""
    out = collections.defaultdict(dict)
    for ver, (dll, syms) in OLD_HL.items():
        pe = pefile.PE(p(dll), fast_load=True)
        pe.parse_data_directories(directories=[5])
        base = pe.OPTIONAL_HEADER.ImageBase
        img = pe.get_memory_mapped_image()
        relocs = set()
        for blk in getattr(pe, "DIRECTORY_ENTRY_BASERELOC", []):
            for e in blk.entries:
                if e.type == 3:
                    relocs.add(base + e.rva)
        for r in csv.DictReader(open(p(syms))):
            if not r["kind"].startswith("func") or not r["size"]:
                continue
            name = r["decorated"] or r["name"]
            out[name][ver] = masked(img, base, int(r["va"], 16), int(r["size"]), relocs)
    return out


def change_summary(versions):
    order = [v for v in ("1.0", "1.4", "1.55") if v in versions]
    if len(order) < 2:
        return ""
    steps = []
    for x, y in zip(order, order[1:]):
        steps.append("%s->%s %s" % (x, y, "same" if versions[x] == versions[y] else "changed"))
    return "; ".join(steps)


# VC6-built Valve components: names come from the Linux build (ELF symbols)
ENGINE_BINS = {"hw": "hw", "gameui": "gameui", "launcher": "launcher"}
LIB_NAME = re.compile(r"^(_|__|@|operator|`|std::|type_info)|^(mem|str|wcs|mbs|sprintf|printf|fopen|fread|fwrite|fclose|malloc|free|realloc|calloc|atoi|atof|qsort|rand|srand|time|localtime)")


def engine_rows(stats):
    existing = {}
    if os.path.exists(p("docs", "progress.csv")):
        for r in csv.DictReader(open(p("docs", "progress.csv"))):
            existing[(r["dll"], r["va"])] = r
    out = []
    for dll, tag in ENGINE_BINS.items():
        path = p("build", "features", "%s_1.6a.jsonl" % tag)
        if not os.path.exists(path):
            continue
        funcs = load_features(tag)
        mpath = p("build", "match", "%s_linux_to_1.6a.csv" % tag)
        names = {int(r["to_va"], 16): r for r in csv.DictReader(open(mpath))} if os.path.exists(mpath) else {}
        for ea in sorted(funcs):
            f = funcs[ea]
            if f["thunk"] or f["name"].startswith(("Unwind@", "Catch@", "Catch_All@")):
                stats[(dll, "skipped")] += 1
                continue
            n = names.get(ea)
            if n:
                dem, evidence = n["demangled"], "linux_symbols:%s:%s" % (n["method"], n["score"])
            elif not f["name"].startswith("FUN_"):
                dem, evidence = f["name"], "ghidra"
            else:
                dem, evidence = "", ""
            if not dem:
                cls = "UNKNOWN"
            elif evidence == "ghidra" and (LIB_NAME.search(dem) or "vgui::" in dem):
                cls = "LIB"
            else:
                cls = "ENGINE"
            prev = existing.get((dll, "%08X" % ea), {})
            out.append({"dll": dll, "va": "%08X" % ea, "size": f["size"], "name": "", "demangled": dem,
                        "class": cls, "source_file": "", "first_version": "", "changed": "",
                        "status": prev.get("status") or ("" if cls == "LIB" else "TODO"),
                        "match_percent": prev.get("match_percent", ""), "evidence": evidence,
                        "notes": prev.get("notes", "")})
            stats[(dll, cls)] += 1
    return out


# binaries whose names come from a link map of our own matching rebuild
SYMBOL_MAP_BINS = {"opengl32": ("PARANOIA", "src/opengl32/")}


def symbol_map_rows(stats):
    existing = {}
    if os.path.exists(p("docs", "progress.csv")):
        for r in csv.DictReader(open(p("docs", "progress.csv"))):
            existing[(r["dll"], r["va"])] = r
    cfg = json.load(open(p("tools", "units.json")))["dlls"]
    out = []
    for dll, (cls, src_dir) in SYMBOL_MAP_BINS.items():
        fpath = p("build", "features", "%s_1.6a.jsonl" % dll)
        if not os.path.exists(fpath):
            continue
        funcs = load_features(dll)
        names = {int(r["va"], 16): r for r in csv.DictReader(open(p("tools", "symbols", "%s_1.6a.csv" % dll)))}
        src_of = {}
        for u in cfg.get(dll, {}).get("units", []):
            for n in open(p(u["functions_file"])).read().split():
                src_of[n] = u["source"]
        for ea in sorted(funcs):
            f = funcs[ea]
            if f["thunk"]:
                stats[(dll, "skipped")] += 1
                continue
            n = names.get(ea)
            name = n["name"] if n else ""
            prev = existing.get((dll, "%08X" % ea), {})
            out.append({"dll": dll, "va": "%08X" % ea, "size": f["size"], "name": name,
                        "demangled": short(name) if name else "", "class": cls if name else "UNKNOWN",
                        "source_file": src_of.get(name, ""), "first_version": "2010-prototype",
                        "changed": "same file in prototype, 1.0 and 1.6a",
                        "status": prev.get("status") or "TODO", "match_percent": prev.get("match_percent", ""),
                        "evidence": n["evidence"] if n else "", "notes": prev.get("notes", "")})
            stats[(dll, cls if name else "UNKNOWN")] += 1
    return out


def main():
    index = json.load(open(p("build", "ref_index.json")))
    existing = {}
    if os.path.exists(p("docs", "progress.csv")):
        for r in csv.DictReader(open(p("docs", "progress.csv"))):
            existing[(r["dll"], r["va"])] = r
    proto_exports = set()
    pe = pefile.PE(p("versions/2010-prototype/The build/CryOfFear/cl_dlls/server.dll"), fast_load=True)
    pe.parse_data_directories(directories=[0])
    proto_exports = {e.name.decode() for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    olds = old_builds()
    src_155 = pdb_sources(p("versions/1.55/cl_dlls/hl.pdb"))
    vmatch = {int(r["to_va"], 16): r for r in csv.DictReader(open(p("build", "match", "hl_1.55_to_1.6a.csv")))}

    rows = []
    stats = collections.Counter()
    for dll, cfg in DLLS.items():
        funcs = load_features(dll)
        names = {int(r["va"], 16): r for r in csv.DictReader(open(p("build", "match", "%s_1.6a_refnames.csv" % dll)))}
        side = cfg["side"]
        sets = {t: set(index["%s/%s" % (t, side)]) for t in ("HLSDK", "SPIRIT", "PARANOIA")}
        crt_names = {int(r["va"], 16): (r["symbol"], r["candidates"] == "1") for r in csv.DictReader(
            open(p("build", "match", "%s_1.6a_crt.csv" % dll)))}
        dll_rows = []
        for ea in sorted(funcs):
            f = funcs[ea]
            if f["thunk"] or f["name"].startswith(("Unwind@", "Catch@", "Catch_All@")):
                stats[(dll, "skipped")] += 1
                continue
            n = names.get(ea)
            name = n["name"] if n else ""
            evidence = n["evidence"] if n else ""
            dem = short(name) if name else ""
            src = ""
            if ea >= cfg["boundary"]:
                cls = "LIB"
                if ea in crt_names:
                    name, uniq = crt_names[ea]
                    evidence = "crt_bytes" if uniq else "crt_bytes_ambiguous"
                    dem = short(name)
                if not name and not f["name"].startswith("FUN_"):
                    dem = f["name"]
            elif not name:
                cls = "UNKNOWN"
            elif evidence.startswith("%s_1.6a_crt" % dll) or "std::" in dem \
                    or any(h in dem for h in COMPILER_HELPERS) \
                    or dem.startswith(("type_info::", "__", "vgui::")) or dem in CRT_INLINE:
                cls = "LIB"
            elif lookup(dem, sets["HLSDK"]):
                cls = "HLSDK"
            elif lookup(dem, sets["SPIRIT"]):
                cls = "SPIRIT"
            elif lookup(dem, sets["PARANOIA"]):
                cls = "PARANOIA"
            else:
                cls = "COF"
                # implicit constructor / assignment of an ancestor class: the
                # compiler generates it from the class definition
                parts = dem.split("::")
                if len(parts) == 2 and (parts[1] == parts[0] or parts[1] == "operator="):
                    for t in ("HLSDK", "SPIRIT", "PARANOIA"):
                        if any(n.startswith(parts[0] + "::") for n in sets[t]):
                            cls = t
                            break
            header = ""
            for t in ("HLSDK", "SPIRIT", "PARANOIA"):
                path = index["%s/%s" % (t, side)].get(dem, "")
                if path.lower().endswith(".h"):
                    header = path
                    break
                if path:
                    break
            if cls not in ("LIB",) and header:
                src = header
            elif cls not in ("LIB",):
                if dll == "hl" and ea in vmatch and vmatch[ea]["object"]:
                    obj = os.path.basename(vmatch[ea]["object"]).lower()
                    src = src_155.get(obj[:-4] if obj.endswith(".obj") else obj, "")
                if not src:
                    src = ref_source(evidence)
            first = ""
            changed = ""
            if dll == "hl" and name:
                if name in proto_exports or dem in proto_exports:
                    first = "2010-prototype"
                vers = olds.get(name) or {}
                if not first:
                    for v in ("1.0", "1.4", "1.55"):
                        if v in vers:
                            first = v
                            break
                if not first and cls == "COF":
                    first = "1.6a"
                changed = change_summary(vers)
            prev = existing.get((dll, "%08X" % ea), {})
            row = {"dll": dll, "va": "%08X" % ea, "size": f["size"], "name": name, "demangled": dem,
                   "class": cls, "source_file": src, "first_version": first, "changed": changed,
                   "status": prev.get("status") or ("" if cls == "LIB" else "TODO"),
                   "match_percent": prev.get("match_percent", ""), "evidence": evidence,
                   "notes": prev.get("notes", "")}
            dll_rows.append(row)
        # source file guess for unnamed functions between two functions of the same file
        for i, r in enumerate(dll_rows):
            if r["source_file"] or r["class"] == "LIB":
                continue
            before = next((x["source_file"] for x in reversed(dll_rows[:i]) if x["source_file"]), "")
            after = next((x["source_file"] for x in dll_rows[i + 1:] if x["source_file"]), "")
            if before and before == after:
                r["source_file"] = "guess:" + before
        for r in dll_rows:
            stats[(dll, r["class"])] += 1
        rows += dll_rows

    rows += engine_rows(stats)
    rows += symbol_map_rows(stats)

    with open(p("docs", "progress.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=FIELDS, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    for dll in list(DLLS) + list(ENGINE_BINS) + list(SYMBOL_MAP_BINS):
        print(dll, {k[1]: v for k, v in stats.items() if k[0] == dll})


if __name__ == "__main__":
    main()
