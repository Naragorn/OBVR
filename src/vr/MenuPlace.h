#pragma once

// Where the game's menu is this frame, as the pointing hand needs it
// (HandMode::PointAtMenu): the quad in tracking space, the hand that points
// at it, and the rectangle of the layer's pixels the quad shows. Three
// places: the tablet in the off hand (vr/TabletLogic.h), the menu wrist
// ([Hands] WristMenu), or the big quad the layer hangs in the room or ahead
// of the head. Pure; covered by hand_mode_test.

#include "vr/HandMode.h"
#include "vr/TabletBook.h"
#include "vr/TabletScroll.h"
#include "vr/TabletTabStrip.h"

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
	TabletTab tab = TabletTab::Page;  // the tab shown: the page's rectangle shows the page, the figure or the mirror
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

// A hand where it holds the tablet and where the scroll unrolls it and finds
// the ring, in tracking space: its palm's middle when read
// (vr::TabletHoldPoint), else its bone - the wrist. How close the hands are
// the scroll reads on the controllers: on Nadi's grip the wrists read 29 to
// 35 cm apart with the hands together (2026-11-22), the palms 24 to 29
// (2026-11-27), the controllers 2 to 9.
inline NiPoint3 HoldingHandAt(const HandModeFrame& f, const HandSettings& s, bool leftRole) {
	const HandPose& hand = leftRole ? f.left : f.right;
	const NiPoint3 bone = DrawnHandOnController(s, PhysicalLeftOf(leftRole, f.rolesSwapped));
	return hand.position + Rotate(hand.orientation, TabletHoldPoint(f.tablet, !leftRole, bone));
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
	const NiPoint3 l = HoldingHandAt(f, s, true);
	const NiPoint3 rr = HoldingHandAt(f, s, false);
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
		PlaceTablet(f.tablet, s.tablet, rightPhysicalLeft,
		            TabletHoldPoint(f.tablet, true, DrawnHandOnController(s, rightPhysicalLeft)), hold,
		            TabletHeldAxes(f.tablet, true), TabletHandReach(f.tablet, true));
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
	g.left = HoldingHandAt(f, s, true);
	g.right = HoldingHandAt(f, s, false);
	g.leftController = f.left.position;  // how close the hands are: the controllers (TabletScrollLimits)
	g.rightController = f.right.position;
	g.head = f.headPosition;
	NiPoint3 forward = Rotate(f.head, NiPoint3{0.0f, 0.0f, -1.0f});
	forward.y = 0.0f;
	const float length = math::Sqrt(Dot(forward, forward));
	g.headForward = length > 0.01f ? forward * (1.0f / length) : NiPoint3{0.0f, 0.0f, -1.0f};
	// The scroll begins in the inventory ring, wherever the player put it.
	g.ring = NiPoint3{s.stow.centreRight, s.stow.centreForward, s.stow.centreUp};
	g.ringRadius = s.stow.radius;
	g.grip[0] = f.left.valid && (GripDown(f.left.buttonsPressed) || f.left.gripPull > 0.7f);
	g.grip[1] = f.right.valid && (GripDown(f.right.buttonsPressed) || f.right.gripPull > 0.7f);
	const bool upright = s.tablet.unroll == TabletUnroll::Vertical;
	g.fullMetres = held.valid ? (upright ? held.height : held.width) : s.tablet.widthMetres;
	g.dt = f.dtSeconds;
	g.rolesSwapped = f.rolesSwapped;
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
	                                                          TabletHoldPoint(f.tablet, false, DrawnHandOnController(s, holderLeft)),
	                                                          hold, TabletHeldAxes(f.tablet, false),
	                                                          TabletHandReach(f.tablet, false))
	                                            : TabletPlace{};
	// The scroll (vr/TabletScroll.h): between the hands while it unrolls or
	// rolls up, in the frame's right hand when that one kept its grip as the
	// other let go (vr/TabletHolder.h) - the left then points and taps.
	const bool keptRight = scroll != nullptr && scroll->holderRight &&
	                       (scroll->phase == TabletScrollPhase::Held || scroll->phase == TabletScrollPhase::RollingUp);
	const TabletPlace held = tablet.valid && keptRight && f.right.valid ? HeldByRight(f, s, hold) : tablet;
	const TabletScrollResult rolled =
		scroll != nullptr ? StepTabletScroll(*scroll, ScrollFrameOf(f, s, held, keptRight, keptOnTop)) : TabletScrollResult{};
	const TabletHold kept = HoldFor(s.tablet.unroll, rolled.holderTop);
	r.tabletMenuKey = rolled.menuKey;
	r.tabletGrips = rolled.gesture;
	r.tabletRefusal = static_cast<UInt8>(rolled.refused);
	r.tabletRefusedBy = rolled.refusedBy;
	r.tabletZone = rolled.zone;
	r.tabletBegan = rolled.began;
	r.tabletGripsOpen = scroll == nullptr || rolled.gripsOpen;
	r.tabletUpright = s.tablet.unroll == TabletUnroll::Vertical;
	r.tabletShare = rolled.betweenHands ? rolled.amount : -1.0f;
	r.tabletRolledUp = rolled.rolledUp;
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

