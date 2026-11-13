# Interfaces of the 1.6a game DLLs

How hl.dll and client.dll talk to the engine, to each other and to third party libraries. Everything here was extracted from the retail 1.6a binaries by `tools/scan_calls.py` (linear-sweep disassembly, then attribution of every call made through the engine tables) and by direct disassembly of the exported entry points. Full lists are in `docs/calls/`:

| File | Content |
|---|---|
| `<dll>_1.6a_exports.csv` | every export with its VA |
| `<dll>_1.6a_imports.csv` | every import (DLL, function, IAT slot) |
| `<dll>_1.6a_engfuncs.csv` | every engine table member with its number of call sites |
| `<dll>_1.6a_usermsgs.csv` | user messages (server: registered name, size, gmsg global; client: hooked name and handler) |
| `<dll>_1.6a_cvars_registered.csv` | cvars the DLL creates (server: cvar_t contents; client: name, default, flags) |
| `<dll>_1.6a_cvars_used.csv` | cvars read or written by name |
| `<dll>_1.6a_commands.csv` | console commands the DLL adds (name, handler) |
| `<dll>_1.6a_engine_calls_args.csv` | constant arguments of selected engine calls (command strings, precaches, sounds, prints) |

Regenerate with:

```
python tools/scan_calls.py hl versions/1.6a/cryoffear/cl_dlls/hl.dll docs/calls
python tools/scan_calls.py client versions/1.6a/cryoffear/cl_dlls/client.dll docs/calls
```

Limits: call sites are counted per instruction, register-loaded function pointers are followed until the register is overwritten, and tail jumps through a table count as calls. Arguments are recovered only when they are pushed constants in the same basic block. Rows with `<reg>` are arguments the scanner could not resolve. Treat counts as lower bounds.

## Changes from 1.55 (2012) to 1.6a (2020)

Same scan run on the 1.55 DLLs (`docs/calls/*_1.55_*.csv`; the 1.55 hl.dll table address comes from its PDB, `g_engfuncs` at 0x10219428):

| Interface | Added in 1.6a | Removed in 1.6a |
|---|---|---|
| Server user messages | `BigPrick`, `PlaySFX`, `SaveHack` | one message whose name is an offensive slur (see `hl_1.55_usermsgs.csv`) |
| Client message hooks | `BigPrick`, `PlaySFX` | the same slur-named message, `AmmoInv` |
| Message sizes | `Damage` 12 to 13 bytes | |
| Server cvars | `cof_monsterhumanscale`, `cof_monsterhumanyawspeed`, `sv_coop_nosave` | |
| Client cvars | `cl_fovmultiplier`, `cl_vignette`, `gl_blurstepsx`, `gl_blurstepsy`, `gl_opacity` | |
| Client commands | `donatorcheck` | |
| Server exports | `CBowlingBall::BowlingUse`, `CMp5::SuicideShot`, `ammo_mp5`, `bowling_ball`, `cof_mp5modesaverestore`, `weapon_mp5` | `CMultiManager::ManagerReport`, `trip_beam` (both `_DEBUG` only) |
| Client imports | `steam_api.dll`: `SteamAPI_Init`, `SteamAPI_Shutdown`, `SteamFriends`, `SteamUser` | |
| Server imports | `steam_api.dll`: `SteamAPI_Init`, `SteamFriends`, `SteamUserStats` | |

The engine tables kept their size (144 and 101 members), so the 2020 rebuild did not move to the Steam-era engine interface.

## Server (hl.dll)

### Entry points

| Export | VA | What it does (disassembly) |
|---|---|---|
| `GiveFnptrsToDll` | 0x100B4AA0 | `__stdcall`; copies 0x90 dwords (0x240 bytes) to `g_engfuncs` at 0x10222F48, stores `gpGlobals` at 0x10223188 |
| `GetEntityAPI` | 0x100183D0 | requires `INTERFACE_VERSION` 140; copies 0x32 dwords (50 entries) from `gFunctionTable` at 0x102119F8 |
| `GetEntityAPI2` | 0x10018400 | same table; writes 140 back on a version mismatch |
| `DllMain` | 0x100B4AD0 | `return TRUE` |

There is no `GetNewDLLFunctions` export, so `NEW_DLL_FUNCTIONS` is not provided. 846 exports in total: 476 plain names (entity classnames from `LINK_ENTITY_TO_CLASS`, plus the entry points and `DelayedUse`) and 370 MSVC-decorated names (think, touch and use functions exported through `EXPORT`, so that save games can find them by name).

### Engine function table (`enginefuncs_t`)

