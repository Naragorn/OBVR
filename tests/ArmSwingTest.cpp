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
	TestRunWanted();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
