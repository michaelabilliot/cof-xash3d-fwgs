# DeCoF

DeCoF is a matching decompilation of **Cry of Fear** (Steam release, version string `1.6a`): the game DLLs, the custom GoldSrc engine build, the Paranoia OpenGL renderer, the menus and both launchers, rebuilt as C, C++, assembly and C# source that the original compilers turn back into the same machine code as the retail binaries, function by function.

> [!IMPORTANT]
> DeCoF is **not** an improved or fixed version of Cry of Fear. It is the complete decompilation of the game, kept as close to 1:1 with the retail binaries as possible: the same behavior, original bugs included. Nothing here makes the game better; it makes the game rebuildable from source.

![DeCoF progress](docs/progress.svg)

Every in-scope function of all seven binaries is MATCHED (byte-identical) or EQUIVALENT (logic proven identical, the remaining difference recorded). All seven binaries link from source, and `tools/make_dist.py` assembles a playable game from them plus your retail assets. In-game testing of the assembled build is the current step. Per-binary and per-category numbers are in [docs/PROGRESS.md](docs/PROGRESS.md), generated from [docs/progress.csv](docs/progress.csv), which has one row per retail function with its status, match percentage, source file and notes.

## At a glance

| Binary | What it is | Compiler | Units | Linked result |
|---|---|---|---|---|
| `cryoffear/cl_dlls/hl.dll` | Server game DLL (Half-Life SDK, Paranoia and Spirit of Half-Life code plus Cry of Fear's own) | MSVC 19.27.29112 (VS 2019 16.7) | 158 | `.text`, `.rdata`, `.data` and entry point at the retail sizes and addresses |
| `cryoffear/cl_dlls/client.dll` | Client DLL with the Paranoia OpenGL renderer | MSVC 19.27.29112 | 137 | all 593 exports identical (names, ordinals, order) |
| `hw.dll` (`sw.dll` is a copy) | GoldSrc engine, Cry of Fear build of 2013-04-21 | Visual C++ 6.0, SP6 with the Processor Pack for 10 objects, MASM for the Quake assembly | 197 | all 679 exports identical |
| `opengl32.dll` | Paranoia OpenGL wrapper | Visual C++ 6.0 | 2 | identical to retail except timestamps and the compiler edition tag in the Rich header |
| `cryoffear/cl_dlls/GameUI.dll` | Menus (VGUI2) | Visual C++ 6.0 | 131 | every function MATCHED, retail entry point |
| `cof.exe` | Engine launcher | Visual C++ 6.0 | 6 | links from source |
| `CoFLaunchApp.exe` | Settings launcher (Windows Forms) | Visual C# 2010 (csc 4.0.30319.1) | 1 project | identical to retail except the build stamps |

In total 21,509 functions in 631 translation units and one C# project: 21,464 MATCHED, 45 EQUIVALENT, and 96.8% of all code bytes identical to retail. Third-party libraries shipped with the game (Steam, tier0, vstdlib, VGUI, FileSystem, Cg, DevIL, irrKlang, id3lib, Miles, voice codecs) are out of scope; the retail files are used as they are.

## What "matching" means

- **MATCHED**: built with the original compiler and flags, the function's machine code is identical to the retail function (objdiff reports 100 percent, relocations point at the same symbols).
- **EQUIVALENT**: the function compiles and its behavior was proven identical to retail, but the bytes differ. The difference is recorded in its `docs/progress.csv` row. Almost all of them are register allocation or stack slot tie-breaks of the optimizer that no source form reproduces, and three hw.dll functions point to a compiler build that is not available (see "Limits" below).
- **TODO**: anything else. A stub is TODO. There are none left.
- Code the toolchain brings along (static CRT, STL instantiations, compiler helpers) is classed LIB. It comes from the compiler's own libraries at link time and is not decompiled or counted.

## How it was done

### 1. Identify the exact toolchains

Every claim about a compiler comes from the binaries: Rich header product IDs and build numbers, linker versions, embedded PDB paths, section layout and code idioms.

- hl.dll and client.dll were built by cl 19.27.29112, the compiler of VS 2019 16.7.6 and 16.7.7 (October 2020). The current installer only offers the later 29120 servicing build, so the 29112 package was extracted on its own. The Windows SDK 10.0.19041.1 libraries reproduce the retail static CRT and import hints.
- hw.dll, GameUI.dll, opengl32.dll and cof.exe were built by Visual C++ 6.0 (cl 12.00.8168). Ten hw.dll objects (bzip2 and the VGUI2 surface font code) carry the Processor Pack tag (c2 13.00.9044) and are built with VC6 SP6 plus the Processor Pack. The four Quake assembly files are assembled with MASM.
- CoFLaunchApp.exe was pinned to the C# 4 compiler of .NET Framework 4.0 by its `.maxstack` values: C# 2 to 4 count one stack slot too many where a `newobj` is the deepest point of a method, and the C# 5 compiler that later .NET versions install emits the exact depth.

Details, evidence and the flags of every unit are in [docs/toolchains.md](docs/toolchains.md) and [docs/tooling.md](docs/tooling.md).

### 2. Map every function

- Names were carried across builds by binary matching (2010 prototype, 1.0, 1.4, 1.55, 1.6a), starting from the server DLLs of 1.0, 1.4 and 1.55, which come with matching PDBs.
- The engine and VGUI2 names come from the DWARF debug information of Valve's Linux `hw.so` and `gameui.so`, plus RTTI, vtables, strings and export tables.
- Every name is stored with the evidence it comes from in `tools/symbols/`. A name with no proof carries the suffix `_guess`, with a stem chosen so it sorts into the function's retail position, because the linker orders the functions of an object by decorated name.
- Every retail function has a row in `docs/progress.csv` with its class (HLSDK, PARANOIA, SPIRIT, COF, ENGINE, UNKNOWN, LIB), source file, status and notes. Struct and class layouts that differ from the Half-Life SDK are in [docs/structs.md](docs/structs.md).

### 3. Cut the retail binaries into objects

The retail binaries are imported into Ghidra, and [ghidra-delinker-extension](https://github.com/boricj/ghidra-delinker-extension) exports one COFF object per reconstructed translation unit. The translation unit list and link order are rebuilt from the retail layout. Each unit can then be compiled and compared on its own with [objdiff](https://github.com/encounter/objdiff).

### 4. Write and match each function

Source starts from the expected ancestry (the [Half-Life SDK](https://github.com/ValveSoftware/halflife), [Paranoia](https://github.com/HLSources/Paranoia) and Spirit of Half-Life 1.7) and takes only the differences the binary shows. Code with no ancestry, including the whole engine, is written from the assembly. Each unit is compiled with the original compiler and flags and diffed per function, and the loop repeats until the function matches. After 8 failed attempts the exact remaining difference is recorded and work moves on.

The compiler behaviors that decide a match are written down in [docs/howto_cof_source.md](docs/howto_cof_source.md). A few examples:

- VC6 without optimization places stack slots and static uninitialized data in an order that follows a hash of their names.
- MSVC 14.27 places initialized data in the order variables are first declared, string literals in parse order, a vtable where its class is completed and RTTI where the class is first declared.
- A `_guess` stem decides where a function lands inside its object.

### 5. Prove what does not match

EQUIVALENT is a claim that needs proof. `tools/symeq.py` executes the retail and the rebuilt function symbolically, side by side. It compares calls with their arguments and order, stores to memory, branch conditions, switch dispatch and return values, and ignores register and stack slot choice. It was tested by planting differences (changed constants, conditions, argument order, a removed store) and caught every one. Regions it cannot follow are compared by hand, and each row's note says how its function was checked. The check also caught a real difference in a function already marked EQUIVALENT and in its sibling: a store to the wrong member, invisible to objdiff because it only changes a relocation addend.

### 6. Link and compare whole binaries

Each binary is linked by the original linker (link 14.27 or link 6.00) in the retail link order, with the retail library order. `tools/link_diff.py` compares the result with retail function by function, plus data contents, references, exports, imports, the Rich header and the layout. For hl.dll this drove the source down to details like the order of class bodies in one file and two unreferenced globals, until every section has the retail size.

hw.dll contains third-party code with no source and no official library: the Aureal A3D library, the IGA Radial ad SDK, a Miles Sound System helper and the DirectX manager of the OpenGL-on-Direct3D wrapper. `tools/delink_retail.py` cuts those pieces out of your own retail `hw.dll` at link time. They are never committed.

### 7. Assemble the game

`tools/make_dist.py` copies your retail 1.6a tree into `dist/` and replaces each binary that links completely from source. The game in `dist/` runs from the rebuilt binaries and the retail assets.

## Limits

- **VC6 Standard Edition.** Some hw.dll, GameUI.dll and opengl32.dll objects were compiled by the Standard Edition of Visual C++ 6.0, which orders some inline functions and strings differently from the Professional edition used here. It has no official download.
- **The SP5 front end.** Three VGUI2 font functions in hw.dll were compiled by a Processor Pack front end older than the SP6 one available.
- **Register tie-breaks.** The optimizer sometimes chooses between equal registers or stack slots based on details that do not exist in the source. The 45 EQUIVALENT rows list what was tried.
- **Rebuilt, not yet played.** The assembled build has not been confirmed in game yet.

## Building it yourself

Building gives you the seven binaries and a playable `dist/` folder. It needs the original compilers and your own copy of the game, but not Ghidra. Comparing your build with retail function by function also needs Ghidra and objdiff (step 6). The paths below are the defaults the scripts use; step 3 says where to change them.

### 1. Get the game

Install Cry of Fear from Steam (it is free). Copy the whole game folder, the one that holds `cof.exe`, to `versions/1.6a` inside the repository. The scripts only read it and never change it.

### 2. Install the tools

| Tool | Where to get it | Default location | Needed for |
|---|---|---|---|
| Python 3 with `pefile` and `capstone` | [python.org](https://www.python.org/downloads/), then `pip install pefile capstone` | on `PATH` | every script |
| MSVC 14.27 headers and libraries, Windows 10 SDK 10.0.19041 | [Visual Studio 2019 Build Tools](https://aka.ms/vs/16/release/vs_buildtools.exe) with the components "MSVC v142 - VS 2019 C++ x64/x86 build tools (v14.27)" and "Windows 10 SDK (10.0.19041.0)" | `C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools` and `C:\Program Files (x86)\Windows Kits\10` | hl.dll, client.dll |
| MSVC compiler 14.27.29112 | the package `Microsoft.VisualCpp.Tools.HostX86.TargetX86` 14.27.29112 of the Visual Studio 2019 16.7.7 release, extracted on its own; the Build Tools installer only offers the later 29120 build (steps in [docs/tooling.md](docs/tooling.md)) | `I:\Tools\msvc-14.27.29112` | hl.dll, client.dll |
| Windows 10 SDK 10.0.19041.1 libraries | Microsoft's `winsdksetup.exe` 10.1.19041.1 run with `/layout`, then its three library MSIs extracted with `msiexec /a` (steps in [docs/tooling.md](docs/tooling.md)) | `I:\Tools\winsdk-10.0.19041.1` | the static CRT of hl.dll and client.dll |
| Visual C++ 6.0 RTM, Professional or Enterprise Edition | your own Visual Studio 6.0 or Visual C++ 6.0 disc; Microsoft never offered a free download (it came with the old MSDN subscriptions). Copy the `VC98` folder; when it comes straight from the disc instead of from setup, also copy the six 8.3 STL headers to their long names, see [docs/toolchains.md](docs/toolchains.md) | `I:\Tools\vc6\VC98` | hw.dll, GameUI.dll, opengl32.dll, cof.exe |
| Visual C++ 6.0 SP6 with the Processor Pack | the portable VC6 package [SmileyAG/MSVC60](https://github.com/SmileyAG/MSVC60): copy its `vc_studio/main/VC98` folder. It is not an official Microsoft download: 1,703 of its files are byte-identical to the official disc, while the Service Pack and Processor Pack files could not be checked against Microsoft's own packages | `I:\Tools\vc6pp\VC98` | ten hw.dll objects (bzip2, VGUI2 fonts) and MASM for the four Quake assembly files |
| C# compiler 4.0.30319.1 | Microsoft's .NET Framework 4.0 redistributable `dotNetFx40_Full_x86_x64.exe`, extracted, not installed (steps in [docs/tooling.md](docs/tooling.md)) | `I:\Tools\csc40` | CoFLaunchApp.exe |
| Ghidra 12.1.4 with JDK 21, ghidra-delinker-extension 0.9.2, GhidraMCP 7.0.0-rc.2 | [Ghidra](https://github.com/NationalSecurityAgency/ghidra/releases), [Temurin JDK 21](https://adoptium.net/), [ghidra-delinker-extension](https://github.com/boricj/ghidra-delinker-extension/releases), [GhidraMCP](https://github.com/bethington/ghidra-mcp/releases) | `I:\Tools\ghidra_12.1.4_PUBLIC` | comparing only (step 6) |
| objdiff-cli 3.8.2 | [objdiff releases](https://github.com/encounter/objdiff/releases) | `I:\Tools\bin\objdiff-cli.exe` | comparing only (step 6) |

### 3. Point the scripts at your tools

If a tool is not at its default location, change the path at the top of the script that uses it: `tools/msvc.py` (`TOOLS_DIR`, `VC_DIR`, `SDK_DIR`, `SDK_LIB_DIR`), `tools/msvc6.py` (`ROOT_DIR`, `PP_ROOT_DIR`), `tools/coflaunchapp/build.py` (`CSC`), `tools/build.py` (`GHIDRA`, `OBJDIFF`) and `tools/ghidra_import.py` (`DEFAULT_GHIDRA`).

### 4. Get the third-party headers

These go into `refs/`, which is git-ignored. They are not game code; the game code needs them to compile.

| Folder | What it is | Where to get it |
|---|---|---|
| `refs/halflife` | Half-Life SDK (for the VGUI 1 import library) | `git clone https://github.com/ValveSoftware/halflife refs/halflife` |
| `refs/a3d-sdk` | Aureal A3D SDK headers | `git clone https://github.com/sigmaco/a3d-sdk refs/a3d-sdk` |
| `refs/bzip2-1.0.2-dl` | the official bzip2 1.0.2 source | `python tools/fetch_bzip2.py` |
| `refs/dxheaders` | `d3d.h`, `d3dcaps.h`, `d3dtypes.h`, `d3dvec.inl`, `ddraw.h` and `dinput.h` from `C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um`, plus `winapifamily.h` from the `shared` folder next to it | copy them from the Windows SDK installed in step 2 |
| `refs/irrklang-1.6.0` | irrKlang 1.6.0 SDK, 32-bit (`include` and `lib/Win32-visualStudio/irrKlang.lib`) | Ambiera only serves it to licensees now; the Internet Archive has a snapshot of `https://www.ambiera.at/downloads/irrKlang-32bit-1.6.0.zip` |
| `refs/id3lib-3.8.3` | id3lib 3.8.3 headers (`include`) | the source tarball on [SourceForge](https://sourceforge.net/projects/id3lib/files/id3lib/3.8.3/) |

### 5. Build

```
python tools/build.py link hl
python tools/build.py link client
python tools/build.py link hw
python tools/build.py link opengl32
python tools/build.py link gameui
python tools/build.py link launcher
python tools/coflaunchapp/build.py
python tools/make_dist.py --bin build/bin
```

Each `link` compiles every unit of that binary with its original compiler and flags, links it with the original linker in the retail order, and prints a section by section comparison with retail. The results land in `build/bin` (`launcher.exe` there becomes `cof.exe` in the game). The third-party pieces of hw.dll that have no source are cut from your own `versions/1.6a/hw.dll` during its link. `make_dist.py` then copies your game from `versions/1.6a` into `dist/` and puts the rebuilt binaries in place.

Test in `dist/` or in another copy of the game, never in your Steam installation.

### 6. Compare with retail (optional)

```
python tools/ghidra_import.py --only 1.6a        # create the Ghidra project re/CoF.gpr with the retail binaries
python tools/build.py target                     # cut the retail object of every unit
python tools/build.py check hl/dlls/cof_glock    # compile one unit and print the match of each function
python tools/objdiff_tool.py show hl/dlls/cof_glock "?PrimaryAttack@CGlock@@UAEXXZ"   # retail and ours side by side
python tools/symeq.py client/cl_dll/entity "?MuzzleFlash_guess@@YAXPAUcl_entity_s@@H_NM@Z"   # symbolic equivalence check
python tools/link_diff.py hl                     # compare a linked binary with retail
```

`target` runs headless Ghidra, or goes through the GhidraMCP server when `tools\ghidra_mcp_server.bat` is running (faster for repeated runs). The original Ghidra project has some function boundaries fixed by hand (recorded in [docs/sessions.md](docs/sessions.md)), so a freshly imported project can cut a few units differently.

The pre-commit hook in `tools/hooks/` regenerates the progress report (`docs/PROGRESS.md`, `docs/progress.svg`, `docs/progress.json`) whenever `docs/progress.csv` is committed. Install it with `cp tools/hooks/pre-commit .git/hooks/pre-commit`.

## Repository layout

| Path | Contents |
|---|---|
| `src/dlls`, `src/pm_shared`, `src/game_shared`, `src/common` | hl.dll (shared parts also in client.dll) |
| `src/cl_dll` | client.dll, including the Paranoia renderer |
| `src/engine_hw`, `src/engine`, `src/vgui2`, `src/public`, `src/mathlib`, `src/iga`, `src/external` | hw.dll |
| `src/opengl32` | opengl32.dll |
| `src/gameui`, `src/HLTV` | GameUI.dll |
| `src/launcher` | cof.exe |
| `src/coflaunchapp` | CoFLaunchApp.exe |
| `tools/` | Build, delink, diff, link and analysis scripts. `tools/units.json` lists every translation unit with its compiler, flags and link order |
| `tools/symbols/` | Names of retail functions and data, each with the evidence it comes from |
| `docs/` | Progress, toolchains, struct layouts, matching patterns, original bugs, references, session log |
| `versions/`, `refs/`, `build/`, `dist/` | Retail game builds, reference repositories, build output and the assembled game (none of them committed) |

## Conventions

- The source follows the Half-Life SDK and Paranoia style it descends from. No decompiler artifacts (`FUN_`, `DAT_`, `iVar1`, raw offsets where a member exists).
- Only language features the original compiler supports.
- Names come from evidence: PDBs of older builds, Linux debug information, strings, vtables or reference source. Anything else carries the suffix `_guess`.
- Original behavior is kept, bugs included. Known bugs are listed in [docs/original_bugs.md](docs/original_bugs.md), so nobody fixes them by accident.
- Commits: one per function or small group, message `<dll>: <function or file> (<status>, <percent>)`, with `docs/progress.csv` committed together with the code it describes.

## Documentation

| File | What it covers |
|---|---|
| [docs/toolchains.md](docs/toolchains.md) | How each compiler was identified, with the evidence |
| [docs/tooling.md](docs/tooling.md) | Tool versions and sources, the build and link commands per binary |
| [docs/howto_cof_source.md](docs/howto_cof_source.md) | The workflow for a source file and the compiler patterns that decide a match |
| [docs/structs.md](docs/structs.md) | Struct and class layouts that differ from the Half-Life SDK |
| [docs/original_bugs.md](docs/original_bugs.md) | Bugs of the retail game, kept on purpose |
| [docs/refs.md](docs/refs.md) | Reference repositories, what each provides and its license |
| [docs/sessions.md](docs/sessions.md) | Dated log of every work session |

## What's next

Play the assembled build and fix anything that behaves differently from retail. The follow-up project is [ReCoF](https://github.com/caixax/ReCoF).

## References and credits

The code descends from the [Half-Life SDK](https://github.com/ValveSoftware/halflife), [Paranoia](https://github.com/HLSources/Paranoia) and Spirit of Half-Life 1.7. [ReHLDS](https://github.com/dreamstalker/rehlds), [Xash3D FWGS](https://github.com/FWGS/xash3d-fwgs), cry-of-fear-src, cof-enhanced, RebuiltSimon, MadSimonX and CryOfFear-FirstPerson were consulted, and everything they claim was checked in the binary. See [docs/refs.md](docs/refs.md) for what each one provides and its license. The tools that made this possible are [Ghidra](https://github.com/NationalSecurityAgency/ghidra), [ghidra-delinker-extension](https://github.com/boricj/ghidra-delinker-extension), [GhidraMCP](https://github.com/bethington/ghidra-mcp), [objdiff](https://github.com/encounter/objdiff) and [capstone](https://github.com/capstone-engine/capstone).

## License

See [LICENSE](LICENSE). In short: the tools and documentation written for DeCoF are under the MIT License; source code derived from the Half-Life SDK stays under the [Half-Life 1 SDK License](LICENSES/Half-Life-1-SDK.txt) (free distribution only), and code derived from Paranoia under Paranoia's terms; the code reconstructed from the Cry of Fear binaries belongs to its original authors, and no rights over it are granted.

This project is not affiliated with or endorsed by Team Psykskallar or Valve. The repository contains no game assets or retail binaries; you need your own copy of Cry of Fear.
