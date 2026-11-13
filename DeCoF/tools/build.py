"""Build and diff driver.

  python tools/build.py base   [unit ...]   compile src/ units (toolchain of the unit or its DLL)
  python tools/build.py target [unit ...]   delink retail units through Ghidra
  python tools/build.py objdiff             write objdiff.json
  python tools/build.py report              objdiff report, per unit and function
  python tools/build.py all    [unit ...]   base + target + objdiff + report
  python tools/build.py check  <unit ...>   compile and report only these units
  python tools/build.py decomp <unit ...>   Ghidra pseudo C into build/decomp/ (reference)
  python tools/build.py link   <dll>        link all units of a DLL into build/bin/<dll>
                                            (only when the DLL has a "link" section and
                                            every unit compiles), then compare with retail

Units are named <dll>/<path>, for example hl/dlls/h_export. Config lives in
tools/units.json. Outputs: build/base/<dll>/<path>.obj (ours) and
build/target/<dll>/<path>.obj (retail, delinked).

Toolchains: "toolchain" of a DLL (msvc1427 by default, vc6) compiles its
units and links it; a unit may name its own compiler ("toolchain": "vc6pp"
for the hw.dll objects of the VC6 processor pack), used for build/base and
build/link_obj alike, while the DLL keeps linking with its own toolchain.
A unit whose source ends in .asm (MASM syntax) is assembled with the ml.exe
of its toolchain and "asflags" (default /nologo /c /Cp /coff /Zm).
"""
import csv
import json
import os
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import msvc  # noqa: E402
import msvc6  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GHIDRA = r"I:\Tools\ghidra_12.1.4_PUBLIC"
OBJDIFF = r"I:\Tools\bin\objdiff-cli.exe"
CONFIG = os.path.join(ROOT, "tools", "units.json")


def load_units(selected):
    cfg = json.load(open(CONFIG))
    out = []
    for dll, d in cfg["dlls"].items():
        for u in d["units"]:
            full = "%s/%s" % (dll, u["name"])
            if selected and full not in selected:
                continue
            out.append((dll, d, u, full))
    if selected and len(out) != len(selected):
        known = {o[3] for o in out}
        sys.exit("unknown unit(s): %s" % ", ".join(s for s in selected if s not in known))
    return out


def obj_path(kind, full):
    return os.path.join(ROOT, "build", kind, *full.split("/")) + ".obj"


TOOLCHAINS = {"msvc1427": msvc, "vc6": msvc6.RTM, "vc6pp": msvc6.PP}


def toolchain_name(d, u=None):
    """msvc1427 (default, the 1.6a game DLLs), vc6 (engine, opengl32, GameUI,
    launcher) or vc6pp (VC6 SP6 with the processor pack). A unit's own
    "toolchain" replaces the DLL's for its compile only."""
    name = (u or {}).get("toolchain", d.get("toolchain", "msvc1427"))
    if name not in TOOLCHAINS:
        sys.exit("unknown toolchain %r (known: %s)" % (name, ", ".join(sorted(TOOLCHAINS))))
    return name


def toolchain(d, u=None):
    return TOOLCHAINS[toolchain_name(d, u)]


# ml flags of WinQuake.dsp's custom build step for the .s files (after
# gas2masm) without /Zi, whose debug sections a link without /debug drops;
# the code bytes are the same with or without /Cp, /Zm and /Zi
ASFLAGS = ["/nologo", "/c", "/Cp", "/coff", "/Zm"]


def is_asm(u):
    return u["source"].lower().endswith(".asm")


def ml_path(d, u=None):
    """ml.exe of the unit's toolchain: ML 6.15.8803 of the Processor Pack in
    vc6pp, ML 14.27.29112 in msvc1427. VC6 RTM has no assembler."""
    name = toolchain_name(d, u)
    if name == "msvc1427":
        path = os.path.join(msvc.TOOLS_DIR, "ml.exe")
    else:
        path = os.path.join(TOOLCHAINS[name].root, "BIN", "ML.EXE")
    if not os.path.exists(path):
        sys.exit("toolchain %s has no assembler (%s): give the unit \"toolchain\": \"vc6pp\"" % (name, path))
    return path


