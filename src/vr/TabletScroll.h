#pragma once

// The tablet as a scroll (docs/inventory-tablet-spec.md, stage 3; Nadi,
// 2026-11-18: "go"): both hands empty, close together before the body, both
// grips closed - pulling them apart unrolls the tablet between them, by the
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
	float minimumForward = 0.15f;   // the hands ahead of the eyes, metres
	float maximumForward = 0.75f;
	float minimumHeight = -0.70f;   // and below them, up to a little above
	float maximumHeight = 0.10f;
	float rollUpReachMetres = 0.25f;  // the free hand this near the holding one to take the free end
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
	bool grip[2] = {false, false};  // left, right
	float fullMetres = 0.34f;       // the tablet's width: fully open when the hands are this far apart
	float dt = 0.0f;
};

enum class TabletScrollPhase : UInt8 { Idle, Ready, Unrolling, Held, RollingUp, AwaitRelease };

struct TabletScrollState {
	TabletScrollPhase phase = TabletScrollPhase::Idle;
	float dwell = 0.0f;
	float amount = 0.0f;
	bool holderRight = false;  // which hand holds it once open
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
	bool gesture = false;      // the scroll owns the frame: unrolling or rolling up
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

// Both hands close together, ahead of and below the eyes.
inline bool ScrollPoseHeld(const TabletScrollFrame& f, const TabletScrollLimits& l) {
	if (!f.tracked) {
		return false;
	}
	const float apart = Distance(f.left, f.right);
	if (!(apart >= l.closedMetres * 0.5f) || !(apart <= l.poseMetres)) {
		return false;
	}
	for (int i = 0; i < 2; ++i) {
		const NiPoint3 rel = (i == 0 ? f.left : f.right) - f.head;
		const float forward = rel.x * f.headForward.x + rel.y * f.headForward.y + rel.z * f.headForward.z;
		if (!(forward >= l.minimumForward && forward <= l.maximumForward && rel.y >= l.minimumHeight &&
		      rel.y <= l.maximumHeight)) {
			return false;
		}
	}
	return true;
}

// The pose held, ready, both grips closed: unrolling begins and the menus
// open. Opened another way (the left B): held by the left hand.
inline void StepIdle(TabletScrollState& s, const TabletScrollFrame& f, const TabletScrollLimits& l, const bool (&down)[2],
                     float dt, float apart, TabletScrollResult& r) {
	if (f.allowed && f.tabletUp && f.menuMode) {
		s.phase = TabletScrollPhase::Held;
		s.holderRight = false;
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
		} else if (f.tracked && down[free] && apart >= 0.0f && apart <= l.rollUpReachMetres + f.fullMetres * 0.5f) {
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
