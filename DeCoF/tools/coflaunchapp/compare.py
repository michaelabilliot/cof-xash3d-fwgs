"""Compare the rebuilt CoFLaunchApp.exe with retail, method by method.

Both executables are disassembled with ildasm (text output, no tokens, no
bytes). Each copy is disassembled from a scratch folder that holds no PDB, so
locals print as V_n on both sides. Lines that only carry addresses or
per-build identifiers (MVID, image base, resource offsets) are dropped.

Reported:
  - per method: identical or different (header, locals and IL)
  - type list, class headers and class level attributes, fields, properties
  - assembly manifest (references, assembly attributes, module flags)
  - managed resources (names and bytes) and the Win32 resource blob
  - byte level: the metadata blob (only the MVID should differ) and the
    region holding the IL bodies and embedded resources
  - whole file: size, then every differing byte, sorted into the per build
    fields (PE and debug directory timestamps, MVID, PDB GUID and age, PDB
    path) and anything else. Needs the same PDB path length on both sides
    (build.py --retail-path); otherwise only the bytes before the debug
    record are compared, because everything after it moves.

Usage:
  python tools/coflaunchapp/compare.py               summary
  python tools/coflaunchapp/compare.py --show NAME   diff of methods whose key contains NAME
  python tools/coflaunchapp/compare.py --json PATH   also write per method results
"""

import argparse
import difflib
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RETAIL = os.path.join(ROOT, "versions", "1.6a", "CoFLaunchApp.exe")
REBUILT = os.path.join(ROOT, "build", "coflaunchapp", "CoFLaunchApp.exe")
ILDASM = r"C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.8.1 Tools\ildasm.exe"

DROP = [
    re.compile(r"^//\s+Microsoft \(R\) \.NET Framework IL Disassembler"),
    re.compile(r"^//\s+Copyright \(c\) Microsoft Corporation"),
    re.compile(r"^// MVID:"),
    re.compile(r"^// Image base:"),
    re.compile(r"^\s*// Offset: 0x[0-9A-Fa-f]+ Length:"),
    re.compile(r"^\s*// WARNING: managed resource file"),
    re.compile(r"^// \*\*\*\*\*\*\*\*\*\*\*"),
    re.compile(r"^// =+"),
]


def ildasm(exe, workdir, tokens=False):
    """Disassemble a copy of exe inside workdir (no PDB next to it)."""
    os.makedirs(workdir, exist_ok=True)
    copy = os.path.join(workdir, "CoFLaunchApp.exe")
    shutil.copyfile(exe, copy)
    cmd = [ILDASM, "/nobar", "/text", "/utf8"]
    if tokens:
        cmd.append("/tokens")
    cmd.append(copy)
    proc = subprocess.run(cmd, cwd=workdir, capture_output=True)
    if proc.returncode != 0:
        sys.exit("ildasm failed on %s" % exe)
    return proc.stdout.decode("utf-8", "replace").replace("\r\n", "\n").split("\n")


def ildasm_resources(exe, workdir):
    """Dump managed .resources and the Win32 .res; return {name: sha256}."""
    os.makedirs(workdir, exist_ok=True)
    copy = os.path.join(workdir, "CoFLaunchApp.exe")
    shutil.copyfile(exe, copy)
    subprocess.run([ILDASM, "/nobar", "/out=dump.il", copy], cwd=workdir, capture_output=True)
    out = {}
    for name in sorted(os.listdir(workdir)):
        if name.endswith(".resources") or name.endswith(".res"):
            data = open(os.path.join(workdir, name), "rb").read()
            label = "Win32 (.rsrc)" if name == "dump.res" else name
            out[label] = (len(data), hashlib.sha256(data).hexdigest())
    return out


def normalize(lines):
    out = []
    for line in lines:
        line = line.rstrip()
        if not line.strip():
            continue
        if any(p.search(line) for p in DROP):
            continue
        out.append(line)
    return out


TOKEN = re.compile(r"/\*\s*([0-9A-Fa-f]{8})\s*\*/")


def method_name(header):
    text = re.sub(r"pinvokeimpl\([^)]*\)", "", header)
    text = TOKEN.sub("", text)
    m = re.search(r"('[^']+'|[\w.$<>`]+)\s*\(", text)
    return m.group(1).strip("'") if m else text


