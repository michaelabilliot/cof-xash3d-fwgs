# Writing a Cry of Fear source file for hl.dll

How the first files (`src/dlls/CoF_Glock.cpp`, `src/dlls/CoF_P345.cpp`) were written and matched. Follow the conventions in README.md for everything else (naming, writing rules, no decompiler artifacts, guess_ prefix).

## Inputs for one unit

| What | Where |
|---|---|
| Functions of the unit (1.6a names) | `tools/units/hl_dlls_<file>.txt`, `docs/progress.csv` |
| Function order and line in the original file (1.55 debug build) | `docs/lines_hl_1.55.csv` (write the file in this order) |
| Ghidra pseudo C (reference only, never commit) | `build/decomp/hl/dlls/<file>/` (`python tools/build.py decomp hl/dlls/<file>`) |
| Exact instructions, strings, constants, member offsets of globals | `python tools/disasm.py hl "<decorated name>"` |
| The same function in the 1.55 debug build (statement lines from the PDB line table, block structure, dropped constants, dead code) | `python tools/disasm.py --version 1.55 hl "<decorated name>"` |
| Parameter and local names, block structure (1.55) | `python tools/pdbtypes.py versions/1.55/cl_dlls/hl.pdb func "CFoo::"` |
| Class declaration (members, methods, order) | `python tools/hdrgen.py CFoo` (weapon classes are already in `src/dlls/weapons.h`) |
| Member at an offset / vtable slot | `python tools/member.py --src src/dlls/items.cpp CBasePlayerWeapon 0x1e4`, `python tools/member.py --vt CBasePlayerWeapon 0x198` |
| Animation enum | `python tools/mdlseq.py versions/1.6a/cryoffear/models/weapons/<w>/v_<w>.mdl PREFIX` |

## Keep the output small

Every tool output stays in the working context, so large outputs repeated over many builds are the main cost of this work:

- `python tools/build.py check <unit>` prints only the functions below 100% (`--all` for every function).
- `python tools/objdiff_tool.py show <unit> "<name>"` prints only the differing rows with 3 rows of context (`--full` for every row).
- `python tools/disasm.py --version 1.55 --outline hl "<name>"` prints one row per source statement (calls, strings, constants, member offsets, jump targets as line numbers): read the 1.55 build this way and use the full listing only for the one statement you need. `--outline` also works on the 1.6a code. A byte count limits any listing: `python tools/disasm.py hl <va> 0x80`.
- Do not print whole files or whole decompiler dumps; read the part you need.

## Loop

1. Write the file (functions in the 1.55 line order, class `CFooAmmo` defined in the .cpp with in-class bodies like the HLSDK ammo classes).
2. `python tools/build.py check hl/dlls/<file>`: compiles the unit and prints the match of every function (one-unit objdiff report).
3. `python tools/objdiff_tool.py show hl/dlls/<file> "<decorated name>"`: retail on the left, ours on the right, differing rows marked `*`. Rows that differ only by a Ghidra name on the left (`s_...`, `DAT_...`, `FUN_...`) are naming, not code: they disappear after a harvest and a re-delink and need no source change.
4. For shape questions (if/else order, operand order, locals) write the alternatives into a variants file and run `python tools/try_variants.py hl/dlls/<file> "<decorated name>" <file> --keep`.
5. After 8 failed attempts on a function, stop: it stays TODO or becomes EQUIVALENT with the exact remaining difference in progress.csv.

## client.dll units

- Toolchain: the same compiler and flags as hl.dll, including `/Ob1` (51 client functions match only with it, none only without it).
- No build of client.dll has a PDB. Names come from the reference source (refs/Paranoia first, then refs/halflife and refs/Spirit), strings and exports; code shared with the server (pm_shared, common headers) can use the hl.dll 1.55 PDB. There is no line table: follow the reference file's function order and layout.
- `src/cl_dll/<file>.cpp` starts as the Paranoia file. Apply only the differences the retail code shows.
- Unit function list: `tools/units/client_cl_dll_<file>.txt`. Functions that Cry of Fear changed match no reference bytes and may have no retail name, so objdiff cannot pair them yet. Once the file compiles (and after adding functions), run `python tools/name_unit.py --dll client client/cl_dll/<file>`: it names unnamed retail functions of the unit by identical bytes or by their position in the object, updates the unit list and delinks. A position name is a proposal until the function reaches 100%; report the ones that stay below.
- When `name_unit.py` cannot decide (it reports a gap such as "7 retail, 6 ours"), read the unnamed retail functions in the unit's range (`disasm.py client <va> --outline`), decide which of our functions each one is from its strings, calls and the reference code, and record it: `python tools/add_name.py --dll client client/cl_dll/<file> <va> "<decorated name from our object>" "<evidence>"`. The evidence names what proves it (a string, a callee, the vtable slot). A function Cry of Fear added and the reference file lacks gets a name only with evidence; otherwise write it as `guess_<name>` and record that name.
- Cry of Fear client classes have no PDB but do have RTTI: `python tools/rtti.py client` lists every class with its vtable (`python tools/rtti.py client CHUDControl` adds the slots). A Cry of Fear file whose name no build records is named after its main class with the `guess_` prefix (`src/cl_dll/guess_HUDControl.cpp`).
- Never stand in for a missing member with a global, a local or a padding member: the bytes can match while the relocation points at the wrong place. `gHUD` (0x1018A0C0) is named in the retail map with its extent, so `tools/relocaudit.py` and `tools/update_status.py` report a function that reads another CHud member than retail (`gHUD+0x30f0` against `gHUD+0x24`) and keep it TODO. Report the member you need instead.
- `cl_util.h` defines `fabs` as a macro (a compare and branch). Retail files that use the real `fabs` (`andps`/`andpd`) need `#undef fabs` after the project headers, with `<math.h>` (double `fabs`) or `<cmath>` (float overload): check which one the retail code uses.
- A HUD method that Cry of Fear disabled with `return 1;` at the top and the old body left below stays out of line (8 bytes); a bare `return 1;` body is not the same.
- `name_unit.py` names the `__MsgFunc_X` wrappers by position, and they all match because they differ only in the callee: check them against the `HOOK_MESSAGE` pushes in `Init` and fix wrong ones with `add_name.py`.
- `x += a * b` and `x = a * b + x` compile differently: try both.
- One retail global has one name in every file: before you name a Cry of Fear global (`extern int guess_iWheelchair;`), grep src/ for its address in comments and for other `guess_` names of the same global, and find what writes it (a user message handler named by its `HOOK_MESSAGE` string is the best evidence: 0x10542BB8 is written by the Wheelchair message).
- A function Cry of Fear disabled keeps `return;` as its first statement with the old body below: it compiles to `ret 0` (cdecl) or `ret N` and its callers keep the call; an empty body gets inlined away. `if ( 0 )` around the old body works too.
- `fabsf( x )` gives the float `andps`, `(fabs)( x )` the double one, and plain `fabs` stays the cl_util.h macro (compare and branch).
- `vec3_t v = args->angles;` (Vector assignment) loads all three components first; `VectorCopy` interleaves loads and stores.
- `x ? 2 : 3` compiles branchless (`neg`/`sbb`); when retail branches, write an if/else with two calls and let the compiler merge the tails.
- A retail function that is 0% or cut short at its first `ret` while ours looks right is usually a Ghidra function whose switch was never resolved: tell the main agent.
- Inside one object the functions are in decorated-name order (most files) or in source order (a few). `tools/align_names.py` uses this; a gap it reports ("3 retail, 1 ours") means functions missing on one side: written in Cry of Fear and not in the reference file yet, or removed.
- `python tools/disasm.py client "<decorated name>"` and `--outline` work on the 1.6a client.dll; `--version` does not apply (no symbols in the older client builds).
- A dead load of a global pointer in both arms of an if/else (`mov eax, [gHUD+0x34b8]` with eax unused) comes from an if/else with identical branches whose condition is `p && p->member`: `if ( gHUD.m_Ammo.m_pWeapon && gHUD.m_Ammo.m_pWeapon->iId ) x; else x;` (AddViewmodelBob_guess). The late merge of the branches drops the tests and the member load but keeps the first load; an unused local, `if ( p ) {}` or `if ( p ) x; else x;` leave nothing.
- `int iTotal, iCount; iTotal = iCount = 0;` gave the retail order of the cmove temporaries in an if-converted loop where two initialized declarations (in either order) did not (CHud::AverageFPS_guess). Swapping the declarations swaps the registers of the two locals.
- A chain `a = b = x;` stores b first: when retail stores the member on the right first, swap the chain (`curstate.angles[0] = angles[0] = x`, StudioDrawHeadShield). `v[0] = v[1] = v[2] = x;` stores [2] first, three statements store [0] first (GL_SetupLighting).
- MSVC 14.27 gives registers and spill slots by live range, not by name or declaration order (tested with scratch files: renaming locals or swapping the declarations of equally used locals leaves the code unchanged; the local whose range ends first gets esi, then edi, ebx, [ebp-4], [ebp-8]). To move a slot, change where the values are used.
- A load that stays below a store to a spilled local instead of being hoisted above it either took the register of the stored value or reads past an address-taken aggregate: in MuzzleFlash_guess an aggregate of iModelIndex and pModel (escaping through &iModelIndex) reproduces the retail iId reloads.
- tools/try_variants.py can fail with `OSError: [Errno 22]` when another process holds the source for a moment; the file is then left at the last variant: check `git diff` and restore it.
- A counter bumped at the top of a loop body that ends in an if/else (`iLen++; if ( c == '@' ) szText[i] = ' '; else szText[i] = c;`) swapped esi and edi against retail; the same increment written into both branches (`if ( ... ) { iLen++; ... } else { iLen++; ... }`) gives the retail registers (CHud::MsgFunc_Objectives, MsgControlHint_guess, IN_ObjectivesDown). The source shape of a statement that both paths share decides the live range order even when the code is the same.
- Two loop variables in swapped stack slots: `int y; for ( i = 0, y = 0; i <= 12; i++, y += tall )` gave the retail slots where `int y = 0;` with `y += tall;` at the end of the body did not (CClothesMenu::CClothesMenu).
- try/catch functions (`catch ( CException * )`, `catch ( ... )`): Ghidra makes the catch funclets functions or labels of their own (`Catch@<va>`, `Catch_All@<va>`) and leaves the funclet's `mov eax, <continuation>` as a bare immediate, so the delinked function is short or the funclet row differs. Merge the range with `tools/ghidra_scripts/MergeFunctionRange.java` (`<entry> <last byte>`), drop the leftover labels with `DropLabelsAt.java` (`<va> ...`), add a reference for the immediate with `AddOperandRef.java` (`<instruction va> 1 <continuation va>`) so the delinker writes a relocation, save, and delink again (TeamFortressViewport::CreateCommandMenu, MP3FileInfo::ShowAllFrames). Run the scripts through the GhidraMCP `/run_ghidra_script` like SetBodyRange.java.
- MSVC 14.27 puts five int3 bytes at the end of the COMDAT of some functions with an EH frame; objdiff then shows five extra int3 rows on our side. Extend the retail body over them with SetBodyRange.java (MP3FileInfo::ShowAllFrames, the gMP3Info_guess atexit destructor, the MP3FileInfo scalar deleting destructor) and update the size in progress.csv.
- MSVC 14.27 lays out the case blocks of a switch in source order, and a case that differs from an earlier one only in its first call stays at its place as a short stub (`push [edi+0xa4]; jmp` into the earlier case): read the retail block order to recover the case order (CHudAmmo::Draw, second dual weapon switch 3, 20, 19, 7, 14, 13, 21, +1.1%). Which member a block reads first also gives the statement order of two independent assignments (the mag cross rect before m_fFade: `iWidth = ...; a = ...;`).
- When objdiff shows the same register and slot permutation over a whole function, cut parts of the function out in a scratch copy and watch the prologue (`mov edi, ecx` against `mov ebx, ecx`) to find the region that decides it (CHudAmmo::Draw: neither the dual weapon block nor the iClip < 0 branch, but the iClip >= 0 block, where retail caches gHUD.m_iHUDColor and ours keeps pw).

