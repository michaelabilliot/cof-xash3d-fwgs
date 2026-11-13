"""Inventory the game DLLs of every version and write docs/versions.csv.

Usage: python tools/pe_inventory.py [--out docs/versions.csv]
"""
import argparse
import csv
import datetime
import hashlib
import os
import struct
import sys

import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pdbfile import PDB  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (version, role, path relative to ROOT, note)
TARGETS = [
    ("2010-prototype", "client", "versions/2010-prototype/The build/CryOfFear/cl_dlls/client.dll", ""),
    ("2010-prototype", "server", "versions/2010-prototype/The build/CryOfFear/cl_dlls/server.dll", "gamedll in liblist.gam; predecessor of hl.dll"),
    ("2010-prototype", "server", "versions/2010-prototype/The build/CryOfFear/cl_dlls/Copy of server.dll", "older backup copy, not loaded"),
    ("2010-prototype", "opengl32", "versions/2010-prototype/The build/opengl32.dll", ""),
    ("2010-prototype", "opengl32", "versions/2010-prototype/The build/parnaoia_opengl32.dll", "renamed, not loaded"),
    ("1.0", "client", "versions/1.0/CryOfFear/cl_dlls/client.dll", ""),
    ("1.0", "server", "versions/1.0/CryOfFear/cl_dlls/hl.dll", ""),
    ("1.0", "opengl32", "versions/1.0/opengl32.dll", ""),
    ("1.4", "server", "versions/hl.dll", "loose file in versions/ root; identified as 1.4 by date and Ghidra log"),
    ("1.55", "client", "versions/1.55/cl_dlls/client.dll", ""),
    ("1.55", "server", "versions/1.55/cl_dlls/hl.dll", ""),
    ("1.6a", "client", "versions/1.6a/cryoffear/cl_dlls/client.dll", "target"),
    ("1.6a", "server", "versions/1.6a/cryoffear/cl_dlls/hl.dll", "target"),
    ("1.6a", "opengl32", "versions/1.6a/opengl32.dll", "target"),
    ("1.6a", "gameui", "versions/1.6a/cryoffear/cl_dlls/GameUI.dll", "target (scope item 5)"),
    ("1.6a", "engine", "versions/1.6a/hw.dll", "target (scope item 3); sw.dll is identical"),
    ("1.6a", "launcher", "versions/1.6a/cof.exe", "target (scope item 6)"),
    ("1.6a", "launcher", "versions/1.6a/CoFLaunchApp.exe", "target (scope item 7), .NET"),
    ("2010-prototype", "engine", "versions/2010-prototype/The build/hw.dll", "prototype engine"),
]

