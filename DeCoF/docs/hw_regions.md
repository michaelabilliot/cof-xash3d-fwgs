# hw.dll regions without a source file yet

Ranges of hw.dll code that tools/align_linux.py could not attribute (no Linux counterpart or no anchors), identified by the strings they use. Addresses are approximate run bounds from progress.csv rows with an empty source_file (2026-10-17).

## Engine code (Valve, in scope)

| Range | Size | What | Evidence |
|---|---|---|---|
| 01D07248-01D0EDA5 | 30 KB | attributed 2026-10-22: FontTextureCache.cpp (01D076B0), HtmlWindow.cpp (01D091A0, RTTI HtmlWindow, FrameSite, FS_*), MemoryBitmap.cpp (01D0E930), each with its COMDAT tail | "VGUI_HTML", "Internet Explorer_Server", "MemoryBitmap" |
| 01D534E9-01D5A601 | 27 KB | attributed 2026-10-22: DetailTexture.cpp COMDAT tail (std::map instantiations noted as LIB candidates), dinput_guess.c (01D59030, DirectInput mouse, file name unknown), download.cpp (01D59B70, DownloadCache and DownloadManager) | "-noforcemparms", "-noforcemspd", "DownloadCache", "%s/overflow" |
| 01D5A723-01D5C307 | 7 KB | attributed 2026-10-22: download.cpp, downloadthread.cpp (01D5B570, WinINet worker, Referer hl1://), eventapi.c (01D5BE60) | "#GameUI_VerifyingAndDownloading" |
| 01D88030-01D895CF | 5 KB | attributed 2026-10-22: COMDAT tail of ipratelimit.cpp (CUtlRBTree<iprate_s>), then the $E helpers of ipratelimitWrapper.cpp | |
| 01D8AC40-01D8CE32 | 9 KB | attributed 2026-10-22: LoadBlob.cpp (01D8AC40, the same five functions as cof.exe LoadBlob.cpp), mathlib.c | "BoxOnPlaneSide:  Bad signbits", "cstrike" |
| 01D96A50-01DA0822 | 40 KB | ObjectList.cpp COMDATs up to 01D96AD0 (attributed), then from 01D96AF0 to about 01D9FD70 the OpenGL-on-Direct3D wrapper (exports glAccum@8 and the other gl*/wgl* entry points, QGL_D3DInit, a COMDAT run 01D9F1E0-01D9F750); its file name is unknown (the link position lies between ObjectList.cpp and pmove.c) | "Wrapper: Lock on primary failed", "GL_SGIS_multitexture", "Software\\Microsoft\\Quake", "GameOverlayRenderer.dll" |
| 01DD0B23-01DD4A7B | 16 KB | snd_win.c (DirectSound, EAX) | "dsound.dll", "DirectSoundCreate", "DS:CreateSoundBuffer for EAX front left" |
| 01DF759A-01DF9267 | 6 KB | video mode and registry settings (sys_dll2, vid) | "EngineDLL", "EngineD3D", "ScreenWindowed", "ScreenBPP" |
| 01DFD212-01DFF007 | 7 KB | vgui2 engine surface | "EngineSurface007" |
| 01E0BD4E-01E0E52B | 9 KB | voice recording through waveIn and DirectSoundCapture | "VoiceRecord_WaveIn::Init", "DirectSoundCaptureCreate" |
| 01E14E50-01E18431 | 8 KB | Valve's A3D renderer (CA3DRenderer, CA3DSource2D, CA3DSource3D, A3D_SetGlobals, A3D_FeedBuffer) written against the Aureal A3D COM interfaces; the SDK's ia3dutil helper library is not linked in (neither the source compiled with VC6 nor the prebuilt ia3dutil.lib matches). Interfaces: refs/a3d-sdk (ia3dapi.h) | "CA3DRenderer::SetMainVolume - not yet implemented in API.", "Software\\Aureal\\A3D" |
| 01E2D83E-01E305A9, 01E322A0-01E33D43, 01E43E40-01E4580D and more | | vgui2 controls library (Panel, Button, RichText, ...), also in GameUI.dll | "vgui::", "DefaultButtonSet", "DoCutSelected" |

## Third-party code linked into hw.dll (no Valve source)

| Range | Size | What | Evidence |
|---|---|---|---|
| 01D10480-01D1E5D0, 01E1A0C0-01E1FBF5, 01E20060-01E27F5A | 110 KB | IGA / Radial in-game advertising. In 01D10480-01D1E5D0 (between vgui_intwrap2.cpp and BaseSystemModule.cpp; 01D0EE19-01D1047F is text_draw.cpp and vgui_intwrap2.cpp, now units): the Valve engine glue 01D10480-01D14D40 (C exports AdsStartup_guess and friends from 01D10D8E, RTTI CInGameAdvertisingSystem, ENGINEINGAMEADVERTISINGVERSION001, iga_shoot, metadata parser; $E helpers at 01D10D40, so 01D10480-01D10D3F may be a separate helper object) with a 313-function COMDAT tail to 01D1A160; a CUtlBuffer reader 01D1A160-01D1B8E0 (COMDATs to 01D1C210); a vector quantizer 01D1C210-01D1DCF0 ("node=%x NSamples=%d", "sample #%d"); the octree CQuantizer 01D1DCF0-01D1E5D0 (RTTI CQuantizer). No file names are known | "RAD_IMPRESSION_SENT", "Software\\IGA", "radial.cdb", "FINAL 2.0.0.0" |
| 01E634F8-01E6FD98 | 47 KB | bzip2 1.0.2 (official source; built with the VC6 processor pack, docs/toolchains.md) | "1.0.2, 30-Dec-2001" |
| 01E83AC5-01E899EF | 22 KB | DirectX manager (ddraw enumeration, COM registration) | "__DXMgrUsageCount__", "DirectDrawEnumerateExA" |

The IGA/Radial SDK and the DirectX manager have no public source (marcinlos/iga-ads is an unrelated isogeometric analysis framework, microsoft/directxtk a modern DirectX 11 library): they will be linked from the retail bytes as binary objects. bzip2 is rebuilt from its official source.
