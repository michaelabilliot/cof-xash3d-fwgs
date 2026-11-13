"""Run the whole Phase 1 naming pipeline for the 1.6a DLLs.

Usage: python tools/map_names.py [--skip-features] [--skip-refbuild] [--apply]

Steps (each one documented in its own script):
  1. Ghidra feature export (ExportFeatures.java) for every build, through the
     running GhidraMCP server (tools/ghidra_mcp_server.bat).
  2. Interface scan of 1.55 and 1.6a (scan_calls.py).
  3. Static CRT byte matching (objmatch.py) against the exact 1.6a libraries.
  4. Reference builds of HLSDK, Spirit 1.7, Paranoia 1.2 (refbuild.py) and the
     reference function index (ref_index.py).
  5. 1.55 -> 1.6a name matching for hl.dll (match_versions.py).
  6. Reference-guided naming (refmatch.py) for both DLLs, then a second
     1.55 pass seeded with its results, then refmatch again.
  7. docs/progress.csv, docs/units_*.csv, docs/class_sizes.csv.
  8. tools/symbols/<dll>_1.6a.csv and, with --apply, ApplyNames.java in Ghidra.
"""
import argparse
import csv
import json
import os
import subprocess
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PY = sys.executable
VC_LIB = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.27.29110\lib\x86"
# the retail static UCRT: Windows 10 SDK 10.0.19041.1 (tools/msvc.py SDK_LIB_DIR)
UCRT_LIB = r"I:\Tools\winsdk-10.0.19041.1\Windows Kits\10\Lib\10.0.19041.0\ucrt\x86"
MCP = "http://127.0.0.1:8089"
BOUNDARY = {"hl": "10121FE5", "client": "100B42A4"}
PROGRAMS = {
    "hl_1.6a": "/1.6a/hl.dll", "client_1.6a": "/1.6a/client.dll",
    "hl_1.55": "/1.55/hl.dll", "client_1.55": "/1.55/client.dll",
    "hl_1.4": "/1.4/hl.dll", "hl_1.0": "/1.0/hl.dll", "server_proto": "/proto/server.dll",
}
DLLS = {"hl": "versions/1.6a/cryoffear/cl_dlls/hl.dll", "client": "versions/1.6a/cryoffear/cl_dlls/client.dll"}


def show(a):
    try:
        return os.path.relpath(a, ROOT) if os.path.isabs(a) else a
    except ValueError:  # other drive
        return a


def run(*args):
    print(">", " ".join(show(a) for a in args), flush=True)
    subprocess.check_call(list(args), cwd=ROOT)


def mcp_script(script, args, program):
    body = json.dumps({"script_name": os.path.join(ROOT, "tools", "ghidra_scripts", script),
                       "args": args, "program": program, "timeout_seconds": 1800,
                       "capture_output": False}).encode()
    req = urllib.request.Request(MCP + "/run_ghidra_script", data=body,
                                 headers={"Content-Type": "application/json"})
    res = json.loads(urllib.request.urlopen(req, timeout=1900).read().decode())
    if not res.get("success"):
        sys.exit("Ghidra script %s failed on %s: %s" % (script, program, res))


