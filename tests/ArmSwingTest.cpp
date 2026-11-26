// Checks running by swinging both arms (vr/ArmSwing.h): both hands moving
// against each other along the body, fast enough, run; one hand, both the
// same way, too slow, or a hand untracked do not; a swing's turn (the hands
// at rest for a moment) keeps the run for the hold time.

#include <cstdio>

#include "vr/ArmSwing.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// Head frame, Oblivion axes: x right, y forward, z up; metres a second.
NiPoint3 Fwd(float y, float z = 0.0f) { return NiPoint3{0.0f, y, z}; }

const float kDt = 1.0f / 90.0f;

void TestSwing() {
	std::printf("Both arms swung against each other\n");
	vr::ArmSwingSettings s;
	s.enabled = true;
	vr::ArmSwingState st;
	Check(vr::StepArmSwing(st, s, true, Fwd(1.2f), Fwd(-1.2f), kDt), "left forward, right back, 1.2 m/s: runs");
	Check(vr::StepArmSwing(st, s, true, Fwd(0.0f), Fwd(0.0f), kDt), "the turn of the swing, both at rest: still runs");
	for (int i = 0; i < 40; ++i) {
		vr::StepArmSwing(st, s, true, Fwd(0.0f), Fwd(0.0f), kDt);
	}
	Check(!vr::StepArmSwing(st, s, true, Fwd(0.0f), Fwd(0.0f), kDt), "still for half a second: walks again");
	vr::ArmSwingState down;
	Check(vr::StepArmSwing(down, s, true, Fwd(0.6f, -0.9f), Fwd(-0.6f, 0.9f), kDt),
	      "looking down, the swing reads up and down as much as forward: runs");
}

void TestNotASwing() {
	std::printf("Not a swing\n");
	vr::ArmSwingSettings s;
	s.enabled = true;
	vr::ArmSwingState st;
	Check(!vr::StepArmSwing(st, s, true, Fwd(1.5f), Fwd(0.1f), kDt), "one arm (a blow): walks");
	Check(!vr::StepArmSwing(st, s, true, Fwd(1.5f), Fwd(1.5f), kDt), "both the same way (a two-hander): walks");
	Check(!vr::StepArmSwing(st, s, true, Fwd(0.4f), Fwd(-0.4f), kDt), "too slow (0.4 m/s): walks");
	Check(!vr::StepArmSwing(st, s, false, Fwd(1.2f), Fwd(-1.2f), kDt), "a hand not tracked: walks");
	vr::ArmSwingState side;
	Check(!vr::StepArmSwing(side, s, true, NiPoint3{1.2f, 0.0f, 0.0f}, NiPoint3{-1.2f, 0.0f, 0.0f}, kDt),
	      "the hands apart and together sideways (a clap): walks");
	vr::ArmSwingSettings off = s;
	off.enabled = false;
	vr::ArmSwingState st2;
	Check(!vr::StepArmSwing(st2, off, true, Fwd(1.2f), Fwd(-1.2f), kDt), "switched off: never");
}

// The harness's slash with a counter-move (arm-swing-weapon.txt, 2026-11-26):
// the sword across to the left and down at 6.5 m/s, the torch hand drawn back
// and up at 2.1 m/s - against each other along the body, a third the speed.
const NiPoint3 kSlash{-5.9f, 1.0f, -2.5f};
const NiPoint3 kDrawnBack{0.0f, -1.5f, 1.5f};

void TestPumpOrStroke() {
	std::printf("A pump of the run or a weapon's stroke (the tester, 2026-11-26)\n");
	vr::ArmSwingSettings s;
	s.enabled = true;
	Check(s.balance > 0.49f && s.balance < 0.51f, "the balance a pump wants: the slower hand half the faster's");
	Check(vr::ArmSwingPump(s, true, Fwd(-1.2f, 0.4f), Fwd(1.2f, -0.4f)), "both arms against each other alike: a pump");
	Check(vr::ArmSwingPump(s, true, Fwd(-0.9f), Fwd(1.5f)), "the torch arm at 60 % of the sword arm: still a pump");
	Check(vr::ArmSwingPump(s, true, Fwd(-1.5f), Fwd(3.0f)), "the slower at half the faster: a pump");
	Check(!vr::ArmSwingPump(s, true, kDrawnBack, kSlash),
	      "a slash with the torch hand drawn back at a third of its speed: a stroke, no pump");
	Check(!vr::ArmSwingPump(s, true, Fwd(0.0f, 2.0f), NiPoint3{-6.0f, -0.5f, -1.0f}),
	      "the balance from the whole speed: a slash mostly sideways, its part along the body as fast as the other "
	      "hand's: a stroke");
	Check(!vr::ArmSwingPump(s, false, Fwd(1.2f), Fwd(-1.2f)), "a hand untracked: no pump");
	Check(!vr::ArmSwingPump(s, true, Fwd(1.5f), Fwd(1.5f)), "both the same way: no pump");
	Check(!vr::ArmSwingPump(s, true, Fwd(0.4f), Fwd(-0.4f)), "too slow: no pump");
	vr::ArmSwingSettings off = s;
	off.enabled = false;
	Check(!vr::ArmSwingPump(off, true, Fwd(1.2f), Fwd(-1.2f)), "ArmSwingRun off: never a pump");
	vr::ArmSwingSettings before = s;
	before.balance = 0.0f;
	Check(!vr::ArmSwingPump(before, true, Fwd(1.2f), Fwd(-1.2f)),
	      "ArmSwingBalance 0: no pump is ever held from striking (the rule before)");
}