## hw.dll units (the VC6 engine)

hw.dll is Cry of Fear's own GoldSrc engine build (2013, Visual C++ 6.0). Valve never released the source, so files are written from the binary; the Linux steam_legacy `hw.so` (about 2019) carries full DWARF and gives names, prototypes, locals, struct layouts and the file of every function. First unit: `src/engine_hw/crc.c` (13/13 at 100%), the model for everything below.

| What | Where |
|---|---|
| Rows of one source file (names, status) | `docs/progress.csv` rows with dll `hw` and source_file `engine/<file>` or `guess:engine/<file>` |
| Ghidra pseudo C of an address range plus the exact text of every string it uses | `python tools/range.py hw <start> <end>` (writes build/decomp/hw_<start>.c) |
| Names, prototypes, local declarations in declaration order, globals of a file | `python tools/dwarf_file.py I:/Tools/_downloads/hl_linux_legacy/hw.so engine/<file> [--types]` |
| Original header declarations (structs with offsets, typedefs, externs) | `build/dwarf_hw/<path>.h` (regenerate: `python tools/dwarf_headers.py I:/Tools/_downloads/hl_linux_legacy/hw.so build/dwarf_hw`) |
| Engine names of globals and helpers by address | ReHLDS (`refs/ReHLDS/rehlds/engine`, consultation only) and the DWARF |
| Record a name | `python tools/add_name.py --dll hw hw/engine/<file> <va> "_<Name>" "<evidence>"` (C functions are `_Name`) |

