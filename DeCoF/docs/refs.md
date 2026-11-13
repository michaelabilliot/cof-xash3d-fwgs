# Reference repositories

All clones live in `refs/` (git-ignored). Pinned commits at the time of Phase 0 (2026-10-10):

| Repo | Commit | Role | License |
|---|---|---|---|
| ValveSoftware/halflife | b1b5cf5 | ancestry (base) | Half-Life 1 SDK license (free distribution only) |
| HLSources/Paranoia | c2d8018 | ancestry (renderer, Spirit fork) | GPL-2.0 (Paranoia code), Valve SDK license (Valve files), "Crusader 2002" (opengl32 wrapper) |
| Mama2uuw/cry-of-fear-src | 161da7b | consultation only | Valve HL1 SDK license (hlsdk-portable), openvgui license, FWGS GPL |
| hajdawery/cof-enhanced | 409c69d | consultation only | GPL-3.0-or-later |
| cd-n0/RebuiltSimon | fca8c87 | consultation only | GPL-3.0 |
| kohtep/MadSimonX | b43724c | consultation only | BSD-2-Clause |
| thomaspatrickpearl53-ctrl/CryOfFear-FirstPerson | d876897 | consultation only | MIT |
| rehlds/ReHLDS | 550f2d6 | consultation only (engine) | MIT |
| FWGS/xash3d-fwgs | 51353ff8 | consultation only (engine) | GPL-3.0-or-later (with some GPL-2.0 files) |
| sohl-modders/Spirit-of-Half-Life-1.7 (`refs/Spirit`) | d50205b | ancestry (Spirit 1.7) | Valve SDK license (Valve files); Spirit code by Laurie Cheers, no separate license file |

Project rule: only `refs/halflife` and `refs/Paranoia` (and Spirit once confirmed) are ancestry that src/ may start from. Everything else is read for facts, every fact is verified in the binary, and no code is copied.

## A. refs/halflife (Valve Half-Life SDK)

What it provides: the HLSDK game code for both DLLs. 63 commits, no tags.

- `de70fad` (2013-08-30, "initial seed of Half-Life 1 SDK") is the closest to SDK 2.3, but it is already a Steam-era seed (`archtypes.h`, `MAXSTUDIOSEQUENCES 2048`, `weapondata[64]`, CZERO additions). `c7240b9` (2019-06-06, tip of `origin/steam_legacy`) is the last state before the HL25 rewrite (`0fc8913`, 2024-08-20). HEAD is post-HL25 and must not be used as the base.
- Paranoia's headers (`MAXSTUDIOSEQUENCES 256`, `weapondata[32]`, no CZERO API) show the CoF lineage is pre-Steam SDK 2.3, so `de70fad` diffs are a guide, not ground truth. Prefer Paranoia's copy of a Valve file when both exist.

File to DLL map:

| DLL | Directories |
|---|---|
| hl.dll | `dlls/`, `dlls/wpn_shared/`, `pm_shared/` (.c), `game_shared/voice_gamemgr.cpp`; includes `engine/`, `common/`, `public/` |
| client.dll | `cl_dll/`, `cl_dll/hl/`, client weapon files from `dlls/` and `dlls/wpn_shared/`, `game_shared/vgui_*`, `game_shared/voice_banmgr.cpp`, `common/parsemsg.cpp`, `public/interface.cpp`, `pm_shared/` (.c); includes `utils/vgui/include` |

Project files: `dlls/hl.dsp` and `cl_dll/cl_dll.dsp` (VC6) at de70fad, `projects/vs2010/` (de70fad to c7240b9), `projects/vs2019/` (HEAD). hl.dsp Release: `/G5 /MT /W3 /GR /Zi /O2`, defines `NDEBUG WIN32 _WINDOWS QUIVER VOXEL QUAKE2 VALVE_DLL CLIENT_WEAPONS`. hl.dsp Debug: `/G5 /MTd /Gm /ZI /Od` (matches the CoF 1.0 to 1.55 hl.dll PDB evidence: Debug config, /ZI, output dir `debughl`).

