# RTX Remix Toolkit map lights

Static BSP lights normally use the Remix API. Moving and directional sources
use D3D9. In the previous runtime, only D3D9 sources consulted Toolkit light
replacement buckets. A Toolkit deletion represented by a zero-intensity USD
override could therefore leave the original API light emitting.

Use the patched 64-bit Remix Plus runtime in the bridge server's
`.trex/d3d9.dll`. The source patch
[remix-plus-toolkit-lights.patch](remix-plus-toolkit-lights.patch) applies to
Remix Plus commit `35a98479fd1a1744193e2998251c13407061788a`. Apply it with
`git apply <patch>` and build with `scripts/build.ps1`. The local runtime
checkout already includes API capture through the prepared frame's light list.
The patch for a clean checkout adds API sphere and distant lights to the
upstream capture code. No API ABI or bridge rebuild is required.

The runtime looks up the original light using its captured `RtLight` hash
each frame. An empty bucket, inactive light prim, or zero-intensity override
suppresses the original. A replacement bucket supplies the lights for that
source until the mod reloads or is disabled. An intensity-only override inherits
the source's position, radius, color and shaping. The handle used to submit the
light can change independently of its capture hash.

Keep `dx_remix_map_light_api 1` for API captures. Use
`dx_remix_map_light_api 0` for a D3D9 capture or a D3D9-authored mod. The two
routes can give the same BSP light different hashes. If a mod's hashes are
unknown, compare its `/RootNode/lights/light_<HASH>` prims to a fresh capture.
Changing an emitter's position, radius or cone can change its hash. In
particular, a moving D3D9 light can change hash as its parent moves. An edit
bound to one captured position has no stable per-entity binding through that
motion.

`dx_listlights` reports the BSP entity index, position and Xash submission
route. For the actual capture hash and replacement decision, start with
`DXVK_LOG_LEVEL=debug` and look for `[ExternalLight]` or `[GameLight]` in
`rtx-remix/logs/remix-dxvk.log`. Match by emitter position. `unmatched`
indicates the runtime selected the original submission. `replaced` and
`suppressed` describe the replacement decision before GPU light packing.
Set `rtx.log.writeAllRepeats = True` temporarily when tracing consecutive
saves. This setting is noisy.

## Verification

- Windows x86 Waf build passed for `xash`, `ref_d3d9`, `verify_camera`
  and `verify_map_light_routes`. Both executables passed. The Remix Plus
  x64 build and `test_hot_reload` passed.
- In `c_attic`, the previously installed runtime exported 15 positioned
  D3D9 lights and no positioned API lights. The rebuilt runtime exported
  all 15 API lights. A restart with a different map light output scale
  preserved all 15 API hashes. The API and D3D9 captures used different
  hashes at all 15 shared positions.
- For the light at `(-1332, -487, 346)`, API hash `65CC68BBBCFCAAEC` and
  D3D9 hash `6D91A8FA3CD40CF8`, the in-game runtime selected suppression,
  the restored original, and the replacement after successive USD saves.
  An inactive API prim also suppressed the source. Editing a different API
  light restored the target and suppressed only the newly edited source.
- The live test used a disposable USD fixture with the Toolkit deletion
  representation. The Toolkit UI, an existing user mod, a moving parent
  in-game, and frame-by-frame illumination were not verified. Direct
  writes to the fixture reloaded reliably. One rapid sequence of
  atomic-renamed fixture writes did not reload the second save.

To compare actual captures with OpenUSD Python installed, run
`verify_remix_light_capture.py <d3d9.usd> <api.usd> --repeat <second-api.usd>`.
The optional `--mod <new-mod.usda>` writes a test fixture for both routes.