Rules that hold for this build:
- Every engine object is built with `/Od` (the Standard Edition has no optimizer): code is literal, so statement order, expression shape and the names of locals decide the bytes.
- Stack slots are not given in declaration order but by a hash of each local's name (16 buckets, bucket 0 at the top of the frame, the last declared first inside a bucket; inner block locals after the outer ones). `python tools/vc6_slots.py <locals in declaration order>` predicts the frame. With the DWARF names the layout usually matches by itself; when two locals have the same type, the slot order tells which name is which. A slot the retail frame has but no DWARF local explains is an unused local (name it `_guess`).
- Global names by address: `tools/symbols/hw_manual.csv` rows from `tools/globals_linux.py` (the Linux relocations paired by the functions that use each global). `python tools/globals_linux.py hw I:/Tools/_downloads/hl_linux_legacy/hw.so --members <global>` prints the Linux member offsets next to the Windows ones, which shows where a Windows struct layout differs.
- Shapes that matter under `/Od`: `while ( 1 )` gives `mov eax,1; test eax,eax; je`; retail often writes while loops with the step at the end of the body instead of for loops; `if ( x > 0 ) return 1; return 0;`, `return x > 0;` and `return x != 0;` are three different shapes; `if ( !a || !b ) return;` and an empty then-branch are shapes of their own; `float v = expr; return v;` gives `fst` without a reload; literals with an f suffix are dword constants, others qword constants; switch cases are emitted in source order behind a two-level byte table.
- Some third-party code is stdcall: bzip2 is built WINAPI (no `add esp` after its calls), so its names carry the decoration (`_BZ2_bzBuffToBuffDecompress@24`).
- Doubles are 8-aligned on Windows and 4-aligned in the Linux build, so `server_t`, `client_t` and `netchan_t` members after a double sit at other offsets than the DWARF says; probe the Windows offsets with VC6 (`USERID_t` is 24 bytes).
- Some Linux locals are function-local `static` on Windows (the Log_Printf buffer, the HPAK_GetItem name, the PM_TraceLine trace, `iTouchLinkSemaphore`). Name buffers are `MAX_PATH`, while `_snprintf` gets 256 and `Q_strncpy` 255.
- A jump straight to the epilogue is a `return`; a jump to another jump ends a nested if/else. A `for` is `init; jmp cond; inc:`, a `while` keeps the increment at the bottom without a jump over it. A `default:` written first puts its body right after the compare chain.
- `x ? 1 : 0` gives neg/sbb; retail's branch form comes from an if/else into a local. Chained float `a = b = 0` re-reads `b`, chained pointer `= NULL` stores directly. `if ((frac -= 0.05) < 0)` gives `fst` then `fcomp`. Declaration initializers are emitted in declaration order.
- VC6 loads a global operand of `float * float` first; retail's pointer-first order needs a double operand: `(double)size[1] * plane.normal[1]`.
- When the delinked retail side splits a global into several `DAT_` labels (an access at +4 then pairs with a different symbol), record the global with its size in `tools/symbols/hw_datasizes.csv` (player_mins, player_maxs).
- An explicit `(float)` cast on an argument makes a stack temporary (an extra `fst` before the push). `i < sizeof(x)` compiles to an unsigned `jae`; write the literal when retail has `jge`.
- `if ( a != b ) return;` gives `je skip; jmp end`, but inside an `||` chain the conditions jump straight to the shared jmp, so two ifs and one `||` can be told apart. `continue` jumps straight to the loop increment; when retail jumps to the end of the body instead, the body is wrapped in an if. A `break` in the then-branch of a switch case is a long jmp to the end of the switch.
- `min`/`max` with a simple operand: VC6 evaluates the complex operand first and reverses the comparison, so `min( bound, x )` and `min( x, bound )` differ in the test (0x41 or 1) and the branch layout.
- In .data, variables with constant initializers come first, function statics included; a static with an address initializer sits next to its function's string literals.
- One global has one type: when two files seem to use a global with different types, read the retail data. `scr_downloading` is the cvar at 01EBA5F0 and net_chan.c writes its `.value` (+12); an `extern float` there links to the cvar name pointer. objdiff and relocaudit cannot see a wrong offset while the retail address is unnamed; `tools/objdiff_tool.py harvest --matched` reports such cases as one name at two addresses or two names at one address (R_NewMap read g_bIsCZeroRitual, SV_SpawnServer copied pr_strings, CL_BatchResourceRequest called memcpy).
- `x == <nonzero float constant>` compiles to an integer compare against the bit pattern (`cmp dword ptr [x], 0x40000000`); `x == 0` uses fcomp. A float just stored stays in st0 for the next statement (`s = ...; s *= ( 1.0 / 64 );` gives fst without a reload). `a = b = 9999` stores two immediates, `a = b = c = 1.0F` stores the rightmost one and copies it through a register.
- Large local arrays of the DWARF (`scaled` in GL_Upload32, `trans` in GL_Upload16) are statics in bss in 2013; unused locals still get frame space at /Od (Draw_TileClear), but locals only the Linux build has do not exist. `x < 0 ? 0 : x` is branchless (setl/dec/and) even at /Od. `if ( mp ) { body }` gives one `je end`, `if ( !mp ) return;` gives jne+jmp. `Q_strcmp( "tga", p )` and `Q_strcmp( p, "tga" )` compile differently.
- Static stdcall functions (the 336 qgl log functions) need their `@N` names in the symbol map, like bzip2.
- Logical values: a logical expression used as a value (`f( !a && !b )`, `x = n >= 1 && n <= max;`) gives a temporary with mov 1 / mov 0 branches; `(x != 0)` gives xor/cmp/setne; `c ? true : false` gives neg/sbb/neg; `!call()` gives neg/sbb/inc. In `var + call()` the call comes before the variable is loaded.
- `p = ...; while ( p ) { ...; p = p->next; }` puts the test right after the init; `for` gives `init; jmp cond`. Many engine list walks are while loops. `cl_entities + n + 1` gives `lea [base+eax+size]`, `&cl_entities[n + 1]` adds 1 before the imul.
- This build calls the C runtime directly where the Linux build has Q_ wrappers: `_stricmp`, `_strnicmp`, `strstr`, `strrchr`, `strncpy`, `sprintf`, `strcat`, `realloc`, `_atoi64`.
- `x -= y * 60` (imul/sub) and `x %= 60` (cdq/idiv) are different shapes. `min( 100, x )` gives `cmp x,100; jle` into a temporary, `max( 0, x )` and `x = *p ? p : NULL` are branchless. A ternary of two integer constants is branchless even at /Od (`(n == 1) ? 8 : 64` gives neg/sbb/and/add); float ternaries keep their branches.
- `char a[10] = { '-', ... }` stores bytes, `= "..."` copies dwords from a literal; VC6 zero-fills the rest of a partly brace-initialized array, so no fill in retail means the array size equals the initializer count. `(int)cvar.value` is ftol, `if ( cvar.value )` is fcomp. A counter stored before the pointer means `for ( i = 0, p = base; ... )`. A 64-bit `!id->m_SteamID` puts the field address in a temporary and ors the halves.
- Strings of global initializers (cvars, tables) all go to .data before any function's literals, in definition order. The "Exe build:" time stamp string differs per file (host.c and host_cmd.c were compiled at different times).
- Ghidra ends a function at a call to a no-return function (longjmp, _exit) although VC6 still emits the epilogue: tools/ghidra_scripts/ExtendAfterNoReturn.java extends such functions before delinking (Host_EndGame, Host_Error, Sys_Error).
- The functions of a file appear in source order (no alphabetical sort, unlike the 2020 game DLLs), and functions GCC inlined in the Linux build are separate functions here.
- The 2013 file is an older version than the Linux one: functions added later are missing, some functions are shorter. Write what the Windows assembly does; the DWARF only names things.
- Calls through the engine's anti-cheat hook table (`call [0x01ebf980]` with a pointer to an argument) are `g_engdstAddrs.pfnXxx( &arg )` (`build/dwarf_hw/engine/APIProxy.h`, `cl_enginefunc_dst_t`).
- Headers live in `src/engine_hw/` and are named like the originals (`common.h`, `cvar.h`, ...). Each header belongs to the agent that owns its .c file; any agent may add a missing prototype or extern to any header, but only the owner changes a struct.
- Retail string literals go into `.data` (VC6 without /GF): the strings at the end of `tools/range.py` output are exact.
- `f( n++ )` as an argument costs a hidden 4-byte temporary per use at `/Od`; retail's frame without them means `p = Cmd_Argv( n ); n++;`. A frame 0x14 bytes too big pointed at five such post-increments.
- `x >= 1` gives `cmp x,1; jl`, `x > 0` gives `cmp x,0; jle`: the literal survives. A size argument like 0xFB is `sizeof( name ) - 5` (room for ".dem" before COM_DefaultExtension).
- When two same-type DWARF locals sit in each other's slots, check the order the source assigns them before renaming: in CL_DemoParseSound the slots exposed a swapped volume and attenuation (docs/original_bugs.md). `idx = n; n++; p = &arr[ idx ];` and `p = &arr[ n ]; n++;` address differently.
- A float product inside `fabs( )` whose global operand is indexed (`studio_planes[j].normal[0] * size[0]`, the SV_HullForBeam and R_StudioHull pattern) loads the global before the pointer only with a double operand: `fabs( (double)studio_planes[j].normal[0] * size[0] )`; which side carries the cast follows retail's load order.
- Function statics of C files are named `_?name@?1??Function@@9@9` in the object (pmovetst's `tr`, r_studio's `pos` and `q`); the retail map needs that spelling for relocaudit.
- `tools/objdiff_tool.py harvest <units> --matched` also takes names from functions at 100%: their relocations still point at unnamed retail globals (vpn, r_refdef, frustum), which a delink then names.
- A function at 99.99% whose only marked row is a jump to another target is not a naming difference: `jmp end` against `jmp return-statement` means a missing `return;` at the end of a branch (R_DrawSequentialPoly).
- `#pragma check_stack( on )` gives retail's `mov eax, N; call __chkstk` prologue in small frames (GL_RSURF decal functions). A global that lies among a file's statics in retail .bss had an explicit zero initializer; uninitialized globals go to a separate common area. Comparing `unsigned >> 1` with an int needs `(int)( x >> 1 )` for a signed jle. An unsigned int in float arithmetic is a 64-bit fild with a zeroed high dword. A function GCC inlined can still be a real, empty out-of-line function on Windows (R_InvalidateSurface).
- VC6 dynamic initializers of constructed globals (`EXPOSE_SINGLE_INTERFACE`, function statics with constructors) are object-local `_$E1`, `_$E2`, ... numbered per object, so several units use the same names: the symbol map, the unit lists and progress.csv keep that spelling (build.py, DelinkUnits.java and update_status.py allow one address per unit for `_$E` names), while both objects spell them `_E$1` (tools/vc6_anon.py, after compile and after delink) because objdiff pairs names that start with `$` by their code and mixes up look-alike initializers; `objdiff_tool.py show` therefore takes `_E$5`. A function with a try/catch (sys_dll2.cpp CEngineAPI::Run) is split by Ghidra at its catch funclet: delete the `Catch@` and continuation functions and the `Catch@` label, clear the flow override of the jump before the funclet and extend the body; `try`/`catch ( ... )` compiles without /GX. Names of C files that the file has a second C++ copy of (sys_dll.c and sys_dll2.cpp each have a Sys_Init) take C++ linkage in the .cpp (`?Sys_Init@@YAXXZ`), the C prototype leaves the shared header.
- A file static that several objects define under the same decorated name (a `static inline CloneString` in BitmapImagePanel.cpp next to the shared_util.h inline of CareerDefinitions.cpp, the file-local `operator new`/`operator delete` of BaseUISurface_Font.cpp next to the CRT ones) gets a row in `tools/symbols/<dll>_local.csv` (`va,name,unit,evidence`) for each object-local copy; the one copy every other object calls stays in `_manual.csv`. build.py and DelinkUnits.java then let the name label one address per unit and delink each unit's own copy (docs/tooling.md). Do not rename such a function `_guess` to dodge the one-address rule.
- VC6 numbers anonymous namespaces at random (`?%Frame.cpp<n>@@`); tools/vc6_anon.py renames them to the retail RTTI spelling and build.py runs it on every VC6 object (extend its table for a new file). `assert()` from <assert.h> is active in the GameUI build (no NDEBUG) and retail has none: use the `Assert` no-op. VC6 calls virtuals on `this` directly inside constructors; a same-type dynamic_cast still emits __RTDynamicCast. Bool bitfields read with shr/and 1 and write with and ~mask / or. Progress.csv sizes leave out switch jump tables that our COFF function size includes.
- Check the DWARF block locals for loop counters too: when two DWARF names sit in each other's slots, the source used them the other way round (GLR_StudioDrawShadow counts meshes with k and tricmds with i).
- An empty `if` emits no code but can still advance VC6's round-robin register choice: `if ( p->x ) {}`, `if ( a & 2 ) {}`, `if ( g > a ) {}` advance by one, `if ( p->x & m ) {}` by two, `if ( a ) {}` and `if ( g > 3 ) {}` not at all. A block shifted by one register after identical code points at a commented-out body (SV_Physics_Step, SV_ParseMove). An empty `else if ( float compare ) {}` keeps its fcompp without a jump.
- An empty `""` literal goes to .bss under VC6: a function returning the address of a bss byte is `return "";`.
- The integer compare of `x == <float constant>` applies only to directly addressed locals and globals; through a pointer it stays fcomp (`ent->v.friction != 1`). `x &= ~0x80` gives `and dl,0x7f`, `x &= 0x7f` gives `and edx,0x7f`. `e = x; for ( e++; ... )` differs from `for ( e = x + 1; ... )`.
- A static GCC inlined in the Linux build exists in the DWARF only as an inline subprogram (dwarf_file.py does not list it, its name and locals are there). A DWARF "local" can be an extern declared inside the function. `build.py check` prints the old object's match under a compile error: look for the "base ... ok" line.
- `__try`/`__except` (VC6 SEH, `_except_handler3`, com_custom.c COM_CreateCustomization): no flow reaches the filter (`mov eax,1; ret`) and the handler, so Ghidra leaves them out of the function body and the delinked function is short. Extend the body with `tools/ghidra_scripts/SetBodyRange.java` (args `<entry> <last byte>`, run through the GhidraMCP `/run_ghidra_script`, then `/save_program`) and delink again; the `fs:[0]` against `fs:[__except_list]` rows that `objdiff_tool.py show` marks do not count in the report. A `goto` out of the `__except` block jumps straight to its label.
- A check that the Linux build has and retail lacks can survive as an empty if that only moves the register rotation: the register choice of the statement after it tells how many pointer loads its condition had (SV_SendRes_f, see its progress.csv note).
- An empty `if ( c ) { /* comment */ }` emits nothing at `/Od`; `if ( c ) ;` keeps the comparison (fld/fcomp) without a branch: retail's lone fld/fcomp in R_StudioSetupBones is the HLSDK's emptied prevframe test.
- Check the DWARF block locals before trying casts: a float copied into a stack temporary before an int-to-float multiply is a named block local (`float baseframe = currententity->curstate.frame;` in CL_StudioEstimateFrame).
- `b = ( n > 0 ) ? true : false;` stores setg straight into a bool, `b = n > 0;` clears the register first (xor before test/setg) (FileSystem_SetGameDirectory). `bool b = a > 0 && _stricmp( x, y );` gives the mov 1 / mov 0 temporary.
- A source in src/common that needs engine headers includes them inside `extern "C"` through quakedef.h and server.h only: the includer's directory is searched first, so src/common/const.h, mathlib.h, usercmd.h and cvardef.h shadow the engine copies, while headers included from src/engine_hw resolve there (steamid.cpp).
- A function in a 16-aligned gap of its own, with no caller and no data of its own or only a pointer to itself, is a one-function /Od object: give it a `_guess` file whose basename sorts into that slot of the link order (the order is case-insensitive by basename, `_` before letters), with its .data checked against the neighbours' .data in the same slot (prefs_guess.c, verify_guess.c). Code bytes that are not instructions inside a `push ebx/esi/edi` frame are an `__asm` block of `_emit` bytes (VerifyMarker_guess; swds.dll has the same block).
- Processor pack objects (vc6pp): when retail saves the callee-saved registers before an early return and ours only after it (shrink wrapping), the flow graph decides it: a search loop that returns from inside the loop instead of breaking to a shared return gives the retail prologue (CFontManager::GetForeignFallbackFontName). Store and load scheduling differences (CFontAmalgam constructor, CWin32Font::Create) did not move with any source shape tried.

