"""Create the Ghidra project re/CoF.gpr and import every in-scope binary.

Usage: python tools/ghidra_import.py [--only FOLDER] [--ghidra DIR]

Each binary goes into a project folder named after its game version. PDBs
next to a DLL (1.0, 1.4, 1.55 hl.dll) are picked up by Ghidra's PDB
analyzer from the import location. Re-running re-imports with -overwrite.
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_GHIDRA = r"I:\Tools\ghidra_12.1.4_PUBLIC"

# (project folder, path relative to ROOT)
BINARIES = [
    ("1.6a", "versions/1.6a/cryoffear/cl_dlls/hl.dll"),
    ("1.6a", "versions/1.6a/cryoffear/cl_dlls/client.dll"),
    ("1.6a", "versions/1.6a/opengl32.dll"),
    ("1.6a", "versions/1.6a/hw.dll"),
    ("1.6a", "versions/1.6a/cryoffear/cl_dlls/GameUI.dll"),
    ("1.6a", "versions/1.6a/cof.exe"),
    ("1.55", "versions/1.55/cl_dlls/hl.dll"),
    ("1.55", "versions/1.55/cl_dlls/client.dll"),
    ("1.4", "versions/hl.dll"),
    ("1.0", "versions/1.0/CryOfFear/cl_dlls/hl.dll"),
    ("1.0", "versions/1.0/CryOfFear/cl_dlls/client.dll"),
    ("proto", "versions/2010-prototype/The build/CryOfFear/cl_dlls/server.dll"),
    ("proto", "versions/2010-prototype/The build/CryOfFear/cl_dlls/Copy of server.dll"),
    ("proto", "versions/2010-prototype/The build/CryOfFear/cl_dlls/client.dll"),
    ("proto", "versions/2010-prototype/The build/parnaoia_opengl32.dll"),
]
# 1.0 and prototype opengl32.dll are byte-identical to the 1.6a copy.


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", help="import only this project folder")
    ap.add_argument("--ghidra", default=DEFAULT_GHIDRA)
    args = ap.parse_args()

    headless = os.path.join(args.ghidra, "support", "analyzeHeadless.bat")
    proj_dir = os.path.join(ROOT, "re")
    log_dir = os.path.join(ROOT, "build", "ghidra_logs")
    os.makedirs(proj_dir, exist_ok=True)
    os.makedirs(log_dir, exist_ok=True)
    env = dict(os.environ)
    env.setdefault("GHIDRA_HEADLESS_MAXMEM", "8G")

    failed = []
    for folder, rel in BINARIES:
        if args.only and folder != args.only:
            continue
        path = os.path.join(ROOT, rel)
        tag = "%s_%s" % (folder, os.path.basename(rel).replace(" ", "_"))
        cmd = [headless, proj_dir, "CoF/" + folder, "-import", path,
               "-overwrite", "-log", os.path.join(log_dir, tag + ".log")]
        print("importing %s -> CoF/%s" % (rel, folder), flush=True)
        rc = subprocess.call(cmd, env=env, stdout=subprocess.DEVNULL,
                             stderr=subprocess.STDOUT)
        print("  exit %d" % rc, flush=True)
        if rc != 0:
            failed.append(rel)
    if failed:
        sys.exit("failed: %s" % ", ".join(failed))


if __name__ == "__main__":
    main()