def parse(lines):
    """Split ildasm text into manifest, classes and methods."""
    result = {"manifest": [], "types": [], "classes": {}, "methods": {}, "order": []}
    i = 0
    n = len(lines)
    stack = []
    while i < n:
        line = lines[i]
        s = line.strip()
        if s.startswith(".class "):
            header = [s]
            i += 1
            while lines[i].strip() != "{":
                header.append(lines[i].strip())
                i += 1
            name = TOKEN.sub("", header[0]).split()[-1]
            full = ".".join(stack + [name]) if stack else name
            stack.append(name)
            result["types"].append(full)
            result["classes"][full] = {"header": [TOKEN.sub("", h).strip() for h in header], "members": []}
            i += 1
            continue
        if s.startswith("} // end of class"):
            stack.pop()
            i += 1
            continue
        if not stack:
            result["manifest"].append(s)
            i += 1
            continue
        cls = ".".join(stack)
        if s.startswith(".method "):
            header = [s]
            i += 1
            while lines[i].strip() != "{":
                header.append(lines[i].strip())
                i += 1
            joined = " ".join(header)
            token = TOKEN.search(joined)
            name = method_name(joined)
            body = []
            depth = 1
            i += 1
            while True:
                t = lines[i].strip()
                if t == "{":
                    depth += 1
                elif t.startswith("}"):
                    depth -= 1
                    if depth == 0:
                        break
                body.append(lines[i].rstrip())
                i += 1
            i += 1
            base = "%s::%s" % (cls, name)
            key = base
            k = 2
            while key in result["methods"]:
                key = "%s#%d" % (base, k)
                k += 1
            size = 0
            for b in body:
                m = re.search(r"// Code size\s+(\d+)", b)
                if m:
                    size = int(m.group(1))
                    break
            result["methods"][key] = {
                "header": re.sub(r"\s+", " ", TOKEN.sub("", joined)),
                "body": body,
                "token": token.group(1).upper() if token else "",
                "size": size,
            }
            result["order"].append(key)
            result["classes"][cls]["members"].append(".method " + name)
            continue
        # property, event, field, custom attribute and their sub blocks
        result["classes"][cls]["members"].append(TOKEN.sub("", s).strip())
        i += 1
    return result


def pe_layout(path):
    """Return (file bytes, rva to offset function, metadata rva, metadata size, .text rva)."""
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    sections = []
    for i in range(nsec):
        vs, va, rs, rp = struct.unpack_from("<IIII", data, pe + 24 + optsize + 40 * i + 8)
        sections.append((va, max(vs, rs), rp))

    def offset(rva):
        for va, size, rp in sections:
            if va <= rva < va + size:
                return rva - va + rp
        raise ValueError(hex(rva))

    clr = offset(struct.unpack_from("<I", data, pe + 24 + 96 + 14 * 8)[0])
    md_rva, md_size = struct.unpack_from("<II", data, clr + 8)
    return data, offset, md_rva, md_size, sections[0][0]


def pe_checks():
    """Byte level checks: metadata blob (MVID excluded) and IL bodies before it."""
    a, aoff, amd, asize, atext = pe_layout(RETAIL)
    b, boff, bmd, bsize, btext = pe_layout(REBUILT)
    if (amd, asize) != (bmd, bsize):
        print("metadata blob: retail rva %#x size %d, rebuilt rva %#x size %d" % (amd, asize, bmd, bsize))
    else:
        ma = a[aoff(amd):aoff(amd) + asize]
        mb = b[boff(bmd):boff(bmd) + bsize]
        diff = sum(1 for x, y in zip(ma, mb) if x != y)
        print("metadata blob: %d of %d bytes differ (16 is the MVID alone)" % (diff, asize))
    # method bodies and embedded resources sit between the CLR header and the metadata
    start = 0x48 + 8
    ia = a[aoff(atext) + start:aoff(amd)]
    ib = b[boff(btext) + start:boff(bmd)]
    if len(ia) != len(ib):
        print("IL bodies and resources: retail %d bytes, rebuilt %d bytes" % (len(ia), len(ib)))
    else:
        diff = [i for i in range(len(ia)) if ia[i] != ib[i]]
        print("IL bodies and resources: %d of %d bytes differ%s" % (
            len(diff), len(ia), (" at rva " + ", ".join("%#x" % (atext + start + i) for i in diff[:8])) if diff else ""))


