# Struct and class layouts that differ from the HLSDK

Only layouts confirmed in the binary go here. Each entry names the binary, the evidence and the method.

## entvars_t (hl.dll 1.6a): 4 bytes inserted after light_level

Confirmed 2026-10-10 from the save/restore table `gEntvarsDescription` (102 TYPEDESCRIPTION records of 16 bytes: fieldType, fieldName, fieldOffset, fieldSize, flags).

| Field | HLSDK / CoF 1.55 offset | CoF 1.6a offset |
|---|---|---|
| gravity | 0x11C | 0x11C |
| light_level | 0x124 | 0x124 |
| *(unnamed, 4 bytes, not saved)* | - | 0x128 |
| sequence | 0x128 | 0x12C |
| frame | 0x130 | 0x134 |
| health | 0x160 | 0x164 |
| flags | 0x1A4 | 0x1A8 |
| iuser1 | 0x244 | 0x248 |
| euser4 (last saved field) | 0x2A0 | 0x2A4 |
| *(unnamed, 4 bytes, not saved)* | - | 0x2A8 |
| sizeof(entvars_t) | 0x2A4 | 0x2AC |
| sizeof(edict_t) | 0x324 | 0x32C |

- 1.55 table: VA 0x10216B40 (PDB symbol `gEntvarsDescription`), offsets identical to the stock HLSDK. The 2012 builds (and so the PDB type information) use the stock layout.
- 1.6a table: VA 0x1021AAE0 (file offset 0x219AE0). Every field from `sequence` on is shifted by +4.
- The new field is absent from the save table, so its name and type are unknown. `src/engine/progdefs.h` names it `guess_cof_light_extra`. The hw.dll delta field tables (name, offset pairs at 0x01EB2630) do not cover entvars_t and the 1.6a hl.dll never reads the field.
- Second extra field, confirmed 2026-10-11: hw.dll clears an edict's fields with `memset( &ed->v, 0, 0x2AC )` (`push 0x2ac; push 0; lea edx, [ed+0x80]` at 0x01DD7C81, also at 0x01DA7A23, 0x01DA7E42, 0x01DE0CF4), so `sizeof(entvars_t)` is 0x2AC. Every saved field up to `euser4` (0x2A4) is accounted for, so the 4 bytes follow `euser4`; `src/engine/progdefs.h` names them `guess_cof_end_extra`.
- Consequence: `edict_t` is 0x32C bytes in 1.6a (as cof-enhanced reports). hl.dll walks the edict array with that stride (`add edi, 0x32c` in `CHalfLifeMultiplay::Think` and `InitHUD`), which matches once both fields are declared.

## ScreenFade (engine/shake.h, hl.dll 1.6a): floats instead of 4.12 fixed shorts

`float duration; float holdTime; int fadeFlags; byte r, g, b, a;` (16 bytes; 1.55 and the HLSDK have `unsigned short duration, holdTime; short fadeFlags;`, 10 bytes). Confirmed by UTIL_ScreenFadeBuild (cvtdq2ps stores at +0 and +4, int at +8, bytes at +0xC..+0xF) and UTIL_ScreenFadeWrite (cvttss2si before WRITE_SHORT). The values are still FixedUnsigned16 results. LinkUserMessages registers the message with size 10 (the HLSDK size), not sizeof(ScreenFade).
## CHud (client.dll 1.6a): 0x5590 bytes, Cry of Fear block before m_hsprCursor

gHUD is at 0x1018A0C0. Cry of Fear inserted 0x30CC bytes after the Paranoia private members (custom sleeve, finger and glove texture names at 0x1C/0x11C/0x21C, the savehack command, damage time 0x328, notes 16 x 0x1A0 at 0xF1C, iron sight vectors, NewFade, xboxfix flags, FPS counters, inventory item names at 0x2C64), so m_hsprCursor is at 0x30E8 and m_flTime at 0x30F0 (Paranoia: 0x20 and 0x24). More Cry of Fear members follow the Paranoia ones (camera zoom, view lock byte 0x31BC, double-tap deadlines 0x31C4-0x31D0, objective strings 0x322C/0x332C). HUD elements: m_Ammo 0x342C, m_Health 0x3574, m_Spectator 0x39C8, CBlackBar 0x4C90, CNeffect 0x4CB4, CHudLensflare 0x51C4, m_scrinfo 0x536C. Full list in src/cl_dll/hud.h with the evidence of each member.

