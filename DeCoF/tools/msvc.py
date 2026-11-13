"""Toolchain environment for the 1.6a target: cl 19.27.29112 / link 14.27.29112.

The compiler binaries come from the official VS 2019 16.7.7 package
Microsoft.VisualCpp.Tools.HostX86.TargetX86 14.27.29112 (extracted to
TOOLS_DIR). Headers and libraries come from the MSVC 14.27.29110 side by
side toolset of VS 2019 Build Tools and the Windows 10 SDK 10.0.19041.0.
The Windows SDK link-time libraries (static UCRT and import libraries) are
those of the original 10.0.19041.1 release that VS 2019 16.7 installed,
extracted to SDK_LIB_DIR: the installed 19041 SDK is a later servicing build
whose libucrt.lib and import libraries differ from the ones retail was
linked with (docs/toolchains.md).

Usage as a module: env(), cl_path(), link_path().
Usage as a script:  python tools/msvc.py cl <args...>
"""
import os
import subprocess
import sys

TOOLS_DIR = r"I:\Tools\msvc-14.27.29112\bin\Hostx86\x86"
VC_DIR = (r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools"
          r"\VC\Tools\MSVC\14.27.29110")
SDK_DIR = r"C:\Program Files (x86)\Windows Kits\10"
SDK_VER = "10.0.19041.0"
# Windows 10 SDK 10.0.19041.1 libraries (Lib\10.0.19041.0\ucrt and um), used
# at link time only; headers and tools stay those of SDK_DIR
SDK_LIB_DIR = r"I:\Tools\winsdk-10.0.19041.1\Windows Kits\10"

# Local integration builds can select an installed compatible x86 toolset
# without changing the defaults used for byte-identical reproduction.
TOOLS_DIR = os.environ.get("DECOF_TOOLS_DIR", TOOLS_DIR)
VC_DIR = os.environ.get("DECOF_VC_DIR", VC_DIR)
SDK_DIR = os.environ.get("DECOF_SDK_DIR", SDK_DIR)
SDK_VER = os.environ.get("DECOF_SDK_VER", SDK_VER)
SDK_LIB_DIR = os.environ.get("DECOF_SDK_LIB_DIR", SDK_LIB_DIR)


def cl_path():
    return os.path.join(TOOLS_DIR, "cl.exe")


def link_path():
    return os.path.join(TOOLS_DIR, "link.exe")


def env():
    e = dict(os.environ)
    e["INCLUDE"] = ";".join([
        os.path.join(VC_DIR, "include"),
        os.path.join(SDK_DIR, "Include", SDK_VER, "ucrt"),
        os.path.join(SDK_DIR, "Include", SDK_VER, "um"),
        os.path.join(SDK_DIR, "Include", SDK_VER, "shared"),
    ])
    e["LIB"] = ";".join([
        os.path.join(VC_DIR, "lib", "x86"),
        os.path.join(SDK_LIB_DIR, "Lib", SDK_VER, "ucrt", "x86"),
        os.path.join(SDK_LIB_DIR, "Lib", SDK_VER, "um", "x86"),
    ])
    # rc.exe and mt.exe (the linker embeds the manifest with them)
    sdk_bin = os.path.join(SDK_DIR, "bin", SDK_VER, "x86")
    e["PATH"] = os.pathsep.join([TOOLS_DIR, sdk_bin, e.get("PATH", "")])
    # Keep the compiler from picking up stray settings
    for k in ("CL", "_CL_", "LINK", "_LINK_"):
        e.pop(k, None)
    return e


def check():
    for p in (cl_path(), link_path(), os.path.join(VC_DIR, "include"),
              os.path.join(SDK_DIR, "Include", SDK_VER),
              os.path.join(SDK_LIB_DIR, "Lib", SDK_VER, "ucrt", "x86", "libucrt.lib")):
        if not os.path.exists(p):
            sys.exit("missing toolchain component: %s" % p)


if __name__ == "__main__":
    check()
    if len(sys.argv) < 2 or sys.argv[1] not in ("cl", "link"):
        sys.exit("usage: msvc.py cl|link <args...>")
    exe = cl_path() if sys.argv[1] == "cl" else link_path()
    sys.exit(subprocess.call([exe] + sys.argv[2:], env=env()))