def per_build_fields(path):
    """Return (file bytes, {offset: field name}, offset of the CodeView record, its size)."""
    data, offset, md_rva, md_size, text = pe_layout(path)
    fields = {}

    def mark(start, size, name):
        for i in range(start, start + size):
            fields[i] = name

    pe = struct.unpack_from("<I", data, 0x3C)[0]
    mark(pe + 8, 4, "PE timestamp")
    dbg_rva, dbg_size = struct.unpack_from("<II", data, pe + 24 + 96 + 6 * 8)
    cv = cv_size = None
    for e in range(offset(dbg_rva), offset(dbg_rva) + dbg_size, 28):
        mark(e + 4, 4, "debug timestamp")
        kind, size, rva, ptr = struct.unpack_from("<IIII", data, e + 12)
        if kind == 2 and data[ptr:ptr + 4] == b"RSDS":
            cv, cv_size = ptr, size
            mark(ptr + 4, 20, "PDB GUID and age")
            mark(ptr + 24, size - 24, "PDB path")
    # metadata root: version string, then the stream headers; MVID is GUID 1
    md = offset(md_rva)
    vlen = struct.unpack_from("<I", data, md + 12)[0]
    pos = md + 16 + vlen
    streams = struct.unpack_from("<H", data, pos + 2)[0]
    pos += 4
    for _ in range(streams):
        soff, ssize = struct.unpack_from("<II", data, pos)
        end = data.index(b"\0", pos + 8)
        name = data[pos + 8:end]
        if name == b"#GUID":
            mark(md + soff, 16, "MVID")
        pos = (end + 4) & ~3
    return data, fields, cv, cv_size


def file_check():
    """Whole file comparison with the per build fields named."""
    a, fields, acv, acv_size = per_build_fields(RETAIL)
    b, _, bcv, bcv_size = per_build_fields(REBUILT)
    print("file: retail %d bytes, rebuilt %d bytes" % (len(a), len(b)))
    limit = min(len(a), len(b))
    note = ""
    if (acv, acv_size) != (bcv, bcv_size):
        limit = min(acv, bcv)
        note = " (PDB path length differs, compared up to the debug record only; build.py --retail-path compares all)"
    counts = {}
    other = []
    for i in range(limit):
        if a[i] != b[i]:
            name = fields.get(i)
            if name:
                counts[name] = counts.get(name, 0) + 1
            else:
                other.append(i)
    if acv is not None and (acv, acv_size) == (bcv, bcv_size):
        pa = a[acv + 24:acv + acv_size].rstrip(b"\0").decode("latin-1")
        pb = b[bcv + 24:bcv + bcv_size].rstrip(b"\0").decode("latin-1")
        if pa != pb:
            print("PDB path: retail %s rebuilt %s" % (pa, pb))
    print("file bytes differing in per build fields: %s" % (
        ", ".join("%s %d" % kv for kv in sorted(counts.items())) or "none"))
    print("file bytes differing elsewhere: %d%s%s" % (
        len(other), (" at offset " + ", ".join("%#x" % i for i in other[:8])) if other else "", note))


def first_diff(a, b):
    for idx in range(max(len(a), len(b))):
        x = a[idx].strip() if idx < len(a) else "<end>"
        y = b[idx].strip() if idx < len(b) else "<end>"
        if x != y:
            return idx, x, y
    return None


def short(text, width=90):
    return text if len(text) <= width else text[:width - 3] + "..."


