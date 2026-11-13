"""Take one member out of an official static library for a VC6 link.

  python tools/libmember.py <lib> <member> <out.obj>

Newer libraries (the Windows SDK dinput8.lib) carry CodeView debug sections
that link 6.00 refuses under /debug ("debugging information corrupt"). The
member is extracted with lib.exe and its .debug$S/.debug$T/.debug$F sections
are renamed .drop$S/... and marked IMAGE_SCN_LNK_REMOVE, so the linker
leaves them out; code, data, relocations and symbols are unchanged.
"""
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import msvc  # noqa: E402


def main():
    lib, member, out = sys.argv[1:4]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    tools = os.path.dirname(msvc.link_path())
    # members are listed with the path they were built under: match the name
    listing = subprocess.run([os.path.join(tools, "lib.exe"), "/nologo", "/list", lib], env=msvc.env(),
                             capture_output=True, text=True).stdout.splitlines()
    full = [m.strip() for m in listing if os.path.basename(m.strip().replace("\\", "/")).lower() == member.lower()]
    if len(full) != 1:
        sys.exit("%s: member %s found %d times" % (lib, member, len(full)))
    member = full[0]
    rc = subprocess.call([os.path.join(tools, "lib.exe"), "/nologo", "/extract:" + member, "/out:" + out, lib],
                         env=msvc.env(), stdout=subprocess.DEVNULL)
    if rc != 0:
        sys.exit("lib.exe /extract failed (%d)" % rc)
    data = bytearray(open(out, "rb").read())
    nsec, opt = struct.unpack_from("<H", data, 2)[0], struct.unpack_from("<H", data, 16)[0]
    dropped = 0
    for k in range(nsec):
        h = 20 + opt + 40 * k
        name = bytes(data[h:h + 8]).rstrip(b"\0")
        if name.startswith(b".debug$"):
            data[h:h + 8] = (b".drop$" + name[7:]).ljust(8, b"\0")[:8]
            flags = struct.unpack_from("<I", data, h + 36)[0] | 0x800
            struct.pack_into("<I", data, h + 36, flags)
            dropped += 1
    open(out, "wb").write(bytes(data))
    print("%s(%s) -> %s, %d debug sections dropped" % (os.path.basename(lib), member, out, dropped))


if __name__ == "__main__":
    main()
