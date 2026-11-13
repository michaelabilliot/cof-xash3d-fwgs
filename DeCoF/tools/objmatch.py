"""Match functions of a linked DLL against functions in COFF objects/libraries.

Usage:
  python tools/objmatch.py <dll> <features.jsonl> <out.csv> <obj|lib|dir> [...]

Every function in the given .obj/.lib files (directories are searched for
*.obj and *.lib) is indexed with its bytes and relocation offsets. A DLL
function (address and size from the Ghidra feature export) matches a
library function when the sizes are equal and every byte outside the
library function's relocation fields is equal. Relocation fields are
skipped because their values depend on the final link.

Output columns: va, size, symbol (decorated), source (lib(member) or obj),
candidates (number of distinct symbol names that matched; >1 means the
bytes are ambiguous, typical for tiny functions), names (all candidates,
when ambiguous).

Used for:
  - the static CRT of the 1.6a build (libcmt, libvcruntime, libcpmt,
    libucrt of the exact versions the retail DLLs were linked with), and
  - reference builds of HLSDK / Paranoia / Spirit source compiled with the
    exact compiler, to find functions CoF did not change.
"""
import collections
import csv
import json
import os
import struct
import sys

IMAGE_SCN_CNT_CODE = 0x20
IMAGE_SCN_LNK_COMDAT = 0x1000


def coff_functions(data, source):
    """Yield (name, bytes, reloc_offsets) for every function in a COFF object."""
    if len(data) < 20:
        return
    machine, nsec, _ts, symoff, nsym, opt, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    if machine != 0x14C or symoff == 0:
        return
    secs = []
    for i in range(nsec):
        o = 20 + opt + 40 * i
        name = data[o:o + 8].rstrip(b"\0")
        vsize, va, rawsize, rawptr, relptr, _ln, nrel, _nln, chars = struct.unpack_from(
            "<IIIIIIHHI", data, o + 8)
        secs.append((name, rawsize, rawptr, relptr, nrel, chars))
    strtab = symoff + 18 * nsym

    def symname(p):
        raw = data[p:p + 8]
        if raw[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", raw, 4)[0]
            end = data.index(b"\0", strtab + so)
            return data[strtab + so:end].decode("latin-1")
        return raw.rstrip(b"\0").decode("latin-1")

    # collect function symbols per section
    per_sec = collections.defaultdict(list)
    i = 0
    while i < nsym:
        p = symoff + 18 * i
        value, secnum, typ, sclass, naux = struct.unpack_from("<IhHBB", data, p + 8)
        if secnum > 0 and (typ & 0xF0) == 0x20 and sclass in (2, 3):
            per_sec[secnum].append((value, symname(p)))
        i += 1 + naux
    for secnum, syms in per_sec.items():
        name, rawsize, rawptr, relptr, nrel, chars = secs[secnum - 1]
        if not chars & IMAGE_SCN_CNT_CODE or rawptr == 0:
            continue
        body = data[rawptr:rawptr + rawsize]
        relocs = []
        for r in range(nrel):
            off = struct.unpack_from("<I", data, relptr + 10 * r)[0]
            relocs.append(off)
        syms.sort()
        for k, (start, sname) in enumerate(syms):
            end = syms[k + 1][0] if k + 1 < len(syms) else rawsize
            fb = body[start:end]
            # trailing int3/nop padding is not part of the function
            while fb and fb[-1:] in (b"\xcc",) and len(fb) > 1:
                fb = fb[:-1]
            rel = [o - start for o in relocs if start <= o < start + len(fb)]
            yield sname, fb, rel


def iter_archive(path):
    data = open(path, "rb").read()
    if data[:8] != b"!<arch>\n":
        return
    off = 8
    longnames = b""
    while off + 60 <= len(data):
        hdr = data[off:off + 60]
        size = int(hdr[48:58].decode().strip())
        name = hdr[:16].decode("latin-1").strip()
        body = data[off + 60:off + 60 + size]
        off += 60 + size + (size & 1)
        if name == "//":
            longnames = body
            continue
        if name == "/" or name == "":
            continue
        if name.startswith("/") and name[1:].isdigit():
            o = int(name[1:])
            name = longnames[o:longnames.index(b"\0", o) if b"\0" in longnames[o:] else len(longnames)]
            name = name.decode("latin-1").rstrip("/\n")
        yield name.rstrip("/"), body


def index_sources(paths):
    index = collections.defaultdict(list)   # size -> [(name, bytes, relocs, source)]
    count = 0
    files = []
    for p in paths:
        if os.path.isdir(p):
            for dp, _dn, fn in os.walk(p):
                files += [os.path.join(dp, f) for f in fn if f.lower().endswith((".obj", ".lib"))]
        else:
            files.append(p)
    for f in files:
        if f.lower().endswith(".lib"):
            members = iter_archive(f)
            base = os.path.basename(f)
        else:
            members = [(os.path.basename(f), open(f, "rb").read())]
            base = None
        for member, data in members:
            src = "%s(%s)" % (base, os.path.basename(member)) if base else member
            for name, fb, rel in coff_functions(data, src):
                if len(fb) < 2:
                    continue
                index[len(fb)].append((name, fb, rel, src))
                count += 1
    return index, count


def masked_equal(target, fb, rel):
    if len(target) < len(fb):
        return False
    mask = bytearray(len(fb))
    for o in rel:
        for k in range(o, min(o + 4, len(fb))):
            mask[k] = 1
    t = target[:len(fb)]
    for k in range(len(fb)):
        if not mask[k] and t[k] != fb[k]:
            return False
    return True


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    dll, feats, out = sys.argv[1:4]
    import pefile
    pe = pefile.PE(dll, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    image = pe.get_memory_mapped_image()
    index, count = index_sources(sys.argv[4:])
    print("indexed %d functions from %d sizes" % (count, len(index)))
    funcs = [json.loads(l) for l in open(feats)]
    funcs = [f for f in funcs if f["t"] == "func" and not f["thunk"]]
    rows = []
    for f in funcs:
        ea = f["ea"]
        tb = image[ea - base:ea - base + f["size"] + 16]
        # Ghidra's size may include trailing padding or stop early; try the
        # library sizes close to it
        cands = []
        for size in range(max(2, f["size"] - 15), f["size"] + 1):
            for name, fb, rel, src in index.get(size, ()):
                if masked_equal(tb, fb, rel):
                    cands.append((name, src, size))
        if not cands:
            continue
        best = max(c[2] for c in cands)
        cands = [c for c in cands if c[2] == best]
        names = sorted(set(c[0] for c in cands))
        rows.append({"va": "%08X" % ea, "size": best, "symbol": cands[0][0],
                     "source": cands[0][1], "candidates": len(names),
                     "names": ";".join(names[:8]) if len(names) > 1 else ""})
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=["va", "size", "symbol", "source", "candidates", "names"],
                           lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    uniq = sum(1 for r in rows if r["candidates"] == 1)
    print("matched %d functions (%d unique names, %d ambiguous) -> %s" % (len(rows), uniq, len(rows) - uniq, out))


if __name__ == "__main__":
    main()
