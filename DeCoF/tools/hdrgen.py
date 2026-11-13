"""Rebuild class declarations from the CoF PDB types.

  python tools/hdrgen.py <class> [--merge FILE] [--write] [--pdb PDB]

Prints the declaration of <class> with its members and methods in the
order of the PDB field list (the declaration order of the original
header). With --merge FILE, every member or method that already exists in
the class of the same name in FILE is copied from FILE as written there
(comments, inline bodies, default arguments, BOOL and string_t types);
only what the PDB has and FILE lacks is generated. Generated methods get
their parameter names from the debug locals of their definition. A method
whose definition sits in a header (debug line table) is marked
"// inline in <file>:<line>" so the body can be written there.

--write replaces the class in FILE (keeps encoding and line endings).
Items that exist in FILE but not in the PDB are listed on stderr and
dropped.
"""
import argparse
import bisect
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdbfile  # noqa: E402
import pdbtypes  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TYPE_SUBST = [
    (r"\bentvars_s\b", "entvars_t"), (r"\bedict_s\b", "edict_t"),
    (r"\bKeyValueData_s\b", "KeyValueData"), (r"\bplayermove_s\b", "playermove_t"),
    (r"\bcvar_s\b", "cvar_t"), (r"\bmovevars_s\b", "movevars_t"),
    (r"\bclientdata_s\b", "clientdata_t"), (r"\bentity_state_s\b", "entity_state_t"),
    (r"\bweapon_data_s\b", "weapon_data_t"), (r"\busercmd_s\b", "usercmd_t"),
    (r"\bnetadr_s\b", "netadr_t"), (r"\bcustomization_s\b", "customization_t"),
    (r"\bresource_s\b", "resource_t"), (r"\bdelta_s\b", "delta_t"),
    (r"\bstd::basic_string<char,std::char_traits<char>,std::allocator<char> >", "std::string"),
]
STRING_NAMES = re.compile(r"^(m_isz|m_str|isz|m_s[A-Z])")


def src_type(types, ti, inner=""):
    s = types.name(ti, inner)
    for pat, rep in TYPE_SUBST:
        s = re.sub(pat, rep, s)
    return s


# -- PDB side --------------------------------------------------------------

class PdbInfo(object):
    def __init__(self, path):
        self.pdb = pdbfile.PDB(path)
        self.types = pdbtypes.Types(self.pdb)
        self.params = {}     # (qualified name, type index) -> [names]
        self.where = {}      # (qualified name, type index) -> (file, line)
        for mod in self.pdb.modules:
            recs = list(self.pdb.module_symbols(mod))
            lines = None
            for i, (_p, kind, d) in enumerate(recs):
                if kind not in (pdbfile.S_GPROC32, pdbfile.S_LPROC32):
                    continue
                (_pa, _end, _nx, length, _ds, _de, ti, off, seg, _fl) = struct.unpack_from("<IIIIIIIIHB", d, 0)
                name = pdbtypes.cstr(d, 35)[0]
                names = []
                j = i + 1
                depth = 1
                while j < len(recs) and depth:
                    k2, d2 = recs[j][1], recs[j][2]
                    if k2 in (pdbfile.S_GPROC32, pdbfile.S_LPROC32, pdbtypes.S_BLOCK32):
                        depth += 1
                    elif k2 == pdbfile.S_END:
                        depth -= 1
                    elif k2 == pdbtypes.S_BPREL32 and depth == 1:
                        boff = struct.unpack_from("<i", d2, 0)[0]
                        pname = pdbtypes.cstr(d2, 8)[0]
                        if boff >= 8 and pname != "this":
                            names.append((boff, pname))
                    j += 1
                key = (name, ti)
                if key not in self.params:
                    self.params[key] = [n for _o, n in sorted(names)]
                if key not in self.where:
                    if lines is None:
                        lines = self.pdb.module_lines(mod)
                        rvas = [x[1] for x in lines]
                    rva = self.pdb.rva(seg, off)
                    x = bisect.bisect_left(rvas, rva)
                    if x < len(lines) and lines[x][1] < rva + max(length, 1):
                        self.where[key] = (lines[x][0], lines[x][2])


# -- source side -----------------------------------------------------------