Regions that no code in 1.6a, 1.55 or 1.0 reads are declared as `guess_m_...Unused...` arrays (0x364 char[3000], 0x2980 int[64], 0x2AD0, 0x30E4, 0x31D4 int[14], two in CHudLensflare): their types cannot be known, and the arrays keep every referenced member at its retail offset. CHudHealth has four [65] arrays (coop health, AdLib time and state, syringe) before m_hSprite at 0x384. CVoiceStatus has two bool[65] arrays at 0x18 and 0x59 (m_bInSquelchMode at 0x17860).
## Engine interface tables (1.6a)

Confirmed 2026-10-10 from the copy loops in the exported entry points (`rep movsd` counts) and the member lists of the headers in `src/`.

| Table | Size in 1.6a | Members | Matches | Evidence |
|---|---|---|---|---|
| `enginefuncs_t` (hl.dll `g_engfuncs` 0x10222F48) | 0x240 | 144 | Paranoia / pre-Steam HLSDK; the Steam-era HLSDK has 159 | `GiveFnptrsToDll` copies 0x90 dwords |
| `DLL_FUNCTIONS` (hl.dll `gFunctionTable` 0x102119F8) | 0xC8 | 50 | HLSDK, `INTERFACE_VERSION` 140 | `GetEntityAPI` copies 0x32 dwords |
| `NEW_DLL_FUNCTIONS` | not exported | - | - | no `GetNewDLLFunctions` export |
| `cl_enginefunc_t` (client.dll `gEngfuncs` 0x101B2608) | 0x194 | 101 | Paranoia, `CLDLL_INTERFACE_VERSION` 7 | `Initialize` copies 0x65 dwords |

## triangleapi_t (client.dll 1.6a): two Steam-era members

client.dll calls `gEngfuncs.pTriAPI` slots 18 and 19 (offsets 0x48 and 0x4C). Paranoia's `common/triangleapi.h` (copied into `src/`) ends before them; the Steam-era HLSDK names them `Color4fRendermode` and `FogParams`. `src/common/triangleapi.h` needs both members appended once client code is reconstructed. Evidence: `docs/calls/client_1.6a_engfuncs.csv`.

## CBaseEntity (hl.dll 1.6a): 4 bytes larger

Confirmed 2026-10-10 (Phase 1). Evidence:

1. **Allocation sizes** of all 468 entity classes present in both builds (`docs/class_sizes.csv`, `tools/class_sizes.py`): 462 classes are exactly 4 bytes larger in 1.6a. The size is the constant passed to `pfnPvAllocEntPrivateData` by `GetClassPtr<T>` (inlined in each `LINK_ENTITY_TO_CLASS` export in 1.6a; pushed for `CBaseEntity::operator new` in the 1.55 debug build).
2. **Save tables**: every field of `CBaseEntity::m_SaveData` (offsets 0x08 to 0x88, from `b_Saving` to `m_bFadeOut`) has the same offset in both builds, while every field of `CBaseDelay`, `CBaseAnimating`, `CBaseToggle`, `CBaseMonster`, `CBasePlayerItem` and `CBasePlayerWeapon` is shifted by +4 (`CBaseDelay::m_flDelay` 0xD8 -> 0xDC).
3. `CBasePlayer::TabulateAmmo` (1.6a 0x100F5EB0) still writes `ammo_9mm` ... `ammo_groza` at 0x8C ... 0xC0, the 1.55 offsets.

So `sizeof(CBaseEntity)` is 0xD8 in 1.55 (PDB) and 0xDC in 1.6a, and the new 4 bytes lie between 0xC4 and the end of the class: before or among `m_flStartThrow` (0xC4), `m_flReleaseThrow` (0xC8), `m_chargeReady` (0xCC), `m_fInAttack` (0xD0), `m_fireState` (0xD4), or at the end. The exact position cannot be observed: no 1.6a function and no save table reads `m_flStartThrow` .. `m_fireState` (checked by comparing the member displacements of every function present in both builds, 2026-10-11), so any position after `ammo_groza` produces the same code. `src/dlls/cbase.h` appends it as `int guess_m_iNew16a;`. MadSimonX also puts it at the end (`__PADDINGD4`, unverified).