# Rich header product ids that occur in VC6 to VS2012 era binaries.
RICH_PRODIDS = {
    0x00: "Import0", 0x01: "Import", 0x02: "Linker510", 0x04: "Linker600",
    0x06: "Cvtres500", 0x0A: "Utc12_C", 0x0B: "Utc12_CPP", 0x0E: "Masm613",
    0x0F: "Masm710", 0x12: "Masm614", 0x13: "Linker512", 0x15: "Utc12_C_Std",
    0x16: "Utc12_CPP_Std", 0x19: "Implib700", 0x1C: "Utc13_C",
    0x1D: "Utc13_CPP", 0x2A: "Masm615", 0x2D: "Masm620", 0x30: "Utc12_2_C",
    0x31: "Utc12_2_CPP", 0x3D: "Linker700", 0x3F: "Export700",
    0x40: "Masm700", 0x45: "Cvtres700", 0x5A: "Linker710", 0x5B: "Cvtomf710",
    0x5C: "Export710", 0x5D: "Implib710", 0x5E: "Cvtres710",
    0x5F: "Utc1310_C", 0x60: "Utc1310_CPP", 0x61: "Utc1310_C_Std",
    0x62: "Utc1310_CPP_Std", 0x63: "Utc1310_LTCG_C", 0x64: "Utc1310_LTCG_CPP",
    0x6D: "Utc1400_C", 0x6E: "Utc1400_CPP", 0x71: "Utc1400_LTCG_C",
    0x72: "Utc1400_LTCG_CPP", 0x78: "Linker800", 0x7A: "Export800",
    0x7B: "Implib800", 0x7C: "Cvtres800", 0x7D: "Masm800", 0x83: "Utc1500_C",
    0x84: "Utc1500_CPP", 0x89: "Utc1500_LTCG_C", 0x8A: "Utc1500_LTCG_CPP",
    0x91: "Linker900", 0x92: "Export900", 0x93: "Implib900",
    0x94: "Cvtres900", 0x95: "Masm900", 0x9A: "Cvtres1000",
    0x9B: "Export1000", 0x9C: "Implib1000", 0x9D: "Linker1000",
    0x9E: "Masm1000", 0xAA: "Utc1600_C", 0xAB: "Utc1600_CPP",
    0xAE: "Utc1600_LTCG_C", 0xAF: "Utc1600_LTCG_CPP", 0xC9: "Cvtres1100",
    0xCA: "Export1100", 0xCB: "Implib1100", 0xCC: "Linker1100",
    0xCD: "Masm1100", 0xCE: "Utc1700_C", 0xCF: "Utc1700_CPP",
    0xC7: "AliasObj1100",
    # VS2015 and later (toolset 14.x, compiler 19.x)
    0xFD: "AliasObj1400", 0xFE: "Cvtpgd1900", 0xFF: "Cvtres1400",
    0x100: "Export1400", 0x101: "Implib1400", 0x102: "Linker1400",
    0x103: "Masm1400", 0x104: "Utc1900_C", 0x105: "Utc1900_CPP",
    0x108: "Utc1900_LTCG_C", 0x109: "Utc1900_LTCG_CPP",
}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def rich_entries(pe):
    rich = getattr(pe, "RICH_HEADER", None)
    if rich is None:
        return []
    vals = rich.values
    out = []
    for i in range(0, len(vals), 2):
        comp_id, count = vals[i], vals[i + 1]
        prodid, build = comp_id >> 16, comp_id & 0xFFFF
        name = RICH_PRODIDS.get(prodid, "prodid_0x%02X" % prodid)
        out.append("%s.%d x%d" % (name, build, count))
    return out


# (Rich product family, build) -> human readable tool version
TOOL_NAMES = {
    ("Utc12", 8168): "cl 12.00.8168 (Visual C++ 6.0 RTM)",
    ("Linker600", 8168): "link 6.00.8168 (Visual C++ 6.0 RTM)",
    ("Utc1310", 3077): "cl 13.10.3077 (Visual C++ .NET 2003 RTM)",
    ("Linker710", 3077): "link 7.10.3077 (Visual C++ .NET 2003 RTM)",
    ("Utc1900", 29112): "cl 19.27.29112 (Visual Studio 2019 16.7, toolset v142 14.27)",
    ("Linker1400", 29112): "link 14.27.29112 (Visual Studio 2019 16.7, toolset v142 14.27)",
}


def toolchain(pe):
    """Return (compiler, linker) for the image from its Rich header.

    The linker is the Linker* entry. The compiler is the Utc* family and
    build used by the most objects that share the linker's build number
    (the project's own code), falling back to the most used Utc* entry.
    """
    rich = getattr(pe, "RICH_HEADER", None)
    if rich is None:
        return "", ""
    entries = []
    vals = rich.values
    for i in range(0, len(vals), 2):
        prodid, build = vals[i] >> 16, vals[i] & 0xFFFF
        entries.append((RICH_PRODIDS.get(prodid, ""), build, vals[i + 1]))
    linkers = [(n, b) for n, b, c in entries if n.startswith("Linker")]
    linker = linkers[-1] if linkers else None
    utc = [(n, b, c) for n, b, c in entries if n.startswith("Utc")]
    own = [e for e in utc if linker and e[1] == linker[1]] or utc
    compiler = None
    if own:
        fam = {}
        for n, b, c in own:
            key = (n.split("_")[0], b)
            fam[key] = fam.get(key, 0) + c
        compiler = max(fam.items(), key=lambda kv: kv[1])[0]
    cname = TOOL_NAMES.get(compiler, "%s.%d" % compiler if compiler else "")
    if compiler and any(n.endswith("_Std") for n, b, c in own if b == compiler[1]):
        cname += " Standard Edition"
    lname = TOOL_NAMES.get(linker, "%s.%d" % linker if linker else "")
    return cname, lname