// How far the page's face looks off the eyes, in degrees - 0 straight at
// them, 180 its back to them - for the log Nadi tunes the page in the hand by
// (docs/inventory-tablet-spec.md 24.4): `place` the page in the holding
// controller's frame (its face the third column, its middle the fourth),
// `holder` that controller's pose and `eyes` the head's place, both in
// tracking space. Negative with the eyes in the page's middle.
inline float FaceOffEyesDegrees(const openvr::HmdMatrix34& place, const HandPose& holder, const NiPoint3& eyes) {
	const NiPoint3 face = Rotate(holder.orientation, NiPoint3{place.m[0][2], place.m[1][2], place.m[2][2]});
	const NiPoint3 middle =
		holder.position + Rotate(holder.orientation, NiPoint3{place.m[0][3], place.m[1][3], place.m[2][3]});
	const NiPoint3 to = eyes - middle;
	if (!(to.LengthSquared() > 1e-8f)) {
		return -1.0f;
	}
	const NiPoint3 across = game::CrossOf3(face, to);
	return math::Atan2(math::Sqrt(across.LengthSquared()), game::DotOf3(face, to)) * (180.0f / math::kPi);
}

// A point on the tablet's face to the game's pixel under it: the figure's
// page, the figure behind its tab, the mirror (nothing of the game's), or a
// tab (nothing of the game's).
inline bool FaceToLayer(const MenuTarget& t, float& x, float& y) {
	if (t.tab == TabletTab::Figure) {
		return FigureTabToLayer(*t.tablet, x, y);
	}
	if (t.tab == TabletTab::Mirror) {
		return MirrorTabToLayer(*t.tablet, x, y);
	}
	return TabletTabAt(*t.tablet, x, y) < 0 && SurfaceToLayer(*t.tablet, x, y);
}

// The finger that taps: on the tablet, the drawn index finger's tip where it
// is read (vr/FingerTip.h, [Tablet] DrawnFinger); elsewhere the controller's
// tip, as it was.
inline IndexTip TapFinger(const HandModeFrame& f, const HandSettings& s, const MenuTarget& t) {
	if (t.tablet == nullptr || !s.tablet.drawnFinger) {
		return IndexTip{};
	}
	return t.pointRight ? f.rightIndexTip : f.leftIndexTip;
}

// The tablet's tabs (vr::SetTabs): which one the finger's tip or the beam is
// on, and the tab chosen by a press there - the finger through the glass or
// the trigger pulled. The tab chosen is what the face then shows; the press on
// a tab reaches nothing of the game's (FaceToLayer).
inline TabletTab StepTabletTabs(TabletTabState& state, const HandModeFrame& f, const HandSettings& s,
                                const MenuTarget& t) {
	const bool tabs = t.tablet != nullptr && t.tablet->tabCount > 0 && t.quad.valid && t.pointHand != nullptr &&
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
		const PokeSample tip = PokeOnQuad(TapTip(hand, pointing, s.pokeTipForward, TapFinger(f, s, t)), t.quad.centre,
		                                  t.quad.right, t.quad.up, t.quad.width, t.quad.height, t.pixelWidth,
		                                  t.pixelHeight);
		const bool finger = FingerPokes(hand) && tip.inside && tip.depth <= s.poke.hover;
		const LaserHit beam = LaserOnQuad(origin, pointing, t.quad.centre, t.quad.right, t.quad.up, t.quad.width,
		                                  t.quad.height, t.pixelWidth, t.pixelHeight);
		const float x = (finger ? tip.pixelX : beam.pixelX) + t.pixelLeft;
		const float y = (finger ? tip.pixelY : beam.pixelY) + t.pixelTop;
		const int at = finger || beam.hit ? TabletTabAt(*t.tablet, x, y) : -1;
		under = at >= 0 ? static_cast<int>(t.tablet->tabKind[at]) : -1;
		down = finger ? tip.depth <= s.poke.press : hand.trigger > 0.5f;
	}
	return StepTabletTab(state, tabs ? TabsOf(*t.tablet) : TabSet{}, under, down,
	                     t.tablet != nullptr && t.tablet->popupUp, t.tablet != nullptr && t.tablet->figureUnderPage);
}

