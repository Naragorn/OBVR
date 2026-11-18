#pragma once

// Where the game's menu is this frame, as the pointing hand needs it
// (HandMode::PointAtMenu): the quad in tracking space, the hand that points
// at it, and the rectangle of the layer's pixels the quad shows. Three
// places: the tablet in the off hand (vr/TabletLogic.h), the menu wrist
// ([Hands] WristMenu), or the big quad the layer hangs in the room or ahead
// of the head. Pure; covered by hand_mode_test.

#include "vr/HandMode.h"
#include "vr/TabletScroll.h"

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

// A drawn hand in tracking space.
inline NiPoint3 DrawnHandAt(const HandPose& hand, const HandSettings& s, bool physicalLeft) {
	return hand.position + Rotate(hand.orientation, DrawnHandOnController(s, physicalLeft));
}

// A place in tracking space (axes and centre) as the left controller's.
inline TabletPlace OnLeftController(const HandPose& left, const NiPoint3& right, const NiPoint3& up,
                                    const NiPoint3& front, const NiPoint3& centre, float width, float height) {
	TabletPlace place;
	const Quaternion back = left.orientation.Conjugate();
	const NiPoint3 axes[4] = {Rotate(back, right), Rotate(back, up), Rotate(back, front),
	                          Rotate(back, centre - left.position)};
	for (int c = 0; c < 4; ++c) {
		place.transform.m[0][c] = axes[c].x;
		place.transform.m[1][c] = axes[c].y;
		place.transform.m[2][c] = axes[c].z;
	}
	place.width = width;
	place.height = height;
	place.valid = true;
	return place;
}

// Unrolled up and down: its middle between the drawn hands, up from the
// lower hand to the upper, facing the eyes across.
inline TabletPlace PlaceBetweenHandsUpright(const TabletPlace& held, const HandModeFrame& f, const NiPoint3& l,
                                           const NiPoint3& rr) {
	const NiPoint3 low = l.y < rr.y ? l : rr;
	const NiPoint3 high = l.y < rr.y ? rr : l;
	NiPoint3 up = high - low;
	const float length = math::Sqrt(Dot(up, up));
	NiPoint3 toEyes = f.headPosition - (l + rr) * 0.5f;
	toEyes.y = 0.0f;
	const float eyes = math::Sqrt(Dot(toEyes, toEyes));
	if (!(length > 0.01f) || !(eyes > 0.01f)) {
		return held;
	}
	up = up * (1.0f / length);
	NiPoint3 across = Cross(up, toEyes * (1.0f / eyes));
	const float acrossLength = math::Sqrt(Dot(across, across));
	if (!(acrossLength > 0.1f)) {
		return held;
	}
	across = across * (1.0f / acrossLength);
	return OnLeftController(f.left, across, up, Cross(across, up), (l + rr) * 0.5f, held.width, held.height);
}

// The scroll between the hands: its middle between the drawn hands, across
// from the left to the right, upright and leant back by the tilt, facing the
// eyes - or, unrolled up and down, from the lower hand to the upper; as the
// left controller's place.
inline TabletPlace PlaceBetweenHands(const TabletPlace& held, const HandModeFrame& f, const HandSettings& s) {
	const NiPoint3 l = DrawnHandAt(f.left, s, PhysicalLeftOf(true, f.rolesSwapped));
	const NiPoint3 rr = DrawnHandAt(f.right, s, PhysicalLeftOf(false, f.rolesSwapped));
	if (s.tablet.unroll == TabletUnroll::Vertical) {
		return PlaceBetweenHandsUpright(held, f, l, rr);
	}
	NiPoint3 across = rr - l;
	const float length = math::Sqrt(Dot(across, across));
	if (!(length > 0.01f)) {
		return held;
	}
	across = across * (1.0f / length);
	NiPoint3 up = NiPoint3{0.0f, 1.0f, 0.0f} - across * across.y;
	const float upLength = math::Sqrt(Dot(up, up));
	if (!(upLength > 0.1f)) {
		return held;
	}
	up = up * (1.0f / upLength);
	const NiPoint3 front0 = Cross(across, up);
	const float tilt = s.tablet.tiltDegrees * (math::kPi / 180.0f);
	up = up * math::Cos(tilt) - front0 * math::Sin(tilt);
	return OnLeftController(f.left, across, up, Cross(across, up), (l + rr) * 0.5f, held.width, held.height);
}

