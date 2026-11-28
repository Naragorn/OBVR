#pragma once

// The tablet as a scroll (docs/inventory-tablet-spec.md, stage 3; Nadi,
// 2026-11-18: "go"): both hands empty, together at the inventory ring
// (vr::StowSettings - the gold ring at the chest; Nadi, 2026-11-22: "lass uns
// die an den stow spot koppeln"), both grips closed - pulling them apart unrolls the tablet between them, by the
// measured distance; fully open, letting go with either hand leaves it in the
// off hand (vr/TabletHolder.h; Nadi, 2026-11-28 - before, the hand that kept
// its grip). Let go short of fully open, it rolls back and the menus close. Held,
// the other hand's grip closed near the holding hand rolls it up again as the
// hands come together. The game's menus open the moment the unrolling starts
// (the menu key, as the left B sends it) and close as it rolls back or up.
// With a weapon, a shield or anything else in a hand the gesture is not
// taken: the left B opens the tablet, which unrolls in the hand by itself
// (vr::TabletUnrollShare). Pure; covered by tablet_scroll_test.
//
// How close the hands are is read on the controllers; where they are (the
// ring) and how far it is unrolled on the drawn palms, which the tablet
// stands between (section 27 of the spec: the palms sit 13 to 15 cm off the
// controllers and never come closer than about 0.2 m).

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/TabletHolder.h"

