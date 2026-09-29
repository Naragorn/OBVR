// Checks the player's body radius (game/PlayerCapsuleLogic.h): the
// setting's range, the radius asked for, and when it is asked.

#include <cmath>
#include <cstdio>

#include "game/PlayerCapsuleLogic.h"

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

bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void TestScale() {
	std::printf("The setting\n");
	Check(Near(ClampBodyRadiusScale(0.7f), 0.7f), "0.7 as it is");
	Check(Near(ClampBodyRadiusScale(0.1f), kBodyRadiusScaleMin) && Near(ClampBodyRadiusScale(3.0f), kBodyRadiusScaleMax),
	      "clamped to 0.3..1.5");
	volatile float zero = 0.0f;
	Check(Near(ClampBodyRadiusScale(zero / zero), 1.0f), "not a number: the game's own");
	Check(!BodyRadiusScaled(1.0f) && BodyRadiusScaled(0.7f) && BodyRadiusScaled(1.2f), "only a scale other than 1 scales");
	Check(!BodyRadiusScaled(zero / zero), "not a number: not scaled");
}

void TestTarget() {
	std::printf("The radius asked for\n");
	Check(Near(BodyRadiusTarget(2.9f, 0.5f), 1.45f), "the game's radius times the scale");
	Check(Near(BodyRadiusTarget(2.9f, 0.1f), 2.9f * kBodyRadiusScaleMin), "the scale clamped");
	Check(BodyRadiusTarget(0.0f, 0.5f) == 0.0f && BodyRadiusTarget(50.0f, 0.5f) == 0.0f,
	      "no sane radius of the game's (no controller yet): nothing");
}

void TestRequest() {
	std::printf("When to ask\n");
	Check(BodyRadiusRequestNeeded(2.9f, 1.45f, true, false), "scaled, the target the game's: asked");
	Check(!BodyRadiusRequestNeeded(1.45f, 1.45f, true, true), "the target already it: not asked again");
	Check(BodyRadiusRequestNeeded(2.9f, 1.45f, true, true), "the engine put its own back (a new world): asked again");
	Check(!BodyRadiusRequestNeeded(1.0f, 2.9f, false, false),
	      "scale 1 and never asked: left alone (a SetSize of the console's stays)");
	Check(BodyRadiusRequestNeeded(1.45f, 2.9f, false, true), "back to 1 after OBVR asked: the game's own given back");
	Check(!BodyRadiusRequestNeeded(2.9f, 0.0f, true, true), "nothing sane to ask for: not asked");
}

}  // namespace

int main() {
	TestScale();
	TestTarget();
	TestRequest();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