def unit_functions(u):
    if "functions_file" in u:
        return [l.strip() for l in open(os.path.join(ROOT, u["functions_file"])) if l.strip()]
    return u.get("functions", [])


def build_base(units, kind="base", extra=()):
    """Compile the units into build/<kind>/. The link may add flags that
    change only how functions are packaged (link section "cflags_extra",
    objects in build/link_obj/), so the objects objdiff compares stay as
    they are."""
    ok = True
    for dll, d, u, full in units:
        tc = toolchain(d, u)
        tc.check()
        out = obj_path(kind, full)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        if is_asm(u):
            # hand-written assembly (the Quake .s files of hw.dll, in MASM
            # syntax): ml.exe of the unit's toolchain, "asflags" of the unit
            # or its DLL (cflags, defines and includes do not apply)
            cmd = [ml_path(d, u)] + u.get("asflags", d.get("asflags", ASFLAGS))
        else:
            # a unit may replace the dll's flags, defines or include paths
            # (third-party code built its own way, such as bzip2 in hw.dll)
            cmd = [tc.cl_path()] + u.get("cflags", d["cflags"]) + list(extra)
            cmd += ["/D" + x for x in u.get("defines", d["defines"])]
            cmd += ["/I" + os.path.join(ROOT, x) for x in u.get("includes", d["includes"])]
            # Opt-in ABI for the local Xash3D integration, never matching builds.
            if dll in ("hl", "client") and os.environ.get("DECOF_XASH_ABI") == "1":
                cmd += ["/DCOFR_XASH_ABI"]
        cmd += ["/Fo" + out, os.path.join(ROOT, u["source"])]
        res = subprocess.run(cmd, env=tc.env(), cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        rc = res.returncode
        if rc == 0 and toolchain_name(d, u).startswith("vc6") and not is_asm(u):
            # VC6 numbers anonymous namespaces at random on each compile:
            # give them the names the retail RTTI records
            import vc6_anon
            vc6_anon.fix(out)
        print("%-6s %-30s %s" % (kind[:6], full, "ok" if rc == 0 else "FAILED (%d)" % rc))
        if rc:
            # cl prints its diagnostics on stdout: show the errors
            errors = [l for l in res.stdout.decode("latin-1").splitlines() if ": error " in l or ": fatal error " in l]
            for l in errors[:40]:
                print("       " + l.replace(ROOT + os.sep, ""))
            if len(errors) > 40:
                print("       ... %d more errors" % (len(errors) - 40))
        ok &= rc == 0
    return ok


MCP_URL = "http://127.0.0.1:8089"


def mcp_up():
    try:
        with urllib.request.urlopen(MCP_URL + "/check_connection", timeout=2) as r:
            return r.status == 200
    except Exception:
        return False


def mcp_call(path, body=None, query=None, timeout=120):
    url = MCP_URL + path
    if query:
        url += "?" + urllib.parse.urlencode(query)
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def local_names(d):
    """tools/symbols/<dll>_local.csv (va,name,unit,evidence): names local to
    their object that several objects define, such as file statics (a static
    CloneString, a file-local operator new). Each row is one object's copy;
    unit is the unit name without the dll ("GameUI/CareerDefinitions").
    Returns {name: {unit: va}}."""
    out = {}
    path = os.path.join(ROOT, d["symbols"]).replace("_1.6a.csv", "_local.csv")
    if os.path.exists(path):
        for r in csv.DictReader(open(path)):
            out.setdefault(r["name"], {})[r["unit"]] = r["va"].upper()
    return out


def merged_symbols(d, dll, tag=""):
    """tools/symbols/<dll>_1.6a.csv, then _decorated.csv (decorated forms of
    its undecorated C++ names), _auto.csv (harvested), then the
    curated _usermsgs.csv, _vtable.csv, _manual.csv and _local.csv (later
    files override earlier ones), written to build/jobs/. A tag gives the job
    its own copies, so concurrent delinks do not overwrite each other's files."""
    rows = {}
    curated = set()
    base = os.path.join(ROOT, d["symbols"])
    # base map, harvested names, then curated maps (later files win)
    for i, path in enumerate((base, base.replace("_1.6a.csv", "_decorated.csv"),
                              base.replace("_1.6a.csv", "_auto.csv"),
                              base.replace("_1.6a.csv", "_usermsgs.csv"),
                              base.replace("_1.6a.csv", "_vtable.csv"),
                              base.replace("_1.6a.csv", "_savetables.csv"),
                              base.replace("_1.6a.csv", "_bytes.csv"),
                              base.replace("_1.6a.csv", "_position.csv"),
                              base.replace("_1.6a.csv", "_manual.csv"))):
        if os.path.exists(path):
            for r in csv.DictReader(open(path)):
                va = r["va"].upper()
                rows[va] = (r["name"], r.get("kind") or "func")
                if i > 0:
                    curated.add(va)
    local = local_names(d)
    for name, units in local.items():
        for va in units.values():
            rows[va] = (name, "func")
            curated.add(va)
    # a name may live at one address only (the COFF exporter refuses
    # duplicates): curated entries win, then the strongest evidence in
    # docs/progress.csv; the losers are forgotten in Ghidra. Names local to
    # their object ($LN jump tables, the VC6 dynamic initializers _$E1,
    # _$E2, ... that every C++ object with constructed globals numbers from
    # 1) may label one address per unit, and so may the copies that
    # _local.csv lists: besides them the name keeps one other address
    rank = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "docs", "progress.csv"))):
        if r["dll"] == dll:
            ev = r["evidence"]
            rank[r["va"].upper()] = (0 if ev.startswith("refbytes") else 1 if "exports" in ev
                                     else 2 if ev.startswith(("hl_1.55", "crt")) else 3)
    by_name = {}
    for va, (name, _kind) in rows.items():
        by_name.setdefault(name, []).append(va)
    forget = []
    for name, vas in by_name.items():
        own = set(local.get(name, {}).values())
        vas = [va for va in vas if va not in own]
        if len(vas) > 1 and not name.startswith(("$", "_$E")):
            keep = [va for va in vas if va in curated]
            if len(keep) != 1:
                best = sorted(vas, key=lambda va: (rank.get(va, 9), va))
                keep = [best[0]] if rank.get(best[0], 9) < rank.get(best[1], 9) else []
            for va in vas:
                if not keep or va != keep[0]:
                    rows.pop(va, None)
                    forget.append((va, name))
    # names removed for good (tools/symbols/<dll>_forget.csv, with evidence)
    fp = base.replace("_1.6a.csv", "_forget.csv")
    if os.path.exists(fp):
        for r in csv.DictReader(open(fp)):
            va = r["va"].upper()
            if rows.get(va, ("",))[0] == r["name"]:
                rows.pop(va)
            forget.append((va, r["name"]))
    os.makedirs(os.path.join(ROOT, "build", "jobs"), exist_ok=True)
    with open(os.path.join(ROOT, "build", "jobs", "forget_%s%s.csv" % (dll, tag)), "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["va", "name"])
        w.writerows(forget)
    # sizes of named globals (tools/data_sizes.py): DelinkUnits defines
    # their extent so references into them become symbol+offset
    sizes = {}
    sp = base.replace("_1.6a.csv", "_datasizes.csv")
    if os.path.exists(sp):
        sizes = {r["name"]: r["size"] for r in csv.DictReader(open(sp))}
    out = os.path.join(ROOT, "build", "jobs", "symbols_%s%s.csv" % (dll, tag))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["va", "name", "kind", "size"])
        for va in sorted(rows):
            name, kind = rows[va]
            w.writerow([va, name, kind, sizes.get(name, "") if kind == "data" else ""])
    if tag:
        # other tools read the shared merged map: refresh it in one step
        shared = os.path.join(ROOT, "build", "jobs", "symbols_%s.csv" % dll)
        shutil.copyfile(out, shared + tag)
        os.replace(shared + tag, shared)
    return out


