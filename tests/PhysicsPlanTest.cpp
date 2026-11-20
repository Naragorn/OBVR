// Checks the physics steps of a frame planned evenly (game/PhysicsPlanLogic.h):
// every frame's own time in the fewest equal steps no longer than the
// maximum, at the frame rates a headset runs at and below, the cap, the
// carry, the slack, and the inputs that are not numbers.

#include <cmath>
#include <cstdio>

#include "game/PhysicsPlanLogic.h"

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

bool Near(float a, float b, float tolerance = 1e-5f) { return std::fabs(a - b) <= tolerance; }

constexpr float k60 = 1.0f / 60.0f;
constexpr float k90 = 1.0f / 90.0f;

bool Plan(const PhysicsStepPlan& p, UInt32 steps, float stepSeconds, float carry) {
	return p.steps == steps && Near(p.stepSeconds, stepSeconds) && Near(p.carry, carry);
}

void TestEven() {
	std::printf("Every frame its own time, in equal steps\n");
	Check(Plan(PlanEvenPhysicsSteps(k90, 0.0f, k60, 3), 1, k90, 0.0f), "90 fps under a 1/60 s step: one step of the frame");
	Check(Plan(PlanEvenPhysicsSteps(2.0f * k90, 0.0f, k60, 3), 2, k90, 0.0f),
	      "45 fps under 1/60 s: two steps of 11.1 ms - not one, one, two of 16.7");
	Check(Plan(PlanEvenPhysicsSteps(3.0f * k90, 0.0f, k60, 3), 2, 1.5f * k90, 0.0f), "30 fps under 1/60 s: two of 16.7 ms");
	Check(Plan(PlanEvenPhysicsSteps(k60, 0.0f, k60, 3), 1, k60, 0.0f), "60 fps under 1/60 s: one step");
	Check(Plan(PlanEvenPhysicsSteps(k60, 0.0f, k90, 3), 2, 0.5f * k60, 0.0f), "60 fps under a 1/90 s step: two of 8.3 ms");
	Check(Plan(PlanEvenPhysicsSteps(3.0f * k90, 0.0f, k90, 3), 3, k90, 0.0f), "30 fps under 1/90 s: three of 11.1 ms");
	Check(Plan(PlanEvenPhysicsSteps(1.0f / 144.0f, 0.0f, k60, 3), 1, 1.0f / 144.0f, 0.0f),
	      "144 fps under 1/60 s: one short step every frame, none skipped");
	const PhysicsStepPlan p = PlanEvenPhysicsSteps(0.025f, 0.0f, k60, 3);
	Check(p.steps == 2 && Near(p.stepSeconds * 2.0f, 0.025f) && Near(p.simulated, 0.025f),
	      "40 fps: two steps that add up to the frame");
}

void TestSlack() {
	std::printf("A frame a little over a whole number of steps\n");
	Check(Plan(PlanEvenPhysicsSteps(0.0114f, 0.0f, k90, 3), 1, 0.0114f, 0.0f),
	      "11.4 ms at 1/90 s (2.6 % over): one step, not two of 5.7 ms");
	Check(PlanEvenPhysicsSteps(0.0118f, 0.0f, k90, 3).steps == 2, "11.8 ms at 1/90 s (6.2 % over): two steps");
	Check(Plan(PlanEvenPhysicsSteps(0.033f, 0.0f, k60, 3), 2, 0.0165f, 0.0f),
	      "a whole-millisecond 33 ms at 1/60 s: two steps, not three");
	Check(PlanEvenPhysicsSteps(0.0174f, 0.0f, k60, 3).steps == 1, "17.4 ms at 1/60 s, within a twentieth: one step");
}

void TestCap() {
	std::printf("The cap and the carry\n");
	Check(Plan(PlanEvenPhysicsSteps(0.1f, 0.0f, k60, 3), 3, k60, k60),
	      "a 100 ms hitch: three whole steps, the rest carried and held to one step");
	Check(Plan(PlanEvenPhysicsSteps(0.06f, 0.0f, k60, 3), 3, k60, 0.01f), "60 ms: three steps, 10 ms carried");
	Check(Plan(PlanEvenPhysicsSteps(0.04f, 0.0f, k60, 2), 2, k60, 0.04f - 2.0f * k60), "the engine's flag: at most two");
	Check(Plan(PlanEvenPhysicsSteps(k90, 0.01f, k60, 3), 2, (k90 + 0.01f) * 0.5f, 0.0f),
	      "a carry from a hitch runs in the next frame's steps, evenly");
	Check(Plan(PlanEvenPhysicsSteps(k90, 0.0f, k60, 0), 1, k90, 0.0f), "a cap of 0 still runs a step");
}

void TestOdd() {
	std::printf("Inputs that are not frames\n");
	Check(Plan(PlanEvenPhysicsSteps(0.0f, 0.0f, k60, 3), 0, 0.0f, 0.0f), "no time: no step");
	Check(Plan(PlanEvenPhysicsSteps(0.0f, 0.004f, k60, 3), 1, 0.004f, 0.0f), "no frame time but a carry: it runs");
	Check(Plan(PlanEvenPhysicsSteps(-0.01f, 0.0f, k60, 3), 0, 0.0f, 0.0f), "negative time: no step");
	const float nan = std::nanf("");
	Check(PlanEvenPhysicsSteps(nan, 0.0f, k60, 3).steps == 0, "a frame time that is not a number: no step");
	Check(Plan(PlanEvenPhysicsSteps(k90, nan, k60, 3), 1, k90, 0.0f), "a carry that is not a number: dropped");
	Check(Plan(PlanEvenPhysicsSteps(0.02f, 0.0f, 0.0f, 3), 1, 0.02f, 0.0f),
	      "no maximum step: one step of the frame, as the engine does");
	Check(Plan(PlanEvenPhysicsSteps(0.02f, 0.0f, nan, 3), 1, 0.02f, 0.0f), "a maximum that is not a number: the same");
}

}  // namespace

int main() {
	TestEven();
	TestSlack();
	TestCap();
	TestOdd();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
