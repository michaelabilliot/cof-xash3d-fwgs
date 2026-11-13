# Toolchains per DLL and game version

Generated from `tools/pe_inventory.py` (Rich header, optional header, debug directory) and `tools/pdbfile.py` (S_COMPILE2 records in the PDBs). Raw data: `docs/versions.csv`.

## Summary

| Version | DLL | Compiler | Linker | Config | CRT |
|---|---|---|---|---|---|
| 2010 prototype | client.dll | cl 13.10.3077 (VC++ .NET 2003 RTM) | link 7.10.3077 | Release (`cl_dll\Release\client.pdb`) | static |
| 2010 prototype | server.dll | cl 13.10.3077 | link 7.10.3077 | unknown, no debug directory | static |
| 1.0 | client.dll | cl 13.10.3077 | link 7.10.3077 | Release (`cl_dll\Release\client.pdb`) | static |
| 1.0 | hl.dll | cl 13.10.3077 | link 7.10.3077 | Debug, /ZI (PDB present) | LIBCMTD (static debug) |
| 1.4 | hl.dll | cl 13.10.3077 | link 7.10.3077 | Debug, /ZI (PDB present) | LIBCMTD |
| 1.55 | client.dll | cl 13.10.3077 | link 7.10.3077 | Release | static |
| 1.55 | hl.dll | cl 13.10.3077 | link 7.10.3077 | Debug, /ZI (PDB present) | LIBCMTD |
| **1.6a** | **client.dll** | **cl 19.27.29112 (VS 2019 16.7, v142 14.27)** | **link 14.27.29112** | Release (`D:\CoFFinalPatch\code\cl_dll\Release\client.pdb`) | static (VC 14.27.29110, UCRT of SDK 10.0.19041.1) |
| **1.6a** | **hl.dll** | **cl 19.27.29112 (VS 2019 16.7, v142 14.27)** | **link 14.27.29112** | Release (`D:\CoFFinalPatch\code\dlls\Releasehl\hl.pdb`) | static (VC 14.27.29110, UCRT of SDK 10.0.19041.1) |
| all | opengl32.dll | cl 12.00.8168 (VC++ 6.0 RTM, Standard Edition) | link 6.00.8168 | Release | MSVCRT.dll (/MD) |
| **1.6a** | **hw.dll** (sw.dll identical) | **cl 12.00.8168 (VC++ 6.0 RTM), mostly Standard Edition objects, plus VC6 processor pack (12.00.9044) and VS 2005 (14.00.50727) objects** | **link 6.00.8168** | Release, no debug directory | static |
| **1.6a** | **GameUI.dll** | **cl 12.00.8168 (VC++ 6.0 RTM), Standard Edition C++ objects** | **link 6.00.8168** | Release, no debug directory | static |
| **1.6a** | **cof.exe** | **cl 12.00.8168 (VC++ 6.0 RTM)** | **link 6.00.8168** | Release, no debug directory | static |
| **1.6a** | **CoFLaunchApp.exe** | **Visual C# 2010, csc 4.0.30319.1 (C# 4, .NET Framework 4 Client Profile)** | - | Release (`...\CoFLaunchApp\obj\x86\Release\CoFLaunchApp.pdb`) | mscoree.dll |

## Evidence

### 2010 to 1.55: Visual C++ .NET 2003