def write_job(d, items, job, dll, tag=""):
    os.makedirs(os.path.dirname(job), exist_ok=True)
    symbols = merged_symbols(d, dll, tag)
    # a name of _local.csv labels several addresses: each unit includes the
    # copy listed for it, any other unit the one copy the list leaves out
    local = local_names(d)
    vas = {}
    for r in csv.DictReader(open(symbols)):
        if r["name"] in local:
            vas.setdefault(r["name"], []).append(r["va"].upper())
    with open(job, "w") as f:
        f.write("symbols %s\n" % symbols)
        f.write("forget %s\n" % os.path.join(ROOT, "build", "jobs", "forget_%s%s.csv" % (dll, tag)))
        for u, full in items:
            f.write("unit %s\n" % obj_path("target", full))
            for name in unit_functions(u) + u.get("data", []):
                if name in local:
                    va = local[name].get(u["name"])
                    rest = [v for v in vas.get(name, []) if v not in local[name].values()]
                    if va is None and len(rest) == 1:
                        va = rest[0]
                    if va is not None:
                        f.write("include %s %s\n" % (name, va))
                        continue
                f.write("include %s\n" % name)


def build_target(units):
    """Delink retail units with tools/ghidra_scripts/DelinkUnits.java.

    Uses the running GhidraMCP server when it is up (it holds the project
    lock and needs GHIDRA_MCP_ALLOW_SCRIPTS=1), otherwise headless Ghidra.
    """
    by_dll = {}
    for dll, d, u, full in units:
        by_dll.setdefault(dll, (d, []))[1].append((u, full))
    script = os.path.join(ROOT, "tools", "ghidra_scripts", "DelinkUnits.java")
    use_mcp = mcp_up()
    ok = True
    for dll, (d, items) in by_dll.items():
        # one set of job files per process: agents delink concurrently
        tag = "_%d" % os.getpid()
        job = os.path.join(ROOT, "build", "jobs", "delink_%s%s.txt" % (dll, tag))
        write_job(d, items, job, dll, tag)
        for u, full in items:
            if os.path.exists(obj_path("target", full)):
                os.remove(obj_path("target", full))
        program = d["ghidra_folder"] + "/" + d["ghidra_program"]
        if use_mcp:
            res = mcp_call("/run_ghidra_script", body={
                "script_name": script, "args": job, "program": program,
                "timeout_seconds": 1800}, timeout=1900)
            mcp_call("/save_program", query={"program": program})
            rc = 0 if res.get("success") or res.get("status") == "success" else 1
            log = json.dumps(res)[:2000]
        else:
            log = os.path.join(ROOT, "build", "ghidra_logs", "delink_%s.log" % dll)
            cmd = [os.path.join(GHIDRA, "support", "analyzeHeadless.bat"),
                   os.path.join(ROOT, "re"), "CoF" + d["ghidra_folder"],
                   "-process", d["ghidra_program"], "-noanalysis",
                   "-scriptPath", os.path.dirname(script),
                   "-postScript", "DelinkUnits.java", job, "-log", log]
            rc = subprocess.call(cmd, cwd=ROOT, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.STDOUT)
        for u, full in items:
            exists = os.path.exists(obj_path("target", full))
            if exists and toolchain_name(d, u).startswith("vc6") and not is_asm(u):
                # the same object-local spellings as our compiled objects
                import vc6_anon
                vc6_anon.fix(obj_path("target", full))
            print("target %-30s %s" % (full, "ok" if rc == 0 and exists else
                                       "FAILED (%s)" % log))
            ok &= rc == 0 and exists
        for path in (job, os.path.join(ROOT, "build", "jobs", "symbols_%s%s.csv" % (dll, tag)),
                     os.path.join(ROOT, "build", "jobs", "forget_%s%s.csv" % (dll, tag))):
            if os.path.exists(path):
                os.remove(path)
    return ok


