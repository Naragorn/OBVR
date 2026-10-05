#pragma once

#include "core/Types.h"

namespace obvr::game {

// Weapons and armour changed whatever the player's action, in Full VR (the
// tester, 2026-10-05: "you must wait until ... before equipping ... nervt
// extrem da in vr ja eh keine player anims kommen. das muss weg").
//
// Read in Oblivion.exe 1.2.0.416: the inventory menu and the hotkeys refuse to
// equip or unequip while Actor::GetCurrentAction (0x005E0EE0, the process's
// action, HighProcess+0x1F4) is anything but -1 - a draw, a sheathe, an
// attack, a bow's draw, a block, a recoil, a stagger - and show
// sAnimationCanNotEquipWeapon ("You cannot change weapons while attacking."),
// sAnimationCanNotEquipArmor or sAnimationCanNotUnequip instead. The equip is
// refused, not queued. Each site compares the action with -1 and jumps over
// the refusal when it is -1: `cmp eax, -1; je <equip>` (83 F8 FF 74 xx).
// In Full VR the player's animations are not seen and OBVR's own hands decide
// what is held, so the guard only gets in the way. Each `je` becomes a `jmp`
// (EB) while Full VR is on and goes back to `je` (74) when it is off:
//
// - 0x005ABFF8, the inventory menu equipping a weapon;
// - 0x005C23F5, a hotkey (OBVR's quick-menu ring taps them) equipping;
// - 0x005AC13C, the inventory menu unequipping what is worn;
// - 0x005FC47A, dropping something worn.
//
// Actor::EquipItem itself (0x005FAEA0), which the holsters and the console
// use, never asks - the engine already equips mid-action there. Not patched:
// GetCurrentAction itself, which everything else asks too.
inline constexpr UInt32 kEquipGateSites[4] = {0x005ABFF8, 0x005C23F5, 0x005AC13C, 0x005FC47A};
inline constexpr UInt8 kEquipGateJe = 0x74;
inline constexpr UInt8 kEquipGateJmp = 0xEB;

// The byte a gate holds: open (jump over the refusal always) or the game's.
inline UInt8 EquipGateByte(bool open) { return open ? kEquipGateJmp : kEquipGateJe; }

// Checks each site (cmp eax,-1 before it, je or jmp at it); a site that is
// neither is left alone and reported.
void InstallEquipWhileActing();

// Every frame: open the gates while Full VR is on.
void SetEquipWhileActing(bool open);

}  // namespace obvr::game