## B. refs/Paranoia (Paranoia 1.2 source kit, 2008-01-09)

What it provides: the direct ancestor of CoF's renderer and of much of its game code. `code_src/` holds `cl_dll/`, `dlls/`, `common/`, `engine/`, `game_shared/`, `pm_shared/`, `opengl32/`, `utils/vgui/`, `zhlt_bump/`. No `public/` directory.

- **Paranoia is a Spirit of Half-Life fork.** `//LRC` comments in 97 dlls files and 13 client files, `m_pMoveWith`, `CalcLocus`, `alias.cpp` ("Mod 'Spirit of Half-Life', by Laurie R. Cheers"), `locus.cpp` ("Spirit of Half-Life 0.7"), output DLL named `spirit.dll`. Its entity set matches Spirit 1.5a4 to 1.7, minus Spirit's FMOD entities.
- Project files (VC6, Release only):
  - `dlls/hl.dsp`: `/G5 /MT /W3 /O2`, no CLIENT_WEAPONS, out `spirit.dll`.
  - `cl_dll/cl_dll.dsp`: `/MT /W3 /GX /Zi /O2`, `CLIENT_DLL CLIENT_WEAPONS`.
  - `opengl32/opengl32.dsp`: `/G6 /Gz /MD /W4 /GX /O2 /Op /Ob2`, `/def:opengl32.def`.
- **opengl32 wrapper source is included**: `code_src/opengl32/` (`opengl32.cpp`, `OtherOGL.cpp`, `opengl32.h`, `opengl32.def`). `OtherOGL.cpp:885` appends `" PARANOIA_HACKS_V1"`, `opengl32.cpp:475` writes `opengl32.log`. This is the source of the retail CoF `opengl32.dll` (VC6 Standard build of 2006-11-27). The retail DLL exports exactly the 361 names of `opengl32.def`, contains both source strings (`PARANOIA_HACKS_V1`, `opengl32.log`) and imports what the source calls (LoadLibraryA, GetSystemDirectoryA, GetProcAddress, MSVCRT fopen and fprintf). cof-enhanced calls the retail copy "modified Paranoia"; nothing found so far supports that. The 2008 source kit may be newer than the 2006 binary, so small code differences are possible.
- `entvars_t` (`engine/progdefs.h`) is identical to HLSDK. The CoF extra field is therefore a CoF change, not inherited from Paranoia.

File to DLL map:

| DLL | Files |
|---|---|
| hl.dll | HLSDK `hl.dsp` set minus hl_wpn_glock, observer, rat, schedule, shotgun; plus `AI_BaseNPC_Schedule`, `alias`, `locus`, `movewith`, `paranoia_*` weapons and monsters |
| client.dll | HLSDK client set plus renderer `gl_*.cpp`, `glmanager`, `glows`, `grass`, `quake_bsp`, `triapiobjects`, particles (`particlemgr/msg/sys`, `miniparticles`), `rain`, `mp3`, `log`, `exception`, extra VGUI panels (`vgui_paranoiatext`, `vgui_subtitles`, `vgui_tips`, ...) |
| opengl32.dll | `code_src/opengl32/*` |

## C. refs/cry-of-fear-src (CoF reimplementation on Xash3D FWGS)

What it provides: a from-scratch rebuild of CoF-style gameplay on hlsdk-portable and Xash3D FWGS. No submodules (vendored: xash3d-fwgs, hlsdk-portable, openvgui under `external/`). Game code in `src/cof/dlls` (hl.dll) and `src/cof/cl_dll` (client.dll).

- CoF classnames were harvested from BSPs (`scripts/scan-bsp-entities.ps1`), so its names for entities are useful as cross-checks against hl.dll exports, but its class layouts, message names (`CofInv*`, `CofLadder`) and logic are inventions.
- Documents no retail binary facts. Lowest value of the set.

## D. refs/cof-enhanced (Xash3D FWGS patch set that runs the retail DLLs)

