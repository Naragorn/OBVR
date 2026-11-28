// Checks the worn shield held rigid to the pinned left hand
// (game/ShieldHoldLogic.h): when the rest is taken, what is held, every
// refusal, and - through the pin's own arithmetic (game/BonePin.h) - that the
// twist bone, which the shield hangs on, keeps its place in the pinned hand's
// frame while the animation turns the hand's and the twist's locals by what
// the 2026-11-28 log measured, mirrored or not.

#include <cmath>
#include <cstdio>

#include "game/BonePin.h"
#include "game/PullTurnLogic.h"
#include "game/ShieldHoldLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kDegrees = math::kPi / 180.0f;

NiMatrix33 About(float x, float y, float z, float degrees) { return AxisAngleRotation(x, y, z, degrees * kDegrees); }

NiTransform Local(const NiMatrix33& rot, const NiPoint3& pos, float scale = 1.0f) {
	NiTransform t{};
	t.rot = rot;
	t.pos = pos;
	t.scale = scale;
	return t;
}

// The animation's locals: at rest, and turned by a draw's frame `k`.
NiTransform HandAt(float degrees) { return Local(About(0.0f, 0.0f, 1.0f, 5.0f + degrees), NiPoint3{24.0f, 0.5f, 0.0f}); }
NiTransform TwistAt(float degrees) { return Local(About(1.0f, 0.0f, 0.0f, 10.0f + degrees), NiPoint3{12.0f, 0.0f, 0.2f}); }

bool SameRot(const NiMatrix33& a, const NiMatrix33& b, float tolerance = 1e-5f) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (std::fabs(a.data[r][c] - b.data[r][c]) > tolerance) {
				return false;
			}
		}
	}
	return true;
}

bool SamePos(const NiPoint3& a, const NiPoint3& b, float tolerance = 1e-4f) {
	return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance && std::fabs(a.z - b.z) <= tolerance;
}

bool SameLocal(const NiTransform& a, const NiTransform& b) {
	return SameRot(a.rot, b.rot) && SamePos(a.pos, b.pos) && a.scale == b.scale;
}

ShieldHoldInput Pin(bool atRest, float handDegrees = 0.0f, float twistDegrees = 0.0f) {
	ShieldHoldInput in;
	in.shieldWorn = true;
	in.beside = true;
	in.bones = 1;
	in.atRest = atRest;
	in.handLocal = HandAt(handDegrees);
	in.twistLocal = TwistAt(twistDegrees);
	return in;
}

// The rest taken: kShieldRestPins pins at rest from a fresh state.
ShieldHoldOut TakeRest(ShieldHoldState& s) {
	ShieldHoldOut out;
	for (UInt32 i = 0; i < kShieldRestPins; ++i) {
		out = StepShieldHold(s, Pin(true));
	}
	return out;
}

void TestOff() {
	std::printf("No shield worn: the animation's, nothing kept\n");
	ShieldHoldState s;
	ShieldHoldInput in = Pin(true);
	in.shieldWorn = false;
	const ShieldHoldOut out = StepShieldHold(s, in);
	Check(out.step == ShieldHoldStep::Off && !out.write, "Off, nothing written");
	TakeRest(s);
	Check(s.have, "a shield worn and the arm at rest: the rest kept");
	StepShieldHold(s, in);
	Check(!s.have && s.restPins == 0, "the shield taken off: the rest forgotten");
	const ShieldHoldOut again = StepShieldHold(s, Pin(false, 40.0f, 40.0f));
	Check(again.step == ShieldHoldStep::Waiting && !again.write, "worn again in a draw: waiting, the animation's");
}

void TestNotBeside() {
	std::printf("The twist bone not beside the hand: left to the animation\n");
	ShieldHoldState s;
	TakeRest(s);
	ShieldHoldInput in = Pin(false, 30.0f, 30.0f);
	in.beside = false;
	const ShieldHoldOut out = StepShieldHold(s, in);
	Check(out.step == ShieldHoldStep::NotBeside && !out.write, "NotBeside, nothing written");
	Check(!s.have, "and the rest forgotten");
}

