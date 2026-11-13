r"""Build CoFLaunchApp.exe from src/coflaunchapp.

The retail launcher (PE timestamp 2013-04-19) is a Visual Studio 2010 Windows
Forms project (Release, x86, .NET Framework 4 Client Profile, PDB path
U:\...\CoFLaunchApp\obj\x86\Release\CoFLaunchApp.pdb). It was compiled by
Visual C# 2010, the C# 4 compiler of .NET Framework 4.0 (csc 4.0.30319.1,
also the in-process compiler of the VS 2010 IDE). This script runs that
compiler, extracted from Microsoft's .NET Framework 4.0 redistributable into
I:\Tools\csc40 (setup in docs/tooling.md).

How the compiler was pinned down:
  - .maxstack: C# 2, 3 and 4 (csc 2.0, 3.5, 4.0.30319.1) count the object a
    newobj pushes before popping its arguments, so a method whose deepest
    point is a newobj gets one more than the exact depth. The C# 5 compiler
    that .NET Framework 4.5 to 4.8 install as csc.exe (4.8.9232 here) emits
    the exact depth. Retail has the higher value in InitializeComponent (new
    Font with 5 arguments at depth 6, .maxstack 7) and in
    Resources.get_ResourceManager (new ResourceManager at depth 2, .maxstack
    3); csc 4.0 reproduces both, csc 4.8 gives 6 and 2.
  - linker version 11.0 in the PE header is written by the runtime's PE
    writer, not by csc: csc 4.0 running on the 4.8 runtime also writes 11.0,
    so the build machine had .NET 4.5 or later installed.
  - with csc 4.0 every method, the metadata (only the MVID differs), the
    managed resources, the Win32 resources and the file size equal retail.

Source order: csc emits types, members, assembly references and assembly
attributes in command line order (csc 4.0 and 4.8 alike). The order below
gives the retail metadata; it is not the order a VS 2010 project would pass
(template order with the target framework file last), so the original build
was most likely the IDE's in-process compiler, which gets its file list from
the project system. Its output path also kept the drive letter "U:", where
csc.exe writes "u:" into the debug directory.

Resources are never stored in the repository. At build time the embedded
.resources files and the Win32 resources are pulled out of the retail exe with
ildasm, the application icon is rebuilt from the Win32 resources, and the
output goes to build/coflaunchapp/CoFLaunchApp.exe, build/bin and
build/dist_coflaunchapp (the folder make_dist.py --bin takes for a test build).

Usage: python tools/coflaunchapp/build.py [--retail-path] [--csc PATH]
  --retail-path  compile on a temporary subst drive U: inside the retail
                 project path, so the debug directory holds the retail PDB
                 path (only the drive letter case differs) and compare.py can
                 compare the whole file byte by byte
  --csc PATH     another compiler (for example the C# 5 csc of the installed
                 framework, to see the .maxstack difference)
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RETAIL = os.path.join(ROOT, "versions", "1.6a", "CoFLaunchApp.exe")
SRC = os.path.join(ROOT, "src", "coflaunchapp")
OUT = os.path.join(ROOT, "build", "coflaunchapp")
RES = os.path.join(OUT, "retail")
OBJ = os.path.join(OUT, "obj")
DIST = os.path.join(ROOT, "build", "dist_coflaunchapp")
# Project folder of the retail build (from the PDB path in its debug directory)
RETAIL_DRIVE = "U:"
RETAIL_PROJECT = r"\common\Cry of Fear\CryOfFear\StatsResetProgram\CoFLaunchApp\CoFLaunchApp"

FRAMEWORK = r"C:\Windows\Microsoft.NET\Framework\v4.0.30319"
CSC = r"I:\Tools\csc40\csc.exe"
ILDASM = r"C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.8.1 Tools\ildasm.exe"

# Compile order. Recovered from the binary: retail lists the types
# Form1, Settings, Resources, Program, and the assembly references in the
# order System.Windows.Forms, System, mscorlib, System.Drawing. csc emits
# types in command line order and this order reproduces both. Form1.cs must
# come before Form1.Designer.cs so the Form1 methods precede Dispose and
# InitializeComponent. AssemblyInfo.cs declares no types, its place only
# matters relative to the target framework file (see below).
SOURCES = [
    "Form1.cs",
    "Form1.Designer.cs",
    r"Properties\Settings.Designer.cs",
    r"Properties\Resources.Designer.cs",
    "Program.cs",
    r"Properties\AssemblyInfo.cs",
]

# Embedded resources, in manifest order.
RESOURCES = [
    "CoFLaunchApp.Form1.resources",
    "CoFLaunchApp.Properties.Resources.resources",
]

# References of the Visual Studio 2010 Windows Forms Application template.
# The compiler only emits assembly references that are actually used.
REFERENCES = [
    "Microsoft.CSharp.dll",
    "mscorlib.dll",
    "System.Core.dll",
    "System.Data.DataSetExtensions.dll",
    "System.Data.dll",
    "System.Deployment.dll",
    "System.dll",
    "System.Drawing.dll",
    "System.Windows.Forms.dll",
    "System.Xml.dll",
    "System.Xml.Linq.dll",
]

# MSBuild generates this file for every project targeting .NET 4 and passes it
# to csc after the project sources. csc emits assembly level attributes
# file by file in reverse command line order, and the CLR metadata writer then
# sorts the CustomAttribute table with an unstable quicksort, so the attribute
# order ildasm shows depends on that input order. Simulating the sort shows
# that retail emitted the AssemblyInfo.cs attributes before
# TargetFrameworkAttribute. With csc that needs this file before
# AssemblyInfo.cs, so it is passed first.
TARGET_FRAMEWORK_FILE = ".NETFramework,Version=v4.0,Profile=Client.AssemblyAttributes.cs"
TARGET_FRAMEWORK_SOURCE = (
    "// <autogenerated />\r\n"
    "using System;\r\n"
    "using System.Reflection;\r\n"
    "[assembly: global::System.Runtime.Versioning.TargetFrameworkAttribute("
    "\".NETFramework,Version=v4.0,Profile=Client\", "
    "FrameworkDisplayName = \".NET Framework 4 Client Profile\")]\r\n"
)


def run(cmd, cwd):
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, errors="replace")
    if proc.returncode != 0:
        sys.stdout.write(proc.stdout[-4000:])
        sys.stdout.write(proc.stderr[-4000:])
        sys.exit("command failed: %s" % os.path.basename(cmd[0]))
    return proc.stdout


def extract_retail_resources():
    os.makedirs(RES, exist_ok=True)
    for name in os.listdir(RES):
        os.remove(os.path.join(RES, name))
    # ildasm writes the managed .resources files into its current directory and
    # the Win32 resources next to the /out file.
    run([ILDASM, "/nobar", "/out=retail.il", RETAIL], RES)
    for name in RESOURCES + ["retail.res"]:
        if not os.path.isfile(os.path.join(RES, name)):
            sys.exit("ildasm did not produce %s" % name)


def read_res_entries(path):
    """Parse a Win32 .res file into (type, name, data) tuples."""
    data = open(path, "rb").read()
    entries = []
    pos = 0

    def read_id(p):
        if struct.unpack_from("<H", data, p)[0] == 0xFFFF:
            return struct.unpack_from("<H", data, p + 2)[0], p + 4
        end = p
        while struct.unpack_from("<H", data, end)[0] != 0:
            end += 2
        return data[p:end].decode("utf-16-le"), end + 2

    while pos + 8 <= len(data):
        size, header = struct.unpack_from("<II", data, pos)
        rtype, p = read_id(pos + 8)
        rname, p = read_id(p)
        entries.append((rtype, rname, data[pos + header:pos + header + size]))
        pos = (pos + header + size + 3) & ~3
    return entries


def rebuild_icon(res_path, ico_path):
    """Rebuild the application .ico from RT_GROUP_ICON and RT_ICON entries."""
    RT_ICON, RT_GROUP_ICON = 3, 14
    entries = read_res_entries(res_path)
    icons = dict((name, blob) for rtype, name, blob in entries if rtype == RT_ICON)
    groups = [blob for rtype, name, blob in entries if rtype == RT_GROUP_ICON]
    if not groups:
        sys.exit("no icon group in the retail Win32 resources")
    group = groups[0]
    reserved, kind, count = struct.unpack_from("<HHH", group, 0)
    header = struct.pack("<HHH", reserved, kind, count)
    directory = b""
    images = b""
    offset = 6 + 16 * count
    for i in range(count):
        w, h, colors, res0, planes, bits, size, icon_id = struct.unpack_from("<BBBBHHIH", group, 6 + 14 * i)
        image = icons[icon_id]
        directory += struct.pack("<BBBBHHII", w, h, colors, res0, planes, bits, len(image), offset)
        images += image
        offset += len(image)
    with open(ico_path, "wb") as f:
        f.write(header + directory + images)


def compile_exe(outdir=OUT):
    """Compile into outdir and return the path of the exe."""
    if not os.path.isfile(CSC):
        sys.exit("compiler not found: %s (setup in docs/tooling.md)" % CSC)
    os.makedirs(OBJ, exist_ok=True)
    os.makedirs(outdir, exist_ok=True)
    attr_file = os.path.join(OBJ, TARGET_FRAMEWORK_FILE)
    with open(attr_file, "w", newline="") as f:
        f.write(TARGET_FRAMEWORK_SOURCE)
    icon = os.path.join(OBJ, "app.ico")
    rebuild_icon(os.path.join(RES, "retail.res"), icon)

    exe = os.path.join(outdir, "CoFLaunchApp.exe")
    cmd = [
        CSC,
        "/noconfig",
        "/nowarn:1701,1702",
        "/nostdlib+",
        "/platform:x86",
        "/errorreport:none",
        "/warn:4",
        "/define:TRACE",
    ]
    cmd += ["/reference:" + os.path.join(FRAMEWORK, r) for r in REFERENCES]
    cmd += [
        "/debug:pdbonly",
        "/filealign:512",
        "/optimize+",
        "/out:" + exe,
    ]
    cmd += ["/resource:%s,%s" % (os.path.join(RES, r), r) for r in RESOURCES]
    cmd += [
        "/target:winexe",
        "/win32icon:" + icon,
        "/utf8output",
    ]
    cmd.append(attr_file)
    cmd += SOURCES
    out = run(cmd, SRC)
    warnings = [l for l in out.splitlines() if ": warning " in l]
    for line in warnings:
        print(line.strip())
    return exe


def compile_at_retail_path():
    """Compile on a temporary subst drive inside the retail project folder."""
    if os.path.exists(RETAIL_DRIVE + "\\"):
        sys.exit("drive %s is in use, build without --retail-path" % RETAIL_DRIVE)
    target = os.path.join(OUT, "retail_drive")
    os.makedirs(target, exist_ok=True)
    run(["subst", RETAIL_DRIVE, target], ROOT)
    try:
        project = RETAIL_DRIVE + RETAIL_PROJECT
        built = compile_exe(os.path.join(project, "obj", "x86", "Release"))
        exe = os.path.join(OUT, "CoFLaunchApp.exe")
        shutil.copyfile(built, exe)
        shutil.copyfile(built[:-4] + ".pdb", exe[:-4] + ".pdb")
    finally:
        run(["subst", RETAIL_DRIVE, "/d"], ROOT)
    return exe


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--retail-path", action="store_true")
    ap.add_argument("--csc")
    args = ap.parse_args()
    if args.csc:
        global CSC
        CSC = args.csc
    extract_retail_resources()
    exe = compile_at_retail_path() if args.retail_path else compile_exe()
    print("built %s with %s" % (os.path.relpath(exe, ROOT), CSC))
    # tools/make_dist.py takes complete rebuilt binaries from build/bin and
    # from the per binary test folder (make_dist.py --bin build/dist_coflaunchapp)
    for folder in (os.path.join(ROOT, "build", "bin"), DIST):
        os.makedirs(folder, exist_ok=True)
        shutil.copyfile(exe, os.path.join(folder, "CoFLaunchApp.exe"))


if __name__ == "__main__":
    main()
