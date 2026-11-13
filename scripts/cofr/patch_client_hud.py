#!/usr/bin/env python3
"""Fix the Cry of Fear Remixed HUD ammo counters in an installed game.

The Remixed HUD is drawn by the closed-source cofr13_client.dll, so these
fixes are byte patches to that DLL. client.dll (the loader proxy) pins the
SHA-256 of cofr13_client.dll and refuses to start on a mismatch, so the
pinned hash is rewritten as well.

Patches (virtual addresses are for the 0.1.4.2 package):
  1. 0x1000ae17  ammo state: treat weapon id 1 (weapon_flashlight) as having
     no ammo. Its server item info declares a "flashlight" battery ammo type
     and a 100 clip, which made the HUD draw a counter for it.
  2. 0x1000ee7f  HUD state: use the dual-wield counters whenever
     weapon_dualweapon (id 15) is held, instead of only while the native
     overlay mode 0x104a8218 equals 3. HeadShield overlays (gas mask, death,
     telescope, book menu) and the VidInit reset change that mode, which
     hid both counters until the dual weapons were set up again.
  3. 0x1000ee8b / 0x1000eeaa  pair the DualAmmo values with the right
     weapons: value 1 belongs to the secondary id (0x104a82dc) and value 2
     to the primary id (0x104a8214), as in the native CHudAmmo::Draw. They
     were swapped, so each counter showed the other hand's ammo, and a
     pistol paired with a melee weapon or light got no counter at all.

Usage:
  python patch_client_hud.py "<game>/CryOfFear/cl_dlls" [--check]

Close the game first. Originals are kept as *.before-hud-fix.dll.
"""

import argparse
import hashlib
import shutil
import sys
from pathlib import Path

CLIENT_HASH_OFFSET = 0x3B40  # ASCII hex SHA-256 of cofr13_client.dll inside client.dll

# (file offset, original bytes, patched bytes, description)
PATCHES = [
	(0xA217, "8d42ff3dfe000000", "8d42fe3dfd000000", "no ammo counter for the flashlight"),
	(0xE27F, "7569", "9090", "dual-wield counters regardless of overlay mode"),
	(0xE28B, "ffb594131100", "ffb598131100", "dual-wield primary uses the second DualAmmo value"),
	(0xE2AA, "ffb598131100", "ffb594131100", "dual-wield secondary uses the first DualAmmo value"),
]

PATCHED_SHA256 = "bcab93b54f5bc2286e6ad98a8502c0576a98f5004bcb8d0003612a710e943c21"


def sha256(data):
	return hashlib.sha256(data).hexdigest()


def main():
	parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
	parser.add_argument("cl_dlls", type=Path, help="the game's CryOfFear/cl_dlls folder")
	parser.add_argument("--check", action="store_true", help="report the state without writing")
	args = parser.parse_args()

	mod_path = args.cl_dlls / "cofr13_client.dll"
	proxy_path = args.cl_dlls / "client.dll"
	mod = bytearray(mod_path.read_bytes())
	proxy = bytearray(proxy_path.read_bytes())

	pinned = bytes(proxy[CLIENT_HASH_OFFSET:CLIENT_HASH_OFFSET + 64]).decode("ascii", "replace")
	if pinned != sha256(mod):
		sys.exit("client.dll does not pin the current cofr13_client.dll; refusing to patch")

	for offset, before, after, what in PATCHES:
		current = bytes(mod[offset:offset + len(before) // 2]).hex()
		if current == after:
			print("already applied: " + what)
		elif current == before:
			print("to apply:        " + what)
			mod[offset:offset + len(after) // 2] = bytes.fromhex(after)
		else:
			sys.exit("unexpected bytes at 0x%x (%s); this is not the supported build" % (offset, current))

	new_hash = sha256(mod)
	if new_hash != PATCHED_SHA256:
		sys.exit("patched DLL hash %s is not the expected %s; nothing written" % (new_hash, PATCHED_SHA256))
	if pinned == new_hash:
		print("cofr13_client.dll is already fully patched")
		return
	if args.check:
		print("check only; nothing written")
		return

	for path in (mod_path, proxy_path):
		backup = path.with_name(path.stem + ".before-hud-fix.dll")
		if not backup.exists():
			shutil.copy2(path, backup)

	proxy[CLIENT_HASH_OFFSET:CLIENT_HASH_OFFSET + 64] = new_hash.encode("ascii")
	mod_path.write_bytes(mod)
	proxy_path.write_bytes(proxy)
	print("patched cofr13_client.dll and updated the hash pinned in client.dll")


if __name__ == "__main__":
	main()
