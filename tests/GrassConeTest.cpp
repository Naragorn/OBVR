// Checks the cone the engine's grass is built in (game/GrassCone.h): with a
// headset the grass at the edge of the view came and went as the head turned
// (avolpe, beta 6, 2026-10-24: "as i turn my head, the grass spawns and
// despawns").

#include <cmath>
#include <cstdio>

#include "game/GrassCone.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kPi = 3.14159265f;

// The engine's cone argument: atan of the frustum's right edge less 0.25
// (0x0066C028 to 0x0066C11E). 75 degrees across on a monitor; 98.1 degrees,
// the union frustum the harness's headset runs with (r 1.1516).
const float kMonitorCone = std::atan(std::tan(37.5f * kPi / 180.0f)) - 0.25f;
const float kHeadsetCone = std::atan(1.1516f) - 0.25f;

void TestCone() {
	std::printf("The cone handed to the grass build\n");
	Check(game::GrassConeFor(false, kHeadsetCone) == kHeadsetCone, "off: the engine's own cone");
	Check(game::GrassConeFor(true, kHeadsetCone) == game::kGrassAllAroundCone, "on: all around");
	Check(game::GrassConeFor(true, kMonitorCone) == game::kGrassAllAroundCone, "on, a monitor's cone: all around");
	Check(game::GrassConeFor(true, 2.0f) == 2.0f, "on, a cone already wider: kept, never narrowed");
	Check(std::isnan(game::GrassConeFor(true, std::nanf(""))), "an unread cone: passed on as it came");
}

void TestCoverage() {
	std::printf("What a cone covers: a patch's corner within twice the cone of the view's heading\n");
	Check(game::GrassConeCoversAll(game::kGrassAllAroundCone), "the all-around cone passes every corner");
	Check(!game::GrassConeCoversAll(kHeadsetCone), "the headset's own cone does not");
	Check(std::fabs(game::GrassConeDegrees(kHeadsetCone) - 69.4f) < 0.2f, "the headset's cone: 69.4 degrees either side");
	Check(std::fabs(game::GrassConeDegrees(kMonitorCone) - 46.3f) < 0.2f, "the monitor's cone: 46.3 degrees either side");
	Check(game::GrassConeDegrees(game::kGrassAllAroundCone) == 180.0f, "all around: 180 degrees either side");
	Check(game::GrassConeDegrees(-1.0f) == 0.0f, "a negative cone: nothing");
}

void TestBytes() {
	std::printf("The engine's bytes the detour stands on\n");
	Check(game::kGrassBuild == 0x004EBDA0, "the grass build, called from the camera update at 0x0066C162");
	Check(sizeof(game::kGrassBuildEntry) == 7, "seven bytes of whole instructions displaced");
	Check(game::kGrassBuildEntry[0] == 0x6A && game::kGrassBuildEntry[2] == 0x68,
	      "push -1; push 0x009CACCB - nothing relative in them");
	Check(game::kGrassNode == 0x00B36094, "the Grass node's pointer");
}

}  // namespace

int main() {
	TestCone();
	TestCoverage();
	TestBytes();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
