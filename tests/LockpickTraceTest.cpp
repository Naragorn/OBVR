// Checks the lock trace's reading of the pick (game/LockpickTrace.h): its
// place from its tile's x, and the tumbler it stands under.

#include <cstdio>
#include <limits>

#include "game/LockpickTrace.h"

namespace {

using namespace obvr;
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
	std::printf("the pick's place and tumbler\n");
	Check(PickPlaceFromTileX(655.0f) == 502.0f, "the tile's x less 153: the XML's own start, 502");
	Check(PickTumblerFor(313.0f) == 1, "at the first centre: tumbler 1");
	Check(PickTumblerFor(365.0f) == 2 && PickTumblerFor(416.0f) == 3 && PickTumblerFor(467.0f) == 4 &&
	          PickTumblerFor(518.0f) == 5,
	      "at each other centre: its tumbler");
	Check(PickTumblerFor(338.0f) == 1 && PickTumblerFor(340.0f) == 2, "between two: the nearer centre's");
	Check(PickTumblerFor(100.0f) == 1, "left of them all: the first");
	Check(PickTumblerFor(900.0f) == 5, "right of them all: the last");
	Check(PickTumblerFor(502.0f) == 5, "the XML's start: under the fifth");
	Check(PickTumblerFor(std::numeric_limits<float>::quiet_NaN()) == 0, "no number: none");
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
