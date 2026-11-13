# hw.dll scope question: IGA glue, hla3d and the A3D renderer

Evidence gathered on 2026-10-25 for the open decision "IGA ad SDK and A3D renderer in scope or LIB" (docs/sessions.md, 2026-10-23). Nothing has been reclassified: every file below keeps its progress.csv rows (ENGINE, TODO) until the user decides. Sizes are the sums of the progress.csv rows; "/Od" means the code shape of the rest of the engine (every function opens with `push ebp; mov ebp, esp`, `this` spilled to `[ebp-4]`, every value reloaded from its stack slot), "optimized" means frame pointer omission, esp-relative locals and register allocation.

## Summary

| File (progress.csv) | Range | Rows | Bytes | Built as | Engine calls | Linux hw.so | Recommendation |
|---|---|---|---|---|---|---|---|
| iga/IGA_Engine_guess.cpp | 01D10480-01D1A160 | 385 | 37589 | /Od, engine link order | 129 (Con_Printf, Q_strncpy, Cmd_AddCommand, FS_*, SV_RecursiveHullCheck, Sys_Error) | absent | in scope (Valve glue) |
| iga/IGA_Metadata_guess.cpp | 01D1A160-01D1C210 | 22 | 8248 | /Od, engine link order | 32 (CUtlBuffer, FS_*, Q_mem*) | absent | in scope (Valve glue) |
| iga/IGA_Quantize_guess.cpp | 01D1C210-01D1DCF0 | 28 | 6865 | /Od, engine link order | CRT only (printf, operator new/delete, qsort, abs) | absent | in scope (Valve mathlib quantize.cpp) |
| iga/IGA_Quantizer_guess.cpp (not asked, same object group) | 01D1DCF0-01D1E5D0 | 15 | 2260 | /Od, engine link order | Q_memset | absent | in scope (public sample code compiled in the engine) |
| engine/hla3d_guess.cpp | 01D7A230-01D7D780 | 88 | 13263 | /Od, engine link order | 27 (Mem_Malloc, Mem_Free, Mod_LeafPVS, S_LoadSound, VectorInverse) plus 53 into the A3D renderer | absent (only the cvars `s_a3d`, `a3d` remain in SND_DMA.C) | in scope (engine glue) |
| a3d/A3DRenderer_guess.cpp | 01E14E40-01E19A14 | 137 | 11586 | optimized, separate library after the engine objects | none (OutputDebugStringA, COM, CRT) | absent | LIB (Aureal A3D code, link the retail bytes) |
| (already LIB) IGA/Radial SDK | 01E1A0C0-01E27F60 | 201 | 50250 | optimized, separate library | MD5Init/Update/Final only | absent | stays LIB |

The four iga files and hla3d are part of Valve's engine project: same compiler and flags as the engine, linked in the engine's own object order, built on engine services. They are matchable with the installed VC6 like the rest of the engine (about 68 KB of /Od code). The A3D renderer is a separately built library whose code carries Aureal's own strings; together with the Radial SDK it is the only third-party code in this set.

## iga/IGA_Engine_guess.cpp (Valve glue)

- Strings: Valve interface versions `ENGINEINGAMEADVERTISINGVERSION001` (exposed, RTTI `CInGameAdvertisingSystem` at 01EA9260) and `CLIENTDLLINGAMEADVERTISINGVERSION001` (looked up in client.dll, RTTI `IClientDLLInGameAdvertising` at 01EA9230); console commands `iga_shoot` and `iga_reload` with engine-style messages ("Usage:  iga_shoot index\n", "Must be in a level\n", "Hit %.2f %.2f %.2f\n"); a metadata reader in Valve's KeyValues style ("placeholders", "cellid", "pos", "normal", "size", "border", "bordercolor", "decals", "Bogus cellid[ %d ] in %s [entry %d]\n", "Error 5899:  Metadata file missing or damaged [%d]!!!\n"); a table of the Radial error codes (`RAD_SUCCESS` ... `RAD_ERROR_DISABLED`, "Error starting up RAD [%s]\n") that turns the SDK's results into text. No copyright, SDK version or source path string.
- Code shape: /Od in all 350 functions of 16 bytes or more (227 spill `this` to `[ebp-4]`), int3 padding, VC6 `_$E` initializers at 01D10D40 (an `EXPOSE_SINGLE_INTERFACE`-style static), C entry points from 01D10D8E for the rest of the engine (AdsStartup_guess and friends, not PE exports). The 312-function COMDAT tail from 01D14D40 is Valve tier1 code in its /Od form (CUtlVector constructor, `RemoveAll` loop, `Count`, CUtlBuffer); no STL.
- Calls: 129 calls into engine functions (Con_Printf 16, Q_strncpy 14, Q_memset 11, Q_memcpy 5, Cmd_Argv 4, Sys_Error 4, Q_stricmp 3, FS_Open/FS_Read/FS_Size/FS_Close, Cmd_AddCommand 2, SV_RecursiveHullCheck 2 for iga_shoot), the engine's placement `operator new`, plus the Radial SDK in the LIB range.
- Placement: between vgui2/vgui_intwrap2.cpp and common/BaseSystemModule.cpp in the engine's object order; its data (01EA8C1C-01EA9350) sits among the engine's early .data, not with the libraries.
- Linux build: no compile unit, no IGA or RAD string, no `iga_` command (Valve removed the ads before the 2019 build).

