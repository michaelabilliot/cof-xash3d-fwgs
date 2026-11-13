"""Make an import library for a retail third-party DLL from its export table.

  python tools/implib.py <retail.dll> <out.lib> [<object dir>]

Out-of-scope DLLs that the game DLLs import from (steam_api.dll) ship
without their .lib. This writes a .def file with every exported name of the
retail DLL, by name in the DLL's own order, and runs lib.exe of
the target toolchain on it. Nothing of the DLL's code is used.

__stdcall functions are exported undecorated (DevIL's ilInit) but imported
as __imp__ilInit@0: with an object directory, every __imp__<name>@<n> that
our objects reference is written as <name>@<n>, which lib.exe turns back
into the undecorated import name.
"""
import glob
import os
import re
import subprocess
import sys

import pefile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import msvc  # noqa: E402


def main():
    dll, out = sys.argv[1], sys.argv[2]
    pe = pefile.PE(dll, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    # the name the DLL gives itself (DevIL.dll), which importers record,
    # rather than the file name on disk (devil.dll)
    name = pe.get_string_at_rva(pe.DIRECTORY_ENTRY_EXPORT.struct.Name).decode() or os.path.basename(dll)
    stdcall = {}
    if len(sys.argv) > 3:
        for obj in glob.glob(os.path.join(sys.argv[3], "**", "*.obj"), recursive=True):
            for m in re.findall(rb"__imp__([A-Za-z_][A-Za-z0-9_]*)@([0-9]+)", open(obj, "rb").read()):
                stdcall[m[0].decode()] = m[1].decode()
    lines = ["LIBRARY %s" % name, "EXPORTS"]
    asis = set()
    for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if s.name:
            n = s.name.decode()
            if re.match(r"_[A-Za-z_][A-Za-z0-9_]*@[0-9]+$", n):
                # the DLL exports the decorated name itself (Miles mss32.dll:
                # _AIL_startup@0): import it under exactly that name
                lines.append("\t%s" % n[1:])
                asis.add(n)
            else:
                lines.append("\t%s@%s" % (n, stdcall[n]) if n in stdcall else "\t%s" % n)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    deffile = os.path.splitext(out)[0] + ".def"
    with open(deffile, "w") as f:
        f.write("\n".join(lines) + "\n")
    tools = os.path.dirname(msvc.link_path())
    rc = subprocess.call([os.path.join(tools, "lib.exe"), "/nologo", "/machine:x86", "/def:" + deffile,
                          "/out:" + out], env=msvc.env(), stdout=subprocess.DEVNULL)
    if rc != 0:
        sys.exit("lib.exe failed (%d)" % rc)
    fixed = undecorate(out, asis) if stdcall or asis else 0
    print("%s: %d exports -> %s%s" % (name, len(lines) - 2, out,
                                      ", %d stdcall imports by undecorated name" % fixed if fixed else ""))


def undecorate(lib, asis=()):
    """lib.exe gives a .def entry name@n the import name type NAME_NOPREFIX,
    so the importer asks the DLL for "name@n". The DLL exports "name": set
    the short import records of decorated names to NAME_UNDECORATE, which
    makes the loader look up the name without the underscore and the @n.
    A name in asis is exported decorated (_name@n): its record gets NAME,
    the symbol name as it is."""
    data = bytearray(open(lib, "rb").read())
    pos, fixed = 8, 0  # after "!<arch>\n"
    while pos + 60 <= len(data):
        size = int(data[pos + 48:pos + 58].decode().strip())
        body = pos + 60
        if data[body:body + 4] == b"\0\0\xff\xff":
            sym = data[body + 20:data.index(b"\0", body + 20)]
            word = int.from_bytes(data[body + 18:body + 20], "little")
            want = 1 if bytes(sym).decode("latin-1") in asis else 3
            if re.match(rb"_[A-Za-z_][A-Za-z0-9_]*@[0-9]+$", bytes(sym)) and (word >> 2) & 7 != want:
                word = (word & ~(7 << 2)) | (want << 2)
                data[body + 18:body + 20] = word.to_bytes(2, "little")
                fixed += 1
        pos = body + size + (size & 1)
    open(lib, "wb").write(bytes(data))
    return fixed


if __name__ == "__main__":
    main()
