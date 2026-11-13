"""Compile reference source trees with the 1.6a toolchain, file by file.

Usage: python tools/refbuild.py <tree> <side> [--jobs N]
  tree: paranoia | spirit | hlsdk     side: server | client

Objects go to build/refobj/<tree>_<side>/. Files that fail to compile are
listed and skipped; the goal is a set of objects to byte-match against the
retail DLLs (tools/objmatch.py), not a working DLL. Flags are the 1.6a
flags from tools/units.json plus /Zc:forScope- (old code relies on it; it
does not change code generation).
"""
import argparse
import concurrent.futures
import glob
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import msvc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TREES = {
    "paranoia": os.path.join(ROOT, "refs", "Paranoia", "code_src"),
    "spirit": os.path.join(ROOT, "refs", "Spirit", "SpiritSource"),
    "hlsdk": os.path.join(ROOT, "build", "hlsdk_c7240b9"),
}

SIDES = {
    "server": {
        "globs": ["dlls/*.cpp", "dlls/wpn_shared/*.cpp", "pm_shared/*.c", "game_shared/voice_gamemgr.cpp"],
        "includes": ["dlls", "engine", "common", "pm_shared", "game_shared", "public"],
        "defines": ["WIN32", "NDEBUG", "_WINDOWS", "QUIVER", "VOXEL", "QUAKE2", "VALVE_DLL",
                    "CLIENT_WEAPONS"],
    },
    "client": {
        "globs": ["cl_dll/*.cpp", "cl_dll/hl/*.cpp", "game_shared/*.cpp", "common/*.cpp",
                  "public/*.cpp", "pm_shared/*.c",
                  "dlls/crossbow.cpp", "dlls/crowbar.cpp", "dlls/egon.cpp", "dlls/gauss.cpp",
                  "dlls/handgrenade.cpp", "dlls/hornetgun.cpp", "dlls/mp5.cpp", "dlls/python.cpp",
                  "dlls/rpg.cpp", "dlls/satchel.cpp", "dlls/shotgun.cpp", "dlls/squeakgrenade.cpp",
                  "dlls/tripmine.cpp", "dlls/glock.cpp", "dlls/wpn_shared/*.cpp"],
        "includes": ["cl_dll", "cl_dll/hl", "dlls", "engine", "common", "pm_shared", "game_shared",
                     "public", "utils/vgui/include", "external"],
        "extra_includes": ["build/extinc"],
        "defines": ["WIN32", "NDEBUG", "_WINDOWS", "CLIENT_DLL", "CLIENT_WEAPONS", "HL_DLL"],
    },
}


def ensure_hlsdk():
    """Unpack the last pre-HL25 HLSDK (steam_legacy, c7240b9) under build/."""
    import io
    import zipfile
    out = TREES["hlsdk"]
    if not os.path.isdir(out):
        data = subprocess.check_output(["git", "-C", os.path.join(ROOT, "refs", "halflife"),
                                        "archive", "--format=zip", "c7240b9"])
        zipfile.ZipFile(io.BytesIO(data)).extractall(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tree", choices=sorted(TREES))
    ap.add_argument("side", choices=sorted(SIDES))
    ap.add_argument("--jobs", type=int, default=16)
    args = ap.parse_args()
    if args.tree == "hlsdk":
        ensure_hlsdk()
    root = TREES[args.tree]
    side = SIDES[args.side]
    units = json.load(open(os.path.join(ROOT, "tools", "units.json")))["dlls"]["hl"]
    cflags = [f for f in units["cflags"] if f not in ("/W3",)] + ["/W0", "/Zc:forScope-",
              "/FI" + os.path.join(ROOT, "tools", "refshim", "refshim.h")]
    out_dir = os.path.join(ROOT, "build", "refobj", "%s_%s" % (args.tree, args.side))
    os.makedirs(out_dir, exist_ok=True)
    files = []
    for g in side["globs"]:
        files += glob.glob(os.path.join(root, g))
    files = sorted(set(files))
    incs = ["/I" + os.path.join(root, d) for d in side["includes"]]
    incs += ["/I" + os.path.join(ROOT, d) for d in side.get("extra_includes", [])]
    defs = ["/D" + d for d in side["defines"]]
    env = msvc.env()

    def compile_one(src):
        rel = os.path.relpath(src, root).replace("\\", "/")
        obj = os.path.join(out_dir, rel.replace("/", "__").rsplit(".", 1)[0] + ".obj")
        cmd = [msvc.cl_path()] + cflags + defs + incs + ["/Fo" + obj, src]
        p = subprocess.run(cmd, env=env, capture_output=True, text=True)
        err = ""
        if p.returncode != 0:
            err = next((l for l in p.stdout.splitlines() if " error " in l), p.stdout.strip()[-200:])
        return rel, p.returncode == 0, err

    ok, bad = 0, []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        for rel, success, err in ex.map(compile_one, files):
            if success:
                ok += 1
            else:
                bad.append((rel, err))
    print("%s/%s: %d compiled, %d failed" % (args.tree, args.side, ok, len(bad)))
    for rel, err in bad:
        print("  FAIL %-40s %s" % (rel, err[:150]))


if __name__ == "__main__":
    main()
