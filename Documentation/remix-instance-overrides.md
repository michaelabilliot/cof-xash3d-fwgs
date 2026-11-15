# Per-occurrence replacements (version 1)

The runtime can address a static occurrence without changing its shared mesh
hash. Xash publishes the current BSP name through Remix's config API. The
address combines that map name, the original mesh asset hash and the original
object-to-world affine matrix. Translation is rounded to 1/64 game unit and
the basis to 1/4096. Runtime instance numbers and USD prim sequence numbers
are excluded from the address.

New captures write `remix:occurrenceVersion`, `remix:occurrenceId`,
`remix:map`, `remix:originalMesh` and `remix:originalPlacement` on addressable
instance prims. The original placement is in runtime coordinates. The capture
prim keeps its normal USD placement and continues referencing the shared mesh.

A packaged mod stores an override under
`/RootNode/instanceOverrides_v1/instance_<16-digit ID>`. The root Xform
specifies the local translation, rotation and scale applied after the source
placement. Child meshes or USD references supply replacement geometry.
Standard material bindings on replacement mesh prims supply materials.
Child USD lights are local attachments. With no replacement geometry, the
original draw remains visible. Set `remix:preserveOriginal` to override that
default, `remix:inheritShared` to layer the edit on top of a shared replacement,
or `remix:visible = false` to suppress the occurrence. Delete the override
prim to resume shared or original behavior. For a mod conflict, the existing
mod load order chooses the highest-priority occurrence bucket.

## Authoring

Capture the map with the updated Xash renderer and Remix runtime. Select the
object in Toolkit and copy its instance prim path. With OpenUSD Python
available, run:

```powershell
python scripts/cofr/author_instance_override.py capture.usd /RootNode/instances/inst_example mod.usda --model assets/bin.usd --translation 0 0 8 --rotation 0 0 45
python scripts/cofr/author_instance_override.py capture.usd /RootNode/instances/inst_bulb mod.usda --inherit --light 0 0 8 1 150 1 0.8 0.6
```

The script reads the capture address for you. Put the mod and its referenced
assets under `rtx-remix/mods/<name>/`. Reopen the package with a USD editor to
edit its authored transforms and lights. Run the same command with `--remove`
to remove an override. Toolkit's built-in packaging UI does not provide the
instance/shared selector or author this section.

Enable **Log Instance Overrides** in the runtime Enhancements menu to inspect
applied addresses and light counts in the Remix log. Repeated logs are limited
by the runtime logger.

## Scope

This version targets static D3D9 draws. A combined world or brush batch is a
single source draw. The current bridge does not publish persistent entity or
surface IDs, so moving or animated objects and independently targeted GPU
placements are outside this version's addressing contract. Overlapping
addresses can be rejected when both draws are visible to the capture or
runtime. Draws already coalesced by the frontend cannot be distinguished.

Build the x86 renderer using the normal Waf setup. Build Remix Plus with
`scripts/build.ps1` and install its x64 runtime under `.trex`. Use a fresh
Meson directory after moving the runtime checkout.

## Verification

The Windows x86 engine and renderer build and map-light routing test passed.
The Windows x64 runtime build and `test_hot_reload` passed, including
occurrence table replacement/deletion, Z-up capture, and direct-light override
coverage. `verify_instance_authoring.py` passed with OpenUSD Python.

A real `c_attic` capture exported 46 addressable draws with Z-up metadata.
After restarting the game, the selected draw matched the captured address.
Live runtime logs recorded an 8-unit offset and one light, then a hot-reloaded
16-unit offset and one light. A later reload loaded a referenced mesh with a
23-unit offset and no attached light. Removing the edit triggered a reload.
This test used one real draw, not a pair of matching trash bins.

The baseline and edited runs both reported 57 direct retained instances and
zero legacy instances during steady frames. These counters do not establish
CPU, frame-time, memory, or BLAS cost. No controlled performance comparison
or separate culling, movement, and GPU-placement acceptance runs were made.

The [combined Remix Plus patch](remix-plus-instance-overrides.patch) applies
to `35a98479fd1a1744193e2998251c13407061788a`. It includes the earlier local
capture and direct-light changes needed to reproduce the installed runtime.

## Toolkit panel

Launch the installed RTX Remix Toolkit with
`CoFR-Tools/Start-RemixToolkit.bat`. The **Cry of Fear Instance Override**
panel opens automatically. Open a project with a writable replacement layer,
then select a capture occurrence or any mesh under it. Use the panel to
save a model reference, local transform, visibility, or attached light.
For a full replacement model, the panel disables shared inheritance when
saving. **Remove override** removes only the selected occurrence's authored
prim from the replacement layer.

The extension does not modify the Toolkit installation. The launcher
registers its extension directory through Kit's `--ext-folder` option and
enables it with `--enable`. The command-line USD authoring tool is still
available.

A headless test ran in the installed Toolkit StageCraft 1.5.2. It loaded
the extension, resolved a nested selection, saved independent transforms
for two synthetic same-hash occurrences, attached one light, and removed
one override without removing the other. Native Toolkit packaging and a
two-object live game test were not verified in this pass. A fresh live
`c_start2` capture exported 396 addressable draws, but no two exported draws
shared their original mesh hash in that capture.

The no-override `c_start2` run reported 832 direct retained instances and
zero legacy instances. Over one 30-second window it reported frame-time
p50/p95/p99 of 7.6/20.3/24.1 ms with frame generation enabled, at
320x180 internal resolution and 960x540 output. This is a single baseline,
not a before/after benchmark.