What it provides: about 90 engine patches plus the ABI research needed to host the unmodified Steam `client.dll` and `hl.dll`. No engine tree in the repo (it targets FWGS `4857b389e6ba`). Highest value for struct layouts. Key documents: `docs/history/entvars-layout-analysis.md`, `docs/history/abi-findings.md`, `docs/patches/*.md`, `docs/dev/patch-stack.md`.

Retail facts it states (to be verified in Phase 1, see `docs/structs.md`):

- `entvars_t`: 4 extra bytes right after `light_level` (0x124); `sequence` at 0x12C instead of 0x128; size 0x2A8.
- `edict_t` stride 0x32C (stock 0x324): 4 bytes beyond the entvars change, location of the extra field unknown.
- `playermove_t`: 4 bytes inserted before `numtouch` (retail 0x45490).
- hl.dll: `GetEntityAPI` table at 0x102119F8 (50 entries), `GiveFnptrsToDll` at 0x100B4AA0, player save table at 0x10217FE8 (207 records), `gEntvarsDescription` at file offset 0x219AE0.
- client.dll: `cl_enginefunc_t` copy at 0x101B2608, 137 user messages.
- hw.dll is a CoF-specific engine build (cheat commands removed).

File to DLL map: none (no game code). Patches touch engine only.

## E. refs/RebuiltSimon (speedrun plugin, C, CMake, MinHook)

What it provides: hardcoded RVAs and byte signatures for the Steam build. `halflife_sdk/` is an xash3d-fwgs submodule (checked out). Plugin code in `RebuiltSimon/` (`Hooks/Client`, `Hooks/Server`, `SDK/`, `Features/`). Facts it states (to verify):

- hl.dll: pmove pointer at RVA 0x220C50; NOCLIP to WALK reset in `CBasePlayer::PostThink` at RVA 0xEB203; signatures for `PM_PreventMegaBunnyJumping` (RVA 0x8E70) and `PM_UnDuck` (RVA 0x95B0, aligned-stack prologue with /GS cookie); CBasePlayer offsets 0x2038, 0x21E8 (infinite stamina), 0x21F0 (stamina float).
- hw.dll (out of scope): engine table addresses (`cl_enginefunc_t`, `enginefuncs_t`, `DLL_FUNCTIONS`, `cldll_func_t`), `Host_ValidSave` signature (used by BunnymodXT to detect the CoF Steam build).
- client.dll: mouse accumulator at RVA 0x543580.

File to DLL map: none (no game code).

## F. refs/MadSimonX (debug plugin, C++, MSVC 2022)

What it provides: RVAs for hl.dll helpers and hand-reconstructed class headers in `MadSimonX/vendor/hlsdk/` (Spirit `//LRC` fields, Paranoia fields, CoF fields, "1.6a" padding blocks). Facts it states (to verify):

- hl.dll RVAs: `UTIL_FindEntityByString` 0x118480, `CBasePlayer::GiveNamedItem` 0xE0A80 (two extra parameters vs HLSDK), `UTIL_ScreenShake` 0x119DE0, `CBaseEntity::Create` 0x17950, `UTIL_Remove` 0x119900, `CWorld::Precache` 0x120EB0, gpGlobals 0x223188.
- CBaseEntity vtable starts with the Spirit virtuals (DesiredAction, SetNextThink, AbsoluteNextThink, ThinkCorrection, CalcPosition, CalcVelocity, CalcRatio, IsAlias) plus a Paranoia GasWarning before Spawn. Derived class sizes: CBaseEntity 0xDC, CBaseDelay 0xEC, CBaseAnimating 0x100, CBaseToggle 0x180, CBaseMonster 0x368, CBasePlayerItem 0x16C.
- entvars_t: `int __padding` after `light_level`; edict_t: trailing pad (stride 0x32C).
- Engine headers are pre-2013 Valve SDK (no `pfnFilteredClientCmd`).

File to DLL map: none (no game code).

## G. refs/CryOfFear-FirstPerson (client and server proxy wrappers, C++, Python tools)

