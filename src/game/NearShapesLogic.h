#pragma once

// Which of the player's shapes are near the eyes when a menu opens ([Debug]
// MenuNearShapes): Nadi, Windows 2026-10-27, "Im Inventar hatte ich
// irgendein Rüstung teil im Blick was störte" - the four Windows logs of
// 2026-10-26/27 name what OBVR hides, never what is drawn near the eyes, so
// the piece could not be told. The glue walks the first-person and the
// third-person trees and logs every shown shape these accept.
//
// A shape OBVR keeps in view has its bound set round the eyes (the hands'
// "kept in view", the body's), so it reads 0 units off whatever its size:
// the line names it, the distance does not place it.
//
// Pure; near_shapes_test.

#include "game/NiMath.h"
#include "core/MathFns.h"

namespace obvr::game {

// Whether a bound (its world centre and radius) lies within `withinUnits` of
// the eye: its surface's distance in `gapUnits`, 0 with the eye inside it. A
// bound of no size or no number is not counted.
inline bool BoundNearEye(const NiPoint3& centre, float radius, const NiPoint3& eye, float withinUnits,
                         float& gapUnits) {
	const float dx = centre.x - eye.x, dy = centre.y - eye.y, dz = centre.z - eye.z;
	const float distance = math::Sqrt(dx * dx + dy * dy + dz * dz);
	if (!(radius > 0.0f) || !(distance == distance)) {
		return false;
	}
	gapUnits = distance > radius ? distance - radius : 0.0f;
	return withinUnits > 0.0f && gapUnits <= withinUnits;
}

// A scene object is shown when the node above it is and its own app-cull bit
// (flags 0x0001, kNiFlagsOffset) is clear.
inline bool ShownUnder(bool parentShown, UInt16 flags) { return parentShown && (flags & 0x0001) == 0; }

}  // namespace obvr::game
