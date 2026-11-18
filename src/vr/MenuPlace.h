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
	// The tablet's face, when the quad is the tablet: a point on the figure's
	// page maps to the figure where the game draws it (vr::SurfaceToLayer).
	const TabletFrame* tablet = nullptr;
	bool showFigure = false;  // the figure's tab chosen: the page's rectangle shows the figure
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

// Where the drawn hand sits on its controller, in the controller's frame
// (OpenVR's: x right, y up, z back): the shared grip and the hand's own, by
// the physical hand (camera/hands/HandGeometry.h, vr::DrawnHandInTracking).
inline NiPoint3 DrawnHandOnController(const HandSettings& s, bool physicalLeft) {
	const float x = physicalLeft ? s.leftHandGripX : s.rightHandGripX;
	const float y = physicalLeft ? s.leftHandGripY : s.rightHandGripY;
	const float z = physicalLeft ? s.leftHandGripZ : s.rightHandGripZ;
	return NiPoint3{x, z + s.handGripUpMetres, -(y + s.handGripForwardMetres)};
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
	const bool holderLeft = PhysicalLeftOf(true, f.rolesSwapped);
	const TabletPlace tablet =
		inHand && f.left.valid ? PlaceTablet(f.tablet, s.tablet, holderLeft, DrawnHandOnController(s, holderLeft))
		                       : TabletPlace{};
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
		target.tablet = &f.tablet;
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

// A point on the tablet's face to the game's pixel under it: the figure's
// page, the figure behind its tab, or a tab (nothing of the game's).
inline bool FaceToLayer(const MenuTarget& t, float& x, float& y) {
	if (t.showFigure) {
		return FigureTabToLayer(*t.tablet, x, y);
	}
	return TabletTabAt(*t.tablet, x, y) < 0 && SurfaceToLayer(*t.tablet, x, y);
}

// The tablet's tabs (vr::SetFigureTab): which one the finger's tip or the
// beam is on, and the tab chosen by a press there - the finger through the
// glass or the trigger pulled. The figure's tab chosen is what the face then
// shows; the press on a tab reaches nothing of the game's (FaceToLayer).
inline bool StepTabletTabs(TabletTabState& state, const HandModeFrame& f, const HandSettings& s,
                           const MenuTarget& t) {
	const bool tabs = t.tablet != nullptr && t.tablet->figureTab && t.quad.valid && t.pointHand != nullptr &&
	                  t.pointHand->valid;
	int under = -1;
	bool down = false;
	if (tabs) {
		const HandPose& hand = *t.pointHand;
		const NiPoint3 pointing = Rotate(
			hand.orientation,
			LaserDirectionLocal(s.laserPitchDegrees, LaserYawFor(!t.pointRight, f.rolesSwapped, s.laserYawDegrees)));
		const NiPoint3 origin =
			hand.position +
			Rotate(hand.orientation, LaserOffsetLocal(s.laserOffsetRightMetres, s.laserOffsetUpMetres, !t.pointRight)) +
			pointing * s.laserOriginMetres;
		const PokeSample tip = PokeOnQuad(hand.position + pointing * s.pokeTipForward, t.quad.centre, t.quad.right,
		                                  t.quad.up, t.quad.width, t.quad.height, t.pixelWidth, t.pixelHeight);
		const bool finger = FingerPokes(hand) && tip.inside && tip.depth <= s.poke.hover;
		const LaserHit beam = LaserOnQuad(origin, pointing, t.quad.centre, t.quad.right, t.quad.up, t.quad.width,
		                                  t.quad.height, t.pixelWidth, t.pixelHeight);
		const float x = (finger ? tip.pixelX : beam.pixelX) + t.pixelLeft;
		const float y = (finger ? tip.pixelY : beam.pixelY) + t.pixelTop;
		under = finger || beam.hit ? TabletTabAt(*t.tablet, x, y) : -1;
		down = finger ? tip.depth <= s.poke.press : hand.trigger > 0.5f;
	}
	return StepTabletTab(state, tabs, under, down, t.tablet != nullptr && t.tablet->popupUp);
}

// The finger's tip and the laser against the target, their pixel in the
// layer's own pixels whichever rectangle of it the quad shows.
inline PokeSample PokeOnTarget(const NiPoint3& tip, const MenuTarget& t) {
	PokeSample sample = PokeOnQuad(tip, t.quad.centre, t.quad.right, t.quad.up, t.quad.width, t.quad.height,
	                               t.pixelWidth, t.pixelHeight);
	sample.pixelX += sample.inside ? t.pixelLeft : 0.0f;
	sample.pixelY += sample.inside ? t.pixelTop : 0.0f;
	if (sample.inside && t.tablet != nullptr && !FaceToLayer(t, sample.pixelX, sample.pixelY)) {
		sample.inside = false;  // the figure's page beside the figure: nothing to press
	}
	return sample;
}

inline LaserHit LaserOnTarget(const NiPoint3& origin, const NiPoint3& direction, const MenuTarget& t) {
	LaserHit hit = LaserOnQuad(origin, direction, t.quad.centre, t.quad.right, t.quad.up, t.quad.width,
	                           t.quad.height, t.pixelWidth, t.pixelHeight);
	hit.pixelX += hit.hit ? t.pixelLeft : 0.0f;
	hit.pixelY += hit.hit ? t.pixelTop : 0.0f;
	if (hit.hit && t.tablet != nullptr && !FaceToLayer(t, hit.pixelX, hit.pixelY)) {
		hit.hit = false;  // the figure's page beside the figure: nothing under the beam
	}
	return hit;
}

}  // namespace obvr::vr
