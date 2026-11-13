#pragma once

namespace cof {
// Native weapon IDs from the CoF GetItemInfo implementations. Some tools
// retain ammo fields for internal behavior, so this is a presentation policy.
inline bool hideWeaponAmmo(int id)
{
    switch (id) {
    case 1:  // Flashlight
    case 2:  // Mobile phone
    case 5:  // Digital camera
    case 6:  // Switchblade
    case 10: // Lantern
    case 12: // Nightstick
    case 16: // Branch
    case 17: // Action hands
    case 18: // Sledgehammer
    case 22: // Radio
    case 24: // Axe
    case 25: // Book laser
        return true;
    default:
        return false;
    }
}
}
