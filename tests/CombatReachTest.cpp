// Checks the combat reach held while Full VR runs (game/CombatReach.h):
// the setting as given, held to 40-200, the game's own when off, for 0 and
// for a value that is not a number.

#include <cstdio>
#include <limits>

#include "game/CombatReach.h"

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
	std::printf("The combat reach\n");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(CombatReachFor(true, 85.0f, 128.0f) == 85.0f, "Full VR, 85: 85");
	Check(CombatReachFor(false, 85.0f, 128.0f) == 128.0f, "Full VR off: the game's own");
	Check(CombatReachFor(true, 0.0f, 128.0f) == 128.0f, "0: the game's own");
	Check(CombatReachFor(true, nan, 128.0f) == 128.0f, "not a number: the game's own");
	Check(CombatReachFor(true, 10.0f, 128.0f) == 40.0f, "too short: 40");
	Check(CombatReachFor(true, 500.0f, 128.0f) == 200.0f, "too long: 200");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
