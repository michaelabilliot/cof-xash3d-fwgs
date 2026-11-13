"""Compare real Remix capture stages and generate a disposable regression mod."""
import argparse
from pathlib import Path

from pxr import Usd, UsdGeom, UsdLux


def lights(path):
    stage = Usd.Stage.Open(str(path.resolve()))
    found = {}
    for prim in stage.Traverse():
        if not prim.HasAPI(UsdLux.LightAPI):
            continue
        transform = UsdGeom.Xformable(prim).ComputeLocalToWorldTransform(Usd.TimeCode.Default())
        position = tuple(round(value, 3) for value in transform.ExtractTranslation())
        if any(position):
            found[position] = prim.GetName()
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("d3d9", type=Path)
    parser.add_argument("api", type=Path)
    parser.add_argument("--repeat", type=Path, help="API capture after restart or brightness change")
    parser.add_argument("--mod", type=Path, help="Path to a new disposable mod.usda")
    args = parser.parse_args()

    d3d9, api = lights(args.d3d9), lights(args.api)
    shared = sorted(d3d9.keys() & api.keys())
    assert shared, "No positioned lights in common between the two captures"
    distinct = [position for position in shared if d3d9[position] != api[position]]
    assert distinct, "Expected a source with distinct API and D3D9 hashes"
    if args.repeat:
        repeated = lights(args.repeat)
        assert api == repeated, "API capture hashes changed across the test runs"
    target = distinct[0]
    print(f"{len(shared)} shared positions, {len(distinct)} route-specific hashes")
    print(f"Selected source at {target}, D3D9 {d3d9[target]}, API {api[target]}")

    if args.mod:
        args.mod.parent.mkdir(parents=True, exist_ok=True)
        with args.mod.open("x", encoding="utf-8") as file:
            file.write(
                "#usda 1.0\n(defaultPrim = \"RootNode\")\n"
                "def Xform \"RootNode\" {\n    def Scope \"lights\" {\n"
                f"        over \"{api[target]}\" {{\n            float inputs:intensity = 0\n        }}\n"
                f"        over \"{d3d9[target]}\" {{\n            float inputs:intensity = 0\n        }}\n"
                "    }\n}\n"
            )


if __name__ == "__main__":
    main()