## iga/IGA_Metadata_guess.cpp (Valve glue, an image loader)

- Despite the name, the object reads 8-bit and 24-bit BMP files through Valve's CUtlBuffer (`BITMAPFILEHEADER` 14 bytes, `BITMAPINFOHEADER` with biSize 0x28, biPlanes 1, biCompression 0, biBitCount 8) for the ad textures. Strings: only "rb" and "L".
- /Od in all 22 functions; calls CUtlBuffer::Get/Put and its constructor (public/UtlBuffer.cpp, already a unit), FS_Open/FS_Read/FS_Size/FS_Close, Q_memset, Q_memcpy; 27 calls into IGA_Engine_guess and 8 into the two quantizers. Absent from the Linux build. Better stem when it is written: something image related (for example IGA_Image_guess), chosen to sort into the same link slot.

## iga/IGA_Quantize_guess.cpp (Valve mathlib quantize.cpp)

- Strings: the debug output of Valve's tree vector quantizer, "sample #%d, count=%d, values=\n { ", "%02x,", "node=%x NSamples=%d value=%d Mean={", "Errors={", "%f,", "Mins={", "} Maxs={", "error1\n", "error2\n". These are the printf formats of `PrintSamples` and `DumpQuantization` in Valve's mathlib/quantize.cpp (Source SDK, QuantizedValue tree with NSamples, Mean, ErrorMeasure, Mins, Maxs); the file is Valve's own and public in the Source SDK, so it is reference source for this object.
- /Od in all 27 functions of 16 bytes or more, engine link order, CRT calls only (printf 19, operator new and delete, qsort, memcmp, memset, memcpy, abs); called 8 times from the image loader. Absent from the Linux build.

## iga/IGA_Quantizer_guess.cpp (octree quantizer, not in the question but in the same group)

- RTTI `CQuantizer` (01EA9350). The constructor stores the vtable, `m_pTree = NULL`, `m_nLeafCount = 0`, clears `m_pReducibleNodes[0..nColorBits]` in a loop and stores `m_nMaxColors` and `m_nColorBits` (+0x30, +0x34): Jeff Prosise's octree color quantizer from Microsoft Systems Journal (1997 "Wicked Code" sample: AddColor, CreateNode, ReduceTree, DeleteTree, GetPaletteColors, GetColorCount, GetColorTable), 15 functions.
- /Od like the engine, compiled from source inside the engine project, so it is matchable the same way. Third-party sample code by origin, but not a prebuilt library.

## engine/hla3d_guess.cpp (engine glue for A3D)