What it provides: the full 846-name hl.dll export list (`src/server/hl_exports.txt`, identical to retail), an entvars_t check script (`test/entvars_layout.py`, reads the save table, same method as `docs/structs.md`), user message names (Flashlight, FlashFlags, DualFlash, FlashBat), client commands (+attack3, +dodge, quicksel, weapontoggle), and SHA-256 pins of retail hw.dll, opengl32.dll and client.dll (all equal to 1.6a). Names the extra entvars field `int cof_extra` (its own name, no evidence) and asserts `offsetof(edict_t, v) == 0x80`.

File to DLL map: none (wrappers only).

## H. refs/Spirit (Spirit of Half-Life 1.7, cloned 2026-10-10)

Unofficial 2020 re-upload by sohl-modders, checked out at `d50205b` ("Add SOHL 1.6"). The only later commit (`6f696bc`, 2020-07-07) adds `slTeleportToScript` to `CBaseMonster::m_scheduleList` in `defaultai.cpp`, a modern fix, so it is deliberately not checked out. In this mirror the 1.6 and 1.7 source trees are byte-identical; 1.7 differs only in data files.

Evidence for 1.7 (details in `docs/sessions.md`, 2026-10-10): the prototype devkit ships the Spirit 1.7 fgd unmodified, and the hl.dll entity classnames match Spirit 1.6/1.7 exactly (no 1.5-only items, no 1.8 or 1.9 additions). CoF has Spirit's FMOD entities (`ambient_fmodstream`, `trigger_mp3audio`) that Paranoia lacks, so Spirit code entered CoF directly, besides through Paranoia.

Layout: `SpiritSource/{cl_dll,dlls,common,engine,pm_shared,game_shared,dedicated,network,utils}`.

| DLL | Files |
|---|---|
| hl.dll | `SpiritSource/dlls/*` (Spirit additions: `alias.cpp`, `locus.cpp`, `movewith.cpp`, `effects.cpp` env_* entities, `triggers.cpp` trigger_*/calc_*/motion_*, FMOD ambient code) |
| client.dll | `SpiritSource/cl_dll/*` (particle system `aurora`, rain, Cg shader water, sky, `hud_spectator`) |

## I. refs/ReHLDS (reverse engineered GoldSrc dedicated engine)

Added 2026-10-11 when hw.dll entered the scope. A from-the-binary rewrite of Valve's dedicated server engine (swds.dll / engine_i486.so), MIT licensed, with Valve's function and structure names (taken from the symbols of the Linux engine). `rehlds/engine/` covers the server half of hw.dll: entity management, physics (`sv_phys`, `sv_move`), save/restore (`host_*`), networking (`net_*`, `sv_main`), console and cvars (`cmd`, `cvar`), model loading (`model.cpp`), `pr_cmds.cpp` (the enginefuncs_t implementation). No client side (rendering, sound, input, VGUI). Consultation only: names, struct layouts and logic are verified in hw.dll before use; no code is copied.

## J. refs/xash3d-fwgs (GoldSrc-compatible engine)

Added 2026-10-11. Independent clean-room GoldSrc engine (GPL), client and server: `engine/client` (rendering glue, sound, input, demo, cl_parse with the user-message protocol), `engine/server`, `engine/common` (filesystem, console, model loading), `ref/gl` (OpenGL renderer). Useful for the protocol, the client-side engine interfaces (cl_enginefunc_t, IEngineStudio, triangle API) and data formats. Its internals differ from Valve's; consultation only. cof-enhanced patches exactly this engine to run the retail CoF DLLs.

## K. Linux Half-Life binaries (symbol source for hw.dll, GameUI.dll)

Not a git repo. Downloaded 2026-10-11 with steamcmd from the user's own Half-Life license: app 70, branch `steam_legacy` (last pre-25th-anniversary build), platform linux, into `I:\Tools\_downloads\hl_linux_legacy` (outside the repo, never committed). Built with GCC 4.6.1, not stripped: `hw.so` 6,352 named functions, `valve/cl_dlls/gameui.so`, `hl_linux`. Imported in the Ghidra project under `/ref_linux`.

