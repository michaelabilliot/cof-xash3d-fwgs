
from pathlib import Path
import tempfile
from pxr import Usd, UsdGeom, Sdf
from author_instance_override import author
with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    capture = folder / "capture.usda"
    stage = Usd.Stage.CreateNew(str(capture))
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.z)
    for name, key in [('binA', 15), ('binB', 16)]:
        p = UsdGeom.Xform.Define(stage, '/RootNode/instances/' + name).GetPrim()
        p.CreateAttribute('remix:occurrenceVersion', Sdf.ValueTypeNames.Int).Set(1)
        p.CreateAttribute('remix:occurrenceId', Sdf.ValueTypeNames.UInt64).Set(key)
    stage.GetRootLayer().Save()
    mod = folder / "mod.usda"
    a = author(capture, '/RootNode/instances/binA', mod, translation=(1, 2, 3),
               light=(0, 0, 5, 1, 100, 1, 0.5, 0), inherit=True)
    b = author(capture, '/RootNode/instances/binB', mod, scale=(2, 2, 2))
    result = Usd.Stage.Open(str(mod))
    assert result.GetPrimAtPath(a + '/Light')
    assert not result.GetPrimAtPath(b + '/Light')
    assert UsdGeom.GetStageUpAxis(result) == 'Z'
    assert tuple(UsdGeom.Xformable(result.GetPrimAtPath(a)).GetLocalTransformation().ExtractTranslation()) == (1, 2, 3)
    author(capture, '/RootNode/instances/binA', mod, remove=True)
    result.GetRootLayer().Reload()
    assert not result.GetPrimAtPath(a)
    assert result.GetPrimAtPath(b)
    print("PASS: select, author, reopen, light isolation, independent transforms and delete")