## cof.exe units (the VC6 launcher)

cof.exe is Valve's hl.exe launcher (`src/launcher`, Visual C++ 6.0, `/G5 /W3 /O1 /MT /GR`). Names come from the hl_linux DWARF (launcher.cpp, commandline.cpp, interface.cpp) and the hw.so DWARF (LoadBlob.cpp, registry.cpp).
- `/O1` calls `strlen`, `memset` and `strcpy` instead of inlining them and uses size idioms (`and [mem],0`, `leave`).
- `while ( 1 ) { ...; if ( !x ) break; }` gives a different loop layout and register assignment than `do {} while ( x )` or `while ( x )`; WinMain and LoadFilesystemModule only matched with it.
- `/O1` implies `/Gf`: string literals are `??_C@...` sections shared across objects, named the same everywhere; map retail strings by content.
- A static function comes right after its first caller. In .bss, VC6 places the uninitialized globals and statics of an object in an order given by their undecorated names alone (a hash: neither the definition order nor the type changes it), then the explicitly zero-initialized ones (`= NULL`, `= ""`, `= { 0 }`) in definition order. retail cof.exe has com_gamedir before g_pFileSystem, which the name order can not give: g_pFileSystem and everything after it are zero-initialized definitions (launcher.cpp). Test an order with a few lines compiled by `tools/msvc6.py cl` and the .bss symbol values of the object.
- `stricmp`/`strnicmp` (OLDNAMES) instead of `_stricmp` change the order the C runtime is linked in; use whichever reproduces the runtime layout and name the retail address with the alias.
- `winsock2.h` gives `call [__imp__WSAStartup@8]`, `winsock.h` calls through a thunk. A brace-initialised local `char` array is one byte store per character.
- A function that delinks as `(bad)` has an empty body in Ghidra: disassemble the range, recreate the function and delink again.

## GameUI.dll units (VC6, the VGUI2 menu DLL)

GameUI.dll is built with Visual C++ 6.0 Standard Edition objects (`/G5 /W3 /Od /MT /GR`, no `/GX`), so the hw.dll rules hold (literal code, stack slots by name hash: tools/vc6_slots.py). Names, prototypes, classes, vtable slots and member offsets come from the Linux gameui.so DWARF (`tools/dwarf_file.py`, build/dwarf_gameui); `python tools/rtti.py gameui` and `tools/vtable_linux.py` pair the MSVC vtables. GameUI's own files go in src/gameui; Valve library code that hw.dll shares (vgui2 controls, KeyValues, interface) goes in src/vgui2 and src/public.
- Overloaded virtuals: VC6 puts all same-name virtuals together at the slot of the first declaration, in reverse declaration order, even when they are not adjacent, so Linux slot arithmetic is off after such a group; keep the DWARF declaration order (KeyValues FindKey, Panel PostMessage and SetParent).
- `""` and `L""` literals go to `.bss`. Global `.bss` order: constructed or uninitialized globals first, then `= NULL` globals in declaration order.
- A static function's code and literals come after its first caller; a `static inline` function has its literals at the definition and its code after the first caller (KeyValues WriteIndents). Dynamic-initializer literals come before the literals of functions defined earlier in the file.
- Shapes: `if (a && b ...) return true; return false;` differs from the negated form; two `if (...) break;` differ from one `||`; `int len = strlen(s) + 1;` differs from adding 1 at each use; `if (c != '/') {...} else {...}` and the reversed test give different branch orders. Case bodies are laid out in source order, the byte index table by case value.
- VGUI2 message maps under VC6 need `(MessageFunc_t)(void (ThisClass::*)(void))&ThisClass::name`. The DWARF class/struct keyword changes the mangling (IBaseSystem and cl_enginefuncs_s are struct). An inline function whose first user is an earlier object (PHandle constructor, IBaseInterface, CUtlBuffer::TellPut) belongs to that object.
- Retail GetPanelBaseClassName returns "vgui::EditablePanel": the GameUI sources wrote `vgui::` aliased to `vgui2`. The 2013 ISurface has 93 slots (from the hw.dll vtable); VC6's std::vector is 16 bytes.
- vgui2 controls (2003 code, older than the Source SDK): MessageMapItem_t is 0x38 bytes (the member pointer is 16 bytes, 8-aligned) and a static m_MessageMap[] is initialized by $E functions; OnMessage uses `switch (numParams)` with `bFound = true;` before and after it; Label has CS_LIGHT ("BrightBaseText"), Button::Init and ApplySettings call SetTextColorState(CS_LIGHT), Button::DoClick plays the released sound, Label::Paint adds only the left text inset, EditablePanel::PaintBackground only calls the base class.
- COMDAT order after an object's functions: ??_G<Class>, the RegisterMap constructor, ChainToMap, GetMessageMap and GetPanelClassName where first used, GetPanelBaseClassName, then each message function's constructor and InitVar, AddToMap after the first InitVar; class template members follow their declaration order (this tells CUtlVector Count from Size). Prepare*Map loops are while loops; `max` is the macro (src/public/minmax.h has a template); `short_member = x + wide;` adds in int and truncates.

## Facts that hold for the CoF weapons

