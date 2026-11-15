
from pathlib import Path
import argparse
from pxr import Usd, UsdGeom, UsdLux, Gf

def stage_data(path, expected):
    stage = Usd.Stage.Open(str(path.resolve()))
    assert stage, path
    assert UsdGeom.GetStageUpAxis(stage) == expected
    transforms = {}
    geometry = {}
    light_paths = set()
    camera = None
    time = Usd.TimeCode(stage.GetStartTimeCode())
    for prim in stage.Traverse():
        name = str(prim.GetPath())
        if prim.IsA(UsdGeom.Camera):
            camera = UsdGeom.Camera(prim).ComputeLocalToWorldTransform(time)
            assert abs(camera.GetDeterminant3() - 1) < 1e-5
        if prim.HasAPI(UsdLux.LightAPI):
            light_paths.add(name)
            transforms[name] = UsdGeom.Xformable(prim).ComputeLocalToWorldTransform(time)
        if prim.IsA(UsdGeom.Mesh) and name.startswith('/RootNode/instances/'):
            mesh = UsdGeom.Mesh(prim)
            transforms[name] = mesh.ComputeLocalToWorldTransform(time)
            geometry[name] = (mesh.GetPointsAttr().Get(time), mesh.GetNormalsAttr().Get(time),
                              mesh.GetFaceVertexIndicesAttr().Get(time))
    assert camera is not None and geometry and light_paths
    assert camera.GetRow3(1)[2] > .99, camera
    return stage, camera, transforms, geometry, light_paths

def main():
    parser = argparse.ArgumentParser(description='Compare captures of the same stationary Z-up scene.')
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    args = parser.parse_args()
    old, old_camera, old_transforms, old_geo, old_lights = stage_data(args.before, 'Y')
    new, new_camera, new_transforms, new_geo, new_lights = stage_data(args.after, 'Z')
    assert not old.HasAuthoredMetadata('upAxis')
    assert new.HasAuthoredMetadata('upAxis')
    assert Gf.IsClose(old_camera, new_camera, 1e-4)
    shared = old_geo.keys() & new_geo.keys()
    fixed = [name for name in shared if old_geo[name] == new_geo[name] and
             Gf.IsClose(old_transforms[name], new_transforms[name], 1e-4)]
    assert fixed, 'No shared unchanged mesh instances'
    lights = old_lights & new_lights
    assert lights, 'No shared light hashes'
    assert all(Gf.IsClose(old_transforms[name], new_transforms[name], 1e-4) for name in lights)
    print(f'PASS: authored Z-up, camera pose, {len(fixed)} unchanged mesh instances and {len(lights)} light placements')

if __name__ == '__main__':
    main()

