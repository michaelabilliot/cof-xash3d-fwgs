
"""Create/update one Remix occurrence override from a selected capture prim path."""
import argparse
import os
from pathlib import Path

from pxr import Gf, Sdf, Usd, UsdGeom, UsdLux


def author(capture, selected, output, *, model=None, translation=None, rotation=None,
           scale=None, light=None, inherit=False, hide=False, remove=False):
    source = Usd.Stage.Open(str(Path(capture).resolve()))
    prim = source.GetPrimAtPath(selected)
    while prim and not prim.GetAttribute("remix:occurrenceId"):
        prim = prim.GetParent()
    if not prim or prim.GetAttribute("remix:occurrenceVersion").Get() != 1:
        raise ValueError("Select an addressable instance in a version 1 capture.")
    key = prim.GetAttribute("remix:occurrenceId").Get()
    if not key:
        raise ValueError("The selected occurrence has no unique address.")

    output = Path(output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Usd.Stage.Open(str(output)) if output.exists() else Usd.Stage.CreateNew(str(output))
    root = UsdGeom.Xform.Define(stage, "/RootNode")
    stage.SetDefaultPrim(root.GetPrim())
    UsdGeom.SetStageUpAxis(stage, UsdGeom.GetStageUpAxis(source))
    UsdGeom.SetStageMetersPerUnit(stage, UsdGeom.GetStageMetersPerUnit(source))
    parent = "/RootNode/instanceOverrides_v1"
    UsdGeom.Scope.Define(stage, parent)
    path = f"{parent}/instance_{key:016X}"
    stage.RemovePrim(path)
    if not remove:
        xform = UsdGeom.Xform.Define(stage, path)
        target = xform.GetPrim()
        target.CreateAttribute("remix:visible", Sdf.ValueTypeNames.Bool).Set(not hide)
        target.CreateAttribute("remix:inheritShared", Sdf.ValueTypeNames.Bool).Set(inherit)
        if translation is not None:
            xform.AddTranslateOp().Set(Gf.Vec3d(*translation))
        if rotation is not None:
            xform.AddRotateXYZOp().Set(Gf.Vec3f(*rotation))
        if scale is not None:
            xform.AddScaleOp().Set(Gf.Vec3f(*scale))
        if model:
            asset = Path(model).resolve(strict=True)
            relative = os.path.relpath(asset, output.parent).replace(os.sep, "/")
            UsdGeom.Xform.Define(stage, path + "/Model").GetPrim().GetReferences().AddReference(relative)
        if light:
            lamp = UsdLux.SphereLight.Define(stage, path + "/Light")
            lamp.AddTranslateOp().Set(Gf.Vec3d(*light[:3]))
            lamp.CreateRadiusAttr(light[3])
            lamp.CreateIntensityAttr(light[4])
            lamp.CreateColorAttr(Gf.Vec3f(*light[5:8]))
            if len(light) == 11:
                lamp.AddRotateXYZOp().Set(Gf.Vec3f(*light[8:]))
                UsdLux.ShapingAPI.Apply(lamp.GetPrim()).CreateShapingConeAngleAttr(45.0)
    if not stage.GetRootLayer().Save():
        raise OSError(f"Could not save {output}")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture")
    parser.add_argument("selected", help="USD prim path copied from Toolkit's Stage Manager")
    parser.add_argument("output", help="Packaged mod.usda path")
    parser.add_argument("--model", help="USD replacement asset with a default prim")
    parser.add_argument("--translation", type=float, nargs=3)
    parser.add_argument("--rotation", type=float, nargs=3)
    parser.add_argument("--scale", type=float, nargs=3)
    parser.add_argument("--light", type=float, nargs="+", metavar="N",
                        help="X Y Z RADIUS INTENSITY R G B [PITCH YAW ROLL]")
    parser.add_argument("--inherit", action="store_true")
    parser.add_argument("--hide", action="store_true")
    parser.add_argument("--remove", action="store_true")
    args = parser.parse_args()
    if args.light and len(args.light) not in (8, 11):
        parser.error("--light needs 8 or 11 numbers")
    print(author(**vars(args)))


if __name__ == "__main__":
    main()

