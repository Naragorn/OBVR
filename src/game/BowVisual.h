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
	// The drawing hand's bone, moved onto the string while the arrow is on it.
	const char* rightHandBone = "Bip01 R Hand";
	// The drawing hand's laser in the world: the arrow in the fist lies along it.
	bool laserValid = false;
	NiPoint3 laserStart{0.0f, 0.0f, 0.0f};
	NiPoint3 laserDirection{0.0f, 1.0f, 0.0f};
	// The bow hand's laser in the world and that controller's up: the bow is
	// turned so it shoots along the laser, its limbs along the up (the aim is
	// the same laser, vr::StepArchery's bowAxis).
	bool bowAimValid = false;
	NiPoint3 bowAim{0.0f, 1.0f, 0.0f};
	NiPoint3 bowUp{0.0f, 0.0f, 1.0f};
};

void StepBowVisual(const BowVisualInput& in);

}  // namespace obvr::game