1.55 layout (PDB, Ghidra `get_struct_layout`): vtable 0x00, `pev` 0x04, `b_Saving` 0x08, `m_bFrozenByCamera` 0x09, `m_bIsBoss` 0x0A (CoF), Spirit MoveWith / Assist / think fields 0x0C-0x77, `m_pfnThink`..`m_pfnBlocked` 0x78-0x87, `m_bFadeOut` 0x88 (CoF), `ammo_9mm`..`ammo_hornets`, `ammo_argrens` (HLSDK) and `ammo_aps`, `ammo_barret`, `ammo_ak`, `ammo_asval`, `ammo_rpk`, `ammo_groza` (Paranoia) 0x8C-0xC3, then the HLSDK weapon state fields 0xC4-0xD7.

## Classes that grew more than CBaseEntity (1.55 -> 1.6a)

| Class (classname) | 1.55 | 1.6a | Delta | Own new data |
|---|---|---|---|---|
| CBasePlayer (`player`) | 0x26C0 | 0x281C | +348 | 344 bytes |
| CPhoneCall (`cof_phonecall`) | 0x1F8 | 0x204 | +12 | 8 bytes |
| CAnimatedCameraModel (`camera_model`) | 0x3EC | 0x3F4 | +8 | 4 bytes |
| CCoFMDLCutscene (`cof_mdlcutscene`) | 0x3EC | 0x3F4 | +8 | 4 bytes |
| CCutsceneModel (`cutscene_model`) | 0x188 | 0x190 | +8 | 4 bytes |
| CCoFLadderManager (`cof_ladder_manager`) | 0x1AC | 0x1B4 | +8 | 4 bytes |

New classes in 1.6a: `CMp5` (`weapon_mp5`, 0x1FC), `CMP5Ammo` (`ammo_mp5`, 0xE0), `CBowlingBall` (`bowling_ball`, 0x100), `CMP5ModeSaveRestoreMP5` (`cof_mp5modesaverestore`, 0xDC). MadSimonX documents three "1.6a" padding blocks in CBasePlayer totalling 316 bytes; to be checked against the 344 measured here.

## Source of class layouts for Phase 2

- 1.55 PDB (Ghidra program `/1.55/hl.dll`): full layouts of every 2012 class, member names included. Apply the 1.6a deltas above on top.
- 1.6a vtables (RTTI): 482 game classes keep the 1.55 vtable length, so no virtual functions were added or removed in those classes.

## usercmd_t (CoF engine): is_donator after impact_position

Confirmed 2026-10-11. The hw.dll delta field table for `usercmd_t` (name, offset pairs at 0x01EB2670) ends with `is_donator` at 0x34, and `cryoffear/delta.lst` has `DEFINE_DELTA( is_donator, DT_INTEGER, 3, 1.0 )`. So `sizeof(usercmd_t)` is 0x38 instead of 0x34. This is the 4 bytes that `playermove_t` has between `cmd` and `numtouch` (`PM_AddGravity` reads `pmove->movevars` at 0x4F4F4 instead of 0x4F4F0); with `int is_donator;` in `src/common/usercmd.h` the playermove_t placeholder is gone.

## clientdata_t (CoF engine): isdonator at the end

The hw.dll clientdata delta table (0x01EB26F8) ends with `isdonator` at 0x1DC, after `vuser4`. `src/common/entity_state.h` has `int isdonator;` there; the type is a guess (the field has no delta.lst entry).

## weapon_data_t (CoF): m_flNextTertiaryAttack

1.55 PDB: `weapon_data_s` is 0x5C bytes with `float m_flNextTertiaryAttack` at 0x10, between `m_flNextSecondaryAttack` and `m_flTimeWeaponIdle`. Added to `src/common/weaponinfo.h`.

## CBasePlayer (hl.dll 1.6a): changes from the save table

