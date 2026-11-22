#pragma once

// The tablet as a scroll (docs/inventory-tablet-spec.md, stage 3; Nadi,
// 2026-11-18: "go"): both hands empty, close together in the inventory ring
// (vr::StowSettings - the gold ring at the chest; Nadi, 2026-11-22: "lass uns
// die an den stow spot koppeln"), both grips closed - pulling them apart unrolls the tablet between them, by the
// measured distance; fully open, letting go with one hand leaves it in the
// other. Let go short of fully open, it rolls back and the menus close. Held,
// the other hand's grip closed near the holding hand rolls it up again as the
// hands come together. The game's menus open the moment the unrolling starts
// (the menu key, as the left B sends it) and close as it rolls back or up.
// With a weapon, a shield or anything else in a hand the gesture is not
// taken: the left B opens the tablet, which unrolls in the hand by itself
// (vr::TabletUnrollShare). Pure; covered by tablet_scroll_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct TabletScrollLimits {
	float closedMetres = 0.08f;     // the hands this close: rolled up
	float poseMetres = 0.20f;       // and at most this far apart to begin
	float dwellSeconds = 0.35f;     // the pose held, grips open, before it counts
	float freeEndReachMetres = 0.10f;  // the free hand this near the tablet's free end to take it
	float closeWaitSeconds = 1.0f;  // a close owed to a menu not yet open, kept this long
};

struct TabletScrollFrame {
	bool allowed = false;     // Full VR, in the world, the tablet on, no other menu of OBVR's up
	bool handsEmpty = false;  // nothing drawn or held in either hand
	bool menuMode = false;    // a game menu is up
	bool tabletUp = false;    // the menus up are the tablet's and it is placed
	bool tracked = false;     // both hands and the head
	NiPoint3 left{0.0f, 0.0f, 0.0f};   // the drawn hands, tracking space
	NiPoint3 right{0.0f, 0.0f, 0.0f};
	NiPoint3 head{0.0f, 0.0f, 0.0f};
	NiPoint3 headForward{0.0f, 0.0f, -1.0f};  // level, unit
	// The inventory ring, where the scroll begins: its centre right, forward and
	// up from the eyes along the head's heading (vr::BodyRelative's frame),
	// and its reach - the player's own place for it (vr::StowSettings, placed
	// in the guide). Both hands in it.
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
};

enum class TabletScrollPhase : UInt8 { Idle, Ready, Unrolling, Held, RollingUp, AwaitRelease };

// Why both grips closing did not begin the scroll, for the log (a gesture
// that does nothing and says nothing cannot be learned).
enum class TabletScrollRefusal : UInt8 { None, NotAllowed, HandsHold, NotTracked, Apart, OutOfRing, NoDwell };

struct TabletScrollState {
	TabletScrollPhase phase = TabletScrollPhase::Idle;
	float dwell = 0.0f;
	float amount = 0.0f;
	bool holderRight = false;  // which hand holds it once open
	bool holderTop = false;    // and whether that one was the upper hand as it let go
	bool menuSent = false;     // the menu key sent to open; the menus not yet seen
	float closeOwed = -1.0f;   // seconds a close has waited for the menus to be up; < 0 none
	bool lastGrip[2] = {false, false};
};

struct TabletScrollResult {
	TabletScrollPhase phase = TabletScrollPhase::Idle;
	bool menuKey = false;      // send the menu key this frame (open or close)
	bool betweenHands = false; // the tablet stands between the hands, unrolled by `amount`
	float amount = 0.0f;       // 0 rolled up .. 1 fully open
	bool holderRight = false;  // held by the right hand (the left points and taps)
	bool holderTop = false;    // the hand that keeps it was the upper one
	bool gesture = false;      // the scroll owns the frame: unrolling or rolling up
	TabletScrollRefusal refused = TabletScrollRefusal::None;  // both grips closed and the scroll not begun: why
	float refusedBy = 0.0f;    // the measure that refused it (metres, or the dwell's seconds)
};

