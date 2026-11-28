#pragma once

// The left arm through a draw or a sheathe, for the log (Nadi, Windows
// 2026-11-27: "the shield, when equipped, still flickers around in the hand
// when I draw or sheathe the weapon"; free-weapons-spec, "Windows run of
// 2026-11-27"). The instrument that read the cause; since 2026-11-28 the
// proof of its fix (game/ShieldHoldLogic.h): a window is any action of the
// arm (the draw, the sheathe, an attack, a block, a stagger ...), the line
// counts the pins the shield was held to the hand and ends with a verdict -
// "held still" when the shield moved under kArmShieldStillUnits and
// kArmShieldStillDegrees a frame against the pinned hand, else "SWUNG". The
// hand's and the twist's local turns stay the animation's, read before the
// hold, so the line shows what the animation did and what the shield did.
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

// One left-hand pin while the player's action may be one of the arm's.
struct ArmDrawSample {
	bool inDraw = false;       // the player's action is one (not none): the draw, the sheathe, an attack, a block ...
	SInt32 action = -1;        // the player's action (game::ReadPlayerAction)
	bool shield = false;       // a shield worn
	bool held = false;         // the shield held to the hand at this pin (game/ShieldHoldLogic.h)
	UInt32 pinFrame = 0;       // the pins' frame number (BeginHandPins)
	NiMatrix33 handLocal = NiMatrix33::Identity();   // the hand bone's local turn, the animation's
	NiMatrix33 twistLocal = NiMatrix33::Identity();  // the twist bone's local turn, the animation's
	bool twistValid = false;
	NiMatrix33 controller = NiMatrix33::Identity();  // the pinned hand's turn in the camera's frame
	NiPoint3 twistInHand{0.0f, 0.0f, 0.0f};          // the twist bone's origin in the pinned hand's frame, units
	NiMatrix33 twistInHandRot = NiMatrix33::Identity();  // and its turn in that frame
};

struct ArmDrawTrace {
	bool running = false;
	SInt32 action = -1;
	bool shield = false;
	UInt32 frames = 0;
	UInt32 skipped = 0;       // pins' frames without a left pin inside the window
	UInt32 heldPins = 0;      // pins with the shield held to the hand
	ArmDrawSample first;
	ArmDrawSample last;
	float mostHand = 0.0f;        // degrees a frame
	float mostTwist = 0.0f;
	float mostController = 0.0f;
	float mostStep = 0.0f;        // the twist against the hand, units a frame
	UInt32 mostStepAt = 0;        // the window's frame of it, from 1
	float mostTurn = 0.0f;        // the twist against the hand, degrees a frame
	float mostAway = 0.0f;        // the twist against the hand, units from the window's first frame
};

// The action's word for the line (HighProcess's kAction_ enum,
// game/GameAddresses.h).
inline const char* ArmActionName(SInt32 action) {
	switch (action) {
	case 0: return "draw";
	case 1: return "sheathe";
	case 2: return "attack";
	case 3: return "attack's follow-through";
	case 4:
	case 5: return "bow's attack";
	case 6: return "block";
	case 7: return "recoil";
	case 8: return "stagger";
	case 9: return "dodge";
	case 10: return "lower body's animation";
	case 11: return "special idle";
	case 12: return "script's animation";
	default: return "action";
	}
}

// The verdict: the shield held still on the pinned hand through the window -
// under a tenth of a unit and half a degree a frame against it.
inline constexpr float kArmShieldStillUnits = 0.1f;
inline constexpr float kArmShieldStillDegrees = 0.5f;

inline bool ArmShieldStill(const ArmDrawTrace& t) {
	return t.mostStep < kArmShieldStillUnits && t.mostTurn < kArmShieldStillDegrees;
}

inline float DegreesBetween(const NiMatrix33& a, const NiMatrix33& b) {
	return AngleBetweenRotations(a, b) * (180.0f / math::kPi);
}

inline float DistanceUnits(const NiPoint3& a, const NiPoint3& b) { return math::Sqrt((a - b).LengthSquared()); }

inline void BeginArmDrawWindow(ArmDrawTrace& t, const ArmDrawSample& s) {
	t = ArmDrawTrace{};
	t.running = true;
	t.action = s.action;
	t.shield = s.shield;
	t.frames = 1;
	t.heldPins = s.held ? 1u : 0u;
	t.first = s;
	t.last = s;
}

// One pin. True when a window just ended, with it in `done`: the action over,
// or another one begun (its own window starts at this pin).
inline bool StepArmDrawTrace(ArmDrawTrace& t, const ArmDrawSample& s, ArmDrawTrace& done) {
	if (!s.inDraw) {
		if (!t.running) {
			return false;
		}
		done = t;
		t = ArmDrawTrace{};
		return true;
	}
	if (!t.running || s.action != t.action) {
		const bool ended = t.running;
		if (ended) {
			done = t;
		}
		BeginArmDrawWindow(t, s);
		return ended;
	}
	++t.frames;
	t.heldPins += s.held ? 1u : 0u;
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
		const float turn = DegreesBetween(t.last.twistInHandRot, s.twistInHandRot);
		t.mostTwist = twist > t.mostTwist ? twist : t.mostTwist;
		t.mostTurn = turn > t.mostTurn ? turn : t.mostTurn;
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