void TestWaitingAndTaken() {
	std::printf("The rest: taken after kShieldRestPins pins in a row at rest\n");
	ShieldHoldState s;
	ShieldHoldOut out;
	for (UInt32 i = 0; i + 1 < kShieldRestPins; ++i) {
		out = StepShieldHold(s, Pin(true));
	}
	Check(out.step == ShieldHoldStep::Waiting && !out.write && !s.have, "one pin short: still waiting, nothing written");
	out = StepShieldHold(s, Pin(false, 20.0f, 20.0f));
	Check(out.step == ShieldHoldStep::Waiting && s.restPins == 0, "an action between: the count starts again");
	for (UInt32 i = 0; i + 1 < kShieldRestPins; ++i) {
		StepShieldHold(s, Pin(true));
	}
	ShieldHoldInput last = Pin(true);
	last.handLocal.scale = 0.9f;
	out = StepShieldHold(s, last);
	Check(out.step == ShieldHoldStep::Taken && out.write, "the last pin at rest: taken, written");
	Check(SameLocal(out.hand, last.handLocal) && SameLocal(out.twist, last.twistLocal),
	      "what is written at the take is the animation's own (no jump)");
	Check(s.have && SameLocal(s.hand, last.handLocal), "the rest kept");
}

void TestHeld() {
	std::printf("Held: every later pin writes the rest, at rest or not\n");
	ShieldHoldState s;
	TakeRest(s);
	ShieldHoldInput draw = Pin(false, 73.9f, 79.3f);
	draw.handLocal.scale = 1.25f;
	const ShieldHoldOut out = StepShieldHold(s, draw);
	Check(out.step == ShieldHoldStep::Held && out.write, "in a sheathe: held, written");
	Check(SameRot(out.hand.rot, HandAt(0.0f).rot) && SamePos(out.hand.pos, HandAt(0.0f).pos), "the hand's rest");
	Check(SameRot(out.twist.rot, TwistAt(0.0f).rot) && SamePos(out.twist.pos, TwistAt(0.0f).pos), "the twist's rest");
	Check(out.hand.scale == 1.25f && out.twist.scale == 1.0f, "each keeps the animation's scale");
	const ShieldHoldOut idle = StepShieldHold(s, Pin(true, 3.0f, -2.0f));
	Check(idle.step == ShieldHoldStep::Held && SameRot(idle.twist.rot, TwistAt(0.0f).rot),
	      "at rest again: still the first rest (the idle's sway held too, no second take)");
	const ShieldHoldOut twice = StepShieldHold(s, Pin(false, 50.0f, 50.0f));
	const ShieldHoldOut sameFrame = StepShieldHold(s, Pin(false, 50.0f, 50.0f));
	Check(SameLocal(twice.hand, sameFrame.hand) && SameLocal(twice.twist, sameFrame.twist),
	      "a second pin in the same frame (the handle's) writes the same");
}

void TestNewBonesAndGaps() {
	std::printf("New bones start afresh; pins that did not run keep the rest\n");
	ShieldHoldState s;
	TakeRest(s);
	// Third person, a menu with the hands away: no pin, no step - the next pin holds.
	const ShieldHoldOut after = StepShieldHold(s, Pin(false, 30.0f, 30.0f));
	Check(after.step == ShieldHoldStep::Held, "after a gap without pins: held with the rest kept");
	ShieldHoldInput loaded = Pin(false, 30.0f, 30.0f);
	loaded.bones = 2;
	const ShieldHoldOut out = StepShieldHold(s, loaded);
	Check(out.step == ShieldHoldStep::Waiting && !out.write && !s.have, "a new tree: waiting, the old rest dropped");
}

