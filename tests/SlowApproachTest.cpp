// Checks the slow approach (game/SlowApproach.h): a run turned to a walk,
// who walks - in combat and near, with a metre's hysteresis - and the
// radius from its setting.

#include <cstdio>
#include <limits>

#include "game/SlowApproach.h"

namespace {

using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

}  // namespace

int main() {
	const float nan = std::numeric_limits<float>::quiet_NaN();
	std::printf("A run turned to a walk\n");
	Check(WalkInsteadOfRun(0x0201) == 0x0101, "forward and running: forward and walking");
	Check(WalkInsteadOfRun(0x0101) == 0x0101, "walking already: kept");
	Check(WalkInsteadOfRun(0x0400) == 0x0400, "sneaking, no run: kept");
	Check(WalkInsteadOfRun(0x0000) == 0x0000, "standing: kept");
	Check(WalkInsteadOfRun(0x0A02) == 0x0902, "swimming backwards at a run: the swim and the direction kept");

	std::printf("Who walks\n");
	Check(SlowApproachNear(false, true, 600.0f, 700.0f), "in combat, 8.6 m off, the radius 10 m: walks");
	Check(!SlowApproachNear(false, true, 720.0f, 700.0f), "10.3 m off: runs");
	Check(SlowApproachNear(true, true, 720.0f, 700.0f), "walking already at 10.3 m: still walks (a metre's slack)");
	Check(!SlowApproachNear(true, true, 780.0f, 700.0f), "11.1 m off: runs again");
	Check(!SlowApproachNear(false, false, 100.0f, 700.0f), "not in combat, however near: as vanilla");
	Check(!SlowApproachNear(false, true, nan, 700.0f), "a distance that is not a number: as vanilla");
	Check(!SlowApproachNear(false, true, 100.0f, 0.0f), "no radius: as vanilla");

	std::printf("Held still\n");
	Check(StandStill(0x0201) == 0x0000, "forward at a run: standing");
	Check(StandStill(0x010F) == 0x0100, "every direction at a walk: the walk kept, no step");
	Check(StandStill(0x0030) == 0x0030, "turning left and right: still turns");
	Check(StandStill(0x0C24) == 0x0C20, "sneaking and swimming kept, the step left dropped, the turn kept");

	std::printf("The speed seen\n");
	Check(SmoothedGroundSpeed(0.0f, 10.0f, 0.2f) == 50.0f, "a fifth of a second: the speed at once");
	Check(SmoothedGroundSpeed(0.0f, 10.0f, 0.5f) == 20.0f, "a long frame: the speed at once");
	Check(SmoothedGroundSpeed(100.0f, 2.0f, 0.02f) == 100.0f, "a frame at its own speed: unchanged");
	Check(SmoothedGroundSpeed(0.0f, 2.0f, 0.02f) > 9.9f && SmoothedGroundSpeed(0.0f, 2.0f, 0.02f) < 10.1f,
	      "100 units/s from standing over one 50 Hz frame: a tenth of the way");
	Check(SmoothedGroundSpeed(42.0f, 5.0f, 0.0f) == 42.0f, "no time passed: kept");
	Check(SmoothedGroundSpeed(42.0f, nan, 0.02f) == 42.0f, "a distance that is not a number: kept");

	std::printf("The radius\n");
	Check(SlowApproachUnits(10.0f, 70.0f) == 700.0f, "10 m: 700 units");
	Check(SlowApproachUnits(1.0f, 70.0f) == 350.0f, "under 2 m: the default 5 m");
	Check(SlowApproachUnits(80.0f, 70.0f) == 350.0f, "over 50 m: the same");
	Check(SlowApproachUnits(nan, 70.0f) == 350.0f, "not a number: the same");
	Check(SlowApproachUnits(5.0f, 70.0f) == 350.0f, "the default 5 m: 350 units");
	Check(SlowApproachUnits(5.0f, 0.0f) == 350.0f, "no scale: 70 units a metre");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