- Rich header of every client.dll, hl.dll and server.dll from the prototype to 1.55: `Utc1310_C/CPP.3077`, `Masm710.3077`, `Export710.3077`, `Linker710.3077`. Optional header linker version 7.10.
- The three hl.dll PDBs (1.0, 1.4, 1.55) agree: every game object carries S_COMPILE2 "Microsoft (R) Optimizing Compiler" 13.10.3077, the link record is "Microsoft (R) LINK" 7.10.3077, and the CRT comes from `c:\Program Files (x86)\Microsoft Visual Studio .NET 2003\Vc7\lib\LIBCMTD.lib` and `libcpmtd.lib`.
- Original source root: `s:\CryofFear\Sourcecode\` (2012) and `s:\half-life\CryOfFear\sourcecode\code_src\` (2010 prototype client).

### hl.dll 1.0 to 1.55 is a Debug build

- Objects live in `s:\CryofFear\Sourcecode\dlls\debughl\`, the HLSDK Debug output directory of `hl.dsp`.
- S_COMPILE2 flags of the 159 game objects in 1.55: 156 C++ and 3 C objects with flag 0x100 (fEC, Edit and Continue, so /ZI). VC++ 7.1 rejects /ZI together with /O1, /O2 or /Ox, so this code is unoptimized (/Od). CRT objects carry 0x2000 (/GS) instead.
- Linked against LIBCMTD.lib (static debug CRT, /MTd).
- Exports `trip_beam` and `?ManagerReport@CMultiManager@@QAEXXZ`. Both exist only under `_DEBUG` in the HLSDK. They are gone in 1.6a.
- All three PDBs match their DLLs exactly (RSDS GUID and age). 1.4 and 1.55 share PDB GUID D13BE348195D42DBBCCBF0E581F409B5 (ages 54 and 90): the same incremental PDB, linked again 36 times between the two releases. 1.0 has its own GUID (age 784).

### 1.6a (Steam): Visual Studio 2019 16.7, Release

- PE timestamps 2020-10-28 (client.dll) and 2020-10-30 (hl.dll).
- Optional header linker version 14.27, subsystem version 6.00, new `.rsrc` section (side-by-side manifest).
- Rich header of hl.dll: `Utc1900_CPP.29112` x159, `Utc1900_C.29112` x3, `Linker1400.29112`, `Export1400.29112`, `Cvtres1400.29112`. client.dll: `Utc1900_CPP.29112` x140, `Utc1900_C.29112` x3, same tools. Build 29112 with linker 14.27 is the v142 toolset of VS 2019 16.7 (cl.exe 19.27.29112, link.exe 14.27.29112).
- The other entries come from prebuilt static libraries, not from game code: `Utc1900_*.27412` and `.28920`, `Masm1400.*` (static CRT and UCRT), `Implib800.50727` (DevIL, irrKlang), `Linker600.8447` (Cg import libs), `Implib1000.30319` (steam_api.lib), `AliasObj1100.41118`.
- No `Utc1900_LTCG_*` entries, so no /GL (no whole program optimization).
- Imports contain no MSVCR/VCRUNTIME DLL, so the CRT is static (/MT).
- Static CRT libraries (settled 2026-10-25): `Utc1900_*.28920` are the VC runtime objects of libcmt.lib, libvcruntime.lib and libcpmt.lib of the 14.27.29110 libraries (the VS 16.7.7 package `Microsoft.VisualCpp.CRT.x86.Desktop` 14.27.29110 holds byte identical libraries to the installed VC.14.27 component). `Utc1900_*.27412` are the libucrt.lib objects of the Windows 10 SDK 10.0.19041. The 10.0.18362 libucrt.lib is compiled by build 26715, so retail did not use it. Retail was linked with the original 10.0.19041.1 release of that SDK (the one VS 2019 16.7 installs), not with the 10.0.19041.5609 servicing build installed here: the servicing libucrt.lib adds `check_trailing` to _fptostr.obj (printf rounding) and differs in cfout.obj, and its kernel32.lib gives other import hints. Linked with the 10.0.19041.1 libucrt.lib and import libraries, hl.dll gets the retail .text size and file size and the retail kernel32 import hints (details in "Static CRT experiment" below).
- 1.55 hl.dll had 159 game objects. 1.6a hl.dll has 162. The new exports (`CBowlingBall`, `CMp5`, `weapon_mp5`, `ammo_mp5`, `bowling_ball`, `cof_mp5modesaverestore`) account for the growth.
- The 102 extra client.dll exports are implicit move constructors and move assignment operators of the exported VGUI classes (`$$QAV01@`), which a C++11 compiler generates. They are not new game code.

### opengl32.dll: Paranoia wrapper, Visual C++ 6.0 Standard

- Same file in the prototype, 1.0 and 1.6a (SHA-256 26732d7a9aeb8079..., PE timestamp 2006-11-27).
- Rich header: `Utc12_CPP_Std.8168` x2, `Linker600.8168`, `Masm613.7299`. "Std" is the Standard Edition, whose compiler has no optimizer (it accepts /O2 and ignores it).
- Contains `PARANOIA_HACKS_V1` and `opengl32.log`. Source is in `refs/Paranoia/code_src/opengl32/` (VC6 project, `/G6 /Gz /MD /W4 /GX /O2`).
- The prototype also has `parnaoia_opengl32.dll` (2008, VC++ .NET 2003), a later rebuild of the same wrapper that CoF never loads.

### Preliminary flag evidence for 1.6a (byte scans of .text, to be confirmed in the RE tool)

| Pattern | 1.6a hl.dll | 1.6a client.dll | 1.55 hl.dll (Debug) |
|---|---|---|---|
| `/GS` prologue `mov eax,[__security_cookie]; xor eax,ebp` | 365 | 547 | 0 |
| SSE `movss` loads | 11043 | 5233 | 0 |
| `this` spilled to `[ebp-x]` | 391 | 373 | 5522 |
| `/RTC` stack fill (`mov eax,0CCCCCCCCh; rep stosd`) | 0 | 0 | 0 |
| frame `push ebp; mov ebp,esp` | 6416 | 5401 | about one per function |
| aligned frames `and esp,-8/-16` | 246 | 52 | 5 |

Reading: optimized Release code (/O2-class), `/GS` on, SSE2 scalar math (the VS 2019 default `/arch:SSE2`), no /RTC. Many functions keep an EBP frame, so `/Oy-` is possible; this needs disassembly to settle. The 1.55 Debug build has no /RTC stack fill either (/ZI /Od without /RTC1).

### 1.6a hw.dll, GameUI.dll, cof.exe, CoFLaunchApp.exe (Phase 0 inventory)

All values from `tools/pe_inventory.py` (`docs/versions.csv`), 2026-10-10.

- **hw.dll** (2013-04-21, linker 6.00): Rich header `Utc12_C_Std.8168` x92, `Utc12_CPP_Std.8168` x98 (Standard Edition), `Utc12_C.8168` x199, `Utc12_CPP.8168` x18, `Utc12_2_C.9044` x7 and `Utc12_2_CPP.9044` x3 (VC6 with the processor pack), `Utc12_2_C.8943` x1, `Utc13_C.8830` x3, `Utc1400_C.50727` x72 (VS 2005), `Masm613.7299` x46, `Linker600.8168`. The header does not say which of these entries are engine objects and which come from prebuilt libraries (Miles, VGUI, steam_api_c); that is settled per object when hw.dll units are built. The 7 `Utc12_2_C.9044` objects are the bzip2 1.0.2 library (blocksort, huffman, crctable, randtable, compress, decompress, bzlib; version string "1.0.2, 30-Dec-2001", stdcall API from BZ_EXPORT): the official bzip2-1.0.2 source (sourceware.org, SHA-512 checked) compiled with VC6 RTM `/O2 /DWIN32 /DBZ_EXPORT` gives BZ2_bzBuffToBuffCompress, BZ2_bzBuffToBuffDecompress and BZ2_bz__AssertH__fail byte for byte, but the larger functions differ in register allocation and loop strength reduction, which needs the processor pack compiler (c2.dll 13.00.9044, VC6 with the Processor Pack). Built with the `vc6pp` toolchain (below, 2026-10-25) all 47 bzip2 functions are byte identical (MATCHED). The 3 `Utc12_2_CPP.9044` objects are vgui2/vgui_surfacelib FontManager.cpp, Win32Font.cpp and FontAmalgam.cpp (linked between the vgui2 controls library and bzip2): with `vc6pp` 65 of their 69 functions match, against 49 with VC6 RTM (docs/tooling.md).
- Rich header audit for the processor pack (2026-10-25, `@comp.id` prod 48 `Utc12_2_C` and prod 49 `Utc12_2_CPP`): only hw.dll has such entries, 7 C and 3 C++ objects of build 9044, which are exactly the 7 bzip2 units and the 3 vgui_surfacelib units, plus one `Utc12_2_C.8943` object (an earlier processor pack build, not identified yet; lead, 2026-10-25: the only C objects hw.dll takes from prebuilt SDK libraries without source are DirectX SDK members, the dinput.lib data formats `c_dfDIMouse` and `c_dfDIMouse2` at 01E8EA58 and 01E8EA70 with their object tables in .text at 01E138C0 right after the `DirectInputCreateA` thunk, then the keyboard table, and dxguid.lib GUIDs such as GUID_Key; with the `Utc13_C.8830` x3 entries these four are the likely candidates, unverified because no DirectX SDK lib is on disk). GameUI.dll (prod 10, 11, 22 of build 8168), opengl32.dll (prod 22 of 8168) and cof.exe (prod 10, 11 of 8168) contain no processor pack object, so nothing outside hw.dll changes toolchain.
- **GameUI.dll** (2013-04-21, linker 6.00): `Utc12_CPP_Std.8168` x131, `Utc12_C.8168` x155, `Utc12_CPP.8168` x13, `Masm613.7299` x35, `Linker600.8168`.
- **cof.exe** (linker 6.00): `Utc12_C.8168` x122, `Utc12_CPP.8168` x10, `Masm613.7299` x21, `Cvtres500.1720`, `Linker600.8168`. Version resource 1, 1, 1, 1.
- **CoFLaunchApp.exe**: managed (imports only `mscoree.dll`), PDB path `U:\common\Cry of Fear\CryOfFear\StatsResetProgram\CoFLaunchApp\...`, assembly version 1.0.0. No native compiler involved. C# compiler: Visual C# 2010 (csc 4.0.30319.1), shown by a .maxstack one above the exact depth where a method peaks at a newobj (the C# 5 compiler of .NET 4.5 and later gives the exact depth); the linker version 11.0 comes from the runtime PE writer of a machine with .NET 4.5 or later (docs/tooling.md, CoFLaunchApp.exe).
- The Standard Edition compiler has no optimizer; it accepts /O2 and generates unoptimized code. The installed Professional Edition with /Od produces the same code (proven on opengl32.dll).

## Inline expansion experiment (/Ob1), 2026-10-11

VC6 `/O2` implied `/Ob1` (expand only functions marked inline or defined in the class body); from VS 2005 on `/O2` implies `/Ob2` (also expand any suitable function). A project converted from the VC6 `hl.dsp` and `cl_dll.dsp` of the HLSDK and Paranoia (`/O2`, no `/Ob`) may keep the old behaviour. Read-only test: all units compiled with the current flags and again with `/Ob1` added, everything else equal.

| DLL | Units compared | Functions | 100% now | 100% with /Ob1 | Lost |
|---|---|---|---|---|---|
| hl.dll | the 48 units that compile | 1,901 | 1,056 | 1,224 (+169) | 1 (`CSquadMonster::ScheduleChange`) |
| client.dll | temporary units for `pm_shared/pm_shared.c`, `pm_math.c`, `pm_debug.c` (the client functions named so far) | 29 | 28 | 28 | 0 |
| client.dll | `pm_math.c` plus `InterpolateAngles` and `VectorMatrix` (unnamed in client.dll, byte identical to the hl.dll copies) | 12 | 9 | 11 (+2) | 0 |
| opengl32.dll | both units | 367 | 367 | 367 | 0 |

Readings:

- hl.dll: in 1.6a every entity export is a 5-byte `jmp GetClassPtr<T>`. The template is not marked inline, so only `/Ob1` keeps it out of line; with the current flags it is expanded into the export. The 169 gains include those and functions whose callees stay calls under `/Ob1`.
- client.dll: no client source is built yet, so the only evidence is the shared pm_shared code. `InterpolateAngles` and `VectorMatrix` match only with `/Ob1` in both DLLs, and their client.dll bytes equal the hl.dll bytes. Two functions; to be confirmed when client units exist.
- opengl32.dll: built with `/Od` (Standard Edition, no optimizer), so inline expansion never happens and `/Ob1` changes nothing. The other VC6 binaries were also built by VC6, whose `/O2` already means `/Ob1`.
- Decision (user, 2026-10-11): `/Ob1` is applied to hl.dll only (`tools/units.json`). Rebuilt from the committed sources, hl.dll goes from 1,036 to 1,201 MATCHED functions. `CSquadMonster::ScheduleChange` drops from 100% to 1.7% and is tracked as a known regression in `docs/progress.csv`. client.dll and opengl32.dll keep their flags; client.dll is tested again first thing once real client units exist.

## Static CRT experiment (Windows 10 SDK 10.0.19041.1 libraries), 2026-10-25

Question: which library versions the static CRT part of the 1.6a hl.dll and client.dll came from. Before, with every hl.dll game function at its retail address, .text was 320 bytes shorter than retail, all inside the static CRT.

- VC runtime (libcmt, libvcruntime, libcpmt): the Rich header counts of `Utc1900_*.28920` equal ours, and the libraries of the VS 16.7.7 package `Microsoft.VisualCpp.CRT.x86.Desktop` 14.27.29110 (SHA-256 checked against Microsoft's channel manifest) are byte identical to the installed 14.27.29110 ones. Nothing to change.
- UCRT: the retail `Utc1900_*.27412` entries (34 C, 191 C++, 25 other, the same counts as ours) are the compiler of the 10.0.19041 libucrt.lib. The 10.0.18362 libucrt.lib (from its official winsdksetup.exe 10.1.18362.1) is compiled by build 26715, so that SDK is excluded. The installed SDK is the servicing build 10.0.19041.5609; the original 10.0.19041.1 release (winsdksetup.exe 10.1.19041.1, the `Win10SDK_10.0.19041` payload of the VS 16.7.7 manifest) has another libucrt.lib (no `check_trailing` in _fptostr.obj) and 132 of 482 import libraries differ.
- Relink with the 10.0.19041.1 `ucrt\x86` and `um\x86` libraries (compiler, headers and flags unchanged; the objects are identical, only the output path in `.debug$S` differs):

| Measure | hl.dll before | hl.dll after | client.dll before | client.dll after |
|---|---|---|---|---|
| .text virtual size (retail hl 0x19FE6C, client 0x13840C) | 0x19FD2C (-320) | 0x19FE6C (equal) | 0x13808C (-896) | 0x1381CC (-576, the rest is TODO and EQUIVALENT game code and its CRT pulls) |
| file size, SizeOfCode | 2,413,056 / 0x19FE00 | 2,413,568 / 0x1A0000 (retail) | unchanged | unchanged |
| static CRT bytes in function chunks that differ from retail (rebuilt / retail) | 31,692 / 32,012 | 4,078 / 4,078 | 78,606 / 79,118 | 64,176 / 64,368 |
| .text bytes that differ at the same offset | 91,675 | 12,081 | 1,143,843 | 1,143,027 |
| link_diff LIB rows without counterpart / with other code | 19 / 2 | 0 / 1 | 30 / 3 (+8 in game code) | 19 / 2 (+7) |
| kernel32 import hints that differ | 102 | 0 | order differs (CRT pulls) | order differs |
| user32 / advapi32 hints that differ | 0 / - | 0 / - | 1 / 2 | 0 / 0 |
| CRT functions named by tools/objmatch.py (unique) | 1,611 | 1,619 | 1,642 | 1,650 |

- Adopted: `tools/msvc.py` links with `SDK_LIB_DIR` (`I:\Tools\winsdk-10.0.19041.1`, docs/tooling.md) and `tools/map_names.py` matches against its libucrt.lib. Headers, rc.exe and mt.exe stay those of the installed SDK.
- Fixed 2026-10-25 by the include order of CoF_DualPistol.cpp (<fstream> before player.h, docs/tooling.md hl.dll notes; an object lists its externals in declaration order). Before: libcpmt uncaught_exception.obj (the 5-byte `jmp __uncaught_exception`) comes right after xlock.obj in ours and after xthrow.obj in retail, so 35 functions up to syserror.obj sit 5 bytes off. The linker pulls library members in the order their symbols are first referenced; our first STL user, cof_dualpistol.obj, lists `?uncaught_exception@std@@YA_NXZ` before `?_Xlength_error@std@@YAXPBD@Z` in its symbol table (multiplay_gamerules.obj has the retail order), so it is the order the compiler emits references in that object, not a library version. client.dll shows the same jmp.

## Change between versions

1. Prototype to 1.55: no toolchain change (VC++ .NET 2003 RTM throughout).
2. 1.55 to 1.6a: client.dll and hl.dll were rebuilt in October 2020 with VS 2019 16.7 (toolset 14.27). hl.dll also changed from Debug to Release. The codegen of the target has nothing in common with the PDB builds, so the PDBs give names, types, source files and link order, not byte patterns.
3. opengl32.dll never changed.

## Toolchains needed for matching

| Purpose | Toolchain | Status on this machine |
|---|---|---|
| 1.6a client.dll and hl.dll (the target) | cl 19.27.29112 / link 14.27.29112, VC libraries 14.27.29110, Windows 10 SDK 10.0.19041.1 static UCRT and import libraries | **installed** (`I:\Tools\msvc-14.27.29112` and `I:\Tools\winsdk-10.0.19041.1`, see `docs/tooling.md`); proven by `hl/dlls/h_export` at 100% and by the hl.dll link (retail .text size, retail kernel32 import hints) |
| opengl32.dll, hw.dll, GameUI.dll, cof.exe | Visual C++ 6.0 RTM, cl 12.00.8168 / link 6.00.8168 | **installed** in `I:\Tools\vc6\VC98` from the user's licensed Visual C++ 6.0 Professional Edition (part X04-58170, disc image supplied by the user, MD5 checked); `tools/msvc6.py`. The install copied from the disc lacked the long-name STL headers (setup renames them from their 8.3 names): `ALGRITHM`, `FCTIONAL`, `STDXCEPT`, `STREAMBF`, `STRSTREM` and `XCEPTION` were copied to `algorithm`, `functional`, `stdexcept`, `streambuf`, `strstream` and `exception` in `I:\Tools\vc6\VC98\INCLUDE`, following `SETUP\VC98PRO.INF` (2026-10-17, needed by GameUI's `<vector>`). Proven by opengl32.dll: all 367 functions MATCHED, `.text` `.data` `.reloc` of the full link identical to retail. The retail opengl32.dll was compiled by the Standard Edition (no optimizer); Professional with `/Od` gives the same code, only the Rich header edition tag differs. |
| hw.dll objects of the processor pack (`Utc12_2_C/CPP.9044`: bzip2, vgui_surfacelib) | Visual C++ 6.0 SP6 with the Processor Pack: cl 12.00.8804, c1 12.00.8867, c1xx 12.00.8964, c2.dll 13.00.9044 | **installed** in `I:\Tools\vc6pp\VC98` with the user's approval (2026-10-25); `tools/msvc6.py` (`msvc6.PP`), toolchain `vc6pp`, chosen per unit (`"toolchain": "vc6pp"` in tools/units.json) while hw.dll keeps linking with the RTM link 6.00.8168. Provenance: the folder is byte-identical to `vc_studio/main/VC98` of the portable VC6 package github.com/SmileyAG/MSVC60 (commit 0cb3ee8, packed by "Psycho-A", copy in `I:\Tools\_downloads\msvc60_smileyag`), not a run of Microsoft's SP6 and Processor Pack setups. 1703 of its files are byte-identical to the official disc in `I:\Tools\vc6`; the others are the SP6 and Processor Pack files, which could not be checked against Microsoft's packages here (Defender scan clean). The folder has no mspdb60.dll (a VC6 install keeps it in Common\MSDev98\Bin): msvc6.py puts the RTM `I:\Tools\vc6\VC98\BIN` after the vc6pp BIN on the PATH for it. Microsoft shipped the Processor Pack for SP5, so retail most likely paired the SP5 front end with the same c2.dll 13.00.9044 back end, while this package has the SP6 front end; whether that explains the four functions that still differ (docs/tooling.md) is untested. Proven by bzip2: 47 of 47 functions MATCHED. |
| hw.dll Quake assembly objects (math.s, snd_mixa.s, sys_wina.s, worlda.s) | retail: MASM 6.13.7299 (`Masm613.7299` x46, the only assembler entry of the hw.dll Rich header; the VC6 static CRT asm objects carry the same tag, which is also the only Masm entry of GameUI.dll x35, cof.exe x21 and opengl32.dll) | ML 6.13.7299 is not installed (Microsoft shipped it as a patch to MASM 6.11). Used instead: ML 6.15.8803 of the Processor Pack in `I:\Tools\vc6pp\VC98\BIN` (the user approved assembling with ml.exe; `.asm` units of build.py, docs/tooling.md); the official ML 14.27.29112 in `I:\Tools\msvc-14.27.29112` gives the same bytes. Proven: all 14 routines of the four objects MATCHED. A link would show `@comp.id` 0x002A2263 (ML 6.15, build 8803) for these four objects in the Rich header instead of Masm613.7299. |
| Optional: solving functions in the 1.55 PDB build first | Visual C++ .NET 2003 RTM, cl 13.10.3077 | not installed, same availability as VC6. Not needed for the target. |
