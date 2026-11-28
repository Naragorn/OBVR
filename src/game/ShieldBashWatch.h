#pragma once

#include "core/Types.h"

namespace obvr::game {

// The shield bash's watch: one log line per attempt, never per frame. Nadi's
// headset run of 2026-11-28 ("mein shield bash hat nicht getroffen obwohl es
// sollte") left no line at all: StepShieldBash returns silently when the
// shield's middle is outside everyone's column, when they cool down from a
// shove, and when the hand is under the speed towards them
// (game/ShieldBashLogic.h). An attempt is the shield moving fast near someone
// - by the drawn shield's speed or the controller's, so a lagged shield's
// attempt is seen from its start - folded frame by frame, and summed up when
// it lands, slows down or they are gone: why it did not land, the frame that
// came closest, the most speed of each. Pure, covered by
// shield_bash_watch_test; the caller is camera/hands/ShieldBashTrace.cpp.
enum class BashAttemptEnd : UInt8 { None, Landed, OutsideColumn, TooSlow, CoolingDown, HandBusy };

// The speed that makes an attempt: half the default gate (ImpactSettings's
// stagger speed, 3 m/s) - a guess for a deliberate jab, not a hand carried.
constexpr float kBashAttemptSpeed = 1.5f;

// One frame, as StepShieldBash judged it against the nearest living actor.
struct BashAttemptFrame {
	bool near = false;          // someone's bound within reach of the shield's middle (the wide look)
	UInt32 actor = 0;           // the nearest
	float speed = 0.0f;         // the drawn shield's speed, m/s
	float controllerSpeed = 0.0f;
	float approach = 0.0f;       // the gate's measure since 2026-11-28 13:55: the approach (game/BashApproachLogic.h)
	float towards = 0.0f;        // this frame's alone: the controller's speed in 3D towards their middle (BashSpeedTowards)
	float towardsGround = 0.0f;  // the same across the ground only, the measure before 2026-11-28 (SpeedTowards)
	bool inColumn = false;      // the gate's place: the shield's middle in their column
	bool cooling = false;       // they cool down from a shove
	bool handBusy = false;      // the hand on a handle or holding someone
	float across = 0.0f;        // the shield's middle from their axis across the ground, units
	float up = 0.0f;            // and above their centre (below: negative)
	float columnWide = 0.0f;    // the column's half width and half height, units
	float columnHigh = 0.0f;
	bool landed = false;        // the bash landed this frame
};

struct BashAttemptState {
	bool open = false;
	bool everInColumn = false;
	bool coolingInColumn = false;
	bool busyInColumn = false;
	UInt32 frames = 0;
	float mostSpeed = 0.0f;
	float mostControllerSpeed = 0.0f;
	BashAttemptFrame best;
};

struct BashAttemptSummary {
	bool ended = false;
	BashAttemptEnd end = BashAttemptEnd::None;
	BashAttemptFrame best;
	UInt32 frames = 0;
	float mostSpeed = 0.0f;
	float mostControllerSpeed = 0.0f;
};

// The frame that came closest: one in the column at the most of the gate's
// measure, the approach (then the frame's own speed towards them); before any
// is, the one nearest the column across the ground.
inline bool CloserToABash(const BashAttemptFrame& f, const BashAttemptFrame& best, bool haveBest) {
	if (!haveBest) {
		return true;
	}
	if (f.inColumn != best.inColumn) {
		return f.inColumn;
	}
	if (!f.inColumn) {
		return f.across < best.across;
	}
	return f.approach != best.approach ? f.approach > best.approach : f.towards > best.towards;
}

inline void FoldBashFrame(BashAttemptState& s, const BashAttemptFrame& f) {
	if (CloserToABash(f, s.best, s.frames > 0)) {
		s.best = f;
	}
	++s.frames;
	s.mostSpeed = f.speed > s.mostSpeed ? f.speed : s.mostSpeed;
	s.mostControllerSpeed = f.controllerSpeed > s.mostControllerSpeed ? f.controllerSpeed : s.mostControllerSpeed;
	if (f.inColumn) {
		s.everInColumn = true;
		s.coolingInColumn = s.coolingInColumn || f.cooling;
		s.busyInColumn = s.busyInColumn || f.handBusy;
	}
}

inline BashAttemptEnd BashAttemptEndOf(const BashAttemptState& s, bool landed) {
	if (landed) {
		return BashAttemptEnd::Landed;
	}
	if (!s.everInColumn) {
		return BashAttemptEnd::OutsideColumn;
	}
	if (s.coolingInColumn) {
		return BashAttemptEnd::CoolingDown;
	}
	return s.busyInColumn ? BashAttemptEnd::HandBusy : BashAttemptEnd::TooSlow;
}

inline BashAttemptSummary StepBashAttempt(BashAttemptState& s, const BashAttemptFrame& f,
                                          float attemptSpeed = kBashAttemptSpeed) {
	BashAttemptSummary r;
	const bool moving = f.near && (f.speed >= attemptSpeed || f.controllerSpeed >= attemptSpeed);
	if (!s.open && !moving && !f.landed) {
		return r;
	}
	if (!s.open) {
		s = BashAttemptState{};
		s.open = true;
	}
	if (moving || f.landed) {
		FoldBashFrame(s, f);
	}
	if (moving && !f.landed) {
		return r;
	}
	r.ended = true;
	r.end = BashAttemptEndOf(s, f.landed);
	r.best = s.best;
	r.frames = s.frames;
	r.mostSpeed = s.mostSpeed;
	r.mostControllerSpeed = s.mostControllerSpeed;
	s = BashAttemptState{};
	return r;
}

inline const char* BashAttemptEndWords(BashAttemptEnd end) {
	switch (end) {
	case BashAttemptEnd::Landed:
		return "landed";
	case BashAttemptEnd::OutsideColumn:
		return "the shield's middle never in their column";
	case BashAttemptEnd::TooSlow:
		return "in their column, too slow towards them";
	case BashAttemptEnd::CoolingDown:
		return "they were cooling down from a shove";
	case BashAttemptEnd::HandBusy:
		return "the hand on a handle or holding someone";
	default:
		return "none";
	}
}

}  // namespace obvr::game
