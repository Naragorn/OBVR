#pragma once

// Around the blade's contact step (game/BladeContactLogic.h): the camera's
// jump that takes the blade up anew, and the span the step uses.

#include "game/BladeContactLogic.h"

namespace obvr::game {

// ------------------------------------------------------------------ around it

// The camera jumped between two frames - a teleport, a snap turn, a load -
// rather than walked or turned: further than this, or turned more.
inline constexpr float kBladeJumpUnits = 40.0f;
inline constexpr float kBladeJumpRadians = 0.2617994f;  // 15 degrees

inline bool CameraJumped(const NiMatrix33& lastRot, const NiPoint3& lastPos, const NiMatrix33& rot,
                         const NiPoint3& pos) {
	const NiPoint3 moved = pos - lastPos;
	if (!(moved.LengthSquared() <= kBladeJumpUnits * kBladeJumpUnits)) {
		return true;
	}
	return !(AngleBetweenRotations(lastRot, rot) <= kBladeJumpRadians);
}

// The blade's span as the step uses it: the first one read, then a new one
// only once the drawn blade has stood apart from it for a while (the
// Havok body's own rule, StepSpanWatch) - the bones are pinned a frame
// behind, so a fast hand alone reads a moved span.
struct BladeSpanCache {
	BodySpan span;
	UInt32 movedFrames = 0;
	UInt32 weapon = 0;  // the weapon it was read for: another starts anew
};

inline const BodySpan& StepBladeSpanCache(BladeSpanCache& c, UInt32 weapon, const BodySpan& now) {
	if (weapon != c.weapon) {
		c = BladeSpanCache{};
		c.weapon = weapon;
	}
	if (!c.span.valid) {
		if (now.valid) {
			c.span = now;
		}
		return c.span;
	}
	if (StepSpanWatch(c.movedFrames, SpanMoved(c.span, now))) {
		c.span = now;
	}
	return c.span;
}

}  // namespace obvr::game
