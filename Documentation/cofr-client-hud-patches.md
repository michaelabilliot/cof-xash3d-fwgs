# Cry of Fear Remixed HUD ammo counter fixes

The Remixed HUD (health, stamina and ammo counters) is drawn by the
closed-source `cofr13_client.dll` of the CoFR 0.1.4.2 package, not by this
engine. These fixes are therefore byte patches to the installed DLL, applied
by `scripts/cofr/patch_client_hud.py`. No game DLL is stored in this repository.

## What is fixed

- **Flashlight showed an ammo counter.** On the server, `weapon_flashlight`
  (weapon id 1) declares a `flashlight` battery ammo type and a clip of 100,
  so the HUD treated it like a firearm. Id 1 now never gets a counter. This
  covers the Old Flashlight (same class) and a flashlight held while dual
  wielding.
- **Dual-wield counters sometimes disappeared.** They were drawn only while
  the native overlay mode (`0x104a8218`) was 3. HeadShield overlays (gas mask,
  death, telescope, book menu) and the level-load reset change that mode. The
  counters are now drawn whenever `weapon_dualweapon` (id 15) is held.
- **Dual-wield counters showed the other hand's ammo.** The first `DualAmmo`
  value belongs to the secondary weapon id (`0x104a82dc`) and the second to the
  primary id (`0x104a8214`), as in the native `CHudAmmo::Draw`. The Remixed HUD
  had them swapped. A pistol paired with a melee weapon or light got the
  other item's -1 and showed nothing, and two pistols each showed the other
  gun's magazine.

## Apply

Close the game, then run (Python 3, no extra packages):

```
python scripts/cofr/patch_client_hud.py "<game>/CryOfFear/cl_dlls"
```

Add `--check` to report the state without writing. The script:

- checks that `client.dll` pins the current `cofr13_client.dll`;
- checks every patch site holds either the original or the patched bytes;
- checks the result matches the known patched SHA-256 before writing anything.

`client.dll` refuses to load a `cofr13_client.dll` whose SHA-256 differs from
the ASCII hash it stores at file offset `0x3b40`, so the script rewrites that
hash too. The originals are kept beside them as `*.before-hud-fix.dll`; copy
them back to undo. Running CoFRSetup again also restores the shipped files.

## Patch sites

| VA in `cofr13_client.dll` | Original | Patched | Effect |
|---|---|---|---|
| `0x1000ae17` | `lea eax,[edx-1]; cmp eax,0xfe` | `lea eax,[edx-2]; cmp eax,0xfd` | valid ammo ids start at 2 |
| `0x1000ee7f` | `jne 0x1000eeea` | `nop; nop` | dual path no longer needs mode 3 |
| `0x1000ee8b` | `push [ebp+0x111394]` | `push [ebp+0x111398]` | primary uses `DualAmmo` value 2 |
| `0x1000eeaa` | `push [ebp+0x111398]` | `push [ebp+0x111394]` | secondary uses `DualAmmo` value 1 |

Patched `cofr13_client.dll` SHA-256:
`bcab93b54f5bc2286e6ad98a8502c0576a98f5004bcb8d0003612a710e943c21`.