// Kept by the right hand: the tablet as the left one holds it, mirrored onto
// the right hand, as the left controller's place.
inline TabletPlace HeldByRight(const HandModeFrame& f, const HandSettings& s, TabletHold hold) {
	const bool rightPhysicalLeft = PhysicalLeftOf(false, f.rolesSwapped);
	const TabletPlace own =
		PlaceTablet(f.tablet, s.tablet, rightPhysicalLeft, DrawnHandOnController(s, rightPhysicalLeft), hold);
	if (!own.valid) {
		return own;
	}
	const openvr::HmdMatrix34& m = own.transform;
	NiPoint3 cols[4];
	for (int c = 0; c < 4; ++c) {
		cols[c] = Rotate(f.right.orientation, NiPoint3{m.m[0][c], m.m[1][c], m.m[2][c]});
	}
	return OnLeftController(f.left, cols[0], cols[1], cols[2], f.right.position + cols[3], own.width, own.height);
}

// What the scroll reads of the frame.
inline TabletScrollFrame ScrollFrameOf(const HandModeFrame& f, const HandSettings& s, const TabletPlace& held,
                                       bool keptRight = false, bool keptOnTop = false) {
	TabletScrollFrame g;
	g.allowed = s.enabled && s.tablet.enabled && f.inWorld && !f.settingsMenuOpen;
	g.handsEmpty = !f.meleeHeld && !f.torchHeld && !f.holdingObject && !f.bothHandsOnHandle;
	g.menuMode = f.menuMode;
	g.tabletUp = f.tablet.up;
	g.tracked = f.left.valid && f.right.valid && f.headValid;
	g.left = DrawnHandAt(f.left, s, PhysicalLeftOf(true, f.rolesSwapped));
	g.right = DrawnHandAt(f.right, s, PhysicalLeftOf(false, f.rolesSwapped));
	g.head = f.headPosition;
	NiPoint3 forward = Rotate(f.head, NiPoint3{0.0f, 0.0f, -1.0f});
	forward.y = 0.0f;
	const float length = math::Sqrt(Dot(forward, forward));
	g.headForward = length > 0.01f ? forward * (1.0f / length) : NiPoint3{0.0f, 0.0f, -1.0f};
	g.grip[0] = f.left.valid && (GripDown(f.left.buttonsPressed) || f.left.gripPull > 0.7f);
	g.grip[1] = f.right.valid && (GripDown(f.right.buttonsPressed) || f.right.gripPull > 0.7f);
	const bool upright = s.tablet.unroll == TabletUnroll::Vertical;
	g.fullMetres = held.valid ? (upright ? held.height : held.width) : s.tablet.widthMetres;
	g.dt = f.dtSeconds;
	// The free end: the edge's middle opposite the holding hand - across the
	// face held at a side, up or down held at the bottom or the top.
	if (held.valid && f.left.valid) {
		const openvr::HmdMatrix34& m = held.transform;
		const NiPoint3 centre{m.m[0][3], m.m[1][3], m.m[2][3]};
		const NiPoint3 across{m.m[0][0], m.m[1][0], m.m[2][0]};
		const NiPoint3 up{m.m[0][1], m.m[1][1], m.m[2][1]};
		NiPoint3 local = centre;
		if (s.tablet.unroll == TabletUnroll::Sideways) {
			local = centre + across * ((keptRight ? -0.5f : 0.5f) * held.width);
		} else {
			local = centre + up * ((keptOnTop ? -0.5f : 0.5f) * held.height);
		}
		g.freeEnd = f.left.position + Rotate(f.left.orientation, local);
		g.freeEndValid = true;
	}
	return g;
}

