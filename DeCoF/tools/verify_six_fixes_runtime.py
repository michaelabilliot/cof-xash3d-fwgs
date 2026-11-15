"""Check the log from verify_six_fixes_runtime.cfg.

Copy the cfg to the game directory under CryOfFear and launch the local x86 game
with -dev 1 -log <log> +exec verify_six_fixes_runtime.cfg. The fixture writes
two named test saves. Pass the resulting engine log to this script.
"""
import argparse
from pathlib import Path
import re


def validate(log):
    sections = {}
    for marker, block in re.findall(
        r"^SIX_(\w+)[ \t]*\n(.*?)(?=^SIX_\w+|\Z)", log, re.M | re.S
    ):
        sections[marker] = block

    def expect(stage, pattern):
        if not re.search(pattern, sections.get(stage, ""), re.M):
            raise AssertionError(f"{stage}: missing {pattern!r}")

    expect("NOTARGET_ON", r"COFR capacity=8 notarget=1")
    expect("NOTARGET_CAMERA", r"COFR capacity=8 notarget=1")
    expect("NOTARGET_OFF", r"COFR capacity=8 notarget=0")
    expect("RESTORED", r"COFR capacity=8 notarget=0")

    for weapon in ("mp5", "tmp"):
        tag = weapon.upper()
        expect(f"{tag}_READY", rf"active=weapon_{weapon} .*clip=30")
        expect(f"{tag}_FIRED", rf"active=weapon_{weapon} .*clip=[12]\d")
        expect("RESTORED", rf"COFR slot [12] .*class=weapon_{weapon}$")

    expect("NATIVE_AMMO", r"COFR ammo mp5=300 tmp=30")
    expect("UNREDUCED", r"COFR capacity=8")
    expect("UNREDUCED", r"COFR slot 8 .*class=weapon_tmp$")
    for stage in ("REDUCED", "REDUCED_RESTORED"):
        expect(stage, r"COFR capacity=4")
        expect(stage, r"COFR inventory visible=\d capacity=4")
        expect(stage, r"COFR slot 1 .*class=weapon_mp5$")
        for slot in range(5, 9):
            expect(stage, rf"COFR slot {slot} script= model= class=$")
        if "class=weapon_tmp" in sections[stage]:
            raise AssertionError(f"{stage}: removed TMP still in the inventory")
    expect("REDUCED", r"Invalid destination slot")
    if "RUNTIME_END" not in sections:
        raise AssertionError("incomplete runtime fixture")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    validate(args.log.read_text(errors="replace"))
    print("Passed campaign inventory, notarget, MP5/TMP, and native ammunition checks.")


if __name__ == "__main__":
    main()
