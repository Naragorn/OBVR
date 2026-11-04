// Checks the walker's grip on the player's arm (game/WalkerGripLogic.h):
// where its hand goes on the held forearm, when the reach is given up, the
// elbow's way, the weight easing in and out, and that a weight of zero gives
// back the animation's own arm.

#include <cstdio>

#include "game/WalkerGripLogic.h"

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

bool Near(float a, float b, float tolerance = 0.01f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance = 0.01f) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

// A walker at the origin facing +y (its right +x), its right shoulder at
// (10, 0, 100), arms of 22 and 15 units (the animation: the upper arm straight
// down, the forearm forward); the player's held forearm before it.
GripInput Standing() {
	GripInput in;
	in.shoulder = NiPoint3{10.0f, 0.0f, 100.0f};
	in.upperLength = 22.0f;
	in.foreLength = 15.0f;
	in.walkerRight = NiPoint3{1.0f, 0.0f, 0.0f};
	in.side = 1.0f;
	in.playerWrist = NiPoint3{5.0f, 30.0f, 95.0f};
	in.playerHandAxis = NiPoint3{0.0f, 1.0f, 0.0f};
	in.animElbow = NiPoint3{10.0f, 0.0f, 78.0f};
	in.animWrist = NiPoint3{10.0f, 15.0f, 78.0f};
	in.weight = 1.0f;
	return in;
}

void TestTarget() {
	std::printf("Where the walker's hand goes\n");
	const GripInput in = Standing();
	const GripPlan plan = PlanGrip(in);
	Check(plan.use, "the forearm in reach: the grip is solved");
	Check(NearPoint(plan.limb.target, NiPoint3{5.0f, 30.0f - kGripBackUnits, 95.0f}),
	      "the hand on the forearm, kGripBackUnits up from the wrist");
	Check(NearPoint(plan.limb.root, in.shoulder), "from the walker's shoulder");
	Check(Near(plan.limb.upperLength, 22.0f) && Near(plan.limb.lowerLength, 15.0f), "with its own bones");
	Check(plan.limb.pole.z < 0.0f, "the elbow down");
	Check(plan.limb.pole.x > 0.0f, "and out on the right arm's side");
	GripInput left = in;
	left.side = -1.0f;
	Check(PlanGrip(left).limb.pole.x < 0.0f, "the left arm's elbow out to the left");
}

void TestRefusals() {
	std::printf("When the grip is given up\n");
	GripInput far = Standing();
	far.playerWrist = NiPoint3{5.0f, 200.0f, 95.0f};  // 200 units: past 2.5 x 37
	const GripPlan farPlan = PlanGrip(far);
	Check(!farPlan.use, "the forearm out of reach: the animation's arm");
	GripInput reaching = Standing();
	reaching.playerWrist = NiPoint3{5.0f, 70.0f, 95.0f};  // 70 units: past the arm, within 2.5 x
	const GripPlan reachPlan = PlanGrip(reaching);
	const LimbSolve reachSolve = SolveLimb(reachPlan.limb);
	Check(reachPlan.use && reachSolve.valid && !reachSolve.reached && reachSolve.stretch == 1.0f,
	      "past the arm's length: the arm reaches straight for it, unstretched, and ends short");
	GripInput none = Standing();
	none.upperLength = 0.0f;
	Check(!PlanGrip(none).use, "bones of no length: nothing laid on");
	GripInput weightless = Standing();
	weightless.weight = 0.0f;
	Check(!PlanGrip(weightless).use, "a weight of zero: the animation's arm");
	GripInput nan = Standing();
	nan.playerWrist.x = 0.0f / 0.0f;
	Check(!PlanGrip(nan).use, "a wrist that is no number: nothing");
	GripInput nanAxis = Standing();
	nanAxis.playerHandAxis = NiPoint3{0.0f, 0.0f, 0.0f};
	Check(!PlanGrip(nanAxis).use, "no forearm direction: nothing");
}

void TestBlend() {
	std::printf("The weight between the animation and the grip\n");
	GripInput half = Standing();
	half.weight = 0.5f;
	const GripPlan plan = PlanGrip(half);
	const NiPoint3 full = NiPoint3{5.0f, 30.0f - kGripBackUnits, 95.0f};
	const NiPoint3 mid{(full.x + half.animWrist.x) * 0.5f, (full.y + half.animWrist.y) * 0.5f,
	                   (full.z + half.animWrist.z) * 0.5f};
	Check(plan.use && NearPoint(plan.limb.target, mid), "half the weight: the target half way from the animation's wrist");
	GripInput tiny = Standing();
	tiny.weight = 0.001f;
	const GripPlan tinyPlan = PlanGrip(tiny);
	const LimbSolve s = SolveLimb(tinyPlan.limb);
	// The animation's arm, solved back: its own elbow and wrist.
	Check(tinyPlan.use && s.valid && NearPoint(s.joint, tiny.animElbow, 0.5f) && NearPoint(s.end, tiny.animWrist, 0.5f),
	      "a weight near zero: the solve gives back the animation's elbow and wrist");
}