Use: carry Valve's function and global names to the CoF 2013 Windows builds (`tools/match_versions.py hw linux 1.6a --data-propagation --src-image .../hw.so --dst-image versions/1.6a/hw.dll`). The Linux build is about six years newer (SDL2, no DirectDraw/DirectInput), so only functions that kept their strings, globals and call structure match. Names only; never code.

## L. irrKlang 1.6.0 and id3lib 3.8.3 headers (client.dll mp3.cpp)

Not git repos; headers and import libraries only, in `refs/irrklang-1.6.0` and `refs/id3lib-3.8.3` (git-ignored), on the client include path. Third-party libraries, out of scope (the game ships their DLLs); needed to compile `cl_dll/mp3.cpp`, which calls irrKlang through its interfaces and id3lib through its classes.

- irrKlang: client.dll passes the SDK version string "1.6.0" to `createIrrKlangDevice`, so it was compiled with the 1.6.0 headers (the game itself ships the July 2010 `irrKlang.dll`). Ambiera now offers downloads to existing licensees only and its official URL `https://www.ambiera.at/downloads/irrKlang-32bit-1.6.0.zip` returns 404; the file was taken from the Internet Archive snapshot of that official URL (2026-10-15). Its own DLLs are the February 2018 1.6.0 build. Kept: `include/*.h`, `lib/Win32-visualStudio/irrKlang.lib`, `readme.txt`. Free for non-commercial use (see readme).
- id3lib 3.8.3: the version string of the shipped `id3lib.dll`. Source tarball from the project's SourceForge page (`https://sourceforge.net/projects/id3lib/files/id3lib/3.8.3/`), LGPL. Kept: `include/id3.h`, `include/id3/*.h`, COPYING, README.

Debug information: `hw.so` and `gameui.so` of the steam_legacy build carry full DWARF (`.debug_info`, `.debug_line`, `.debug_aranges`), not only symbols. `python tools/elf_files.py <lib.so> <out.csv>` lists every function with its source file and declaration line from the compile units: hw.so 5656 of 5799 functions in 168 files (`../engine/*.c`, `../engine/vgui2/*`, `../vgui2/controls/*.cpp`, `../public/KeyValues.cpp`, ...), gameui.so 3369 of 3422 in 126 files (`controls`, `GameUI`, `common`, `public`). Outputs: `build/match/hw_linux_files.csv`, `build/match/gameui_linux_files.csv`. The DWARF also holds struct layouts, parameter and local variable names: names come from there, layouts must still be checked in the 2013 Windows binary (the Linux build is about six years newer).

## M. MP3FileInfo (CodeProject, compiled into client.dll)

client.dll contains the class "MP3FileInfo 1.0" by Christian Richardt (CodeProject, 2003, "MP3FileInfo - Extract Header and ID3 Tags of an MP3 File"; default CodeProject licence CPOL, not checked), built on id3lib, with Cry of Fear changes (printf replaced by gEngfuncs.Con_Printf with a "MP3 INFO" prefix, GetTitle_guess added). src/cl_dll/MP3FileInfo.cpp is rebuilt from the binary (11 of 11 functions identical); copies of the original class were consulted to name members, not copied. Not in refs/.

## refs/Quake (id-Software/Quake, GPL-2.0)

id Software's Quake source (WinQuake, QW). GoldSrc's engine descends from it, and its Windows-only files have no counterpart in the Linux hw.so DWARF: in_win.c, snd_win.c, vid_win.c, gl_vidnt.c, sys_win.c, cd_win.c, net_wins.c and conproc.c give the function names, order and structure of the matching hw.dll code (docs/hw_regions.md). Consultation only, like the other engine references: write hw.dll from its assembly and verify every name in the binary.

## refs/a3d-sdk (sigmaco/a3d-sdk, no licence stated)

The Aureal A3D SDK 1.2, 2.0 and 3.0: headers (Ia3d.h, ia3dapi.h, Ia3dutil.h), the ia3dutil helper library source and prebuilt libraries, samples. hw.dll's A3D renderer (CA3DRenderer and the CA3DSource classes) is Valve code written against these COM interfaces; the helper library itself is not in hw.dll. Consultation only: the interface declarations give the vtable slots and GUIDs the engine calls.
