#pragma once

#include "core/Types.h"
#include "vr/Archery.h"

namespace obvr::game {

// The bow by hand, as it is seen (vr/Archery.h): the arrow in the drawing hand
// from the quiver on, on the string from the nock, and the string pulled to
// where the drawing hand is. Called each frame after the hands are pinned.
//
// The engine's own picture of the draw is its animation, which the pinned
// hands no longer play: the arrow it hangs on the bow's ArrowBone (a clone of
// "Arrow:0" from the ammunition's quiver, made at the draw's Attach key,
// 0x005FD004) and the string's "BowMorph" weight from bowattack.kf. So OBVR
// makes the same clone the same way (NiObject::Clone, 0x00700900) at the take
// and places it in the drawing hand; the engine's arrow is hidden while the bow
// is drawn by hand; and the string's weight is written and blended by the
// morpher's own blend (0x006D0CF0) after the animation.
struct BowVisualInput {
	bool active = false;  // Full VR, hands pinned, the bow by hand on and a bow drawn
	vr::ArrowShown arrow = vr::ArrowShown::None;
	vr::StringSource string = vr::StringSource::Engine;
};

void StepBowVisual(const BowVisualInput& in);

}  // namespace obvr::game