void TestEase() {
	std::printf("The weight easing in and out\n");
	Check(Near(StepGripWeight(0.0f, true, 0.1f), 0.1f / kGripEaseSeconds), "held: grows by dt over the ease");
	Check(Near(StepGripWeight(0.95f, true, 0.1f), 1.0f), "held: never past 1");
	Check(Near(StepGripWeight(1.0f, false, 0.1f), 1.0f - 0.1f / kGripEaseSeconds), "let go: shrinks by dt over the ease");
	Check(Near(StepGripWeight(0.05f, false, 0.1f), 0.0f), "let go: never below 0");
	Check(Near(StepGripWeight(0.5f, true, 0.0f / 0.0f), 0.5f), "a dt that is no number: unchanged");
	Check(Near(StepGripWeight(0.5f, true, -1.0f), 0.5f), "a negative dt: unchanged");
	Check(Near(StepGripWeight(0.5f, true, 5.0f), 1.0f), "a long frame: straight to the end");
}

void TestSide() {
	std::printf("Which of the walker's arms holds\n");
	const NiPoint3 grip{0.0f, 30.0f, 95.0f};
	Check(WalkerHandFor(NiPoint3{-10.0f, 20.0f, 95.0f}, NiPoint3{10.0f, 10.0f, 80.0f}, grip) == kWalkerLeftHand,
	      "the walker's left hand the nearer: the left holds");
	Check(WalkerHandFor(NiPoint3{-10.0f, 0.0f, 60.0f}, NiPoint3{2.0f, 25.0f, 94.0f}, grip) == kWalkerRightHand,
	      "the walker's right hand the nearer: the right holds");
	const float nan = 0.0f / 0.0f;
	Check(WalkerHandFor(NiPoint3{nan, 0.0f, 0.0f}, NiPoint3{2.0f, 25.0f, 94.0f}, grip) == kWalkerRightHand,
	      "a left hand that reads as nothing: the right");
	Check(WalkerHandFor(NiPoint3{nan, 0.0f, 0.0f}, NiPoint3{nan, 0.0f, 0.0f}, grip) == kWalkerNoHand,
	      "neither readable: none");
	Check(GripBoneNames(kWalkerLeftHand).upper[6] == 'L' && GripBoneNames(kWalkerRightHand).upper[6] == 'R',
	      "the bone names on that side");
	Check(GripSideSign(kWalkerLeftHand) < 0.0f && GripSideSign(kWalkerRightHand) > 0.0f, "the side's sign for the elbow");
}

void TestForearmMessage() {
	std::printf("'WGHP' to Walkers: where the held forearm is\n");
	const GripForearmMessage msg = ForearmMessageFor(kGripArmRight, NiPoint3{1.5f, -2.0f, 3.25f});
	Check(kWalkersForearmMessage == 0x50484757u, "the type 'WGHP'");
	Check(sizeof(msg) == 16, "16 bytes");
	Check(msg.arm == 2 && msg.x == 1.5f && msg.y == -2.0f && msg.z == 3.25f, "the arm, then x, y, z");
	Check(ForearmMessageFor(kGripArmLeft, NiPoint3{0.0f, 0.0f, 0.0f}).arm == 1, "the left arm: 1");

	std::printf("How often the forearms go to Walkers\n");
	float since = kForearmSendSeconds;
	Check(ForearmSendDue(since, 0.0f), "the first frame: at once");
	Check(!ForearmSendDue(since, 0.05f), "half the interval later: not yet");
	Check(ForearmSendDue(since, 0.06f), "past the interval: again");
	Check(!ForearmSendDue(since, 0.0f / 0.0f), "a dt that is no number: not sent, the clock kept");
	since = 0.0f;
	Check(ForearmSendDue(since, 5.0f), "a long frame: sent");
	Check(!ForearmSendDue(since, 0.01f), "and the clock starts again");
}

}  // namespace

int main() {
	TestForearmMessage();
	TestTarget();
	TestRefusals();
	TestBlend();
	TestEase();
	TestSide();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
