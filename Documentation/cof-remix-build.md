# Current Cry of Fear Remix build

The co-op flashlight fix is already part of `master` through commit `d1f152d9`.
It decodes the nine-byte CoF `ScoreInfo` equipment flags and uses bit `0x20` for
the persistent per-player vest flashlight. `FlashFlags`/`EF_DIMLIGHT` alone do not
cover this multiplayer protocol. See `common/cof_player_lights.h`,
`engine/client/parse/cl_parse.c` and `ref/d3d9/verify_camera.cpp`.

Updating the renderer alone is insufficient: install both the matching
`xash.dll` and `ref_d3d9.dll`. Unmodified official engine binaries do not contain
this CoF-specific network fix.

The current source also includes the Audi replacement/glass, volumetric weapon
smoke, lighting/culling work, and the complete c_lake renderer integration:
5,595 reed clumps with gentle wind/LODs, 1,652 forest trees, supplied undergrowth
and shrubs, native-model replacement, walkway clearance and the removed floating
trees. The rejected black vegetation barrier is absent. An opaque horizontal
sediment floor corrects the original underwater sky surface.

## Update the engine and renderer

This repository update contains source code and tests only. Build the Windows
32-bit engine and D3D9 renderer, close the game, then copy `build/engine/xash.dll`
and `build/ref/d3d9/ref_d3d9.dll` into your existing game directory. Both files
are needed for the co-op flashlight fix. Keep the original client DLLs and the
existing RTX Remix bridge; no new cl_dlls or game assets are required for it.

Custom lake/Audi/PBR assets are deliberately not uploaded with this change.
Those optional replacements require their separate local resources. Missing
resources retain native rendering; the flashlight fix is independent of them.

## Build and verify source

Build the 32-bit Windows engine and D3D9 renderer with Visual Studio, SDL2 and
the DirectX SDK June 2010, using this fork's D3D9 Waf configuration and
`--verify-camera`. Build `xash`, `ref_d3d9`, `verify_camera`, `verify_lake_reeds`,
`verify_lake_forest` and `verify_lake_bed`. The tests use the matching renderer
PDB; keep it beside its build output. See the main README for general engine
prerequisites.

From an x86 Visual Studio developer prompt, with your own SDK paths:

```powershell
git submodule update --init --recursive
python waf configure --check-c-compiler=msvc --check-cxx-compiler=msvc --enable-d3d9 --verify-camera --sdl2=C:/SDK/SDL2 --dxsdk=C:/SDK/DXSDK --enable-bundled-deps --disable-mbedtls --disable-werror --build-type=release
python waf build --targets=xash,ref_d3d9,verify_camera -j8
```

`verify_camera` exercises the real compiled flashlight path, including co-op
ScoreInfo bytes, persistent on/off, independent players/non-host, malformed
messages, camera/death suppression and map reset. Lake tests check real asset
loading, materials, wind, LODs, native suppression, other maps and failure cleanup.

No binaries, models, textures, maps, asset archives, Ghidra installation,
personal diagnostics, saves, credentials or shader caches are part of this update.

## Automatic blend emission and Toolkit materials

`dx_remix_auto_emissive` is an archived D3D9 renderer variable. Its default
value of `0` disables Remix's automatic emissive blend override. Set it to
`1` in the game console to restore automatic conversion. The renderer submits
the setting during gameplay and replays it after device reset. Original
additive draw states are preserved.

Legacy materials use the runtime defaults of `rtx.legacyMaterial.enableEmissive = False`
and `rtx.legacyMaterial.emissiveIntensity = 0`. The renderer leaves both settings
under the user's control. The global `rtx.emissiveIntensity` multiplier also
remains under runtime control.

1. Open a capture in the Toolkit and select the target mesh in **Asset Replacements**.
   In **Material Properties**, select or create an override for the material bound to that mesh.
2. In the material's **Emissive** section, enable emission, set the intensity,
   and assign an ingested **Emissive Mask** texture. Use an intensity of zero
   when the material should produce no light.
3. Save the replacement layer, reload it in Remix, and check the surface and
   nearby illumination. Restart and confirm that the exported mod still loads
   the same binding, texture and intensity.
4. If the controls are missing or the replacement ignores the settings, check
   the bound USD shader and its MDL material type, then check how the installed
   Remix Plus runtime loads that replacement.

[NVIDIA RTX Remix material replacement guide](https://docs.omniverse.nvidia.com/kit/docs/rtx_remix/latest/docs/howto/learning-materials.html)

Build `verify_remix_auto_emissive` alongside `ref_d3d9` and run the resulting
verification executable from the build directory's `ref/d3d9` subdirectory.
It uses a mocked config sink and requires no game or GPU. For a visual A/B,
use the same viewpoint and original additive surface with values `0` and `1`
and compare the light cast on nearby surfaces.
