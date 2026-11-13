"""Write the resources of a retail PE as a .res file for the linker.

  python tools/res_from_pe.py <pe> <out.res>

The resources of a retail binary (icons, version information) are assets,
not source: the build takes them from the retail file under versions/
(read only) instead of committing an .rc file with the icons. Every
resource keeps its type, name, language and data bytes; tools/build.py link
passes the .res to the linker (the "resources" entry of a link section in
tools/units.json), which converts it with cvtres.
"""
import struct
import sys

import pefile


def name_bytes(entry):
    """A type or name of a .res header: 0xFFFF + id, or a NUL-terminated UTF-16 string."""
    if entry.name is not None:
        return str(entry.name).encode("utf-16-le") + b"\0\0"
    return struct.pack("<HH", 0xFFFF, entry.id)


def pad4(b):
    return b + b"\0" * (-len(b) % 4)


def res_entry(type_b, name_b, lang, data, flags=0x1030):
    hdr = pad4(type_b + name_b)
    # DataVersion, MemoryFlags (MOVEABLE | PURE | DISCARDABLE as rc writes for
    # icons; cvtres does not store them in the image), LanguageId, Version,
    # Characteristics
    hdr += struct.pack("<IHHII", 0, flags, lang, 0, 0)
    head = struct.pack("<II", len(data), 8 + len(hdr)) + hdr
    return head + pad4(data)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    pe = pefile.PE(sys.argv[1])
    out = [res_entry(struct.pack("<HH", 0xFFFF, 0), struct.pack("<HH", 0xFFFF, 0), 0, b"", 0)]
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        for n in t.directory.entries:
            for lang in n.directory.entries:
                d = lang.data.struct
                data = pe.get_data(d.OffsetToData, d.Size)
                out.append(res_entry(name_bytes(t), name_bytes(n), lang.id, data))
    with open(sys.argv[2], "wb") as f:
        f.write(b"".join(out))


if __name__ == "__main__":
    main()
