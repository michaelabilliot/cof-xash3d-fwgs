"""Extract the ordered member names of function-pointer tables from C headers.

members(path, end_marker) returns the member names of the struct that ends
with end_marker (for example "} enginefuncs_t;"), one per declaration, so
function pointer parameters inside a member are not counted.
"""
import re


def members(path, marker):
    """marker is either the closing line ("} name_t;") of a typedef struct or
    the opening of a plain declaration ("struct name_s")."""
    s = open(path, encoding="latin-1").read()
    if marker.startswith("struct "):
        i = re.search(re.escape(marker) + r"\s*\{", s).start()
        start = s.index("{", i) + 1
        depth, j = 1, start
        while depth:
            depth += {"{": 1, "}": -1}.get(s[j], 0)
            j += 1
        body = s[start:j - 1]
    else:
        j = s.index(marker)
        i = s.rindex("typedef struct", 0, j)
        body = s[s.index("{", i) + 1:j]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    body = re.sub(r"^\s*#.*$", "", body, flags=re.M)
    out = []
    for decl in body.split(";"):
        decl = decl.strip()
        if not decl:
            continue
        m = re.search(r"\(\s*\*\s*(\w+)\s*\)", decl)
        if m:
            out.append(m.group(1))
            continue
        m = re.search(r"(\w+)\s*(\[[^\]]*\])?\s*$", decl)
        if m:
            out.append(m.group(1))
    return out


if __name__ == "__main__":
    import sys
    for name in members(sys.argv[1], sys.argv[2]):
        print(name)
