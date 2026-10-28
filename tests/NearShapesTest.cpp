// Checks which of the player's shapes count as near the eyes when a menu
// opens (game/NearShapesLogic.h): a shape's bound within reach of the eye,
// shown only when neither it nor a node above it is culled.

#include <cstdio>
#include <limits>

#include "game/NearShapesLogic.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; }

void TestGap() {
	std::printf("How far a bound lies from the eye\n");
	const NiPoint3 eye{0.0f, 0.0f, 0.0f};
	float gap = -1.0f;
	Check(game::BoundNearEye(NiPoint3{30.0f, 0.0f, 0.0f}, 10.0f, eye, 40.0f, gap) && Near(gap, 20.0f),
	      "30 away, 10 round: its surface 20 off, near");
	Check(!game::BoundNearEye(NiPoint3{80.0f, 0.0f, 0.0f}, 10.0f, eye, 40.0f, gap) && Near(gap, 70.0f),
	      "80 away, 10 round: 70 off, not near");
	Check(game::BoundNearEye(NiPoint3{5.0f, 0.0f, 0.0f}, 10.0f, eye, 40.0f, gap) && Near(gap, 0.0f),
	      "the eye inside the bound: 0 off, near");
	Check(!game::BoundNearEye(NiPoint3{5.0f, 0.0f, 0.0f}, 0.0f, eye, 40.0f, gap), "no bound (radius 0): not counted");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!game::BoundNearEye(NiPoint3{nan, 0.0f, 0.0f}, 10.0f, eye, 40.0f, gap), "a bound that is no number: not counted");
	Check(!game::BoundNearEye(NiPoint3{5.0f, 0.0f, 0.0f}, 10.0f, eye, 0.0f, gap), "reach 0: nothing near");
}

void TestShown() {
	std::printf("Shown: neither it nor a node above it culled\n");
	Check(game::ShownUnder(true, 0x0000), "under a shown node, not culled: shown");
	Check(!game::ShownUnder(true, 0x0001), "its own app-cull bit: not shown");
	Check(!game::ShownUnder(false, 0x0000), "under a culled node: not shown");
	Check(game::ShownUnder(true, 0x0004), "other flags: shown");
}

}  // namespace

int main() {
	TestGap();
	TestShown();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
