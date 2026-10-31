// Checks the sticks on the engine's fly camera (vr/FlyCamLogic.h).

#include <cstdio>
#include <limits>

#include "vr/FlyCamLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.01f) { return a - b < tolerance && b - a < tolerance; }

using namespace obvr::vr;

constexpr float kHalfPi = 1.5707963f;

void TestDeadZone() {
	std::printf("the dead zone\n");
	const FlyCamSettings s;
	Check(FlyCamAxis(0.1f, s.deadZone) == 0.0f, "inside: nothing");
	Check(Near(FlyCamAxis(1.0f, s.deadZone), 1.0f), "all the way: one");
	Check(Near(FlyCamAxis(-1.0f, s.deadZone), -1.0f), "all the way back: minus one");
	Check(Near(FlyCamAxis(2.0f, s.deadZone), 1.0f), "past one: one");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(FlyCamAxis(nan, s.deadZone) == 0.0f, "not a number: nothing");
	FlyCamInput in;
	in.moveX = 0.1f;
	in.moveY = -0.1f;
	in.turnX = 0.1f;
	in.liftY = 0.1f;
	in.dtSeconds = 0.1f;
	const FlyCamStep f = StepFlyCam(in, 0.0f, s);
	Check(f.move.x == 0.0f && f.move.y == 0.0f && f.move.z == 0.0f && f.yaw == 0.0f, "a resting stick: nothing moves");
}

void TestMove() {
	std::printf("the left stick flies along the heading, level\n");
	const FlyCamSettings s;
	FlyCamInput in;
	in.moveY = 1.0f;
	in.dtSeconds = 1.0f / 90.0f;
	const float step = s.speedUnits / 90.0f;
	FlyCamStep f = StepFlyCam(in, 0.0f, s);
	Check(Near(f.move.x, 0.0f) && Near(f.move.y, step) && f.move.z == 0.0f, "yaw 0, forward: along +y, level");
	f = StepFlyCam(in, kHalfPi, s);
	Check(Near(f.move.x, step) && Near(f.move.y, 0.0f), "yaw 90 (turned right): along +x");
	in.moveY = 0.0f;
	in.moveX = 1.0f;
	f = StepFlyCam(in, 0.0f, s);
	Check(Near(f.move.x, step) && Near(f.move.y, 0.0f), "yaw 0, the stick right: along +x");
	in.moveX = -1.0f;
	f = StepFlyCam(in, 0.0f, s);
	Check(Near(f.move.x, -step), "the stick left: along -x");
	in.moveX = 0.0f;
	in.moveY = -1.0f;
	f = StepFlyCam(in, 0.0f, s);
	Check(Near(f.move.y, -step), "back: along -y");
	// The head turned right 90 degrees from the body (the walk direction's sign: minus).
	in.moveY = 1.0f;
	in.headYaw = -kHalfPi;
	f = StepFlyCam(in, 0.0f, s);
	Check(Near(f.move.x, step) && Near(f.move.y, 0.0f), "the head turned right: forward is where it looks");
	in.headYaw = 0.0f;
	in.moveY = 0.5f;
	f = StepFlyCam(in, 0.0f, s);
	Check(f.move.y > 0.0f && f.move.y < 0.5f * step, "half pushed: slower than half the speed (the dead zone taken off)");
	in.dtSeconds = 1.0f;
	in.moveY = 1.0f;
	f = StepFlyCam(in, 0.0f, s);
	Check(f.move.y == 0.0f, "a hitch (a second's frame): no jump across the map");
}

void TestLift() {
	std::printf("the right stick's y flies up and down\n");
	const FlyCamSettings s;
	FlyCamInput in;
	in.liftY = 1.0f;
	in.dtSeconds = 1.0f / 90.0f;
	FlyCamStep f = StepFlyCam(in, 0.7f, s);
	Check(Near(f.move.z, s.speedUnits / 90.0f) && f.move.x == 0.0f && f.move.y == 0.0f, "up: straight up");
	in.liftY = -1.0f;
	f = StepFlyCam(in, 0.7f, s);
	Check(Near(f.move.z, -s.speedUnits / 90.0f), "down: straight down");
}

void TestTurn() {
	std::printf("the right stick's x turns: smooth, or the snap's steps\n");
	const FlyCamSettings s;
	FlyCamInput in;
	in.turnX = 1.0f;
	in.dtSeconds = 0.1f;
	FlyCamStep f = StepFlyCam(in, 0.0f, s);
	Check(Near(f.yaw, s.turnDegrees * 0.1f * 3.14159265f / 180.0f), "smooth, right: the yaw grows at FlyCamTurnDegrees");
	in.turnX = -1.0f;
	f = StepFlyCam(in, 0.0f, s);
	Check(f.yaw < 0.0f, "smooth, left: it shrinks");
	in.snapOwnsStick = true;
	in.snapYaw = 0.0f;
	f = StepFlyCam(in, 0.0f, s);
	Check(f.yaw == 0.0f, "snap turning, no step this frame: no smooth turn either");
	in.snapYaw = 0.5f;
	f = StepFlyCam(in, 0.0f, s);
	Check(Near(f.yaw, 0.5f), "snap turning: the snap's step");
}

}  // namespace

int main() {
	TestDeadZone();
	TestMove();
	TestLift();
	TestTurn();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