// The mirror's page is OBVR's own glass: nothing the hand does while it is
// shown - the trigger, the stick - reaches the game's page underneath, which
// stays as it was. Applied to the frame's answer last.
inline void KeepMirrorFromGame(HandModeResult& r) {
	if (r.tabletTab != TabletTab::Mirror) {
		return;
	}
	r.menuScroll = 0;
	r.controls.menuClick = false;
	r.menuDragging = false;
	r.menuPressRunning = false;
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

// The pointing hand's finger on the target. It presses only while it is out:
// with the trigger pulled, or the index curled round it, the laser decides,
// however near the menu the hand is (the tester, 2026-10-10: near the menu it
// was "nicht mehr klickbar" - the cursor sat under the finger's tip, not the
// beam). On the wrist menu and the big quad the click goes down the moment the
// tip reaches the glass, as it always did; on the tablet the finger works the
// glass as a touch screen (vr/TabletTouch.h). While the finger owns the
// cursor it sits straight under the tip, no easing: a finger on a button must
// not find the cursor still on its way there.
struct FingerVerdict {
	bool hover = false;      // the finger owns the cursor
	bool press = false;      // the tip reached the glass (the touch's rising edge)
	bool mouseDown = false;
	float cursorX = 0.0f;
	float cursorY = 0.0f;
	float lengthMetres = 0.0f;  // the beam drawn to the tip
	int wheel = 0;
	int pageStep = 0;
	TabletTouchReport report;   // the tablet's touch this frame, for the log
};

inline FingerVerdict StepFingerOnTarget(PokeState& poke, TabletTouchState& touch, const HandModeFrame& f,
                                        const HandSettings& s, const MenuTarget& t, const NiPoint3& pointing,
                                        bool onScrollBar) {
	FingerVerdict v;
	const HandPose& hand = *t.pointHand;
	const NiPoint3 tip = TapTip(hand, pointing, s.pokeTipForward, TapFinger(f, s, t));
	const PokeSample sample = PokeOnTarget(tip, t);
	const bool out = FingerPokes(hand);
	PokeVerdict p;
	if (out) {
		p = StepPoke(poke, sample, s.poke);
	} else {
		poke = PokeState{};
	}
	v.press = p.press;
	v.cursorX = sample.pixelX;
	v.cursorY = sample.pixelY;
	v.lengthMetres = math::Sqrt((tip - hand.position).LengthSquared());
	if (t.tablet == nullptr) {
		touch = TabletTouchState{};
		v.hover = p.hover;
		v.mouseDown = p.held;
		return v;
	}
	// Where the touch begins and ends is the contact's: the glass gives no
	// resistance, so the tip may go through it and let go behind it.
	const bool moving = touch.phase == TabletTouchPhase::Scrolling || touch.phase == TabletTouchPhase::Turned ||
	                    touch.phase == TabletTouchPhase::Dragging;
	TabletContactLimits limits;
	limits.press = s.poke.press;
	limits.release = s.poke.release;
	limits.begin = s.poke.through;
	const TabletContact contact =
		StepTabletContact(touch.contact, out, sample.inside, sample.depth, moving, limits, TabletTouchStill(touch));
	TabletTouchInput in;
	in.onGlass = contact.onGlass;
	in.touching = contact.touching;
	in.glassX = Dot(tip - t.quad.centre, t.quad.right);
	in.glassY = -Dot(tip - t.quad.centre, t.quad.up);
	in.pixelX = sample.pixelX;
	in.pixelY = sample.pixelY;
	in.onScrollBar = onScrollBar;
	in.turns = TabletTouchTurns(TabletLookOf(*t.tablet));
	in.scrolls = TabletTouchScrolls(TabletLookOf(*t.tablet));
	const TabletTouchVerdict touched = StepTabletTouch(touch, in, f.dtSeconds);
	// On the tablet the finger leads from 5 cm, held to 7 (vr/TouchHandover.h),
	// not from the poke's hover distance: the laser leads until then, fading.
	const TouchHandover lead = StepTouchHandover(touch.handover, true, out, sample.inside, sample.depth);
	v.hover = lead.fingerLeads || touched.active;
	v.mouseDown = touched.mouseDown;
	v.wheel = touched.wheel;
	v.pageStep = touched.pageStep;
	v.report = TabletTouchReport{touched.event, contact.end, contact.deepest, touched.travel, touched.seconds};
	v.report.laserAlpha = v.hover ? 0.0f : lead.laserAlpha;
	v.report.leadChanged = lead.changed;
	v.report.leadDepthMetres = sample.depth;
	v.report.mark = lead.mark;
	v.report.markShare = lead.markShare;
	// Drawn on the face where the tip is, not where a figure's pixel maps to in the layer.
	const PokeSample onFace = PokeOnQuad(tip, t.quad.centre, t.quad.right, t.quad.up, t.quad.width, t.quad.height,
	                                     t.pixelWidth, t.pixelHeight);
	v.report.markX = onFace.pixelX + t.pixelLeft;
	v.report.markY = onFace.pixelY + t.pixelTop;
	if (touched.pinned) {
		v.cursorX = touched.pinX;
		v.cursorY = touched.pinY;
	}
	return v;
}

}  // namespace obvr::vr