- Globals: `gpGlobals->v_forward` is +0x28, `v_up` +0x34, `v_right` +0x40. Shell eject: `vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40`, `vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20`, `EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL )`.
- Fire event: `UTIL_MakeVectors( v_angle ); Vector vecSrc = m_pPlayer->GetGunPosition( ); Vector vecShellEnd = vecSrc + gpGlobals->v_right * 32;` then `PLAYBACK_EVENT_FULL( flags, ..., m_usThirdFlash, 0.0, m_pPlayer->pev->origin, vecShellEnd, 0, 0, m_iId, 0, 0, 0 )`; `int flags; if ( m_pPlayer->m_iViewValue != 1 ) flags = FEV_NOTHOST; else flags = 0;` comes before `UTIL_MakeVectors`.
- Bullets: `vecDir = m_pPlayer->FireBulletsPlayer( 1, vecShellSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, gSkillData.Dmg<Weapon>, m_pPlayer->pev, m_pPlayer->random_seed )` (check the bullet type and the gSkillData member against the disassembly).
- Fire sound: `guess_UTIL_PlayWeaponSound( "weapons_new/<w>/<w>_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true )`.
- Timers that `ja` skip in retail (`comiss member, time; ja skip`) are written `!( member > gpGlobals->time )`; plain early returns are `if ( member > gpGlobals->time ) return;`.
- In ItemPostFrame, `m_iSprintState = 0;` is the last statement of the sprint/jump blocks (after the timers): MSVC hoists the store up to the last read of gpGlobals->time, so its retail position tells where it was.
- Store a GetSequenceTime result in the function scope `float time;` before using it (`time = GetSequenceTime( ... ); m_iNextSprintAnim = time + gpGlobals->time;`).
- The suicide punch sound: `{ int pitch = 90 + RANDOM_LONG( 0, 15 ); EMIT_SOUND_DYN( ..., pitch ); }`.
- `pev->body` from the flashlight flag: `if ( m_pPlayer->m_iFlashlightFlags & 4 ) pev->body = 2; else pev->body = 1;` (not a ternary).
- Which branch comes first (`if ( m_iClip )` or `if ( !m_iClip )`) varies per weapon and per function: follow the retail jump (`je`/`jne`) and the push order.
- Ammo `AddAmmo`: two separate `GiveAmmo` calls in an if/else on `g_pGameRules && g_pGameRules->IsMultiplayer()`, each followed by the `EMIT_SOUND` and `return TRUE;`, then `return FALSE;`.
- `GiveAmmo` has a fourth argument (`false`); `SendWeaponAnim` defaults (`skiplocal = 1, body = 0`) are used everywhere.
- Ironsight toggle button is `IN_CANCEL`; suicide button mash tests `IN_ATTACK | IN_ATTACK2`; sprinting is `pev->flags & (1<<18)`.
- `IronsightsTo`/`IronsightsFrom` are the same in every weapon except the enum names (copy from `CoF_P345.cpp`).

## Read the 1.55 debug build first

The 1.55 hl.dll is an unoptimized debug build of nearly the same source. `disasm.py --version 1.55` prints a `-- line N` row where each source statement starts (1.55 PDB line table): write the statements in that order and with that if/else layout. Most remaining differences after a first draft come from source structure that the 1.6a release build merges or drops, and the debug build still shows it:

- Statements repeated in both branches of an if/else (the release build merges the copies after the branches). Example: glock AttachTaclight is `if ( m_iClip ) { SendWeaponAnim( GLOCK_DRAW ); m_pPlayer->m_flNextAttack = gpGlobals->time + 1; } else { ...NOSHOT...; same store }`. Writing the store once after the if/else changes register use.
- Unused locals whose initializer has side effects or affects register allocation. The 1.55 PDB lists them (`tools/pdbtypes.py ... func`). Examples: `float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();` after `Vector vecDir;` in PrimaryAttack (without it the v_angle + punchangle sum loads its x operands in the other order); `int pitch = 95 + RANDOM_LONG( 0, 10 );` left before a sound call that no longer uses it (retail calls RANDOM_LONG and drops the result).
- Constants the release build folds away (the `+ 0x5f` above is the 95).
- `x + 1` against `x + 1.0`: the debug build shows `fadd dword` (float constant, write `1`) or `fadd qword` (double, write `1.0`).

## More patterns

- Operand order of a sum: the operand retail loads first is usually the second one in the source. `v_angle + punchangle` loads punchangle (+0x68) first.
- Comparison constants survive: `cmp 3; jl` is `>= 3`, `cmp 2; jle` is `> 2`. Write the constant that appears in the retail `cmp`.
- `if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )` and `if ( !g_pGameRules || !g_pGameRules->IsMultiplayer() )` differ in where the null test jumps: both jumps to the same target means `&&`.
- Spawn tests `pev->frags == -2` in most weapons (P345 uses -1): check the constant per weapon.
- Weapons without `_NOSHOT` sequences send every animation without a clip test; the guide's clip-branch notes only apply where the model has them.

- A four-timer chain `m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + X;` compiles to `shufps` + `movups [esi+0x1cc]`: when retail has that pair, write the chain.
- `m_bReloading = m_bWantToEndReload = false;` compiles to one word store.
- Identical blocks written twice in the source (early outs, doctor mode and normal `MESSAGE_BEGIN` blocks) are merged by the compiler: write them twice when the 1.55 line rows show two copies.
- Globals that no header declares (`g_iDoctorMode`, `m_usRevolverShells`, `gmsg*` ids) take an `extern` at the top of the .cpp, with the type from the 1.55 PDB (`docs/symbols_1.55.csv`, module `globals.obj` and others).
- Switch jump tables are part of the function on both sides since the delinker change of this session; `switchD_...` names on the retail side are naming only.

## Patterns from the second batch of weapons

- The fire event flags follow the 1.55 form of each weapon: `int flags = 0; if ( m_pPlayer->m_iViewValue != 1 ) flags = FEV_NOTHOST;` (M16, rifle, shotgun, FAMAS, G43) or the if/else form (pistols). Both give `setne`, but the choice moves other code, so take it from the 1.55 line rows.
- A 1.55 `int pitch = 95 + RANDOM_LONG( 0, 10 );` before the fire sound is kept only when retail 1.6a still calls RANDOM_LONG there (VP70); in most weapons 1.6a dropped the line together with the call.
- Checks that an earlier return already implies (`m_iSprintState == 1 || m_iSprintState > 2` after `if ( m_iSprintState ) return;`) are written anyway in the 1.55 order; the release build removes them.
- Two calls that differ only in one argument (FireBulletsPlayer with `m_AccuracyVector` or `VECTOR_CONE_1DEGREES`, three GiveAmmo calls) are written out in full as in 1.55; retail merges them into one call with a select.
- `test [m_afButtonPressed], 0x801` can come from `( x & IN_ATTACK ) || ( x & IN_ATTACK2 )`.
- A WeaponIdle made only of early returns compiles to `ret 0`, the same as an empty body: keep the 1.55 body.
- Rifle-class melee: `if ( m_pPlayer->m_fStamina <= 0 ) return;` then `m_pPlayer->SetNextThink( 0.23 ); m_pPlayer->SetThink( &CBasePlayer::MeleePushBack );`. Pistols use `WeaponMelee`.
- Take member offsets from 1.6a (`tools/member.py`); 1.55 offsets differ (CBasePlayerWeapon members 4 lower, CBasePlayer very different). Use 1.55 only for statement order.
- The `check` printout rounds per-function percentages: trust the unit line and `matched_functions` in `build/check_project/<unit>/report.json`.

## Patterns from the base units (weapons, combat, client, gamerules, triggers)

- The 1.55 debug build is not always the same code as 1.6a: compare with the 1.6a disassembly before trusting a 1.55 statement (FireBulletsPlayer moved the doctor boss test into the shot loop; entvars_t offsets from 0x128 on are 4 higher in 1.6a).
- A call in the 1.55 debug build with no test after it comes from an `if` whose whole body is commented out; the release build keeps the call and drops the test.
- MSVC 19.27 drops calls to functions of the same file whose compiled body has no side effects (empty void function, `return FALSE;`), whatever the definition order and even with `__declspec(noinline)`. If retail keeps such a call, it cannot be reproduced from one file; note it and move on. Exception: an empty function that takes a `Vector` by value (`CBoat::Splash`) keeps its calls, because building the argument copy is not removed with it.
- `/Ob1` sometimes keeps an inline helper as a call (the third `FStrEq` of a function stays out of line while the first two are inlined): write the source plainly. A helper the 1.55 build shows as a function (`GET_INFOBUFFER`) has to be `inline` in our source.
- Write `FStrEq(...)` where retail calls it, `!strcmp( STRING( pev->classname ), ... )` where retail reads `pev->classname` directly, and `FClassnameIs( edict(), ... )` where retail goes through `pContainingEntity`.
- A dead `sub eax, N` after the case tests of a switch gives the value of a case whose body was merged into default: a cheap check of enum values.
- After `ucomiss`/`fucompp` and `test ah, 0x44`: `jnp skip` means the source says `!=`, `jp skip` means `==`.
- Register base/index order in inlined copy loops and the load order of commutative operands depend on the whole function; a change in one block can flip another. Stop early on these.
- `for ( i = 0; i < ARRAYSIZE( a ); i++ )` with a function scope `int i` compiles to an unsigned compare (`jb`).
- A call written `CGameRules::VoteKickThink()` is a direct call in both builds: when the 1.55 build calls a virtual directly, the source used the qualified name.
- A Steam API call compiles to `call [IAT]` and needs `extern "C" __declspec(dllimport)`.
- In many objects the 1.6a functions are laid out in alphabetical order of their decorated names (some, like cbase.obj, keep source order). The neighbours of an unnamed retail function tell roughly what its name must be.
- Functions unnamed in the retail map are missing from the unit lists and never reported. `tools/bytematch.py` pairs them with our functions by bytes; check the address gaps of a unit when a function you wrote is not in the check output.
- To test a header change without touching the repo, copy the .cpp next to a modified copy of the header in a scratch folder (`#include "x.h"` looks next to the .cpp first; an extra `/I` does not override it).