def codeview(pe):
    for entry in getattr(pe, "DIRECTORY_ENTRY_DEBUG", []):
        if entry.struct.Type != 2:
            continue
        raw = pe.get_data(entry.struct.AddressOfRawData, entry.struct.SizeOfData)
        if raw[:4] == b"RSDS":
            d1, d2, d3 = struct.unpack_from("<IHH", raw, 4)
            guid = "%08X%04X%04X%s" % (d1, d2, d3, raw[12:20].hex().upper())
            age = struct.unpack_from("<I", raw, 20)[0]
            path = raw[24:].split(b"\x00")[0].decode("latin-1")
            return path, guid, age, "RSDS"
        if raw[:4] == b"NB10":
            sig, age = struct.unpack_from("<II", raw, 8)
            path = raw[16:].split(b"\x00")[0].decode("latin-1")
            return path, "%08X" % sig, age, "NB10"
    return "", "", "", ""


def version_info(pe):
    out = {}
    for fi in getattr(pe, "FileInfo", []) or []:
        for item in fi:
            for st in getattr(item, "StringTable", []):
                for k, v in st.entries.items():
                    out[k.decode("latin-1")] = v.decode("latin-1")
    return out


def imports(pe):
    return [e.dll.decode("latin-1") for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "versions.csv"))
    args = ap.parse_args()

    fields = ["version", "role", "path", "size", "sha256", "pe_timestamp_utc",
              "pe_timestamp_hex", "image_base", "size_of_image",
              "linker_version", "subsystem_version", "sections",
              "compiler", "linker", "rich_header", "pdb_path_embedded", "cv_type", "cv_guid",
              "cv_age", "pdb_present", "pdb_matches", "pdb_compiler",
              "export_count", "imports", "file_version", "note"]
    rows = []
    for version, role, rel, note in TARGETS:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            print("missing: %s" % rel, file=sys.stderr)
            continue
        pe = pefile.PE(path)
        fh, oh = pe.FILE_HEADER, pe.OPTIONAL_HEADER
        ts = fh.TimeDateStamp
        cv_path, cv_guid, cv_age, cv_type = codeview(pe)
        pdb_path = os.path.splitext(path)[0] + ".pdb"
        pdb_present = os.path.exists(pdb_path)
        pdb_matches = ""
        pdb_compiler = ""
        if pdb_present:
            pdb = PDB(pdb_path)
            pdb_matches = "yes" if (pdb.guid == cv_guid and pdb.age == cv_age) else \
                "no (pdb %s age %d)" % (pdb.guid, pdb.age)
            seen = set()
            for m in pdb.modules:
                ci = pdb.module_compile_info(m)
                if "version_string" in ci:
                    seen.add("%s %s %s" % (ci["version_string"], ci["language"],
                                           ci["be_version"]))
            pdb_compiler = "; ".join(sorted(seen))
        exports = 0
        if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
            exports = len(pe.DIRECTORY_ENTRY_EXPORT.symbols)
        vi = version_info(pe)
        compiler, linker = toolchain(pe)
        rows.append({
            "version": version,
            "role": role,
            "path": rel,
            "size": os.path.getsize(path),
            "sha256": sha256(path),
            "pe_timestamp_utc": datetime.datetime.fromtimestamp(
                ts, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S"),
            "pe_timestamp_hex": "%08X" % ts,
            "image_base": "%08X" % oh.ImageBase,
            "size_of_image": "%08X" % oh.SizeOfImage,
            "linker_version": "%d.%02d" % (oh.MajorLinkerVersion, oh.MinorLinkerVersion),
            "subsystem_version": "%d.%02d" % (oh.MajorSubsystemVersion, oh.MinorSubsystemVersion),
            "sections": " ".join(s.Name.rstrip(b"\x00").decode("latin-1") for s in pe.sections),
            "compiler": compiler,
            "linker": linker,
            "rich_header": "; ".join(rich_entries(pe)),
            "pdb_path_embedded": cv_path,
            "cv_type": cv_type,
            "cv_guid": cv_guid,
            "cv_age": cv_age,
            "pdb_present": "yes" if pdb_present else "no",
            "pdb_matches": pdb_matches,
            "pdb_compiler": pdb_compiler,
            "export_count": exports,
            "imports": " ".join(imports(pe)),
            "file_version": vi.get("FileVersion", ""),
            "note": note,
        })
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    print("wrote %d rows to %s" % (len(rows), args.out))


if __name__ == "__main__":
    main()
