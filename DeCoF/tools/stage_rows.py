"""Stage HEAD's version of a symbol csv with the working-tree rows whose va (first field) is listed.

  python stage_rows.py [--sort] <csv> <va> [<va> ...]   (or @file with one va per line)
"""
import subprocess, sys
args = sys.argv[1:]
sort = "--sort" in args
args = [a for a in args if a != "--sort"]
path, keys = args[0], []
for a in args[1:]:
    keys += open(a[1:]).read().split() if a.startswith("@") else [a]
keys = {k.upper() for k in keys}
head = subprocess.run(["git", "show", "HEAD:" + path], capture_output=True).stdout.decode("latin-1")
work = open(path, newline="", encoding="latin-1").read()
nl = "\r\n" if "\r\n" in work else "\n"
def lines(t):
    l = t.split(nl)
    return l[:-1] if l and l[-1] == "" else l
va = lambda l: l.split(",", 1)[0].upper()
hl, wl = lines(head), lines(work)
add = [l for l in wl[1:] if va(l) in keys]
wmap = {}
for l in add:
    wmap.setdefault(va(l), []).append(l)
out = [hl[0]]
done = set()
for l in hl[1:]:
    k = va(l)
    if k in keys:
        if k in wmap and k not in done:
            out += wmap[k]
            done.add(k)
        continue
    out.append(l)
out += [l for l in add if va(l) not in done]
if sort:
    out = [out[0]] + sorted(out[1:], key=va)
data = (nl.join(out) + nl).encode("latin-1")
sha = subprocess.run(["git", "hash-object", "-w", "--stdin"], input=data, capture_output=True).stdout.decode().strip()
subprocess.run(["git", "update-index", "--cacheinfo", "100644," + sha + "," + path], check=True)
print("staged %s: %d rows from the work tree" % (path, len(add)))