def find_class(text, name):
    m = re.search(r"(?m)^[ \t]*(class|struct)\s+%s\b[^;{]*\{" % re.escape(name), text)
    if not m:
        return None
    start = m.start()
    body = m.end()
    depth = 1
    i = body
    n = len(text)
    while i < n and depth:
        c = text[i]
        if text.startswith("//", i):
            i = text.find("\n", i)
            if i < 0:
                i = n
            continue
        if text.startswith("/*", i):
            i = text.find("*/", i) + 2
            continue
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    end = i  # just after the closing brace
    semi = text.find(";", end)
    return start, body, end - 1, semi + 1


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    s = re.sub(r"//[^\n]*", " ", s)
    s = re.sub(r"(?m)^[ \t]*#.*$", " ", s)
    return s


def split_items(body):
    """Split a class body into (leading+statement text, code) items."""
    items = []
    i = 0
    n = len(body)
    start = 0
    paren = brace = 0
    while i < n:
        c = body[i]
        if body.startswith("//", i):
            j = body.find("\n", i)
            i = n if j < 0 else j
            continue
        if body.startswith("/*", i):
            i = body.find("*/", i) + 2
            continue
        if c == "#" and (i == 0 or body[body.rfind("\n", 0, i) + 1:i].strip() == ""):
            j = body.find("\n", i)
            i = n if j < 0 else j
            continue
        if c in "\"'":
            j = i + 1
            while j < n and body[j] != c:
                j += 2 if body[j] == "\\" else 1
            i = j + 1
            continue
        end = None
        if c == "(":
            paren += 1
        elif c == ")":
            paren -= 1
        elif c == "{":
            brace += 1
        elif c == "}":
            brace -= 1
            if brace == 0 and paren == 0:
                code = strip_comments(body[start:i + 1])
                # function body or enum/struct definition followed by ';'
                k = i + 1
                while k < n and body[k] in " \t":
                    k += 1
                if k < n and body[k] == ";":
                    end = k + 1
                elif re.search(r"\b(enum|struct|union|class)\b[^(]*\{", code) or re.search(r"\btypedef\b", code):
                    k2 = body.find(";", i)
                    end = k2 + 1
                else:
                    end = i + 1
        elif c == ";" and paren == 0 and brace == 0:
            end = i + 1
        # access labels stay in the leading text of the next statement
        if end is not None:
            # keep a trailing comment on the same line
            k = end
            while k < n and body[k] in " \t":
                k += 1
            if body.startswith("//", k) or body.startswith("/*", k):
                if body.startswith("//", k):
                    e = body.find("\n", k)
                    end = n if e < 0 else e
                else:
                    end = body.find("*/", k) + 2
            elif k < n and body[k] == "\n":
                end = k  # trailing blanks
            items.append(body[start:end])
            start = end
            i = end
            continue
        i += 1
    tail = body[start:]
    return items, tail


ACCESS_RE = re.compile(r"(?m)^[ \t]*(public|private|protected)[ \t]*:(?!:)")


def access_label(text):
    """The access label in an item's leading text, if any (last one wins)."""
    found = ACCESS_RE.findall(strip_comments(text))
    return found[-1] if found else None


def set_label(text, acc):
    """Replace the access label lines of an item's text by `acc:`."""
    return ACCESS_RE.sub(acc + ":", text, count=1)


def classify(text):
    """(kind, name, nparams) for an item's code."""
    code = ACCESS_RE.sub("", strip_comments(text)).strip()
    if not code:
        return ("blank", None, None)
    m = re.match(r"^typedef\s+enum\b.*\}\s*(\w+)\s*;$", code, re.S)
    if m:
        return ("nested", m.group(1), None)
    m = re.match(r"^(?:typedef\s+)?(enum|struct|union|class)\s+(\w+)\s*\{", code, re.S)
    if m:
        return ("nested", m.group(2), None)
    if code.startswith("friend"):
        return ("friend", None, None)
    m = re.search(r"\(\s*(?:\w+\s*::\s*)?\*\s*(\w+)\s*\)\s*\(", code)
    if m and code.index("(") == m.start():
        return ("member", m.group(1), None)
    head = code.split("{", 1)[0]
    if "(" in head:
        pre = head[:head.index("(")]
        m = re.search(r"(operator\s*(?:new|delete)(?:\s*\[\s*\])?|operator\s*[^\s\w(]+|~?\w+)\s*$", pre)
        name = re.sub(r"\s+", " ", m.group(1)) if m else None
        if name and name.startswith("operator") and not name.startswith("operator "):
            name = "operator " + name[8:].strip() if re.match(r"operator\w", name) else name
        # parameter count
        depth = 0
        k = head.index("(")
        params = ""
        for ch in head[k + 1:]:
            if ch == "(":
                depth += 1
            elif ch == ")":
                if depth == 0:
                    break
                depth -= 1
            params += ch
        p = params.strip()
        if p in ("", "void"):
            cnt = 0
        else:
            cnt = 1
            depth = 0
            for ch in p:
                if ch in "(<[":
                    depth += 1
                elif ch in ")>]":
                    depth -= 1
                elif ch == "," and depth == 0:
                    cnt += 1
        return ("method", name, cnt)
    code = re.sub(r"\s*=.*$", "", code.rstrip(";"), flags=re.S)
    names = []
    for decl in re.split(r",(?![^\[]*\])", code):
        decl = re.sub(r":\s*\d+\s*$", "", decl.strip())
        m = re.search(r"(\w+)\s*(\[[^\]]*\]\s*)*$", decl)
        if m:
            names.append(m.group(1))
    if code.startswith("static"):
        return ("static", names[0] if names else None, None)
    return ("member", names, None)