inline float Distance(const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 d = b - a;
	return math::Sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// How far it is unrolled at this distance between the hands.
inline float UnrollAmount(float distance, const TabletScrollLimits& l, float fullMetres) {
	const float full = fullMetres > l.poseMetres ? fullMetres : l.poseMetres + 0.05f;
	const float a = (distance - l.closedMetres) / (full - l.closedMetres);
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

// How far a hand is outside the inventory ring, metres; 0 or less in it.
inline float OutsideRing(const TabletScrollFrame& f, const NiPoint3& hand) {
	const NiPoint3 rel = ScrollBodyRelative(f, hand);
	return Distance(rel, f.ring) - f.ringRadius;
}

// Both hands close together in the inventory ring - or what of that is not
// so, with its measure (for the ring, how far the further hand is out of it).
inline TabletScrollRefusal ScrollPoseRefusal(const TabletScrollFrame& f, const TabletScrollLimits& l, float& by) {
	by = 0.0f;
	if (!f.tracked) {
		return TabletScrollRefusal::NotTracked;
	}
	const float apart = Distance(f.left, f.right);
	if (!(apart >= l.closedMetres * 0.5f) || !(apart <= l.poseMetres)) {
		by = apart;
		return TabletScrollRefusal::Apart;
	}
	const float left = OutsideRing(f, f.left);
	const float right = OutsideRing(f, f.right);
	const float outside = left > right ? left : right;
	if (!(outside <= 0.0f)) {
		by = outside;
		return TabletScrollRefusal::OutOfRing;
	}
	return TabletScrollRefusal::None;
}

inline bool ScrollPoseHeld(const TabletScrollFrame& f, const TabletScrollLimits& l) {
	float by = 0.0f;
	return ScrollPoseRefusal(f, l, by) == TabletScrollRefusal::None;
}

// Both grips closed, the second this frame, with the hands no further apart
// than twice the pose's reach - a try at the scroll - and it does not begin:
// why. Hands further apart are an ordinary two-handed grab and say nothing.
inline TabletScrollRefusal ScrollRefusal(const TabletScrollState& s, const TabletScrollFrame& f,
                                         const TabletScrollLimits& l, float apart, float& by) {
	by = 0.0f;
	if (f.menuMode || !(apart >= 0.0f) || apart > l.poseMetres * 2.0f) {
		return TabletScrollRefusal::None;
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
	by = s.dwell;
	return TabletScrollRefusal::NoDwell;
}

// The pose held, ready, both grips closed: unrolling begins and the menus
// open. Opened another way (the left B): held by the left hand.
inline void StepIdle(TabletScrollState& s, const TabletScrollFrame& f, const TabletScrollLimits& l, const bool (&down)[2],
                     float dt, float apart, TabletScrollResult& r) {
	if (f.allowed && f.tabletUp && f.menuMode) {
		s.phase = TabletScrollPhase::Held;
		s.holderRight = false;
		s.holderTop = false;
		return;
	}
	const bool pose = f.allowed && f.handsEmpty && !f.menuMode && ScrollPoseHeld(f, l);
	if (s.phase == TabletScrollPhase::Ready && pose && (down[0] || down[1]) && f.grip[0] && f.grip[1]) {
		s.phase = TabletScrollPhase::Unrolling;
		s.amount = UnrollAmount(apart, l, f.fullMetres);
		s.menuSent = true;
		r.menuKey = true;
		return;
	}
	if ((down[0] || down[1]) && f.grip[0] && f.grip[1]) {
		r.refused = ScrollRefusal(s, f, l, apart, r.refusedBy);
	}
	if (pose && !f.grip[0] && !f.grip[1]) {
		s.dwell += dt;
		s.phase = s.dwell >= l.dwellSeconds ? TabletScrollPhase::Ready : TabletScrollPhase::Idle;
	} else if (!pose || (s.phase != TabletScrollPhase::Ready)) {
		s.dwell = 0.0f;
		s.phase = TabletScrollPhase::Idle;
	}
}

// Held: the free hand's grip closed near it takes the free end; rolling up,
// it follows the hands until they meet (closed) or the grip opens (held).
// False when the scroll is to close.
inline bool StepHeld(TabletScrollState& s, const TabletScrollFrame& f, const TabletScrollLimits& l,
                     const bool (&down)[2], float apart) {
	const int free = s.holderRight ? 0 : 1;
	if (s.phase == TabletScrollPhase::Held) {
		if (!f.menuMode && !s.menuSent) {
			s.phase = TabletScrollPhase::AwaitRelease;  // the menus closed another way
		} else if (f.tracked && down[free] && f.freeEndValid &&
		           Distance(free == 0 ? f.left : f.right, f.freeEnd) <= l.freeEndReachMetres) {
			s.phase = TabletScrollPhase::RollingUp;
			s.amount = UnrollAmount(apart, l, f.fullMetres);
		}
		return true;
	}
	if (!f.tracked || !f.grip[free]) {
		s.phase = TabletScrollPhase::Held;
		s.amount = 1.0f;
		return true;
	}
	s.amount = UnrollAmount(apart, l, f.fullMetres);
	return apart > l.closedMetres;
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
	};
	if (f.menuMode) {
		s.menuSent = false;
	}
	switch (s.phase) {
	case TabletScrollPhase::Idle:
	case TabletScrollPhase::Ready:
		StepIdle(s, f, l, down, dt, apart, r);
		break;
	case TabletScrollPhase::Unrolling: {
		if (!f.allowed || !f.tracked) {
			close();
			break;
		}
		const float amount = UnrollAmount(apart, l, f.fullMetres);
		if (f.grip[0] && f.grip[1]) {
			s.amount = amount;
			if (apart <= l.closedMetres && s.amount <= 0.0f) {
				close();
			}
			break;
		}
		if ((f.grip[0] || f.grip[1]) && s.amount >= 1.0f) {
			s.phase = TabletScrollPhase::Held;
			s.holderRight = f.grip[1];
			s.holderTop = s.holderRight ? f.right.y > f.left.y : f.left.y > f.right.y;
			s.amount = 1.0f;
			break;
		}
		close();  // let go short of fully open, or with both hands
		break;
	}
	case TabletScrollPhase::Held:
	case TabletScrollPhase::RollingUp:
		if (!StepHeld(s, f, l, down, apart)) {
			close();
		}
		break;
	case TabletScrollPhase::AwaitRelease:
		if (!f.grip[0] && !f.grip[1] && !f.menuMode) {
			s.phase = TabletScrollPhase::Idle;
			s.dwell = 0.0f;
			s.amount = 0.0f;
		}
		break;
	}
	s.lastGrip[0] = f.grip[0];
	s.lastGrip[1] = f.grip[1];
	r.phase = s.phase;
	r.amount = s.amount;
	r.holderRight = s.holderRight;
	r.holderTop = s.holderTop;
	r.betweenHands = s.phase == TabletScrollPhase::Unrolling || s.phase == TabletScrollPhase::RollingUp;
	r.gesture = r.betweenHands;
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
