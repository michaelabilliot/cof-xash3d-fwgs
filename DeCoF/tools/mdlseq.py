"""List the sequences of a GoldSrc studio model (.mdl).

  python tools/mdlseq.py <model.mdl> [ENUM_PREFIX]

Prints index and label of every sequence. With ENUM_PREFIX, prints an enum
in the HLSDK weapon style (GLOCK_IDLE1, ...) named after the labels: the
labels are the only names the retail files have for animation indices.
"""
import re
import struct
import sys


def sequences(path):
    data = open(path, "rb").read()
    if data[:4] != b"IDST":
        raise SystemExit("%s: not a studio model" % path)
    numseq, seqindex = struct.unpack_from("<II", data, 0xA4)
    out = []
    for i in range(numseq):
        o = seqindex + 176 * i
        label = data[o:o + 32].split(b"\0")[0].decode("latin-1")
        out.append(label)
    return out


def main():
    seqs = sequences(sys.argv[1])
    if len(sys.argv) > 2:
        prefix = sys.argv[2]
        print("enum %s_e\n{" % prefix.lower())
        for i, s in enumerate(seqs):
            name = re.sub(r"[^A-Za-z0-9]", "_", s).upper()
            print("\t%s_%s%s," % (prefix, name, " = 0" if i == 0 else ""))
        print("};")
    else:
        for i, s in enumerate(seqs):
            print("%3d (0x%02X) %s" % (i, i, s))


if __name__ == "__main__":
    main()