- 144 members, 0x240 bytes: the **pre-Steam** layout. It matches `src/engine/eiface.h` (Paranoia) member for member. The Steam-era HLSDK has 159 members; hl.dll uses none of the extra 15.
- 105 of 144 members are called, 8,450 call sites in total. The most used: `pfnWriteByte` (1031), `pfnRandomLong` (721), `pfnAlertMessage` (638), `pfnMessageBegin` (599), `pfnMessageEnd` (544), `pfnCreateEntity` and `pfnPvAllocEntPrivateData` (452 each, one per `LINK_ENTITY_TO_CLASS`), `pfnCVarRegister` (451).
- Precache, trace and sound calls appear with very few direct sites: they go through small wrappers (HLSDK `UTIL_*` helpers and CoF code), which is where the decompilation will find them.

### User messages (server registers, client hooks)

158 messages are registered with `pfnRegUserMsg`; `docs/calls/hl_1.6a_usermsgs.csv` gives each name, size (-1 = variable) and the `gmsgXxx` global that receives the id.

- HLSDK: `SelAmmo`, `CurWeapon`, `Geiger`, `Flashlight`, `FlashBat`, `Health`, `Damage`, `Battery`, `Train`, `HudText`, `SayText`, `TextMsg`, `WeaponList`, `ResetHUD`, `InitHUD`, `DeathMsg`, `ScoreInfo`, ...
- Spirit and Paranoia: `SetFog`, `KeyedDLight`, `SetSky`, `HUDColor`, `AddShine`, `Particle`, `DelPart`, `TeamNames`, `StatusIcon`, `SpecTank`, `TextWindow`, `PlayMP3`, `RainData`, `GrassInfo`, `RadioIcon`, `TabPanel`, `customdecal`, `mydlight`, `skymark_sky`, `skymark_w`.
- CoF (about 110 names): `Logo`, `ProFont`, `Inventory`, `Maglite`, `BlackBars`, `Blur`, `UnreadSMS`, `Stamina`, `NoteDoc`, `Keypad`, `Phone`, `Puzzlebar`, `CoFLogo`, `Save`, `CoFWeapon`, `ViewKnock`, `ViewEntity`, `ViewLock`, `CanUse`, `Concuss`, `Ladder`, `FallOver`, `WpnInfo`, `ShowHint`, `Accuracy`, `YesNo`, `Statue`, `Computer`, `UserDoc`, `HeadShield`, `CameraZoom`, `Padlock`, `SaveLoad`, `GameMenu`, `BlackWhite`, `WeaponBody`, `Scope`, `Bomb`, `PhoneOff`, `Muzzleflash`, `DualWield`, `DualAnim`, `DualFlash`, `ModelAttach`, `BossBar`, `HealthCap`, `DualAmmo`, `ThirdPerson`, `Telephone`, `Telescope`, `QSlotClear`, `FlashFlags`, `Documents`, `Boat`, `Ripple`, `AddNote`, `AttachCam`, `BigPrick`, `NewFade`, `Chapter`, `SkipScene`, `KillWpnPart`, `WpnDeploy`, `IronSights`, `Ignite`, `Wheelchair`, `ControlHint`, `LensFlare`, `GreyFade`, `AmmoInv`, `ReadyRoom`, `RevolverCyl`, `Credits`, `QSlotUpdate`, `TakePhoto`, `Clothes`, `Objectives`, `Unlocks`, `Timer`, `TimerPlus`, `AdLib`, `Lobby`, `InventChck`, `CoopStats`, `MPModel`, `BeatCoop`, `HoldBreath`, `DevDonator`, `CoopCamAnim`, `Introdn`, `LookingAt`, `SimonMode`, `Syringe`, `CustomDoc`, `Materials`, `CustomSlv`, `CustomFng`, `CustomGlv`, `PlaySFX`, `SaveHack`.

Server and client do not agree on every name:

- Registered but not hooked by client.dll: `AmmoInv`, `Bomb`, `Documents`, `LensFlare`, `ReadyRoom`, `RevolverCyl`, `SaveHack`, `board`, plus `ScreenFade`, `ScreenShake`, `SelAmmo` (handled by the engine itself in GoldSrc).
- Hooked by the client but never registered: `AllowSpec`, `BuildSt`, `Detpack`, `Feign`, `GasMask`, `RandomPC`, `SecAmmoIcon`, `SecAmmoVal`, `Spectator`, `TVFrame`, `ValClass`, `ViewMode` (TFC and Paranoia leftovers), and `Board` / `Lensflare`, which differ from the server's `board` / `LensFlare` only in case. Whether the engine compares message names case-insensitively is not verified (hw.dll is out of scope); if it does not, those two messages never reach their client handlers. Kept as found; it is original behavior.

### Cvars