// Where the menu is and who points. The tablet comes first: the off hand
// holds it and the weapon hand points and taps (left-handed the roles are
// swapped as a whole, so the same two). Then the menu wrist, the other hand
// pointing. Otherwise the big quad, or the cinema screen behind it when
// there is none: the hand that last pulled its trigger points
// (`pointRightLatch`), the other one when that hand is not tracked. Outside
// menu mode there is no quad and no pointing hand.
inline MenuTarget PlaceMenu(const HandModeFrame& f, const HandSettings& s, bool pointRightLatch,
                            MenuPlaceResult& r, TabletScrollState* scroll = nullptr) {
	MenuTarget target;
	target.pixelWidth = f.layerPixelsWidth;
	target.pixelHeight = f.layerPixelsHeight;
	const bool pixels = f.layerPixelsWidth > 0.0f && f.layerPixelsHeight > 0.0f;
	const bool inHand = s.enabled && f.menuMode && f.inWorld;
	const bool holderLeft = PhysicalLeftOf(true, f.rolesSwapped);
	// Held at its side, or its bottom or top edge (vr::HoldFor): as the scroll
	// last let go of it, the bottom edge when opened by the B up and down.
	const bool keptOnTop = scroll != nullptr && scroll->holderTop &&
	                       (scroll->phase == TabletScrollPhase::Held || scroll->phase == TabletScrollPhase::RollingUp);
	const TabletHold hold = HoldFor(s.tablet.unroll, keptOnTop);
	TabletPlace tablet = inHand && f.left.valid ? PlaceTablet(f.tablet, s.tablet, holderLeft,
	                                                          DrawnHandOnController(s, holderLeft), hold)
	                                            : TabletPlace{};
	// The scroll (vr/TabletScroll.h): between the hands while it unrolls or
	// rolls up, in the right hand when that one kept it.
	const bool keptRight = scroll != nullptr && scroll->holderRight &&
	                       (scroll->phase == TabletScrollPhase::Held || scroll->phase == TabletScrollPhase::RollingUp);
	const TabletPlace held = tablet.valid && keptRight && f.right.valid ? HeldByRight(f, s, hold) : tablet;
	const TabletScrollResult rolled =
		scroll != nullptr ? StepTabletScroll(*scroll, ScrollFrameOf(f, s, held, keptRight, keptOnTop)) : TabletScrollResult{};
	const TabletHold kept = HoldFor(s.tablet.unroll, rolled.holderTop);
	r.tabletMenuKey = rolled.menuKey;
	r.tabletGrips = rolled.gesture;
	r.tabletUpright = s.tablet.unroll == TabletUnroll::Vertical;
	r.tabletShare = rolled.betweenHands ? rolled.amount : -1.0f;
	if (tablet.valid && f.right.valid && rolled.betweenHands) {
		tablet = PlaceBetweenHands(tablet, f, s);
	} else if (tablet.valid && f.right.valid && rolled.holderRight &&
	           (rolled.phase == TabletScrollPhase::Held || rolled.phase == TabletScrollPhase::RollingUp)) {
		tablet = HeldByRight(f, s, kept);
		r.tabletHolderRight = true;
	}
	const HandPose& wristHand = s.menuOnRight ? f.right : f.left;
	if (tablet.valid) {
		r.menuOnTablet = true;
		r.menuTransform = tablet.transform;
		r.tabletWidth = tablet.width;
		r.tabletHeight = tablet.height;
		target.quad = QuadOnHand(f.left, tablet.transform, tablet.width, tablet.height, pixels);
		target.pointHand = r.tabletHolderRight ? &f.left : &f.right;
		target.pointRight = !r.tabletHolderRight;
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