Compared `CBasePlayer::m_SaveData` of 1.55 (0x10213D70, 206 fields) and 1.6a (0x10217FE8, 207 fields), 2026-10-11:

- new saved field `m_bUsedVanNumber` (BOOLEAN) at 0x399;
- 12 more bytes between `m_pWheelchair` (0x390 in 1.6a) and `m_bJustJumped` (0x39A -> 0x3AA), which includes `m_bUsedVanNumber`; the order of `m_iCoopDeadYaw`, `m_bKilledLyingDown` and `m_bCustomSleeveApplied` around it is still open;
- arrays grew by one element: `m_strHintsShown` 32 -> 33, `transferredentity_message`, `_origin`, `_classname`, `_clip` 50 -> 51, `inventorySlot`, `inventorySlotModels`, `inventorySlotClassname` 6 -> 7.

## Notes on the 1.55 PDB as a layout source

- The 1.55 hl.dll is a debug build (objects in `.\debughl\`, frame pointers, `this` spilled to `[ebp-4]`), so its code is not comparable byte for byte, but its type stream and locals are complete: `tools/pdbtypes.py` (classes, method order, virtual slots, locals) and `tools/pdb_lines.py` (file and line of every function, `docs/lines_hl_1.55.csv`).
- VC 7.1 `std::string` is 28 bytes, VS2019 is 24. No CoF class has a `std::string` data member (only `CBasePlayer::EncryptStatsFile` takes and returns one), so this does not move any member.
- Method lists in the type stream are stored in reverse declaration order; overloaded virtuals take their slots in reverse declaration order (MSVC), confirmed on the 1.6a vtable of CBaseEntity (`SetNextThink`, `GetState`, `FVisible`).

## Interface changes found while decompiling CoF_Glock.cpp (1.6a)

Confirmed in the 1.6a code, 2026-10-11:

- `CBaseEntity::GiveAmmo` (vtable +0x94) takes four arguments in 1.6a (`ret 0x10`): `( int iAmount, char *szName, int iMax, bool )`. The new bool suppresses the "Picked up ..." message in `CBasePlayer::GiveAmmo`. Name unknown: `guess_bNoMessage`.
- `PRECACHE_MODEL` and `PRECACHE_SOUND` are functions in util.cpp, each with three overloads `( int s, char *e )`, `( int s )`, `( char *s )` (1.55 PDB names and parameters), not the engine macros of the HLSDK.
- util.cpp has a new function at 0x10116A40 (no symbol) that plays the `weapons_new/` fire sounds: `( const char *sample, int, bool, Vector vecOrigin, CBaseEntity *pPlayer, bool )`, called with `m_pPlayer->EarPosition()` and `m_pPlayer`. It checks `g_pGameRules->IsMultiplayer()` and `strstr( sample, "weapons" )` and loops over the players. Name: `guess_UTIL_PlayWeaponSound`.
- `CBasePlayer` has a new out of line getter at 0x100F5DA0 returning `m_bJustJumped != false` as BOOL (the caller tests eax). Name: `guess_HasJustJumped`.
- `CBasePlayer::GetAutoaimVector` only returns `gpGlobals->v_forward`; `CBasePlayerWeapon::DefaultReload` takes five arguments `( int iClipSize, int iAnim, float fDelay, int body, bool infinitereload )` and `DefaultDeploy` six.
- Weapon constants of the glock: `WEAPON_GLOCK` 3, clip 15, weight 5, carry 150 (45 in multiplayer), bullet type 3 (`BULLET_PLAYER_357` in the Paranoia enum). The ironsight toggle is `IN_CANCEL` (0x40); the suicide button mash tests `IN_ATTACK | IN_ATTACK2`. Sprinting is entity flag `1<<18`.
- gSkillData is at 0x10226728 (`DmgGlock` at +0x128); the glock event is the 1.55 global `m_usThirdFlash` (0x10231F50).
- Convention: weapon animation enums are named after the sequence labels of the retail view model (`tools/mdlseq.py`), for example `GLOCK_IRON_TO` for sequence `iron_to` of `models/weapons/glock/v_glock.mdl`.