def compare(verbose_filter=None):
    tmp = tempfile.mkdtemp(prefix="coflaunchapp_cmp_")
    try:
        ret = parse(normalize(ildasm(RETAIL, os.path.join(tmp, "retail"))))
        new = parse(normalize(ildasm(REBUILT, os.path.join(tmp, "rebuilt"))))
        ret_tokens = parse(normalize(ildasm(RETAIL, os.path.join(tmp, "retail_tok"), tokens=True)))
        res_ret = ildasm_resources(RETAIL, os.path.join(tmp, "retail_res"))
        res_new = ildasm_resources(REBUILT, os.path.join(tmp, "rebuilt_res"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    report = []
    results = []
    same = 0
    for key in ret["order"]:
        r = ret["methods"][key]
        tok = ret_tokens["methods"].get(key, {}).get("token", "")
        entry = {"key": key, "token": tok, "size": r["size"], "header": r["header"]}
        if key not in new["methods"]:
            entry["status"] = "missing"
            report.append("MISSING  %s" % key)
        else:
            b = new["methods"][key]
            if r["header"] != b["header"]:
                entry["status"] = "different"
                entry["diff"] = "header: retail '%s' rebuilt '%s'" % (r["header"], b["header"])
            else:
                d = first_diff(r["body"], b["body"])
                if d is None:
                    entry["status"] = "identical"
                    same += 1
                else:
                    entry["status"] = "different"
                    entry["diff"] = "line %d: retail '%s' rebuilt '%s'" % d
                    strip = lambda body: [l for l in body if ".maxstack" not in l]
                    if strip(r["body"]) == strip(b["body"]):
                        entry["maxstack_only"] = True
                        entry["maxstack"] = [int(re.search(r"\.maxstack\s+(\d+)", "\n".join(x)).group(1))
                                             for x in (r["body"], b["body"])]
                        entry["diff"] = "instructions identical, only %s" % entry["diff"].split(": ", 1)[1]
            if entry["status"] == "different":
                report.append("DIFF     %s  %s" % (key, short(entry["diff"], 160)))
            if verbose_filter and verbose_filter in key:
                diff = list(difflib.unified_diff(
                    [r["header"]] + r["body"], [b["header"]] + b["body"],
                    "retail", "rebuilt", lineterm="", n=2))
                report.extend(diff[:60])
                if len(diff) > 60:
                    report.append("... %d more diff lines" % (len(diff) - 60))
        results.append(entry)
    for key in new["order"]:
        if key not in ret["methods"]:
            report.append("EXTRA    %s" % key)

    maxstack = sum(1 for e in results if e.get("maxstack_only"))
    print("methods: %d identical / %d total (%d more differ only in .maxstack)"
          % (same, len(ret["order"]), maxstack))
    for line in report:
        print(line)

    # method order inside the Method table
    if [k for k in ret["order"] if k in new["methods"]] != [k for k in new["order"] if k in ret["methods"]]:
        print("method order: differs")
    else:
        print("method order: identical")

    if ret["types"] != new["types"]:
        print("types: differ: retail %s rebuilt %s" % (ret["types"], new["types"]))
    else:
        print("types: identical (%d)" % len(ret["types"]))

    for t in ret["types"]:
        if t not in new["classes"]:
            continue
        rc, nc = ret["classes"][t], new["classes"][t]
        if rc["header"] != nc["header"]:
            d = first_diff(rc["header"], nc["header"])
            print("class %s header: line %d retail '%s' rebuilt '%s'" % ((t,) + tuple(d)))
        if rc["members"] != nc["members"]:
            d = first_diff(rc["members"], nc["members"])
            print("class %s members (fields, attributes, properties, order): line %d retail '%s' rebuilt '%s'"
                  % (t, d[0], short(d[1]), short(d[2])))
        else:
            print("class %s: header and members identical" % t)

    if ret["manifest"] != new["manifest"]:
        diff = [l for l in difflib.unified_diff(ret["manifest"], new["manifest"], lineterm="", n=0)
                if not l.startswith("@@") and not l.startswith("---") and not l.startswith("+++")]
        print("manifest: %d differing lines" % len(diff))
        for line in diff[:20]:
            print("  " + short(line, 150))
    else:
        print("manifest: identical")

    for name in sorted(set(res_ret) | set(res_new)):
        a = res_ret.get(name)
        b = res_new.get(name)
        if a == b:
            print("resource %s: identical (%d bytes)" % (name, a[0]))
        else:
            print("resource %s: retail %s rebuilt %s" % (
                name, "%d bytes" % a[0] if a else "absent", "%d bytes" % b[0] if b else "absent"))
    pe_checks()
    file_check()
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--show", help="print a diff for methods whose key contains this text")
    ap.add_argument("--json", help="write per method results to this file")
    args = ap.parse_args()
    if not os.path.isfile(REBUILT):
        sys.exit("build first: python tools/coflaunchapp/build.py")
    results = compare(args.show)
    if args.json:
        with open(args.json, "w") as f:
            json.dump(results, f, indent=1)


if __name__ == "__main__":
    main()