// The pin's arithmetic (PinHandBone): the forearm placed so the hand with its
// local lands at `wanted`; the twist, the forearm's other child, read back in
// the pinned hand's frame.
BonePose TwistInHand(const BonePose& wanted, const NiTransform& hand, const NiTransform& twist) {
	const BonePose forearm = ParentForChildAt(wanted, hand.rot, hand.pos, 1.0f);
	const NiMatrix33 back = InverseMatrix(wanted.rot);
	BonePose in;
	in.rot = back * (forearm.rot * twist.rot);
	in.pos = back * ((forearm.pos + forearm.rot * twist.pos) - wanted.pos);
	return in;
}

void TestWorld(bool mirrored) {
	std::printf("Through the pin%s: the twist (the shield) still in the hand's frame\n", mirrored ? ", mirrored" : "");
	const NiMatrix33 cameraRot = About(0.0f, 0.0f, 1.0f, 30.0f);
	const NiPoint3 cameraPos{100.0f, 200.0f, 50.0f};
	const NiMatrix33 relative = About(0.3f, 0.5f, 0.8f, 40.0f);
	const NiPoint3 offset{-6.0f, 4.0f, -11.0f};
	const BonePose wanted = HandBoneWorld(cameraRot, cameraPos, relative, offset,
	                                      MirroredCalibration(About(1.0f, 0.0f, 0.0f, 90.0f), mirrored),
	                                      MirroredGrip(NiPoint3{1.0f, 2.0f, 0.5f}, mirrored));
	ShieldHoldState s;
	TakeRest(s);
	const BonePose rest = TwistInHand(wanted, HandAt(0.0f), TwistAt(0.0f));
	// The sheathe of line 21985: the hand's local up to 73.9 degrees a frame, the twist's 79.3, the controller still.
	const float hand[] = {0.0f, 20.0f, 73.9f, 60.0f, 10.0f};
	const float twist[] = {0.0f, 25.0f, 79.3f, 55.0f, 5.0f};
	float mostHeld = 0.0f;
	float mostHeldDegrees = 0.0f;
	float mostFree = 0.0f;
	bool handOnController = true;
	for (UInt32 k = 0; k < 5; ++k) {
		const ShieldHoldInput in = Pin(false, hand[k], twist[k]);
		const ShieldHoldOut out = StepShieldHold(s, in);
		const BonePose held = TwistInHand(wanted, out.hand, out.twist);
		const BonePose free = TwistInHand(wanted, in.handLocal, in.twistLocal);
		const float d = std::sqrt((held.pos - rest.pos).LengthSquared());
		const float f = std::sqrt((free.pos - rest.pos).LengthSquared());
		const float a = AngleBetweenRotations(held.rot, rest.rot) / kDegrees;
		mostHeld = d > mostHeld ? d : mostHeld;
		mostFree = f > mostFree ? f : mostFree;
		mostHeldDegrees = a > mostHeldDegrees ? a : mostHeldDegrees;
		const BonePose forearm = ParentForChildAt(wanted, out.hand.rot, out.hand.pos, 1.0f);
		handOnController = handOnController && SamePos(forearm.pos + forearm.rot * out.hand.pos, wanted.pos) &&
		                   SameRot(forearm.rot * out.hand.rot, wanted.rot);
	}
	std::printf("    held: at most %.4f units, %.4f degrees off its rest; the animation's: %.1f units\n",
	            static_cast<double>(mostHeld), static_cast<double>(mostHeldDegrees), static_cast<double>(mostFree));
	Check(mostFree > 5.0f, "the animation's locals alone swing the twist round the hand (as the log read)");
	Check(mostHeld < 1e-3f && mostHeldDegrees < 0.01f, "held: the twist does not move in the hand's frame");
	Check(handOnController, "the hand still lands on the controller's pose");
}

}  // namespace

int main() {
	TestOff();
	TestNotBeside();
	TestWaitingAndTaken();
	TestHeld();
	TestNewBonesAndGaps();
	TestWorld(false);
	TestWorld(true);
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