void TestRunStartsOnAPump() {
	std::printf("The run starts on a pump and holds on any swing against each other\n");
	vr::ArmSwingSettings s;
	s.enabled = true;
	vr::ArmSwingState st;
	Check(!vr::StepArmSwing(st, s, true, kDrawnBack, kSlash, kDt),
	      "a slash with the torch hand drawn back: no run (it set off a sprint before)");
	Check(vr::StepArmSwing(st, s, true, Fwd(1.2f), Fwd(-1.2f), kDt), "a pump: runs");
	Check(vr::StepArmSwing(st, s, true, Fwd(0.8f), Fwd(-2.5f), kDt),
	      "running, the arms against each other lopsided (a third): runs on");
	for (int i = 0; i < 40; ++i) {
		vr::StepArmSwing(st, s, true, Fwd(0.8f), Fwd(-2.5f), kDt);
	}
	Check(vr::StepArmSwing(st, s, true, Fwd(0.8f), Fwd(-2.5f), kDt), "lopsided for half a second on end: still runs");
	for (int i = 0; i < 40; ++i) {
		vr::StepArmSwing(st, s, true, Fwd(0.0f), Fwd(0.0f), kDt);
	}
	Check(!vr::StepArmSwing(st, s, true, Fwd(0.8f), Fwd(-2.5f), kDt),
	      "walking again, a lopsided swing (a third) does not start the run");
	vr::ArmSwingSettings before = s;
	before.balance = 0.0f;
	vr::ArmSwingState st2;
	Check(vr::StepArmSwing(st2, before, true, kDrawnBack, kSlash, kDt),
	      "ArmSwingBalance 0: the slash with the hand drawn back runs, as before");
}

void TestNoBlockWhileRunning() {
	std::printf("No block from a hand raised while the arms run (the torch's block walks the player)\n");
	vr::ArmSwingSettings s;
	s.enabled = true;
	vr::ArmSwingState st;
	Check(!vr::ArmSwingHoldsBlocks(st, s), "standing: a raised hand blocks as ever");
	vr::StepArmSwing(st, s, true, Fwd(1.2f), Fwd(-1.2f), kDt);
	Check(vr::ArmSwingHoldsBlocks(st, s), "running by the arms: a hand swung up past the block's height is no block");
	vr::StepArmSwing(st, s, true, Fwd(0.0f), Fwd(0.0f), kDt);
	Check(vr::ArmSwingHoldsBlocks(st, s), "at the turn of the swing, the hands still for a moment: still no block");
	for (int i = 0; i < 40; ++i) {
		vr::StepArmSwing(st, s, true, Fwd(0.0f), Fwd(0.0f), kDt);
	}
	Check(!vr::ArmSwingHoldsBlocks(st, s), "the run over: the raised hand blocks again");
	vr::ArmSwingSettings before = s;
	before.balance = 0.0f;
	vr::ArmSwingState st2;
	vr::StepArmSwing(st2, before, true, Fwd(1.2f), Fwd(-1.2f), kDt);
	Check(!vr::ArmSwingHoldsBlocks(st2, before), "ArmSwingBalance 0: a raised hand blocks while running, as before");
	vr::ArmSwingSettings off = s;
	off.enabled = false;
	Check(!vr::ArmSwingHoldsBlocks(st, off), "ArmSwingRun off: never");
}

void TestDefault() {
	std::printf("Off by default\n");
	Check(!vr::ArmSwingSettings{}.enabled, "off until the first start or the settings switch it on");
}

void TestRunWanted() {
	std::printf("What runs\n");
	Check(vr::ArmSwingRun(true, true, false, false), "on: the swing runs");
	Check(!vr::ArmSwingRun(true, false, true, false), "on: the stick held no longer runs");
	Check(vr::ArmSwingRun(true, false, false, true), "on: the inventory's drop key still holds Shift");
	Check(vr::ArmSwingRun(false, false, true, false), "off: the stick runs as before");
	Check(!vr::ArmSwingRun(false, true, false, false), "off: a swing does nothing");
}

}  // namespace

int main() {
	TestSwing();
	TestNotASwing();
	TestPumpOrStroke();
	TestRunStartsOnAPump();
	TestNoBlockWhileRunning();
	TestDefault();
	TestRunWanted();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
