// Checks the tablet's pages by the right stick (vr/TabletPages.h): the key of
// the page beside the one shown, either way and round the ends, and the flick
// that steps once per push to a side.

#include <cstdio>

#include "vr/TabletPages.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestPageAfter() {
	std::printf("The page beside it\n");
	Check(TabletPageKeyAfter(game::kMenuIdInventory, 1) == 0x72, "inventory, right: magic (F3)");
	Check(TabletPageKeyAfter(game::kMenuIdInventory, -1) == 0x70, "inventory, left: stats (F1)");
	Check(TabletPageKeyAfter(game::kMenuIdMap, 1) == 0x70, "map, right: round to stats (F1)");
	Check(TabletPageKeyAfter(game::kMenuIdStats, -1) == 0x73, "stats, left: round to map (F4)");
	Check(TabletPageKeyAfter(game::kMenuIdMagic, 1) == 0x73, "magic, right: map (F4)");
	Check(TabletPageKeyAfter(game::kMenuIdInventory, 0) == 0, "no step: no key");
	Check(TabletPageKeyAfter(game::kMenuIdNone, 1) == 0, "no page shown: no key");
}

void TestFlick() {
	std::printf("The flick\n");
	TabletFlickState s;
	Check(StepTabletFlick(s, true, 0.9f, 0.4f) == 1, "pushed right: one step right");
	Check(StepTabletFlick(s, true, 0.9f, 0.4f) == 0, "held: no more");
	Check(StepTabletFlick(s, true, 0.3f, 0.4f) == 0, "back under the dead zone but not under half: nothing");
	Check(StepTabletFlick(s, true, 0.9f, 0.4f) == 0, "and out again: still not re-armed");
	Check(StepTabletFlick(s, true, 0.1f, 0.4f) == 0, "under half: re-armed");
	Check(StepTabletFlick(s, true, -0.8f, 0.4f) == -1, "pushed left: one step left");
	Check(StepTabletFlick(s, false, -0.8f, 0.4f) == 0, "not allowed (no tablet up): nothing");
	Check(StepTabletFlick(s, true, -0.8f, 0.4f) == -1, "and allowed again while held: armed by the pause, one step");
	TabletFlickState t;
	Check(StepTabletFlick(t, true, 0.39f, 0.4f) == 0, "within the dead zone: nothing");
}

}  // namespace

int main() {
	TestPageAfter();
	TestFlick();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
