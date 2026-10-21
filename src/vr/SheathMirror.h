#pragma once

// Which of the body's sheathed-weapon nodes hang mirrored for a left-hander
// (Nadi in the headset, 2026-10-21: "die waffe sheathed ist weiterhin an
// linker hüfte obwohl man die an rechter hüfte zieht. der sheathed zustand
// muss auch noch gespiegelt werden wenn left hand mode an ist").
//
// The game hangs a sheathed weapon on three nodes of the body's skeleton
// (skeleton.nif read with pyffi, 2026-10-21, the skeleton root's x to the
// body's right, y forward): "SideWeapon" under Bip01 Pelvis at the LEFT hip
// (x -14.6, a sword's hilt forward), "BackWeapon" under Bip01 Spine2 behind
// the LEFT shoulder (a claymore's pommel at x -17.5 over it, a bow's grip at
// x -4.3) and "Quiver" under Spine2 behind the RIGHT hip (x +7.4). The rule:
// with left-handed play on, a node is mirrored when the place its weapon is
// drawn from (vr/Holster.h, vr/Archery.h - the side each place has this
// frame, a side chosen in the settings included) is on the other side than
// the game's node. Right-handed play keeps the game's nodes as they are
// (the two-hander's place over the right shoulder against the game's hilt
// over the left is how it has always been, and nobody looks at their back).
//
// BackWeapon holds the two-hander, the staff or the bow, so its side follows
// what is equipped: a left-hander's two-hander is drawn over the left
// shoulder, where the game hangs it - not mirrored; their bow behind the
// right shoulder - mirrored. Pure, covered by holster_test.

#include "vr/Holster.h"

namespace obvr::vr {

struct SheathMirror {
	bool side = false;    // SideWeapon, the one-hander at the hip
	bool back = false;    // BackWeapon, the two-hander, staff or bow
	bool quiver = false;  // Quiver
};

// The place's side this frame: right of the body, or not.
inline bool PlaceOnRight(const NiPoint3& zone, HolsterSide side, bool leftHanded) {
	return ZoneFor(zone, side, leftHanded).x > 0.0f;
}

// `quiverX` the quiver place's right-handed x ([Hands] QuiverX; mirrored
// with the hands, vr/Archery.h).
inline SheathMirror SheathMirrorFor(const HolsterSettings& holster, float quiverX, bool leftHanded,
                                    EquippedKind equipped) {
	SheathMirror m;
	if (!leftHanded) {
		return m;
	}
	// The game's SideWeapon at the left hip.
	m.side = PlaceOnRight(holster.oneHandZone, holster.oneHandSide, true);
	// The game's BackWeapon behind the left shoulder, for both kinds.
	if (equipped == EquippedKind::TwoHand) {
		m.back = PlaceOnRight(holster.twoHandZone, holster.twoHandSide, true);
	} else if (equipped == EquippedKind::Bow) {
		m.back = PlaceOnRight(holster.bowZone, holster.bowSide, true);
	}
	// The game's Quiver behind the right hip; a left-hander's quiver place
	// is QuiverX mirrored.
	const bool quiverOnRight = -quiverX > 0.0f;
	m.quiver = !quiverOnRight;
	return m;
}

}  // namespace obvr::vr
