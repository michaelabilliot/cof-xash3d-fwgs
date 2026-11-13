"""Index function definitions of the reference source trees.

Usage: python tools/ref_index.py [--out build/ref_index.json]

For each tree (HLSDK, Paranoia, Spirit) collects the qualified names of the
functions defined in its server (dlls, pm_shared, game_shared) and client
(cl_dll, pm_shared, game_shared, common) code:
  - ordinary definitions: "void CBaseEntity :: Spawn( void )" -> CBaseEntity::Spawn
  - IMPLEMENT_SAVERESTORE(Class, Base) -> Class::Save, Class::Restore
  - LINK_ENTITY_TO_CLASS(name, Class) -> name
  - DECLARE_MESSAGE(obj, Name) -> __MsgFunc_Name ; DECLARE_COMMAND(obj, Name) -> __CmdFunc_Name
The HLSDK is read from git at the last pre-HL25 commit (steam_legacy,
c7240b9) so the 2024 rewrite does not pollute it.
"""
import argparse
import json
import os
import re
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HLSDK_COMMIT = "c7240b9"

TREES = {
    "HLSDK": ("git", os.path.join(ROOT, "refs", "halflife")),
    "PARANOIA": ("dir", os.path.join(ROOT, "refs", "Paranoia", "code_src")),
    "SPIRIT": ("dir", os.path.join(ROOT, "refs", "Spirit", "SpiritSource")),
}
SERVER_DIRS = ("dlls", "pm_shared", "game_shared")
CLIENT_DIRS = ("cl_dll", "pm_shared", "game_shared", "common", "public")

DEF_RE = re.compile(
    r"^[ \t]*(?:(?:static|inline|virtual|extern\s+\"C\"|extern|EXPORT|DLLEXPORT|__declspec\(\w+\))\s+)*"
    r"(?:[A-Za-z_][\w:<>,\s\*&]*?[\s\*&]+)?"
    r"((?:[A-Za-z_]\w*\s*::\s*)?~?[A-Za-z_]\w*|operator\s*\S+)\s*\(((?:[^;{}()]|\([^;{}()]*\))*)\)\s*(?:const\s*)?"
    r"(?://[^\n]*)?\n?\s*\{", re.M)
NOT_FUNCS = {"if", "while", "for", "switch", "return", "sizeof", "catch", "else", "do"}


def git_files(repo, commit, dirs):
    out = subprocess.check_output(["git", "-C", repo, "ls-tree", "-r", "--name-only", commit] + list(dirs),
                                  text=True)
    for path in out.splitlines():
        if path.lower().endswith((".cpp", ".c", ".h")):
            data = subprocess.check_output(["git", "-C", repo, "show", "%s:%s" % (commit, path)])
            yield path, data.decode("latin-1")


def dir_files(root, dirs):
    for d in dirs:
        base = os.path.join(root, d)
        for dp, _dn, fn in os.walk(base):
            for f in fn:
                if f.lower().endswith((".cpp", ".c", ".h")):
                    p = os.path.join(dp, f)
                    yield os.path.relpath(p, root).replace("\\", "/"), open(p, encoding="latin-1").read()


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


CLASS_RE = re.compile(r"\b(?:class|struct)\s+(?:DLLEXPORT\s+|EXPORT\s+)?([A-Za-z_]\w*)\s*(?::[^{;]*)?\{")


def class_spans(s):
    """(start, end, name) of every class/struct body, for inline members."""
    spans = []
    for m in CLASS_RE.finditer(s):
        start = m.end() - 1
        depth, i = 0, start
        while i < len(s):
            c = s[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        spans.append((start, i, m.group(1)))
    return spans


def functions(text):
    s = strip_comments(text)
    names = set()
    spans = class_spans(s)
    for m in DEF_RE.finditer(s):
        name = re.sub(r"\s+", "", m.group(1))
        if name.split("::")[-1] in NOT_FUNCS:
            continue
        if "::" not in name:
            inner = [sp for sp in spans if sp[0] < m.start(1) < sp[1]]
            if inner:
                cls = min(inner, key=lambda sp: sp[1] - sp[0])[2]
                name = "%s::%s" % (cls, name)
        names.add(name)
    for m in re.finditer(r"IMPLEMENT_SAVERESTORE\s*\(\s*(\w+)\s*,", s):
        names.add(m.group(1) + "::Save")
        names.add(m.group(1) + "::Restore")
    for m in re.finditer(r"LINK_ENTITY_TO_CLASS\s*\(\s*(\w+)\s*,", s):
        names.add(m.group(1))
    for m in re.finditer(r"DECLARE_MESSAGE\s*\(\s*\w+\s*,\s*(\w+)\s*\)", s):
        names.add("__MsgFunc_" + m.group(1))
    for m in re.finditer(r"DECLARE_COMMAND\s*\(\s*\w+\s*,\s*(\w+)\s*\)", s):
        names.add("__CmdFunc_" + m.group(1))
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "ref_index.json"))
    args = ap.parse_args()
    index = {}
    for tree, (kind, root) in TREES.items():
        for side, dirs in (("server", SERVER_DIRS), ("client", CLIENT_DIRS)):
            files = git_files(root, HLSDK_COMMIT, dirs) if kind == "git" else dir_files(root, dirs)
            names = {}
            for path, text in files:
                for n in functions(text):
                    names.setdefault(n, path)
            index["%s/%s" % (tree, side)] = names
            print("%-16s %5d functions" % ("%s/%s" % (tree, side), len(names)))
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    json.dump(index, open(args.out, "w"), indent=0, sort_keys=True)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
