"""Toolchain environments for the VC6 binaries.

vc6    cl 12.00.8168 / link 6.00.8168, Visual C++ 6.0 RTM (the default).
       hw.dll, opengl32.dll, GameUI.dll and cof.exe of 1.6a were built with
       it (Rich header build 8168; opengl32.dll and part of GameUI.dll with
       the Standard Edition, which has no optimizer). Files come from the
       user's licensed Visual C++ 6.0 Professional Edition (part X04-58170),
       copied from the disc's VC98 folder to ROOT_DIR (no installer run).
vc6pp  Visual C++ 6.0 SP6 with the Processor Pack (cl 12.00.8804, c1xx
       12.00.8964, c2.dll 13.00.9044), installed by its official setups into
       PP_ROOT_DIR. The Rich header entries Utc12_2_C / Utc12_2_CPP build
       9044 of hw.dll (bzip2 and vgui_surfacelib) come from it. Used per unit
       ("toolchain": "vc6pp" in tools/units.json); the DLL still links with
       the vc6 link 6.00.8168.

Usage as a module: RTM and PP (cl_path(), link_path(), env(), check()); the
module-level functions are the RTM ones. As a script:
  python tools/msvc6.py [--pp] cl|link <args...>
"""
import os
import subprocess
import sys

ROOT_DIR = r"I:\Tools\vc6\VC98"
PP_ROOT_DIR = r"I:\Tools\vc6pp\VC98"


class Toolchain:
    def __init__(self, name, root, extra_path=()):
        self.name = name
        self.root = root
        self.extra_path = list(extra_path)

    def cl_path(self):
        return os.path.join(self.root, "BIN", "CL.EXE")

    def link_path(self):
        return os.path.join(self.root, "BIN", "LINK.EXE")

    def env(self):
        r = self.root
        e = dict(os.environ)
        e["INCLUDE"] = ";".join([os.path.join(r, "INCLUDE"), os.path.join(r, "MFC", "INCLUDE"),
                                 os.path.join(r, "ATL", "INCLUDE")])
        e["LIB"] = ";".join([os.path.join(r, "LIB"), os.path.join(r, "MFC", "LIB")])
        e["PATH"] = os.pathsep.join([os.path.join(r, "BIN")] + self.extra_path + [e.get("PATH", "")])
        for k in ("CL", "_CL_", "LINK", "_LINK_"):
            e.pop(k, None)
        return e

    def check(self):
        for p in (self.cl_path(), self.link_path(), os.path.join(self.root, "INCLUDE")):
            if not os.path.exists(p):
                sys.exit("missing %s component: %s" % (self.name, p))


RTM = Toolchain("vc6", ROOT_DIR)
# the VC98 folder of vc6pp has no mspdb60.dll (a VC6 install keeps it in
# Common\MSDev98\Bin), which cl, c1, c1xx and c2 import: take the RTM one
# from the PATH. cl loads c1, c1xx and c2 from its own folder first.
PP = Toolchain("vc6pp", PP_ROOT_DIR, extra_path=[os.path.join(ROOT_DIR, "BIN")])

cl_path = RTM.cl_path
link_path = RTM.link_path
env = RTM.env
check = RTM.check


if __name__ == "__main__":
    args = sys.argv[1:]
    tc = RTM
    if args and args[0] == "--pp":
        tc = PP
        args = args[1:]
    tc.check()
    if not args or args[0] not in ("cl", "link"):
        sys.exit("usage: msvc6.py [--pp] cl|link <args...>")
    exe = tc.cl_path() if args[0] == "cl" else tc.link_path()
    sys.exit(subprocess.call([exe] + args[1:], env=tc.env()))
