#pragma once

// The left arm through a draw or a sheathe, for the log (Nadi, Windows
// 2026-11-27: "the shield, when equipped, still flickers around in the hand
// when I draw or sheathe the weapon"; free-weapons-spec, "Windows run of
// 2026-11-27"). An instrument, not a fix: the cause is not read yet.
//
// What is read [R]: the hand pin (game/HandBones.cpp PinHandBone) places the
// first-person forearm so that the hand, with the local turn the animation
// gave it, lands on the controller; the shield hangs on "Bip01 L ForearmTwist"
// (camera/FrameLogic.h). What is guessed [G]: the twist is the forearm's child
// beside the hand, so whatever the draw's animation does to the wrist (the
// hand's local turn) or to the twist swings the forearm - and the shield -
// round a hand that stays put. The trace tells: through one draw or sheathe,
// how far the twist bone (the shield) moved against the pinned hand from
// frame to frame and from where it sat, and how far the hand's local, the
// twist's local and the controller turned a frame - and the pins' frames
// skipped, should the flicker be a frame the pin did not run.
//
// Pure; arm_draw_trace_test. Read by game/ArmDrawTrace.cpp, one line a window.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "game/PullTurnLogic.h"

namespace obvr::game {

// One left-hand pin while the player's action may be the draw or the sheathe.
struct ArmDrawSample {
	bool inDraw = false;       // the player's action is the draw or the sheathe (SheathStillFor)
	bool sheathe = false;      // the sheathe
	bool shield = false;       // a shield worn
	UInt32 pinFrame = 0;       // the pins' frame number (BeginHandPins)
	NiMatrix33 handLocal = NiMatrix33::Identity();   // the hand bone's local turn, the animation's
	NiMatrix33 twistLocal = NiMatrix33::Identity();  // the twist bone's local turn
	bool twistValid = false;
	NiMatrix33 controller = NiMatrix33::Identity();  // the pinned hand's turn in the camera's frame
	NiPoint3 twistInHand{0.0f, 0.0f, 0.0f};          // the twist bone's origin in the pinned hand's frame, units
};

struct ArmDrawTrace {
	bool running = false;
	bool sheathe = false;
	bool shield = false;
	UInt32 frames = 0;
	UInt32 skipped = 0;       // pins' frames without a left pin inside the window
	ArmDrawSample first;
	ArmDrawSample last;
	float mostHand = 0.0f;        // degrees a frame
	float mostTwist = 0.0f;
	float mostController = 0.0f;
	float mostStep = 0.0f;        // the twist against the hand, units a frame
	UInt32 mostStepAt = 0;        // the window's frame of it, from 1
	float mostAway = 0.0f;        // the twist against the hand, units from the window's first frame
};

inline float DegreesBetween(const NiMatrix33& a, const NiMatrix33& b) {
	return AngleBetweenRotations(a, b) * (180.0f / math::kPi);
}

inline float DistanceUnits(const NiPoint3& a, const NiPoint3& b) { return math::Sqrt((a - b).LengthSquared()); }

// One pin. True when a window just ended, with it in `done`.
inline bool StepArmDrawTrace(ArmDrawTrace& t, const ArmDrawSample& s, ArmDrawTrace& done) {
	if (!s.inDraw) {
		if (!t.running) {
			return false;
		}
		done = t;
		t = ArmDrawTrace{};
		return true;
	}
	if (!t.running) {
		t = ArmDrawTrace{};
		t.running = true;
		t.sheathe = s.sheathe;
		t.shield = s.shield;
		t.frames = 1;
		t.first = s;
		t.last = s;
		return false;
	}
	++t.frames;
	if (s.pinFrame > t.last.pinFrame + 1) {
		t.skipped += s.pinFrame - t.last.pinFrame - 1;
	}
	const float hand = DegreesBetween(t.last.handLocal, s.handLocal);
	const float controller = DegreesBetween(t.last.controller, s.controller);
	t.mostHand = hand > t.mostHand ? hand : t.mostHand;
	t.mostController = controller > t.mostController ? controller : t.mostController;
	if (s.twistValid && t.last.twistValid) {
		const float twist = DegreesBetween(t.last.twistLocal, s.twistLocal);
		const float step = DistanceUnits(t.last.twistInHand, s.twistInHand);
		t.mostTwist = twist > t.mostTwist ? twist : t.mostTwist;
		if (step > t.mostStep) {
			t.mostStep = step;
			t.mostStepAt = t.frames;
		}
	}
	if (s.twistValid && t.first.twistValid) {
		const float away = DistanceUnits(t.first.twistInHand, s.twistInHand);
		t.mostAway = away > t.mostAway ? away : t.mostAway;
	}
	t.shield = t.shield || s.shield;
	t.last = s;
	return false;
}

}  // namespace obvr::game