## Patterns from the melee weapons

- CAxe, CNightstick, CBranch, CSledgeHammer and CBookSimonSledgehammer are not in weapons.h: declare them in the .cpp (from `hdrgen.py`) before `LINK_ENTITY_TO_CLASS`, with their small inline methods (`ShouldWeaponIdle`, `CheckRangeAttack1/2`) in the class body. The 1.55 rows tell where each class lives.
- Where 1.55 calls `EMIT_SOUND` (the util.h inline), retail shows `EMIT_SOUND_DYN( ..., 0, 100 )`: write `EMIT_SOUND`.
- A pushed constant sequence in `GetSequenceTime` instead of a load of `pev->weaponanim` means the source passes the sequence directly.
- The melee ItemPostFrame is one template (axe, nightstick, branch, booklaser): function scope `float speed` and `float time = 0`, a second `float speed` inside the fourth block; the sledgehammer drops the suicide check, the half time on sprint-to and the idle timer on sprint-from.
- Content can differ from 1.55: Book Simon's SetYawSpeed and Spawn read the `cof_monsterhumanyawspeed` and `cof_monsterhumanscale` cvars in 1.6a. Use 1.55 for statement order and check the content against retail.

## Patterns from the CoF monsters

- The 1.6a monsters read cvars where 1.55 had constants: `SetYawSpeed` uses `ys = CVAR_GET_FLOAT( "cof_monsterhumanyawspeed" );`, `Spawn` uses `int guess_iScale = CVAR_GET_FLOAT( "cof_monsterhumanscale" );` then `UTIL_SetSize( pev, Vector( -s, -s, 0 ), Vector( s, s, 72 ) )` (one int local gives the single `cvttss2si` plus `neg`).
- entvars `+0x114` is `body`: `if ( pev->body == -1 ) pev->body = RANDOM_LONG( 0, 10 );`. The view offset `0x41e00000` (28) is `VEC_VIEW`.
- `CheckTraceHullAttack` has no default arguments in CoF: `( 55, gSkillData.x, DMG_SLASH, false, 0 )`.
- Keep the HLSDK `velocity + v_forward * -100`; retail compiles it to `mulss 100` plus `subss`.
- In 1.55, a `break` in the last switch case produces no code; a missing `break` before another case is a missing `jmp`, which is how a fallthrough shows.
- A local the 1.55 PDB lists for an otherwise empty body is still declared (`CSlower3::AlertSound` is `int pitch;` with the rest commented out; retail is `ret 0`).
- The Killed copies inline the kill statistics: `if ( m_iLastDamageType & (1 << 26) ) ((CBasePlayer *)pAttacker)->playerstats.monsters_killed_melee++; else ...monsters_killed_shooting++;` then `GetPointsForKill( (char *)STRING( pev->classname ), (CBasePlayer *)pAttacker )`; only the twitchers add the `weapon_axe` check that sets `m_iFoundHoodies |= 0x20000`.
- `bool newgore;` is defined in CoF_Slower.cpp (the 1.55 PDB module); player.cpp declares it `extern`.

## Patterns from the tool items

- For commutative adds the 1.55 operand order is not reliable for 1.6a: `Ignite` needed `gpGlobals->time + gSkillData.flareBurnTime` although 1.55 has the reverse. The general rule (retail loads the second source operand first) is what worked.
- A retail function with no entry in the symbol map is absorbed by its neighbour and shows as a very low match on an otherwise identical function: check the address against our functions with `tools/bytematch.py` or a direct byte compare.
- In single-symbol `objdiff_tool.py show`, references to other functions of the same unit (`mov [esi+0x78], &Func`, `jmp Func`) are flagged; the unit report counts them as matched.
- The 1.55 debug build drops unreachable code after a `return` (except loop heads reached by a back edge): a jump in the line numbers means dead source.
- Two separate bool stores (`m_bThrowing = false; m_bThrown = false;`) merge into one word store, like the chained form.
- An unused `float speed = ...Length2D();` disappears entirely in 1.6a, sqrt included.
- Where 1.55 calls `?EMIT_SOUND_DYN` directly with `edict()`, write `EMIT_SOUND_DYN( edict(), ... )`; `STOP_SOUND` inlines to `EMIT_SOUND_DYN( ..., 0, 0, SND_STOP, PITCH_NORM )`. `strcpy( x, "" )` becomes one byte store and `strcmp` an inline loop.

## Patterns from the dual weapons

- One sound call, not two: when retail has a single `call dword ptr [eax+0x104]` (EarPosition) where 1.55 had a per-hand if/else of EMIT_SOUND_DYN, 1.6a used one `guess_UTIL_PlayWeaponSound` call. Two identical calls in an if/else compile to a hoisted `mov eax, [eax+0x104]; call eax` instead; when the branches also hold other statements (an ammo decrement), the duplicated form matches.
- gmsgKillWpnPart messages use `MSG_ONE_UNRELIABLE` (8).
- Empty helpers (CameraShoot, TMPShoot, SetBodyForWeapons) lose their calls in retail, so their switch cases merge with the default.
- CDualWeaponRight members are 4 bytes higher in 1.6a than in 1.55, like CBasePlayerWeapon: never take offsets from 1.55.
- An `if ( x )` with no body and code only in the else (lines commented out in 1.55) compiles the same as `if ( !x )`.
- Functions missing from a unit list are not checked at all: compare every function you write that the check does not list against its retail address (`tools/bytematch.py`).

## Patterns from the AI sweep

- Float `fabs`: monsters.cpp and AI_BaseNPC_Schedule.cpp use the float overload (`andps` mask, float compare), func_break.cpp and nodes.cpp the double one. In 1.55 those files called `?fabs@@YAMM@Z`; with MSVC 19.27 the float overload comes from `<cmath>`, so those files include it after the project headers.
- `xor eax, eax; je` inside a 1.55 condition is a literal `&& 0`: 1.6a keeps the calls before it (`IsMultiplayer()`) and drops the body.
- When objdiff shows `.dword $LN` inserts, a truncated retail function or a function paired with the wrong retail name, compare the raw bytes with relocations masked before changing source: these were all retail-side problems (fixed in the delinker, the Ghidra function bodies or the name map).

## Patterns from the second monster group

- Monster size: `int guess_iScale = CVAR_GET_FLOAT( "cof_monsterhumanscale" ); UTIL_SetSize( pev, Vector( -guess_iScale, -guess_iScale, 0 ), Vector( guess_iScale, guess_iScale, H ) );`; for the taller the doubling is in the declaration (`(int)CVAR_GET_FLOAT( ... ) * 2`). Yaw speed: `int ys; ys = CVAR_GET_FLOAT( "cof_monsterhumanyawspeed" );`.
- A 1.55 `if ( X )` followed by a `return` line whose false jump goes to the next statement is an if whose call was commented out: the return became the body.
- A call to a same-file function like `IsHeadless()` stays when its result is tested and is dropped when unused.
- A 1.55 `jmp` after an if/else plus a retail fall-through into the next case means a missing `break`; merged identical cases are still written out separately.
- Use `EXPORT` on the think and touch functions retail exports (`SawTouch`, `FakeDeath`, `ActivateSewmo`, `ActivateSpitter`, the `InactiveThink`s).
- `PRECACHE_SOUND( int )` compiles to a call to a helper (`STRING()` then `PRECACHE_SOUND`); `FClassnameIs( pEnt->pev, ... )` inlines as a strcmp on `pev->classname`.

## Patterns from the world entities

- A 1.55 `jmp` back to the loop increment is `continue`, not `break`.
- `CBaseAnimating::HandleAnimEvent` is an empty inline, so calls to it vanish; a switch with only `default:` that calls `CBaseMonster::HandleAnimEvent` compiles to a tail jump.
- If the retail push order looks swapped against 1.55, check which parameter is which (the first push is the last parameter), as in `GetDescriptionFromItem( "3dmdl", STRING( pev->message ) )`.
- A class that grew between builds shows in the retail save table count and the member offsets: `CCoFLadderManager::m_cAnimation` is `int [33]` in 1.6a (32 in 1.55).

## Patterns from the puzzles and doors