- No string literals. RTTI `A3dWorld` and `A3dLeaf` (01EBC888, 01EBC8A0, in the engine's .data): the BSP world and leaves fed to the A3D 2.0 geometry engine (wavetracing: polygons, reflections, occlusions, the `_hwSet*_guess` tunables, PVS use).
- /Od in all 76 functions of 16 bytes or more; linked in the engine's alphabetical order between glide_guess.c and HUD.c.
- Calls: Mem_Malloc 5, Mem_Free 8, VectorInverse 3, VectorScale, Mod_LeafPVS, S_LoadSound, Q_memset, and 53 calls into the A3D renderer's C entry points; it reads engine structures (models, leaves, sounds). It cannot exist outside the engine.
- Linux build: absent; SND_DMA.C there still registers the cvars `s_a3d` and `a3d`, which were the switches for this code.
- The `A3d` spelling follows Aureal's naming (IA3d, CA3dRenderer in the A3D 1.2 samples), so Aureal engineers may have written it for Valve, but it lives in the engine source tree and is built with it. Interfaces are in refs/a3d-sdk (a3d-sdk-v2.0/sdk/inc/ia3dapi.h).

## a3d/A3DRenderer_guess.cpp (Aureal A3D code, recommend LIB)

- Code shape: optimized in 85 of 127 functions (esp-relative locals, no frame pointer), C++ exception unwind funclets, int3 padding; a different build from the engine. It is linked after all engine objects and the Quake .s files, at the start of the library block (A3D renderer 01E14E40, Radial SDK 01E1A0C0, vgui2 controls, vgui_surfacelib, bzip2, CRT), and its data (01ED299C-01ED2E24, RTTI `CA3DSource`, `CA3DSource2D`, `CA3DSource3D`) sits with the libraries' data, not the engine's: a prebuilt static library.
- Self-contained: no call into engine code (the few byte matches of `E8` into engine addresses are data inside instructions); diagnostics go to `OutputDebugStringA` ("Shutting down A3D\n", "CA3DRenderer::SetMainVolume - not yet implemented in API.", "A3D_SetGlobals - not yet implemented for Doppler or Rolloff.\n", " - Currupted sound source!\n"), not to Con_Printf.
- Aureal's own code is inside: 01E15E20 and 01E15F20 read and write the `SplashAudio` and `SplashScreen` values under `HKLM\Software\Aureal\A3D`, and 01E19710-01E19A20 checks the version resource of A3D.DLL and creates `CLSID_A3dApi` {92FA2C24-253C-11D2-90FB-006008A1F441} with `IID_IA3d3` through CoCreateInstance. Its string block ("Comments", "SpecialBuild", "PrivateBuild", "InternalName", "LegalTrademarks", "LegalCopyright", "CompanyName", "FileVersion", "FileDescription", "OriginalFilename", "ProductVersion", "ProductName", "SM Emulation", "Aureal Semiconductor Inc.", "Aureal Semiconductor", "\\StringFileInfo\\", "04B0", "%04X", "\\A3D.DLL", "A3D.DLL") is the same block in the same order as in Aureal's a3dapi1.dll of the A3D 3.0 SDK (refs/a3d-sdk/a3d-sdk-v3.0/sdk/bin), and the A3D 2.0 a3dapi.dll has it next to Aureal's source path `C:\Anzio 2.0\Src\A3d\apidll\a3dclsfc.cpp`; the splash values also appear in Aureal's a3d.dll (SDK 1.2) and a3dapi1.dll (SDK 3.0).
- What was tried before (docs/hw_regions.md): the SDK's ia3dutil (source compiled with VC6 and the prebuilt ia3dutil.lib) does not match, so the exact library is unknown. The compiler is unknown too: the Rich header has 18 `Utc12_CPP.8168` objects (VC6 RTM Professional C++) and 61 objects with no Rich id (pre-VC6 tools), either of which can hold it.
- Recommendation: LIB, linked from the retail bytes as a binary object like the Radial SDK and the DirectX manager. Its C entry points (`_hwSetListenerPosition_guess` and the others) stay named so hla3d can be written against them. If the user wants it in scope anyway, it needs its own compiler identification first (optimized C++, possibly not VC6).

## The Radial SDK (already LIB, for comparison)

01E1A0C0-01E27F60 is IGA Worldwide's Radial client: version string "FINAL 2.0.0.0", ad server "72.3.184.153", `Software\IGA`, cache files radial.cdb and radial.idb, its own HTTP POST client and wait-thread messages ("radStop(): res == WAIT_TIMEOUT\n"). Optimized code with nop padding, no engine calls except MD5Init, MD5Update and MD5Final, which the linker resolved to the engine's crc.c (the SDK expects the RSA reference MD5 API). It stays LIB; the iga glue above calls into it the way the engine calls into any import.

## What a decision changes

- In scope for the five /Od files: register them as units like the other engine files (rows stay ENGINE), rename the stems if better evidence turns up (IGA_Metadata_guess is an image loader), and write them with the engine flags. Reference source exists for IGA_Quantize (Valve mathlib quantize.cpp) and IGA_Quantizer (the MSJ CQuantizer sample).
- LIB for the A3D renderer: reclassify its 137 rows to LIB and link the range from the retail bytes.
