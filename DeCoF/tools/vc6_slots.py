"""Predict the VC6 /Od stack slot order of a function's locals from their names.

  python tools/vc6_slots.py <name> [<name> ...]      (locals in declaration order)

VC6 (cl 12.00.8168) does not give locals their stack slots in declaration
order. It keeps them in a 16-bucket hash table keyed by the name:

  for each character c:  h = ((h << 2) + (h >> 4) + c) & 0xFFFFFFFF
  bucket = (h ^ (h >> 16)) & 15

and hands out slots from the top of the frame (-4) downward, bucket 0
first; inside one bucket the last declared local comes first. Locals of an
inner block come after all outer ones and sibling blocks do not share
space; compiler temporaries come last. Fitted on probe functions and
confirmed on every hw.dll frame checked (common.c, delta.c, net_chan.c).
It also tells which DWARF name plays which role when two locals have the
same type: the slot order fixes the names.

Prints the locals from the top of the frame down, with their buckets.
"""
import sys


def name_hash(name):
    v = 0
    for ch in name:
        v = ((v << 2) + (v >> 4) + ord(ch)) & 0xFFFFFFFF
    return v


def bucket(name):
    v = name_hash(name)
    return (v ^ (v >> 16)) & 15


def slot_order(names):
    idx = {n: i for i, n in enumerate(names)}
    return sorted(names, key=lambda n: (bucket(n), -idx[n]))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    print(" ".join("%s(b%d)" % (n, bucket(n)) for n in slot_order(sys.argv[1:])))