- Where retail compares with `jl` but 1.55 has `jb`, the source casts: `(int)strlen( szCode ) >= pev->iuser3`.
- MSVC 19.27 merges two adjacent `memset` calls on neighbouring arrays into one call: write both, as in 1.55.
- A null check written after a dereference (`if ( pSprite )`) is removed by the compiler: keep it in the source.
- Leave locals uninitialized where retail loads an arbitrary register on the default path of a switch.
- When `ObjectCaps` folds to a constant after the `m_pMoveWith` call, take the `|` and `&` order from 1.55 (`( CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE ) & ~FCAP_ACROSS_TRANSITION`).
- `FClassnameIs( edict, ... )` inlines with a `neg/sbb/and` null test on the edict; `FClassnameIs( pev, ... )` does not.
- Functions Ghidra merged into a neighbour have no row and no unit list entry: name them with `tools/bytematch.py --at <va,...>`.

## Patterns from the bosses

- 1.6a removed some debug alerts (the roof boss `ALERT( at_console, "ROOFBOSS - ..." )` calls): keep them as comments when retail has no `pfnAlertMessage` there.
- An if whose body was commented out still shapes the code: `if ( flDelta >= 0.4 && (...).Length() <= 300 ) { }` produces no code of its own, but leaving it in changes the surrounding register use.
- Spirit's monsters.h has `SF_MONSTER_NO_YELLOW_BLOBS` (128) in place of `SF_MONSTER_WAIT_FOR_SCRIPT`.
- 1.55 save tables are 16-byte `TYPEDESCRIPTION` entries: reading them from the 1.55 data gives field order and types.
- Unreferenced non-virtual methods (`CChainsawBoss::KillMonster`, `CRoofBoss::UpdateOnRemove`) are dropped from 1.6a by `/OPT:REF`; write them anyway.

## Patterns from the HLSDK leftovers (util, sound, subs)

- MSVC 19.27 compiles `flValue != 1.0` and `>= 1.0` on a float as ucomiss/comiss against a float constant, even where the 1.55 debug build shows a double compare. Write the constant as in the HLSDK; do not add an `f` suffix to force it.
- Retail `movzx reg, byte [param]; mov [param], reg` at the start of a function is an int local initialized from a bool parameter at the top: `int iValue = bValue;`.
- A full MESSAGE_BEGIN ... MESSAGE_END block written out in both branches of an if/else can match where one shared body after `if ( ... ) MESSAGE_BEGIN( ... ); else MESSAGE_BEGIN( ... );` gets a different register allocation. Try both.
- gEntvarsDescription has 102 entries in 1.6a: iuser1-4, fuser1-4, vuser1-4 and euser1-4 are appended. The count shows as `push 0x66` in ReadEntVars, WriteEntVars and EntvarsKeyvalue.
- The ScreenFade struct (engine/shake.h) is `float duration; float holdTime; int fadeFlags; byte r, g, b, a;` in 1.6a (16 bytes; 1.55 still has the HLSDK shorts).
- A function at 80-99% whose bytes are identical is usually a delinker extent problem, not source: check the retail bytes before editing. Tell the main agent instead of changing code.
## Patterns from effects.cpp

