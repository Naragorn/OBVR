#pragma once

// The worn shield's disc, read in the hand's frame (game/ShieldShape,
// HandContactLogic.h ShieldDisc), put into the frame's guard in the world
// (BlockAngleLogic.h, BlockGuard): the disc as the block rules take it - its
// centre, normal, the longer spoke as its radius, half its thickness - and,
// for the arrows (game/ArrowShieldLogic.h), its two in-plane axes and its
// outline (ShieldOutlineLogic.h), kept in the disc and pointed to. Pure;
// shield_outline_test.

#include "game/BlockAngleLogic.h"
#include "game/BladeStopperIds.h"
#include "game/HandContactLogic.h"

namespace obvr::game {

// `rot`, `pos`: the left hand's pose in the world, the frame the disc was
// read in. `disc` must outlive the guard's use of its outline (CameraHook's
// g_shieldDisc does).
inline void ShieldIntoGuard(const ShieldDisc& disc, const NiMatrix33& rot, const NiPoint3& pos, BlockGuard& g) {
	const BladeDisc d = ShieldDiscInWorld(disc, rot, pos, kStopperOwnShield);
	g.shieldValid = true;
	g.shieldCentre = d.centre;
	g.shieldNormal = d.normal;
	g.shieldRadius = d.radius;
	g.shieldHalfThick = d.halfThick;
	g.shieldAxisA = rot * disc.axis[0];
	g.shieldAxisB = rot * disc.axis[1];
	g.shieldOutline = &disc.outline;
}

}  // namespace obvr::game
