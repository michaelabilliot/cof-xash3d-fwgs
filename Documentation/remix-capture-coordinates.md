# RTX Remix capture coordinates

The public capture layer used to omit stage metrics. Its scene sublayer
authored Z-up, but opening the public layer returned the USD Y-up fallback.
Toolkit StageCraft copies that value to the project stage, where it controls
the viewport grid and orientation. The exporter now authors the up axis,
scale, time rate and playback range on the public layer.

Xash world coordinates are already right-handed and Z-up. The renderer uses
a right-handed look-at, multiplies by a reflection on view-space Z, and
projects with positive view Z forward. The resulting view is reflected, but
the two convention changes cancel for world orientation. USD therefore
preserves the Xash world axes and camera pose with an identity world basis.
USD cameras still look along their local -Z and use local +Y for up.

Remix now derives the world basis from the capture camera's view and
projection. Camera, instance, standalone mesh and analytic light export
use that basis. Triangle winding uses its determinant, and skeletal poses
remain in instance-local space. The configured runtime handedness is logged
but is independent of the capture basis. The old option-based skip could
make lights use a different basis from their geometry.

No configuration changes are required for the tested Z-up configuration.
The fix runs at capture time. It does not alter live D3D9 matrices or
replacement hashes. For an older capture, recapture or repair the stage
metrics on the public capture and re-import it into the Toolkit project.
A project's previously authored Y-up metadata does not update by itself.

The [Remix Plus patch](remix-plus-capture-coordinates.patch) applies to
`35a98479fd1a1744193e2998251c13407061788a`. It includes the local capture
publication and validation changes that introduce the public layer, plus
the previously installed Toolkit light replacement fix. Apply it to a
clean checkout of that commit and run `scripts/build.ps1`. For the local
runtime checkout, build the existing source directly. Install the x64
`d3d9.dll` under the development game's `.trex` folder.

## Verification

- Windows x86 engine and renderer build, `verify_camera`, and
  `verify_map_light_routes` passed. The x64 Remix Plus build and
  `test_hot_reload` passed, including the synthetic capture/export test
  and the previously added Toolkit light override test.
- Real `c_attic` captures logged view determinant -1, projection
  determinant +4.00195, camera up +Z, right -Y, forward +X and identity
  world basis. The public USD stage authored Z-up on both API and D3D9
  map-light routes. All 15 positioned lights retained their expected
  route-specific hashes and shared positions.
- The same-route before/after comparison verified identical camera pose,
  mesh positions, normal buffers and index buffers for 58 matching mesh
  instances and identical transforms for 16 matching light prims.
- Toolkit StageCraft 1.5.2 ran its normal capture importer headlessly.
  Its composed project stage was Y-up for the old capture and Z-up for
  the fixed capture, with camera up +Z in both. The viewport was not
  visually inspected. Animated skinning and moving lights were not
  separately exercised in a real capture.
