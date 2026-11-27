#pragma once

// The drawn staff's line for the block by contact (Nadi, 2026-11-27: "go same
// for ... stab"). A staff (weapon type 4) is no swung weapon in OBVR's code
// (game/MeleeHit.h WeaponIsSwung): its blade span is never read, and its
// contacts with walls and bodies, the stoppers and the parry stay off - the
// block alone reads its line here, apart from all of that.
//
// The line: the first-person "Weapon" node the drawn weapon hangs on, from
// the node along its own y axis to its bound's far end on that axis - the
// reading a foe's blade gets (ParryLogic.h, TheirBladeFromNode), in the world.
// The same node and axis convention as the player's blade span
// (BladeSpanFromNode in camera/CameraHook's StepWeaponContact) [read]; that a
// staff's model lies along the node's y axis as a blade's does is derived
// from that shared node, not seen - the first headset log says its length and
// direction (camera/hands/Locomotion.cpp, the guard's line).

#include "core/Types.h"
#include "game/BlockAngleLogic.h"
#include "game/BowVisual.h"
#include "game/NiMath.h"

namespace obvr::game {

// Once a frame (from game/BlockCone's SetBlockGuard): a staff drawn and its
// line read; false for anything else (the node, its bound or its reach -
// 10 to 200 units - not a staff's).
bool ReadStaffLine(NiPoint3& a, NiPoint3& b);

// The last frame's reading, for the hand mode's next frame and the log.
bool StaffLineRead();
void LastStaffLine(NiPoint3& a, NiPoint3& b);

// The frame's reads for the blocks by contact, the staff's line among them.
inline GuardReads GuardReadsFor(bool shieldDisc, bool weaponBlade, bool torchShaft) {
	GuardReads r;
	r.shieldDisc = shieldDisc;
	r.weaponBlade = weaponBlade;
	r.torchShaft = torchShaft;
	r.staffLine = StaffLineRead();
	NiPoint3 a;
	NiPoint3 b;
	r.bowLimb = BowLimbLine(a, b);
	return r;
}

}  // namespace obvr::game