def write_objdiff(units):
    cfg = {
        "$schema": "https://raw.githubusercontent.com/encounter/objdiff/main/config.schema.json",
        "build_base": False,
        "build_target": False,
        "units": [],
    }
    for dll, d, u, full in units:
        cfg["units"].append({
            "name": full,
            "target_path": os.path.relpath(obj_path("target", full), ROOT).replace("\\", "/"),
            "base_path": os.path.relpath(obj_path("base", full), ROOT).replace("\\", "/"),
            "metadata": {"source_path": u["source"]},
        })
    with open(os.path.join(ROOT, "objdiff.json"), "w") as f:
        json.dump(cfg, f, indent=2)
        f.write("\n")
    print("wrote objdiff.json (%d units)" % len(cfg["units"]))


def report():
    # objdiff refuses the whole report when one object is missing (a unit
    # that does not compile): report from a copy of the config that leaves
    # out missing objects
    cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
    proj = os.path.join(ROOT, "build", "report_project")
    os.makedirs(proj, exist_ok=True)
    for u in cfg["units"]:
        for key in ("target_path", "base_path"):
            full = os.path.join(ROOT, u[key])
            if os.path.exists(full):
                u[key] = os.path.relpath(full, proj).replace("\\", "/")
            else:
                del u[key]
    with open(os.path.join(proj, "objdiff.json"), "w") as f:
        json.dump(cfg, f, indent=2)
    out = os.path.join(ROOT, "build", "report.json")
    rc = subprocess.call([OBJDIFF, "report", "generate", "-p", proj, "-o", out,
                          "-f", "json-pretty"], cwd=ROOT)
    if rc != 0:
        return False
    rep = json.load(open(out))
    for unit in rep.get("units", []):
        m = unit.get("measures", {})
        print("%-30s code %6.2f%%  (%s/%s bytes)" % (
            unit["name"], m.get("matched_code_percent", 0.0),
            m.get("matched_code", "0"), m.get("total_code", "0")))
        for fn in unit.get("functions", []):
            print("    %7.2f%%  %s" % (fn.get("fuzzy_match_percent", 0.0), fn["name"]))
    return True