- 451 `cvar_t` registered: 394 skill cvars (`sk_*`), 17 `mp_*`, 14 `sv_*`, 11 `cof_*`, `sohl_impulsetarget` and `sohl_mwdebug` (Spirit), `voice_*`, `displaysoundlist`, `decalfrequency`, `bullet_*`, `blast_*`.
- About 75 cvars read by name, among them engine cvars (`sv_gravity`, `sv_cheats`, `host_framerate`, `hostname`, `fps_current`, `gl_bump`, `gl_dynlight`, `cg_water`, `r_weather`) and CoF cvars (`cof_subtitlelanguage`, `cof_wheelchair_*`, `cof_monsterhuman*`, `cof_*steptime`, `sv_coop*`, `sv_custom_hoodie`, `sv_custommaterials`, `sv_disablehints`, `sv_morphinetriptime`, `sv_skycolor_*`, `sv_skyvec_*`, `sv_votekick_time`, `sv_joebiden`, `ironsights_toggle`, `difficulty`, `dev_commentary`, `violence_*`).
- The server reads several client-side cvar names (`gl_bump`, `gl_dynlight`, `r_weather`, `cg_water`, `fps_current`), which works only on a listen server where both DLLs share the engine cvar list.

### Console commands and command strings

- The server adds no console commands (`pfnAddServerCommand` is never called). Client-to-server commands arrive through `ClientCommand` in `gFunctionTable` and are matched by string there.
- Strings the server executes with `pfnServerCommand`: `exec game.cfg`, `exec skill.cfg`, `load cofsave1` to `load cofsave5`, `map c_difficulty_settings`, `map c_video_stress_test`, `reload`.
- Strings it sends to the client with `pfnClientCommand`: `cd pause`, `donatorcheck`, `finishladder`, `map c_city`, `map c_forest3`, `map c_game_menu1`, `map c_intro`, `map c_start`, `map c_subwaysick1`.

### Imports

KERNEL32 (102, static CRT), USER32 (1), steam_api.dll (3): `SteamAPI_Init`, `SteamFriends`, `SteamUserStats` (achievements and stats, new in the Steam build).

## Client (client.dll)

### Entry points

| Export | VA | What it does (disassembly) |
|---|---|---|
| `Initialize` | 0x1001D5A0 | requires `CLDLL_INTERFACE_VERSION` 7; copies 0x65 dwords (0x194 bytes, 101 members) to `gEngfuncs` at 0x101B2608, then calls the event hook setup at 0x1004DFD0 |
| `HUD_Init` | 0x1001D4A0 | |
| `HUD_Frame` | 0x1001D440 | |
| `HUD_PlayerMove` / `HUD_PlayerMoveInit` | 0x1001D4C0 / 0x1001D4D0 | |
| `HUD_GetStudioModelInterface` | 0x10053820 | |
| `CreateInterface` | 0x100013C0 | |

41 plain exports (the standard HLSDK client interface: `HUD_*`, `IN_*`, `CAM_Think`, `CL_CreateMove`, `CL_CameraOffset`, `CL_IsThirdPerson`, `KB_Find`, `V_CalcRefdef`, `Demo_ReadBuffer`, `HUD_DrawNormalTriangles`, `HUD_DrawTransparentTriangles`, ...). There is no `F` export (the Steam-era single entry point), so the engine uses the classic per-function exports. The other 552 exports are decorated VGUI class members re-exported from vgui.dll headers, including compiler-generated move constructors and move assignments (`$$QAV01@`).

### Engine function table (`cl_enginefunc_t`)

- 101 members, 0x194 bytes, identical in size and order to `src/engine/cdll_int.h` (Paranoia).
- 73 of 101 members called directly, 3,166 call sites including the sub-APIs.
- Sub-APIs reached through pointers in `gEngfuncs`: `pTriAPI` (266 loads), `pEventAPI` (173), `pEfxAPI`, `pNetAPI`, `pDemoAPI`, `pVoiceTweak`. **The client calls `pTriAPI` members 18 and 19, which do not exist in Paranoia's pre-Steam `triangleapi.h`.** In the Steam-era HLSDK they are `Color4fRendermode` and `FogParams`. The 1.6a client was therefore built with a newer `triangleapi.h` than the rest of its Paranoia headers, and `src/common/triangleapi.h` must gain those two members.

### User messages hooked

162 names with handler addresses in `docs/calls/client_1.6a_usermsgs.csv`. See the server section for the comparison.

### Cvars