# -- generation ------------------------------------------------------------

def method_decl(info, cls, a, t, name, vbo):
    types = info.types
    kind, d = types.recs[t]
    mprop = (a >> 2) & 7
    if kind == pdbtypes.LF_MFUNCTION:
        rv, _c, _this, _cc, _fa, _n, al = struct.unpack_from("<IIIBBHI", d, 0)
    else:
        rv, _cc, _fa, _n, al = struct.unpack_from("<IBBHI", d, 0)
    args = types.args(al)
    if args and args[-1] == 0:
        args = args[:-1]
    qual = "%s::%s" % (cls, name)
    pnames = info.params.get((qual, t), [])
    parts = []
    for i, at in enumerate(args):
        pn = pnames[i] if i < len(pnames) else ""
        parts.append(src_type(types, at, pn))
    plist = " " + ", ".join(parts) + " " if parts else " void "
    pre = {1: "virtual ", 2: "static ", 4: "virtual ", 5: "virtual ", 6: "virtual "}.get(mprop, "")
    post = " = 0" if mprop in (5, 6) else ""
    if name == cls or name == "~" + cls:
        decl = "%s%s(%s)" % (pre, name, plist)
    else:
        decl = "%s%s" % (pre, src_type(types, rv, "%s(%s)" % (name, plist)))
    note = ""
    w = info.where.get((qual, t))
    if w and w[0].lower().endswith(".h"):
        note = "\t// inline in %s:%d" % (re.split(r"[\\/]", w[0])[-1], w[1])
    return "\t%s%s;%s" % (decl, post, note)


def member_decl(types, t, name):
    s = src_type(types, t, name)
    if re.match(r"^int\s", s) and STRING_NAMES.match(name):
        s = "string_t" + s[3:]
    return "\t%s;" % s


def nested_decl(types, t, name):
    ti = types.resolve(t)
    if ti >= 0x1000 and types.recs[ti][0] == pdbtypes.LF_ENUM:
        u = types.udt(ti)
        vals = [(n, v) for _k, _a, _t, v, n in types.fields(u["field"])] if u["field"] else []
        short = name.split("::")[-1]
        body = ", ".join("%s = %d" % (n, v) for n, v in vals)
        return "\tenum %s { %s };" % (short, body)
    return "\t// nested type %s" % name