namespace obvr::vr {

struct TabletScrollLimits {
	// The controllers this close: the hands together, the scroll may begin.
	// Nadi's five tries of 2026-11-27 had them 0.02 to 0.09 m apart, while the
	// drawn palms - 4 to 5 cm outward of each controller and turned with the
	// wrists - read 0.24 to 0.29.
	float togetherMetres = 0.20f;
	// Both grips closed this soon after each other: the squeeze that begins it.
	float pairSeconds = 0.5f;
	// The drawn hands' middle this far past the inventory ring's own reach
	// still counts: two hands side by side span 20 cm and more, the ring is a
	// sphere round one hand's place.
	float ringMarginMetres = 0.10f;
	// The drawn palms pulled at least this much further apart than where the
	// scroll began: fully open (else when they stand the tablet's width apart).
	float fullPullMetres = 0.12f;
	// The drawn palms' distance with the hands together, until a scroll has
	// begun and measured it (rolling up a tablet the B opened): Nadi's tries
	// of 2026-11-27, 0.24 to 0.29.
	float togetherPalmMetres = 0.24f;
	float pulledShare = 0.25f;  // unrolled this far once, brought back to rolledShare: it rolls back
	float rolledShare = 0.10f;  // unrolled no further than this: rolled up
	float freeEndReachMetres = 0.10f;  // the free hand this near the tablet's free end to take it
	float closeWaitSeconds = 1.0f;  // a close owed to a menu not yet open, kept this long
};

struct TabletScrollFrame {
	bool allowed = false;     // Full VR, in the world, the tablet on, no other menu of OBVR's up
	bool handsEmpty = false;  // nothing drawn or held in either hand
	bool menuMode = false;    // a game menu is up
	bool tabletUp = false;    // the menus up are the tablet's and it is placed
	bool tracked = false;     // both hands and the head
	NiPoint3 left{0.0f, 0.0f, 0.0f};   // the drawn hands (their palms), tracking space
	NiPoint3 right{0.0f, 0.0f, 0.0f};
	// The controllers themselves, tracking space: how close the hands are.
	NiPoint3 leftController{0.0f, 0.0f, 0.0f};
	NiPoint3 rightController{0.0f, 0.0f, 0.0f};
	NiPoint3 head{0.0f, 0.0f, 0.0f};
	NiPoint3 headForward{0.0f, 0.0f, -1.0f};  // level, unit
	// The inventory ring, where the scroll begins: its centre right, forward and
	// up from the eyes along the head's heading (vr::BodyRelative's frame),
	// and its reach - the player's own place for it (vr::StowSettings, placed
	// in the guide). The drawn hands' middle at it.
	NiPoint3 ring{0.0f, 0.17f, -0.33f};
	float ringRadius = 0.16f;
	bool grip[2] = {false, false};  // left, right
	float fullMetres = 0.34f;       // the tablet's width (its height up and down): fully open this far apart
	// The held tablet's free end - the middle of the edge opposite the hand
	// that holds it, tracking space: the other hand closes its grip there to
	// roll it up (Nadi, 2026-11-18: closing it anywhere over the page took the
	// item under it out of the inventory).
	bool freeEndValid = false;
	NiPoint3 freeEnd{0.0f, 0.0f, 0.0f};
	float dt = 0.0f;
	// The roles swapped this frame (vr::AssignHandRoles): the hands above are
	// roles; a switch between the pair's grips is read against it.
	bool rolesSwapped = false;
};

enum class TabletScrollPhase : UInt8 { Idle, Unrolling, Held, RollingUp, AwaitRelease };

// Why both grips closing did not begin the scroll, for the log (a gesture
// that does nothing and says nothing cannot be learned).
enum class TabletScrollRefusal : UInt8 { None, NotAllowed, HandsHold, NotTracked, Apart, OutOfRing, Late };

struct TabletScrollState {
	TabletScrollPhase phase = TabletScrollPhase::Idle;
	float gripAge[2] = {-1.0f, -1.0f};  // seconds each grip has been closed; < 0 open
	float amount = 0.0f;
	float base = 0.0f;         // the drawn palms' distance where it began, or rolls up to
	bool baseSeen = false;     // a scroll has begun and measured it
	bool pulled = false;       // unrolled past pulledShare since it began
	bool holderRight = false;  // which hand holds it once open
	bool holderTop = false;    // and whether that one was the upper hand as it let go
	bool menuSent = false;     // the menu key sent to open; the menus not yet seen
	float closeOwed = -1.0f;   // seconds a close has waited for the menus to be up; < 0 none
	bool lastGrip[2] = {false, false};
	bool rolesAtGrip = false;  // the roles as the pair's first grip closed from both open
	bool rolledUp = false;     // the scroll sent its close; kept until menu mode is over
};

struct TabletScrollResult {
	TabletScrollPhase phase = TabletScrollPhase::Idle;
	bool menuKey = false;      // send the menu key this frame (open or close)
	bool betweenHands = false; // the tablet stands between the hands, unrolled by `amount`
	float amount = 0.0f;       // 0 rolled up .. 1 fully open
	bool holderRight = false;  // held by the right hand (the left points and taps)
	bool holderTop = false;    // the hand that keeps it was the upper one
	bool gesture = false;      // the scroll owns the frame: unrolling or rolling up
	// Rolled up or back and its close sent: nothing of it to show while the
	// menus are still up (vr::TabletDrawnShare).
	bool rolledUp = false;
	TabletScrollRefusal refused = TabletScrollRefusal::None;  // both grips closed and the scroll not begun: why
	float refusedBy = 0.0f;    // the measure that refused it (metres, or the seconds between the grips)
	// For the handover's gate (vr/GripClaim.h): the hands where the gesture can
	// begin (allowed, no menu, both empty, the controllers together at the
	// ring), the frame it began, both grips open.
	bool zone = false;
	bool began = false;
	bool gripsOpen = false;
};

inline float Distance(const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 d = b - a;
	return math::Sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// How far it is unrolled with the drawn palms `distance` apart, having begun
// (or rolling up to) `base`: open when they stand the tablet's width apart,
// or fullPullMetres past the base when that is further.
inline float UnrollAmount(float distance, float base, float fullMetres, const TabletScrollLimits& l) {
	const float span = fullMetres - base > l.fullPullMetres ? fullMetres - base : l.fullPullMetres;
	const float a = (distance - base) / span;
	return a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
}

// A point in tracking space as right, forward and up from the eyes along the
// level heading - vr::BodyRelative's frame, where the ring's place is kept.
inline NiPoint3 ScrollBodyRelative(const TabletScrollFrame& f, const NiPoint3& point) {
	const NiPoint3 d = point - f.head;
	const float fx = f.headForward.x;
	const float fz = f.headForward.z;
	return NiPoint3{d.x * -fz + d.z * fx, d.x * fx + d.z * fz, d.y};
}

// How far a point is outside the inventory ring, metres; 0 or less in it.
inline float OutsideRing(const TabletScrollFrame& f, const NiPoint3& hand) {
	const NiPoint3 rel = ScrollBodyRelative(f, hand);
	return Distance(rel, f.ring) - f.ringRadius;
}

// The hands together (the controllers) at the inventory ring (the drawn
// hands' middle, with the margin) - or what of that is not so, with its
// measure: the controllers' distance, or how far the middle is past the
// counted reach.
inline TabletScrollRefusal ScrollPoseRefusal(const TabletScrollFrame& f, const TabletScrollLimits& l, float& by) {
	by = 0.0f;
	if (!f.tracked) {
		return TabletScrollRefusal::NotTracked;
	}
	const float together = Distance(f.leftController, f.rightController);
	if (!(together <= l.togetherMetres)) {
		by = together;
		return TabletScrollRefusal::Apart;
	}
	const float outside = OutsideRing(f, (f.left + f.right) * 0.5f) - l.ringMarginMetres;
	if (!(outside <= 0.0f)) {
		by = outside;
		return TabletScrollRefusal::OutOfRing;
	}
	return TabletScrollRefusal::None;
}

// The second grip closed this frame, `waited` seconds after the first: the
// scroll begins (None) or why not. With the controllers further apart than
// twice the together reach, a hand lost or another menu up it is no try -
// an ordinary two-handed grab - and nothing is said (`said` false).
inline TabletScrollRefusal ScrollTry(const TabletScrollFrame& f, const TabletScrollLimits& l, float waited, float& by,
                                     bool& said) {
	by = 0.0f;
	const float together = f.tracked ? Distance(f.leftController, f.rightController) : -1.0f;
	said = !f.menuMode && together >= 0.0f && together <= l.togetherMetres * 2.0f;
	if (!said) {
		return TabletScrollRefusal::NotTracked;
	}
	if (!f.allowed) {
		return TabletScrollRefusal::NotAllowed;
	}
	if (!f.handsEmpty) {
		return TabletScrollRefusal::HandsHold;
	}
	const TabletScrollRefusal pose = ScrollPoseRefusal(f, l, by);
	if (pose != TabletScrollRefusal::None) {
		return pose;
	}
	by = waited;
	return waited <= l.pairSeconds ? TabletScrollRefusal::None : TabletScrollRefusal::Late;
}

// The second grip closed with the hands together at the ring, soon after the
// first: unrolling begins and the menus open. Opened another way (the left
// B): held by the off hand.
inline void StepIdle(TabletScrollState& s, const TabletScrollFrame& f, const TabletScrollLimits& l, const bool (&down)[2],
                     float apart, TabletScrollResult& r) {
	if (f.allowed && f.tabletUp && f.menuMode) {
		const TabletHolder holder = TabletHolderOnceOpen(TabletOpenedBy::Button, f.left.y, f.right.y, false, f.rolesSwapped);
		s.phase = TabletScrollPhase::Held;
		s.holderRight = holder.weaponHand;
		s.holderTop = holder.onTop;
		return;
	}
	if (!(f.grip[0] && f.grip[1] && (down[0] || down[1]))) {
		return;
	}
	// Both this frame: no wait; else how long the first has been closed
	// (AgeGrips has counted this frame in).
	const float waited = down[0] && down[1] ? 0.0f : (down[0] ? s.gripAge[1] : s.gripAge[0]);
	bool said = false;
	float by = 0.0f;
	const TabletScrollRefusal why = ScrollTry(f, l, waited, by, said);
	if (why != TabletScrollRefusal::None) {
		r.refused = said ? why : TabletScrollRefusal::None;
		r.refusedBy = said ? by : 0.0f;
		return;
	}
	s.phase = TabletScrollPhase::Unrolling;
	s.base = apart;
	s.baseSeen = true;
	s.pulled = false;
	s.amount = 0.0f;
	s.menuSent = true;
	r.menuKey = true;
}

// Held: the free hand's grip closed near it takes the free end; rolling up,
// it follows the hands until they are back where a scroll begins (rolled)
// or the grip opens (held). False when the scroll is to close.
inline bool StepHeld(TabletScrollState& s, const TabletScrollFrame& f, const TabletScrollLimits& l,
                     const bool (&down)[2], float apart) {
	const int free = s.holderRight ? 0 : 1;
	if (s.phase == TabletScrollPhase::Held) {
		if (!f.menuMode && !s.menuSent) {
			s.phase = TabletScrollPhase::AwaitRelease;  // the menus closed another way
		} else if (f.tracked && down[free] && f.freeEndValid &&
		           Distance(free == 0 ? f.left : f.right, f.freeEnd) <= l.freeEndReachMetres) {
			s.phase = TabletScrollPhase::RollingUp;
			if (!s.baseSeen) {
				s.base = l.togetherPalmMetres;
			}
			s.amount = UnrollAmount(apart, s.base, f.fullMetres, l);
		}
		return true;
	}
	if (!f.tracked || !f.grip[free]) {
		s.phase = TabletScrollPhase::Held;
		s.amount = 1.0f;
		return true;
	}
	s.amount = UnrollAmount(apart, s.base, f.fullMetres, l);
	return s.amount > l.rolledShare;
}

// Unrolling: both grips closed it follows the hands, brought back together
// after a pull it rolls back; one let go fully open, the off hand keeps it.
// False when the scroll is to close.
inline bool StepUnrolling(TabletScrollState& s, const TabletScrollFrame& f, const TabletScrollLimits& l, float apart) {
	if (!f.allowed || !f.tracked) {
		return false;
	}
	if (f.grip[0] && f.grip[1]) {
		s.amount = UnrollAmount(apart, s.base, f.fullMetres, l);
		s.pulled = s.pulled || s.amount >= l.pulledShare;
		return !(s.pulled && s.amount <= l.rolledShare);
	}
	if ((f.grip[0] || f.grip[1]) && s.amount >= 1.0f) {
		// Whichever hand let go, the off hand holds it (vr/TabletHolder.h). The
		// scroll works in roles; the controller is the caller's (ControlsOut).
		// The sides as the pair's first grip closed decide, so a switch since
		// cannot move it to the weapon hand.
		const TabletHolder holder = TabletHolderOnceOpen(TabletOpenedBy::Scroll, f.left.y, f.right.y,
		                                                 f.rolesSwapped != s.rolesAtGrip, f.rolesSwapped);
		s.phase = TabletScrollPhase::Held;
		s.holderRight = holder.weaponHand;
		s.holderTop = holder.onTop;
		s.amount = 1.0f;
		return true;
	}
	return false;  // let go short of fully open, or with both hands
}

// How long each grip has been closed, this frame's included.
inline void AgeGrips(TabletScrollState& s, const TabletScrollFrame& f, const bool (&down)[2], float dt) {
	for (int h = 0; h < 2; ++h) {
		if (!f.grip[h]) {
			s.gripAge[h] = -1.0f;
		} else if (down[h] || s.gripAge[h] < 0.0f) {
			s.gripAge[h] = 0.0f;
		} else {
			s.gripAge[h] += dt;
		}
	}
}

inline TabletScrollResult StepTabletScroll(TabletScrollState& s, const TabletScrollFrame& f,
                                           const TabletScrollLimits& l = TabletScrollLimits{}) {
	TabletScrollResult r;
	const bool down[2] = {f.grip[0] && !s.lastGrip[0], f.grip[1] && !s.lastGrip[1]};
	const float dt = f.dt > 0.0f && f.dt < 0.25f ? f.dt : 0.0f;
	const float apart = f.tracked ? Distance(f.left, f.right) : -1.0f;
	// A close owed to menus not yet open: sent the frame they are.
	if (s.closeOwed >= 0.0f) {
		if (f.menuMode) {
			r.menuKey = true;
			s.closeOwed = -1.0f;
		} else {
			s.closeOwed += dt;
			if (s.closeOwed >= l.closeWaitSeconds) {
				s.closeOwed = -1.0f;
			}
		}
	}
	const auto close = [&]() {
		if (f.menuMode) {
			r.menuKey = true;
		} else if (s.menuSent) {
			s.closeOwed = 0.0f;
		}
		s.menuSent = false;
		s.phase = TabletScrollPhase::AwaitRelease;
		s.rolledUp = true;
	};
	if (f.menuMode) {
		s.menuSent = false;
	}
	AgeGrips(s, f, down, dt);
	// The sides as the pair's first grip closes from both open (vr/TabletHolder.h).
	if ((down[0] || down[1]) && !s.lastGrip[0] && !s.lastGrip[1]) {
		s.rolesAtGrip = f.rolesSwapped;
	}
	// Where the gesture can begin, for the handover's gate (vr/GripClaim.h).
	float by = 0.0f;
	r.zone = s.phase == TabletScrollPhase::Idle && f.allowed && !f.menuMode && f.handsEmpty &&
	         ScrollPoseRefusal(f, l, by) == TabletScrollRefusal::None;
	const bool wasIdle = s.phase == TabletScrollPhase::Idle;
	switch (s.phase) {
	case TabletScrollPhase::Idle:
		StepIdle(s, f, l, down, apart, r);
		break;
	case TabletScrollPhase::Unrolling:
		if (!StepUnrolling(s, f, l, apart)) {
			close();
		}
		break;
	case TabletScrollPhase::Held:
	case TabletScrollPhase::RollingUp:
		if (!StepHeld(s, f, l, down, apart)) {
			close();
		}
		break;
	case TabletScrollPhase::AwaitRelease:
		if (!f.grip[0] && !f.grip[1] && !f.menuMode) {
			s.phase = TabletScrollPhase::Idle;
			s.amount = 0.0f;
		}
		break;
	}
	s.lastGrip[0] = f.grip[0];
	s.lastGrip[1] = f.grip[1];
	// Rolled up until its menus are gone - or, owed, until they came and went.
	s.rolledUp = s.rolledUp && (f.menuMode || s.closeOwed >= 0.0f);
	r.rolledUp = s.rolledUp;
	r.phase = s.phase;
	r.amount = s.amount;
	r.holderRight = s.holderRight;
	r.holderTop = s.holderTop;
	r.betweenHands = s.phase == TabletScrollPhase::Unrolling || s.phase == TabletScrollPhase::RollingUp;
	r.gesture = r.betweenHands;
	r.began = wasIdle && s.phase == TabletScrollPhase::Unrolling;
	r.gripsOpen = !f.grip[0] && !f.grip[1];
	return r;
}

// The unrolling of a tablet opened by the left B: in the hand by itself, over
// kTabletUnrollSeconds.
inline constexpr float kTabletUnrollSeconds = 0.35f;

inline float TabletUnrollShare(float secondsUp) {
	if (!(secondsUp > 0.0f)) {
		return 0.0f;
	}
	return secondsUp >= kTabletUnrollSeconds ? 1.0f : secondsUp / kTabletUnrollSeconds;
}

}  // namespace obvr::vr
