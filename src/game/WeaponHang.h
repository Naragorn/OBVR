#pragma once

#include "core/Types.h"

namespace obvr::game {

// The taken weapon onto the hand in the frame its equip makes its model
// (free-weapons-spec "The take's last three frames"; Nadi, 2026-11-18:
// "schließ nun die Pause").
//
// Straight into the hand ([Hands] TakeDrawsAtOnce) the equip puts the new
// model on the sheath node (SideWeapon or BackWeapon) and the engine plays
// the Equip group; the model changes to the hand only at the group's Attach
// key, two frames later even with the draw moved on to it - the weapon drawn
// at the hip and the hand empty in between.
//
// What moves it, read 2026-11-18 in Oblivion.exe 1.2.0.416 [R]:
// HighProcess's virtual at +0x150 (0x00656270, the same entry in
// MiddleHighProcess's vtable) - thiscall (toHand, biped, animData, actor),
// ret 10h. It asks the process for the hand's node (+0x118, 0x0064B110: the
// cached "Weapon", or the biped's slot for the first person) and the
// sheath's (+0x124, 0x006508D0), takes the sheath's first child (toHand) or
// the hand's (back), and hangs it on the other through NiNode's AttachChild
// (vtable +0x84); then it sets the model's local turn for the weapon type
// and tells the anim data and the actor (0x00476410, 0x005EF9B0). Nothing
// under the sheath, it changes nothing. The player passes its biped and anim
// data per person: PlayerCharacter 0x006600D0 (+0x5C8 first, +0x104 third)
// and 0x0065D750 (+0x5CC first, 0x004D8370 third) - as the engine's own
// re-hang after a model change does (0x00656D1D: first, then third person,
// toHand from HighProcess +0x115). +0x115 is the "on the hand" byte, set by
// the process's virtual +0x308 (0x0064AD40, `mov [ecx+0x115], al`), read by
// +0x304 (0x0064AD30).
struct WeaponHangReport {
	bool called = false;             // the engine's hang was called
	bool firstOnHand = false;        // the first person's hand holds a model after it
	bool thirdOnHand = false;        // the third person's
	UInt16 firstSheathBefore = 0;    // children under the first person's sheath before
	UInt16 firstHandBefore = 0;      // and under its hand
	UInt16 thirdSheathBefore = 0;
	UInt16 thirdHandBefore = 0;
	const char* why = "";            // not called: why
};

// The player's drawn weapon from its sheath onto the hand, first and third
// person, through the engine's own hang; the on-hand byte set first, as the
// engine's re-hang reads it. Safe to call with nothing on the sheath.
WeaponHangReport HangPlayerWeaponOnHand();

}  // namespace obvr::game