def link(dll):
    cfg = json.load(open(CONFIG))["dlls"][dll]
    if "link" not in cfg:
        sys.exit("%s has no link section in tools/units.json" % dll)
    lk = cfg["link"]
    units = load_units([])
    # "skip_units": units whose retail bytes are linked instead
    # ("retail_objects", third-party code classed LIB)
    units = [x for x in units if x[0] == dll and x[3].split("/", 1)[1] not in lk.get("skip_units", ())]
    # "order": the units in the order of the retail image (the link order)
    if "order" in lk:
        pos = {n: i for i, n in enumerate(lk["order"])}
        missing = [full for _d, _c, _u, full in units if full.split("/", 1)[1] not in pos]
        if missing:
            sys.exit("units missing from the link order: %s" % " ".join(missing))
        units.sort(key=lambda x: pos[x[3].split("/", 1)[1]])
    # "objdir": compile into build/<objdir>/ (hw.dll: build/link_obj, so a
    # link never rewrites the build/base objects other work is diffing)
    kind = lk.get("objdir") or ("link_obj" if lk.get("cflags_extra") else "base")
    if not build_base(units, kind, lk.get("cflags_extra", ())):
        sys.exit("not linking: some units do not compile")
    # the DLL's toolchain links (and makes "staticlibs"), whatever compiled
    # its units
    tc = toolchain(cfg)
    tmp = os.path.join(ROOT, "build", "link", dll)
    os.makedirs(tmp, exist_ok=True)
    # "implibs": import libraries made from retail third-party DLLs. "libs" can
    # place one with "@<name>.lib" (the import table follows the library
    # order); the others go after "libs".
    made = {}
    for src, lib in lk.get("implibs", {}).items():
        path = os.path.join(tmp, lib)
        if subprocess.call([sys.executable, os.path.join(ROOT, "tools", "implib.py"),
                            os.path.join(ROOT, src), path, os.path.join(ROOT, "build", kind, dll)],
                           stdout=subprocess.DEVNULL) != 0:
            sys.exit("import library for %s failed" % src)
        made[lib] = path
    # "lib_members": {"<out>.obj": ["<official lib>", "<member>"]}: one
    # member of a newer official library with its debug sections dropped
    # (tools/libmember.py; link 6.00 cannot read them), placed by "@<out>.obj".
    # {"<out>.lib": ["<official lib>", "<member>", "<other official lib>"]}
    # adds the member to a copy of the other library instead, so the linker
    # pulls it among that library's members ("@<out>.lib")
    for name, spec in lk.get("lib_members", {}).items():
        lib, member = spec[0], spec[1]
        path = os.path.join(tmp, name)
        obj = path if len(spec) < 3 else os.path.join(tmp, "%s_%s" % (os.path.splitext(name)[0], member))
        if subprocess.call([sys.executable, os.path.join(ROOT, "tools", "libmember.py"), lib, member, obj],
                           stdout=subprocess.DEVNULL) != 0:
            sys.exit("library member %s(%s) failed" % (lib, member))
        if len(spec) > 2:
            if os.path.exists(path):
                os.remove(path)
            res = subprocess.run([os.path.join(os.path.dirname(tc.link_path()), "lib.exe"), "/nologo",
                                  "/out:" + path, spec[2], obj], env=tc.env(), cwd=ROOT,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if res.returncode != 0:
                print(res.stdout.decode("latin-1")[-2000:])
                sys.exit("library %s failed" % name)
        made[name] = path
    # "staticlibs": {"<name>.lib": "<unit>"} puts that unit and every unit
    # after it in the link order into a static library (lib.exe of the
    # toolchain) instead of the command line, like a retail project that
    # linked a library of its own (GameUI.dll and vgui_controls.lib). The
    # linker then pulls its members in the order they are first needed, after
    # the import thunks of the libraries before it.
    objs = [obj_path(kind, full) for _d, _c, _u, full in units]
    names = [full.split("/", 1)[1] for _d, _c, _u, full in units]

    def make_lib(lib, members):
        path = os.path.join(tmp, lib)
        if os.path.exists(path):
            os.remove(path)
        res = subprocess.run([os.path.join(os.path.dirname(tc.link_path()), "lib.exe"), "/nologo",
                              "/out:" + path] + members, env=tc.env(), cwd=ROOT,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if res.returncode != 0:
            print(res.stdout.decode("latin-1")[-2000:])
            sys.exit("static library %s failed" % lib)
        made[lib] = path

    # several libraries: each takes the units from its first one up to the
    # next library's first unit (taken from the last one backwards)
    for lib, first in sorted(lk.get("staticlibs", {}).items(), key=lambda x: -names.index(x[1])):
        k = names.index(first)
        make_lib(lib, objs[k:])
        objs, names = objs[:k], names[:k]
    # "retail_objects": third-party code without source or official library
    # (classed LIB), written from the retail bytes by tools/delink_retail.py
    # into build/link/<dll>/retail_<name>.obj and put into the library its
    # entry names, which "libs" places like any other ("@a3d_retail.lib")
    if lk.get("retail_objects"):
        if subprocess.call([sys.executable, os.path.join(ROOT, "tools", "delink_retail.py"), dll, tmp]) != 0:
            sys.exit("retail-derived objects have unresolved references")
        groups = {}
        for p in lk["retail_objects"]:
            groups.setdefault(p["lib"], []).append(os.path.join(tmp, "retail_%s.obj" % p["name"]))
        for lib, members in groups.items():
            if lib in made:
                # an import library made above: the piece joins it as a
                # member (the Miles mss32.lib holds MSS_auto_cleanup next to
                # its import members, so its thunks come before and after it)
                imp = made[lib] + ".imp"
                os.replace(made[lib], imp)
                members = [imp] + members
            make_lib(lib, members)
    libs = []
    for lib in lk["libs"]:
        if lib.startswith("@"):
            libs.append(made.pop(lib[1:]))
        else:
            libs.append(lib)
    libs += list(made.values())
    # "resources": a retail PE whose resources (icons, version) are linked in
    # as they are (tools/res_from_pe.py), so no asset is committed
    if "resources" in lk:
        res = os.path.join(tmp, os.path.splitext(lk["out"])[0] + ".res")
        if subprocess.call([sys.executable, os.path.join(ROOT, "tools", "res_from_pe.py"),
                            os.path.join(ROOT, lk["resources"]), res]) != 0:
            sys.exit("resources from %s failed" % lk["resources"])
        libs.append(res)
    out = os.path.join(tmp, lk["out"])
    flags = list(lk["flags"])
    if dll == "hl" and os.environ.get("DECOF_XASH_ABI") == "1":
        # Adding source entities must not renumber the original game exports.
        import pefile
        with pefile.PE(os.path.join(ROOT, lk["retail"])) as reference:
            native = [(e.ordinal, e.name.decode("ascii")) for e in reference.DIRECTORY_ENTRY_EXPORT.symbols]
        additions = ["weapon_rpg7", "ammo_rpg7", "cof8_rpg7_rocket", "?Fly@CCoFRRocket@@QAEXXZ", "?Hit@CCoFRRocket@@QAEXPAVCBaseEntity@@@Z"]
        ordinal = max(n for n, name in native) + 1
        definition = os.path.join(tmp, "cofr_hl_xash.def")
        with open(definition, "w") as stream:
            stream.write("LIBRARY hl\nEXPORTS\n")
            for n, name in native:
                stream.write('  %s @%d\n' % (name, n))
            for name in additions:
                stream.write('  %s @%d\n' % (name, ordinal)); ordinal += 1
        flags = [f for f in flags if not f.lower().startswith("/def:")] + ["/def:" + definition]
    cmd = [tc.link_path()] + flags + ["/out:" + out, "/map:" + out + ".map"]
    cmd += objs + libs
    res = subprocess.run(cmd, env=tc.env(), cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log = res.stdout.decode("latin-1").splitlines()
    with open(out + ".log", "w") as f:
        f.write("\n".join(log) + "\n")
    msgs = [l for l in log if " error " in l or " warning " in l or "fatal error" in l]
    if res.returncode != 0:
        print("\n".join(msgs[:40]))
        sys.exit("link failed (%d), %d messages, full log build/link/%s/%s.log"
                 % (res.returncode, len(msgs), dll, lk["out"]))
    if msgs:
        print("%d linker warnings, full log build/link/%s/%s.log" % (len(msgs), dll, lk["out"]))
    bindir = os.path.join(ROOT, "build", "bin")
    os.makedirs(bindir, exist_ok=True)
    import shutil
    shutil.copyfile(out, os.path.join(bindir, lk["out"]))
    print("linked build/bin/%s" % lk["out"])
    if "retail" in lk:
        subprocess.call([sys.executable, os.path.join(ROOT, "tools", "compare_pe.py"),
                         os.path.join(bindir, lk["out"]), os.path.join(ROOT, lk["retail"])])
    return True


def check(units):
    """Compile the units and print the per-function match of only those
    units (a one-unit objdiff report): the edit-diff loop."""
    build_base(units)
    names = {full for _d, _c, _u, full in units}
    cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
    # a unit created after the last objdiff.json (tools/make_unit.py)
    if names - {u["name"] for u in cfg["units"]}:
        write_objdiff(load_units([]))
        cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
    # one project directory per unit set, so parallel checks do not collide
    tag = "_".join(sorted(n.replace("/", "_") for n in names))[:120]
    proj = os.path.join(ROOT, "build", "check_project", tag)
    os.makedirs(proj, exist_ok=True)
    keep = []
    for u in cfg["units"]:
        if u["name"] not in names:
            continue
        for key in ("target_path", "base_path"):
            full = os.path.join(ROOT, u[key])
            if os.path.exists(full):
                u[key] = os.path.relpath(full, proj).replace("\\", "/")
            else:
                del u[key]
        keep.append(u)
    cfg["units"] = keep
    with open(os.path.join(proj, "objdiff.json"), "w") as f:
        json.dump(cfg, f, indent=2)
    out = os.path.join(proj, "report.json")
    subprocess.call([OBJDIFF, "report", "generate", "-p", proj, "-o", out, "-f", "json"], cwd=ROOT,
                    stderr=subprocess.DEVNULL)
    rep = json.load(open(out))
    for unit in rep.get("units", []):
        m = unit.get("measures", {})
        print("%-30s code %6.2f%%  (%s/%s bytes)" % (unit["name"], m.get("matched_code_percent", 0.0),
                                                   m.get("matched_code", "0"), m.get("total_code", "0")))
        funcs = unit.get("functions", [])
        exact = [fn for fn in funcs if fn.get("fuzzy_match_percent", 0.0) >= 100.0]
        print("    %d of %d functions at 100%%" % (len(exact), len(funcs)))
        # only the functions below 100% (the full list costs a lot of output
        # on large units); --all prints every function
        for fn in sorted(funcs, key=lambda f: f.get("fuzzy_match_percent", 0.0)):
            pct = fn.get("fuzzy_match_percent", 0.0)
            if pct < 100.0 or SHOW_ALL:
                # floor, so 99.996 does not print as 100.00
                print("    %7.2f%%  %s" % (int(pct * 100) / 100.0, fn["name"]))


def decomp(units):
    """Ghidra pseudo C of every function of the units, into build/decomp/<unit>/
    (reference only, through the running GhidraMCP server)."""
    if not mcp_up():
        sys.exit("decomp needs the GhidraMCP server")
    script = os.path.join(ROOT, "tools", "ghidra_scripts", "DecompileList.java")
    for dll, d, u, full in units:
        job = os.path.join(ROOT, "build", "jobs", "decomp_%s.txt" % full.replace("/", "_"))
        with open(job, "w") as f:
            f.write("out %s\n" % os.path.join(ROOT, "build", "decomp", *full.split("/")))
            for name in unit_functions(u):
                f.write("func %s\n" % name)
        res = mcp_call("/run_ghidra_script", body={
            "script_name": script, "args": job,
            "program": d["ghidra_folder"] + "/" + d["ghidra_program"],
            "timeout_seconds": 1800}, timeout=1900)
        out = res.get("console_output", "")
        print("decomp %-30s %s" % (full, "ok" if "DecompileList:" in out else json.dumps(res)[:300]))


SHOW_ALL = False


def main():
    global SHOW_ALL
    if "--all" in sys.argv:
        SHOW_ALL = True
        sys.argv.remove("--all")
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, sel = sys.argv[1], sys.argv[2:]
    if cmd == "link":
        link(sel[0])
        return
    if cmd == "decomp":
        decomp(load_units(sel))
        return
    if cmd == "check":
        check(load_units(sel))
        return
    all_units = load_units([])
    units = load_units(sel) if sel else all_units
    if cmd == "base":
        ok = build_base(units)
    elif cmd == "target":
        ok = build_target(units)
    elif cmd == "objdiff":
        write_objdiff(all_units)
        ok = True
    elif cmd == "report":
        ok = report()
    elif cmd == "all":
        ok = build_base(units) and build_target(units)
        write_objdiff(all_units)
        ok = ok and report()
    else:
        sys.exit(__doc__)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
