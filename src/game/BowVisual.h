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
//
// The bow itself sits in the bow hand as the game holds it (the tester,
// 2026-09-30 evening: "linke hand wieder wie sie in vanilla war vor unseren
// changes einfach normaler griff 90 grad am bogen"); the shot goes where it
// points (BowShotAxis).
struct BowVisualInput {
	bool active = false;  // Full VR, hands pinned, the bow by hand on and a bow drawn
	vr::ArrowShown arrow = vr::ArrowShown::None;
	vr::StringSource string = vr::StringSource::Engine;
	// The drawing hand's bone, moved onto the string while the arrow is on it;
	// the arrow in the fist lies along it, from it to its middle finger.
	const char* rightHandBone = "Bip01 R Hand";
	float dtSeconds = 0.0f;  // the frame's, for the hand's way onto the string
	// In the hand: how far it is led towards the nock (vr::NockGuide).
	float nockGuide = 0.0f;
};

void StepBowVisual(const BowVisualInput& in);

// Where the drawn bow shoots in the world this frame, as the last
// StepBowVisual found it: its model's +x, the line the string pulls back
// along. False while no bow is drawn by hand.
bool BowShotAxis(NiPoint3& world);

// The drawn bow's limbs in the world at the last StepBowVisual: its model's
// y, from grip to tip (not unit length with a scaled bow). False while no bow
// is drawn by hand.
bool BowLimbAxis(NiPoint3& world);

// The arrow in the drawing hand (not on the string) in the world at the last
// StepBowVisual, its nock and its head: the blade of a stab with it
// (vr::ArrowStabs). False with none shown in the hand.
bool ArrowInHandWorld(NiPoint3& nock, NiPoint3& head);

// How far the string was drawn by the hand at the last StepBowVisual: 0 at
// rest, 1 at full draw (the weight its morph was given). False when the arrow
// was not on the string, or the string's morph could not be set.
bool BowDrawWeight(float& weight);

// Whether the equipped ammunition's quiver still holds an arrow ("Arrow:0"
// in the first-person "Quiver"): with none left the engine takes the quiver
// away (2026-10-01: a harness run with none equipped found none).
bool QuiverHasArrows();

}  // namespace obvr::game