def generate(info, cls, merge_text):
    types = info.types
    ti = types.by_name.get(cls)
    if not ti:
        raise SystemExit("%s: not in the PDB" % cls)
    u = types.udt(ti)
    kw = "struct" if u["kind"] == pdbtypes.LF_STRUCTURE else "class"
    fl = types.fields(u["field"]) if u["field"] else []
    bases = ["%s %s" % (pdbtypes.ACCESS.get(a & 3, "public"), types.name(t)) for k, a, t, _o, _n in fl if k == "base"]

    # index the existing text
    pool = {}
    tail = ""
    head_line = None
    if merge_text is not None:
        loc = find_class(merge_text, cls)
        if loc:
            start, body, close, _end = loc
            head_line = merge_text[start:body - 1].rstrip()
            items, tail = split_items(merge_text[body:close])
            for it in items:
                kind, name, cnt = classify(it)
                if kind in ("blank", "access"):
                    continue
                if kind == "member" and isinstance(name, list):
                    for nm in name:
                        pool.setdefault(("member", nm, None), []).append(it)
                    continue
                pool.setdefault((kind, name, cnt), []).append(it)
    used = set()

    def take(key):
        lst = pool.get(key)
        if lst:
            for it in lst:
                if id(it) not in used:
                    used.add(id(it))
                    return it
            return ""  # already emitted (multi-declarator line)
        return None

    out = []
    if head_line and all(b.split()[-1] in head_line for b in bases):
        out.append(head_line)
    else:
        out.append("%s %s%s" % (kw, cls, (" : " + ", ".join(bases)) if bases else ""))
    out.append("{")
    state = {"access": None}
    default_access = "private" if kw == "class" else "public"

    def emit(text, acc, reused):
        """Append an item; reused text keeps its blank lines, comments and
        access label, a label is added or corrected only when needed."""
        cur = state["access"]
        if reused:
            text = text[1:] if text.startswith("\n") else text
            text = text.rstrip("\n")
            lbl = access_label(text)
            if acc is None:
                acc = lbl or cur
            if lbl and lbl != acc:
                text = set_label(text, acc)
            elif not lbl and acc != (cur or default_access):
                out.append("%s:" % acc)
        else:
            if acc is None:
                acc = cur
            if acc != (cur or default_access):
                out.append("%s:" % acc)
        out.append(text)
        state["access"] = acc

    for k, a, t, off, name in fl:
        if k in ("base", "vfptr", "friendcls", "friendfcn"):
            continue
        if k == "method" and a & 0x100:
            continue
        acc = pdbtypes.ACCESS.get(a & 3) if k != "nested" else None
        if k == "member":
            it = take(("member", name, None))
            if it is None:
                emit(member_decl(types, t, name), acc, False)
            elif it:
                emit(it, acc, True)
        elif k == "static":
            it = take(("static", name, None))
            if it:
                emit(it, acc, True)
            else:
                emit("\tstatic %s;" % src_type(types, t, name), acc, False)
        elif k == "nested":
            it = take(("nested", name.split("::")[-1], None))
            if it:
                emit(it, acc, True)
            else:
                emit(nested_decl(types, t, name), acc, False)
        elif k == "method":
            kind, d = types.recs[t]
            if kind == pdbtypes.LF_MFUNCTION:
                al = struct.unpack_from("<I", d, 16)[0]
            else:
                al = struct.unpack_from("<I", d, 8)[0]
            args = types.args(al)
            cnt = len(args) - (1 if args and args[-1] == 0 else 0)
            it = take(("method", name, cnt))
            if it is None and cnt:
                # default arguments do not change the count; varargs may
                it = take(("method", name, cnt + 1))
            if it:
                emit(it, acc, True)
            else:
                emit(method_decl(info, cls, a, t, name, off), acc, False)
    if tail.strip():
        out.append(tail.strip("\n").rstrip())
    out.append("};")
    dropped = []
    for key, lst in pool.items():
        for it in lst:
            if id(it) not in used:
                dropped.append((key, strip_comments(it).strip()[:100]))
    return "\n".join(out) + "\n", dropped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cls")
    ap.add_argument("--merge")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--pdb", default=os.path.join(ROOT, "versions/1.55/cl_dlls/hl.pdb"))
    args = ap.parse_args()
    info = PdbInfo(args.pdb)
    raw = None
    text = None
    crlf = False
    if args.merge:
        raw = open(args.merge, "rb").read().decode("latin-1")
        crlf = "\r\n" in raw
        text = raw.replace("\r\n", "\n")
    decl, dropped = generate(info, args.cls, text)
    for key, code in dropped:
        sys.stderr.write("dropped (not in the PDB): %s %s: %s\n" % (key[0], key[1], code))
    if args.write:
        loc = find_class(text, args.cls)
        if not loc:
            raise SystemExit("%s not found in %s" % (args.cls, args.merge))
        start, _body, _close, end = loc
        new = text[:start] + decl.rstrip("\n") + text[end:]
        if crlf:
            new = new.replace("\n", "\r\n")
        open(args.merge, "wb").write(new.encode("latin-1"))
        print("rewrote %s in %s" % (args.cls, args.merge))
    else:
        sys.stdout.write(decl)


if __name__ == "__main__":
    main()