def symbol_maps():
    for dll in DLLS:
        rows = {}
        for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))):
            if r["dll"] == dll and r["name"] and r["evidence"] != "crt_bytes_ambiguous":
                rows[int(r["va"], 16)] = (r["name"], "func", r["evidence"])
        for r in csv.DictReader(open(os.path.join(ROOT, "build", "match", "%s_1.6a_refnames.csv" % dll))):
            va = int(r["va"], 16)
            if r["kind"] == "data" and va not in rows and r["name"]:
                rows[va] = (r["name"], "data", r["evidence"])
        manual = os.path.join(ROOT, "tools", "symbols", "%s_manual.csv" % dll)
        if os.path.exists(manual):
            for r in csv.DictReader(open(manual)):
                rows[int(r["va"], 16)] = (r["name"], r["kind"], "manual")
        path = os.path.join(ROOT, "tools", "symbols", "%s_1.6a.csv" % dll)
        with open(path, "w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["va", "name", "kind", "evidence"])
            for va in sorted(rows):
                w.writerow(["%08X" % va] + list(rows[va]))
        print("%s: %d symbols -> %s" % (dll, len(rows), os.path.relpath(path, ROOT)))


def refmatch(dll):
    seeds = ["--seed", "build/match/%s_1.6a_crt_strong.csv:va:name" % dll,
             "--seed", "docs/calls/%s_1.6a_exports.csv:va:name" % dll]
    if dll == "hl":
        seeds = ["--seed", "build/match/hl_1.55_to_1.6a.csv:to_va:name"] + seeds + \
                ["--seed", "tools/symbols/hl_manual.csv:va:name"]
    else:
        seeds += ["--seed", "build/match/client_1.6a_ghidra_names.csv:va:name"]
    refs = []
    for tree in ("paranoia", "spirit", "hlsdk"):
        refs += ["--ref", "%s=build/refobj/%s_%s" % (tree, tree, "server" if dll == "hl" else "client")]
    run(PY, "tools/refmatch.py", DLLS[dll], "build/features/%s_1.6a.jsonl" % dll,
        "build/match/%s_1.6a_refnames.csv" % dll, "--boundary", BOUNDARY[dll], "--similarity", *(seeds + refs))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--skip-features", action="store_true")
    ap.add_argument("--skip-refbuild", action="store_true")
    ap.add_argument("--apply", action="store_true", help="apply names in Ghidra through the MCP server")
    args = ap.parse_args()
    os.makedirs(os.path.join(ROOT, "build", "features"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "build", "match"), exist_ok=True)

    if not args.skip_features:
        for tag, prog in PROGRAMS.items():
            mcp_script("ExportFeatures.java", os.path.join(ROOT, "build", "features", tag + ".jsonl"), prog)
    for tag, dll, kind, table in (("1.6a", DLLS["hl"], "hl", None), ("1.6a", DLLS["client"], "client", None),
                                  ("1.55", "versions/1.55/cl_dlls/hl.dll", "hl", "10219428:240"),
                                  ("1.55", "versions/1.55/cl_dlls/client.dll", "client", None)):
        extra = ["--table", table] if table else []
        run(PY, "tools/scan_calls.py", kind, dll, "docs/calls", "--tag", tag, *extra)
    for dll, path in DLLS.items():
        run(PY, "tools/objmatch.py", path, "build/features/%s_1.6a.jsonl" % dll, "build/match/%s_1.6a_crt.csv" % dll,
            os.path.join(VC_LIB, "libcmt.lib"), os.path.join(VC_LIB, "libvcruntime.lib"),
            os.path.join(VC_LIB, "libcpmt.lib"), os.path.join(UCRT_LIB, "libucrt.lib"))
        rows = [r for r in csv.DictReader(open(os.path.join(ROOT, "build", "match", "%s_1.6a_crt.csv" % dll)))
                if r["candidates"] == "1" and int(r["size"]) >= 16]
        with open(os.path.join(ROOT, "build", "match", "%s_1.6a_crt_strong.csv" % dll), "w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["va", "name"])
            w.writerows([[r["va"], r["symbol"]] for r in rows])
    if not args.skip_refbuild:
        for tree in ("paranoia", "spirit", "hlsdk"):
            for side in ("server", "client"):
                run(PY, "tools/refbuild.py", tree, side)
    run(PY, "tools/ref_index.py")
    feats = [json.loads(l) for l in open(os.path.join(ROOT, "build", "features", "client_1.6a.jsonl"))]
    with open(os.path.join(ROOT, "build", "match", "client_1.6a_ghidra_names.csv"), "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["va", "name"])
        for r in feats:
            # only names Ghidra derived itself (RTTI analysis), never names we applied
            if r["t"] == "func" and not r["thunk"] and r["ea"] < int(BOUNDARY["client"], 16) and "::" in r["name"] \
                    and r.get("src") == "ANALYSIS" and not r["name"].startswith(("FUN_", "Unwind@", "Catch")):
                w.writerow(["%08X" % r["ea"], r["name"]])
    run(PY, "tools/match_versions.py", "hl", "1.55", "1.6a")
    refmatch("hl")
    run(PY, "tools/match_versions.py", "hl", "1.55", "1.6a", "--seed", "build/match/hl_1.6a_refnames.csv")
    refmatch("hl")
    refmatch("client")
    run(PY, "tools/make_progress.py")
    run(PY, "tools/make_units.py")
    run(PY, "tools/class_sizes.py")
    symbol_maps()
    if args.apply:
        for dll in DLLS:
            mcp_script("ApplyNames.java", os.path.join(ROOT, "tools", "symbols", "%s_1.6a.csv" % dll),
                       "/1.6a/%s.dll" % dll)
        urllib.request.urlopen(MCP + "/save_all_programs", timeout=300).read()


if __name__ == "__main__":
    main()
