// Checks the menu haptics' decisions (vr/MenuHaptics.h): one pulse per
// thing the laser comes onto, one per click, none for leaving.

#include <cstdio>

#include "vr/MenuHaptics.h"

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

void TestTiles() {
	std::printf("The game's tiles\n");
	MenuHapticState s;
	Check(StepTileHaptic(s, true, 0, false) == MenuPulse::None, "pointing at no tile: nothing");
	Check(StepTileHaptic(s, true, 0x1000, false) == MenuPulse::Hover, "onto a tile: a hover pulse");
	Check(StepTileHaptic(s, true, 0x1000, false) == MenuPulse::None, "still on it: nothing more");
	Check(StepTileHaptic(s, true, 0x2000, false) == MenuPulse::Hover, "onto another: a pulse again");
	Check(StepTileHaptic(s, true, 0, false) == MenuPulse::None, "off onto nothing: no pulse");
	Check(StepTileHaptic(s, true, 0x2000, false) == MenuPulse::Hover, "back onto it: a pulse");
	Check(StepTileHaptic(s, true, 0x2000, true) == MenuPulse::Click, "the click on the hovered tile: a click pulse");
	Check(StepTileHaptic(s, true, 0x2000, false) == MenuPulse::None, "held on it after: nothing");
	Check(StepTileHaptic(s, true, 0x3000, true) == MenuPulse::Click && s.tile == 0x3000,
	      "a click as the laser lands on a new tile: one click pulse, the tile remembered");
	Check(StepTileHaptic(s, false, 0x3000, false) == MenuPulse::None && s.tile == 0,
	      "the laser off the picture: nothing, and forgotten");
	Check(StepTileHaptic(s, true, 0x3000, false) == MenuPulse::Hover, "back on the picture onto the tile: a pulse");
	Check(StepTileHaptic(s, false, 0x3000, true) == MenuPulse::None, "a click with the laser off the picture: nothing");
}

void TestRows() {
	std::printf("OBVR's rows and slots\n");
	MenuHapticState s;
	Check(StepRowHaptic(s, true, -1, false) == MenuPulse::None, "no row: nothing");
	Check(StepRowHaptic(s, true, 3, false) == MenuPulse::Hover, "onto a row: a hover pulse");
	Check(StepRowHaptic(s, true, 3, false) == MenuPulse::None, "still on it: nothing");
	Check(StepRowHaptic(s, true, 4, false) == MenuPulse::Hover, "the next row: a pulse");
	Check(StepRowHaptic(s, true, 4, true) == MenuPulse::Click, "clicked: a click pulse");
	Check(StepRowHaptic(s, true, -1, true) == MenuPulse::None, "a click on no row: nothing");
	Check(StepRowHaptic(s, false, 4, false) == MenuPulse::None && s.row == -1, "not pointing: nothing, forgotten");
}

}  // namespace

int main() {
	TestTiles();
	TestRows();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
