#pragma once

// Where the game's menu is this frame, as the pointing hand needs it
// (HandMode::PointAtMenu): the quad in tracking space, the hand that points
// at it, and the rectangle of the layer's pixels the quad shows. Three
// places: the tablet in the off hand (vr/TabletLogic.h), the menu wrist
// ([Hands] WristMenu), or the big quad the layer hangs in the room or ahead
// of the head. Pure; covered by hand_mode_test.

#include "vr/HandMode.h"

namespace obvr::vr {

struct MenuTarget {
	MenuQuad quad;
	const HandPose* pointHand = nullptr;
	bool pointRight = false;
	// The layer's pixels the quad shows: the whole layer, or the tablet's crop.
	float pixelLeft = 0.0f;
	float pixelTop = 0.0f;
	float pixelWidth = 0.0f;
	float pixelHeight = 0.0f;
};

// A quad carried by a hand: the device-to-overlay transform's position is
// its centre and its first two columns its right and up axes, all turned
// into tracking space by the hand's pose.
inline MenuQuad QuadOnHand(const HandPose& hand, const openvr::HmdMatrix34& t, float width, float height,
                           bool valid) {
	MenuQuad quad;
	quad.valid = valid;
	quad.centre = hand.position + Rotate(hand.orientation, NiPoint3{t.m[0][3], t.m[1][3], t.m[2][3]});
	quad.right = Rotate(hand.orientation, NiPoint3{t.m[0][0], t.m[1][0], t.m[2][0]});
	quad.up = Rotate(hand.orientation, NiPoint3{t.m[0][1], t.m[1][1], t.m[2][1]});
	quad.width = width;
	quad.height = valid ? height : 0.0f;
	return quad;
}

// Where the menu is and who points. The tablet comes first: the off hand
// holds it and the weapon hand points and taps (left-handed the roles are
// swapped as a whole, so the same two). Then the menu wrist, the other hand
// pointing. Otherwise the big quad, or the cinema screen behind it when
// there is none: the hand that last pulled its trigger points
// (`pointRightLatch`), the other one when that hand is not tracked. Outside
// menu mode there is no quad and no pointing hand.
inline MenuTarget PlaceMenu(const HandModeFrame& f, const HandSettings& s, bool pointRightLatch,
                            MenuPlaceResult& r) {
	MenuTarget target;
	target.pixelWidth = f.layerPixelsWidth;
	target.pixelHeight = f.layerPixelsHeight;
	const bool pixels = f.layerPixelsWidth > 0.0f && f.layerPixelsHeight > 0.0f;
	const bool inHand = s.enabled && f.menuMode && f.inWorld;
	const TabletPlace tablet =
		inHand && f.left.valid ? PlaceTablet(f.tablet, s.tablet, PhysicalLeftOf(true, f.rolesSwapped)) : TabletPlace{};
	const HandPose& wristHand = s.menuOnRight ? f.right : f.left;
	if (tablet.valid) {
		r.menuOnTablet = true;
		r.menuTransform = tablet.transform;
		r.tabletWidth = tablet.width;
		r.tabletHeight = tablet.height;
		target.quad = QuadOnHand(f.left, tablet.transform, tablet.width, tablet.height, pixels);
		target.pointHand = &f.right;
		target.pointRight = true;
		target.pixelLeft = f.tablet.crop.left;
		target.pixelTop = f.tablet.crop.top;
		target.pixelWidth = f.tablet.crop.right - f.tablet.crop.left;
		target.pixelHeight = f.tablet.crop.bottom - f.tablet.crop.top;
	} else if (inHand && s.wristMenu && wristHand.valid) {
		r.menuOnWrist = true;
		r.menuWristRight = s.menuOnRight;
		r.menuTransform = WristOverlayTransform(s.wristUp, s.wristBack, s.wristTiltDegrees);
		target.quad = QuadOnHand(wristHand, r.menuTransform, s.wristMenuWidth,
		                         pixels ? s.wristMenuWidth * (f.layerPixelsHeight / f.layerPixelsWidth) : 0.0f, pixels);
		target.pointHand = s.menuOnRight ? &f.left : &f.right;
		target.pointRight = !s.menuOnRight;
	} else if (f.menuMode) {
		target.quad = f.menuQuad;
		target.pointRight = pointRightLatch ? f.right.valid : !f.left.valid;
		target.pointHand = target.pointRight ? &f.right : &f.left;
	}
	return target;
}

// The finger's tip and the laser against the target, their pixel in the
// layer's own pixels whichever rectangle of it the quad shows.
inline PokeSample PokeOnTarget(const NiPoint3& tip, const MenuTarget& t) {
	PokeSample sample = PokeOnQuad(tip, t.quad.centre, t.quad.right, t.quad.up, t.quad.width, t.quad.height,
	                               t.pixelWidth, t.pixelHeight);
	sample.pixelX += sample.inside ? t.pixelLeft : 0.0f;
	sample.pixelY += sample.inside ? t.pixelTop : 0.0f;
	return sample;
}

inline LaserHit LaserOnTarget(const NiPoint3& origin, const NiPoint3& direction, const MenuTarget& t) {
	LaserHit hit = LaserOnQuad(origin, direction, t.quad.centre, t.quad.right, t.quad.up, t.quad.width,
	                           t.quad.height, t.pixelWidth, t.pixelHeight);
	hit.pixelX += hit.hit ? t.pixelLeft : 0.0f;
	hit.pixelY += hit.hit ? t.pixelTop : 0.0f;
	return hit;
}

}  // namespace obvr::vr