- When 1.6a code reads past the size of a 1.55 PDB class, the PDB type is stale: add the missing member with a `guess_` name.
- A method folded by /OPT:ICF with an identical method of another class (retail shows the other class's name) matches when written as its own method. Example: `CEnvModel::VecToYaw` appears in retail as `CBaseMonster::VecToYaw`.
- `ThinkSet` with the string `"CheckIfDead"` in the 1.55 debug build means `SetThink( CheckIfDead )` in the source.
- Vtable slot 0xd0 is `IsPlayer` and slot 0xbc is `IsAlive` in CBaseEntity.
- Edit source files with Python or the Edit tool: `sed -i` in Git Bash turns CRLF files into LF.
## Patterns from player.cpp

- A float subtraction compiled as two `cvtps2pd` and a `subsd` is a `(double)` cast in the source, for example `m_flTimeDelta = (double)m_flTime - m_fOldTime;` (from the HLSDK hud_redraw.cpp).
- `test eax,eax; je; sub eax,1; je; sub eax,1; jne` comes from `if ( x ) switch ( x ) { case 1: ... case 2: ... }`.
- When retail hoists a shared store above a branch and compares early, the source may duplicate the whole branch body, including the stores after the call, in both arms.
- Check the `msg_dest` value that is pushed: the ripple messages push 2 (`MSG_ALL`), not `MSG_PVS`.
- A filter that hides every retail `s_`/`DAT_` row can hide a wrong literal: check those literals by value against the retail bytes.
- `REG_USER_MSG( "ScreenFade", 10 )`: 1.6a registers the HLSDK size although the ScreenFade struct in engine/shake.h is 16 bytes there.
## Patterns from the HLSDK leftovers (second pass)

- Cry of Fear adds early returns at the top of base HLSDK functions, `if ( FClassnameIs( pev, "monster_spitter" ) ) return;`: look for them when a retail function is larger than the HLSDK one.  Look also for calls Cry of Fear dropped (`ResetEmptySound()` in a WeaponIdle).
- `IMPLEMENT_SAVERESTORE( CClass, CBaseEntity )` is common in Cry of Fear classes that derive from CBaseToggle or CBaseMonster: the retail Save/Restore call CBaseEntity::Save/Restore. The relocation check shows it.
- A local declared at function scope and one in a nested block cannot share stack space: when retail has a smaller frame, move the declaration into its block (`char szMessage[128];` in GiveAmmo).
- `-(float)sin( yaw )` (the cast before the negation) when retail negates a float (16-byte `xorps` mask).
- VC6 STL objects (DetailTexture.cpp: `std::string`, `std::map`, `std::vector`) are built without /GX but with /MT (unit cflags): only `_MT` makes `std::_Lockit` a real call. The whole COMDAT tail then comes out in retail order; the linker folded identical COMDATs (`string::size` into `string::length`, placement new), so skip those when pairing by position. Ghidra splits a function with a try/catch (VC6 `basic_string::_Copy`) at its catch funclet: remove the `Catch@` and continuation functions, extend the body and turn the jump it marked as a call back into a jump before delinking. File statics with an explicit `= false` lie in retail .bss after the uninitialized ones in definition order; `once`, `defaultID` and the lexer tables that the DWARF gives as const values or const arrays are plain initialized statics in 2013 (.data).

## Patterns from the hl.dll EQUIVALENT pass (MSVC 14.27)

- A callee in the same object whose body is empty (or a bare `return FALSE;` with the old code commented out) lets MSVC 14.27 drop calls to it and the code that only feeds them, even when it is defined after the caller: when retail keeps the calls, the callee keeps its old body after `return;` (CBaseMonster::CallGibMonster, seen in CBaseMonster::Killed).
- A `nop dword ptr [eax]` (or another multi-byte nop) at a block join that no jump comes back to is the aligned head of a loop whose body is gone: an empty `for ( int i = 0; i < 1; i++ ) {}` at that place gives the same padding (CSuicider::Killed, where the 1.55 gun setup used to be).
- Swapped operands of one component of a Vector sum or product (`movss xmm1, [view_ofs.x]` against `[origin.x]`) and swapped registers that follow from it change when a pointer or scalar of the expression is read through a local first: `float flSpeed = m_flFlySpeed;` (CRoofBossHead::TrackTarget), `entvars_t *pevStart = m_pPlayer->pev;` for one operand (CSyringe::SecondaryAttack), `CBasePlayer *pPlayer = m_pPlayer;` hoists the load above a branch (CShotgun::ItemPostFrame). The effect is not local: a new local can flip the operands of a far away expression in the same function, so check the whole function after each try (PostThink, CheatImpulseCommands stay EQUIVALENT for that reason).
- `try_variants.py` writes the whole file with CRLF when it finds any CRLF in it: keep the file's own line ending in every inserted line, or a LF file comes back converted.
- An object lists its external symbols in the order of their first declaration in the translation unit (not the order of use), and the linker pulls library members in that order: CoF_DualPistol.cpp, the first STL user, includes <fstream> before player.h (whose <string> declares `std::uncaught_exception` before the `_X*` helpers), so libcpmt xthrow.obj comes before uncaught_exception.obj as in retail. The same creation order places initialized .data (first declaration, also for static data members; file statics and folded dynamic initializers after all others), string literals (parse order), vtables (class completion) and RTTI type descriptors (first declaration of the class). A class whose save table must precede earlier data can therefore not keep its vtable in place (sound.cpp CSpeaker, docs/tooling.md).
- The base/index order of an inlined strcpy loop and the operand order of a commutative SSE add follow numbering over the whole function, not the statement: in ClientCommand (quickselset1/2) and ApplyCFGSettingToPlayer one of several identical blocks differs, and no local, cast, comparison form, scope or `sizeof` in place of `strlen` moves it. A named copy of a member (`Vector vecTurn = m_vTurnVector;` in PostThink) and swapped `origin`/`view_ofs` operands each flip some components and not others; check every sum of the function after each try.

## What not to do

- Do not edit shared headers (`cbase.h`, `weapons.h`, `player.h`, `util.h`, ...) without coordinating: several files depend on them.
- Do not rename retail symbols to make a function match. Retail names come from evidence (older builds, strings, vtables, exports) and are committed separately with the evidence (`tools/symbols/hl_manual.csv`).
- An empty then-branch with an else, `if ( !x ) {} else { ... }`, gives `jne` plus `jmp` instead of a single `je` (seen in BaseUI_Interface Initialize and FontTextureCache AllocatePageForChar).
- VGUI1 functions are `VGUIAPI` (dllexport), so calls into vgui.dll go through import thunks; declare them with the same attribute or the call form differs.
- The CoF `cl_enginefunc_t` is 0x21C bytes; only src/gameui/cdll_int.h declares it at that size, so units that need the full table include that header.
- VC6 orders a file's static .bss variables by a hash of their names, like stack slots: when a static table must land at a fixed offset, its name is part of the match (VGUI_EngineSurfaceWrap staticKeyTable_guess).

## Patterns from HtmlWindow.cpp (VC6 COM code)

- VC6 emits a static function right after its first caller, so a function that retail places before its first caller is a global one (InitKeyTable_guess and the window, hook and EnumChildWindows procedures of HtmlWindow.cpp).
- The COM interfaces of the old IE headers (`interface DECLSPEC_UUID`: IDocHostUIHandler, DWebBrowserEvents2, HTMLDocumentEvents) have no `novtable`, so their implicit constructors store a vtable; MIDL_INTERFACE ones give empty constructor COMDATs. Write the interface methods with the SDK signatures (STDMETHODCALLTYPE) and the whole COMDAT tail (FS_ constructors, forwarders, scalar deleting destructors, _bstr_t from comutil.h) comes out in retail order.
- `IsEqualIID` calls the C++ inline `?IsEqualGUID@@YAHABU_GUID@@0@Z` of objbase.h (one copy at 01D0E2C0); IIDs are uuid.lib externs (`_IID_IOleObject`), laid out in .rdata in the order of the object's external symbols.
- A call result stored into a stack slot before the other arguments are pushed is a named local (`const char *text_guess = tag_guess;` before `_stricmp`); a temporary bool kept across `_bstr_t` temporaries is the compiler's.
- `mov byte [x], 1; mov ecx, 1; test ecx, ecx; je` is an assignment used as a condition: `else if ( m_bOnLink = true )`.
- Local names without a source get `_guess` names picked by hash bucket (tools/vc6_slots.py); when two locals share a bucket, declare them at the top of the function in reverse slot order (CreateBrowser_guess).
- A chained assignment `qglX = dllX = glX;` stores right to left through one register; separate statements reload (opengl2d3d QGL_D3DInit).
- DirectDraw surface caps come from a `DDSURFACEDESC2` member (`ddsd.ddsCaps.dwCaps`), not a separate DDSCAPS2 local: the member offset is part of the match (opengl2d3d glTexImage2D).

## Patterns from the hw input, voice capture and helper objects

- A COM call that loads the vtable before pushing the object (`mov eax,[p]; mov ecx,[eax]; mov edx,[p]; push edx; call [ecx+X]`) is C++ `p->Method()`; the C macro form `p->lpVtbl->Method( p )` pushes `p` first. dinput_guess.cpp is therefore C++ with every function in `extern "C"` (its names are C, host.c and the cl_enginefuncs table use them).
- DirectInput 7 needs a newer dinput.h than VC6's DX5 one: refs/dxheaders/dinput.h (git-ignored) is the Windows Kits 10 copy, used with `#define DIRECTINPUT_VERSION 0x0700` and the unit include `refs/dxheaders`. `SPI_GETMOUSESPEED` is missing from the VC6 headers (112).
- VC6 spells file statics `_name` in C++ objects too (`_s_DemoPlayer`, `_g_LauncherDirectDraw_guess`); dxguid and dinput.lib data (`_GUID_SysMouse`, `_c_dfDIMouse2`, `_IID_IDirectSoundNotify`) and import thunks (`_DirectInputCreateA@16`) need rows in hw_manual.csv before the relocations match.
- On a call result (in eax), `f() != 0` gives neg/sbb/neg and `f() ? true : false` gives `test; setne` (DemoPlayer_Init uses the first, DemoPlayer_LoadGame the second), unlike the memory-operand rules above.
- A method retail calls directly although the class has virtuals is non-virtual (VoiceRecord_WaveIn::Init); `xor eax, eax` / `mov eax, 1` returns of a method mean BOOL, not bool.
- utils/procinfo/procinfo.cpp (refs/halflife) is linked into hw.dll: HasCpuid, HasTsc, GetRDTSC and PROC_GetSpeed byte for byte, PROC_IsMMX in a later `_asm pushad` + `__try` form, plus a 3DNow test after it. The `__try` bodies needed SetBodyRange.java before delinking.
- pe_win32_guess.c holds the stdcall helpers of the PEFILE.C sample (ImageFileType, NumOfSections, ImageDirectoryOffset, GetSectionNames, GetSectionHdrByName, IsDebugInfoStripped, `ret 4/8/12`); its local names (psh, nSections, ped, pNames, pCnt, nCnt) fit the VC6 slot hash unchanged.
- GameUI: retail often uses a `while` loop where the Source SDK code has `for`; the loop shape decides the match (OptionsSub*, dialogs).
- `iswalnum` and friends must come from ctype.h, not wctype.h: VC6 disables the C++ inline in ctype.h, so the call goes to the CRT like retail.
- A compiler-generated destructor does not store the vptr; a written (even empty) destructor does. Write one only when retail stores it.
- VC6 orders the COMDAT tail of an object (inline and template functions) in a way not reproduced yet (CDKeyEntryDialog, GameConsoleDialog): tails were named by content; objdiff ignores order, a full link will not.
- The Windows GameUI code sometimes differs from the Linux DWARF (an extra local such as enemyListStr, a 2-element intbuf): locals the DWARF lacks get `_guess` names fitted to the VC6 slot hash.
- tools/name_unit.py and align_names.py pair identical STL template copies and `_$E` initializers wrongly; name such tails from the call targets of callers that already match byte for byte (scratch helpers tailmatch.py, verify_lib.py, strcheck.py in the session scratchpad tools folder did this for the Career files).
- A panel class with multiple inheritance and a message map needs `#pragma pointers_to_members( full_generality, virtual_inheritance )` before it (vgui2 HTML.h): retail uses the general member-pointer form for its message map entries.

## Patterns from the small GameUI files, Socket_guess and bot_profile

- A class with a `double` member puts its first data member at +8 after the vptr (CSocketThread): no padding member is needed; a 4-byte gap after the vptr of a class without doubles is a real member (CMsgHandler).
- A static function defined before its first caller is emitted right after that caller (GetSocketThread_guess, SocketThreadFunc_guess); defined after it, it stays at its definition.
- The class-key seen first in a translation unit decides the decoration (`struct CPanelListPanel;` in ScriptObject.h gives `PAU`): every unit calling a function must see the same key first, or a full link fails (CreateMultiplayerGameGameplayPage includes ScriptObject.h first).
- A 100% function can still point at the wrong literal: tools/relocaudit.py caught `"Resource\\..."` versus `"Resource/..."` paths (CDKeyEntryDialog) and `vgui2::` versus `vgui::` base class names in DECLARE_CLASS_SIMPLE (CTaskFrame, CCreateMultiplayerGameServerPage, TestScriptController).
- Winsock calls go through import thunks (VC6 winsock.h has no dllimport); name the thunks in gameui_manual.csv (`_htons@4`, `_recvfrom@24`, ...). `#define FD_SETSIZE 1024` precedes windows.h in Socket_guess.cpp. GameUI has no engine heap: src/gameui/mem.h maps the shared common/ObjectList.cpp onto the HLTV Mem_ZeroMalloc and the CRT free.
- In this shell, `/D` and `/I` arguments passed to a Python wrapper are rewritten as MSYS paths (use `-D`, `-I`), and heredocs collapse a double backslash to a single one (write sources with backslashes through the editor); Python text-mode writes produce CRLF (open files in binary mode to keep LF).

## Patterns from CareerGameTasks.cpp and the GameUI leftovers

- VC6 <string.h> has C++ inline overloads (`wcsstr( wchar_t *, const wchar_t * )`, `wcschr`, `wcsrchr`, `wcspbrk`): a call with a non-const first argument goes through a 21-byte COMDAT `?wcsstr@@YAPAGPAGPBG@Z` that calls `_wcsstr`, emitted in the tail of the first object that uses it.
- `searchStr[2] = (char)stringNum + '0';` gives `movsx eax, byte [stringNum]; add eax, 0x30`; without the cast it is a plain dword load.
- A template COMDAT that exists twice with identical bytes has two names: the linker keeps one copy per name, in the first object of the link order that defines it. The CGameUI handle at +0x140 is therefore not `DHANDLE<Frame>` (DemoPlayerDialog.cpp comes first) but the `DHANDLE<CCDKeyEntryDialog>` of the emptied ValidateCDKey; a member that is never read can still be typed by such evidence.
- Resource paths of LoadControlSettings use either slash (`"Resource/OptionsSubKeyboard.res"`): check each one with relocaudit, objdiff counts the function at 100% either way.