163 registered with `pfnRegisterVariable`. Groups:
- Paranoia renderer: `gl_glows`, `gl_glowstraces`, `gl_glowsdebug`, `gl_customdecals`, `gl_decalstatus`, `gl_posteffects`, `gl_grayscale`, `gl_renderer`, `gl_bump`, `gl_detailtex`, `bump_vecs`, `bump_debug`, `gl_specular`, `gl_dynlight`, `gl_dynlight_ent`, `gl_nocombs`, `gl_noshaders`, `gl_highspecular`, `gl_gamma`, `gl_brightness`, `gl_contrast`, `gl_twopassspecular`, `gl_twopassdyn`, `gl_blurtest`, `gl_modelrenderer`, `gl_lambert`, `gl_brushents`, `gl_clipplanes`, `gl_showplane`, `gl_screenblur`, `gl_blurstepsx`, `gl_blurstepsy`, `gl_opacity`, `gl_customsky`.
- CoF: `cof_reducedlight*`, `cof_fullbrighttextures`, `dev_commentary`, `brightvalue`, `skybright*`, `goro`, `cl_raininfo`, `fmod_volume`, `cg_water`, `cl_ripples`, `cl_subtitles`, `r_weather`, `fps_current`, `cl_noiseeffect`, `cl_noiseamount`, `cl_vignette`, `cl_fovmultiplier`, `cl_lazyweapon_force`, `cl_weapon_idle_*`, `xbox_controller_sensitivity*`, `cl_smokeparticlelimit`, `cl_cheapweaponsmoke`, `cl_bloodeffects`, `cl_cameraeffect`, `cl_crosshair`, `cl_fastcredits`, `cl_waterspeed_*`, `cl_coop_language`, `cl_lowflashlight`, `cl_propdistance`, `cl_downwiththesickness`, `cl_nodoubletapdodge`, `subt_*`, `subtitles`, `gpanel_hidetime`.
- HLSDK: `hud_*`, `cl_*` movement, `m_*`, `joy*`, `cam_*`, `c_*`, `spec_*`, `v_*`, `scr_ofs*`.

### Console commands

122 added with `pfnAddCommand` (names and handlers in `docs/calls/client_1.6a_commands.csv`): HLSDK input and camera commands, `slot1` to `slot10`, plus CoF and Paranoia commands such as `+sprint`, `+attack3`, `+dodge`, `+inventory`, `+objectives`, `documents`, `finishladder`, `donatorcheck`, `stopmp3`, `beginspgame`, `to3dmenu`, `returnmenu` (registered twice), `mp3player`, `unlockablescmd`, `texturetest`, `pastedecal`, `decalsclear`, `dumptextures`, `dumplevel`, `makelight`, `dumpleaf`, `skyangles`, `xboxfix`.

### Client to server command protocol

Strings sent with `pfnClientCmd` / `pfnServerCmd` and parsed by the server in `ClientCommand` (full list in `client_1.6a_engine_calls_args.csv`): `setclothes 0..11`, `closeclothes`, `padlockrotate 1..4`, `padlockcancel`, `boilercheck 1..5`, `cofload 1..5`, `cofsave <n>`, `dodgeforward`, `dodgebackward`, `dodgeleft`, `dodgeright`, `ironsightzoomin`, `ironsightzoomout`, `boatup`, `boatdown`, `boatleft`, `boatright`, `boatdisembark`, `attack1pressed`, `attack1released`, `attack2pressed`, `attack2released`, `forwardpressed`, `forwardreleased`, `backpressed`, `backreleased`, `dropammo 1`, `closedocument`, `stopcomputer`, `stresscontinue`, `readylobby`, `optionshack 0..2`, `skillset 1`, `subtitleset 1..2`, `dev_commentary 0/1`, `freeze 1024`, plus engine commands (`disconnect`, `kill`, `quit`, `snapshot`, `map ...`).

### Imports

| DLL | Count | Use |
|---|---|---|
| vgui.dll | 560 | VGUI 1 (HLSDK `utils/vgui`) |
| KERNEL32.dll | 106 | static CRT, file access |
| OPENGL32.dll | 36 | Paranoia renderer (loads the Paranoia `opengl32.dll` wrapper) |
| id3lib.dll | 12 | MP3 tag reading for the MP3 player |
| cgGL.dll / cg.dll | 9 / 5 | NVIDIA Cg shaders (Spirit 1.5+ water and shader code) |
| DevIL.dll | 5 | image loading |
| steam_api.dll | 4 | `SteamAPI_Init`, `SteamUser`, `SteamFriends`, `SteamAPI_Shutdown` |
| WINMM.dll | 3 | joystick / timing (HLSDK `inputw32.cpp`) |
| ADVAPI32.dll | 2 | registry |
| WSOCK32.dll | 1 | HLSDK server browser |
| irrKlang.dll | 1 | `createIrrKlangDevice` (SDK version string "1.6.0") |
