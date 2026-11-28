// Checks what of the tablet's menus may show where (vr/TabletOverlayGate.h):
// the share drawn as the scroll's close goes out, and the room's panel coming
// back after the tablet - primed a frame before it shows.

#include <cstdio>

#include "vr/TabletOverlayGate.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestDrawnShare() {
	std::printf("The share drawn:\n");
	Check(TabletDrawnShare(false, 0.4f, 1.0f) == 0.4f, "between the hands: the scroll's share");
	Check(TabletDrawnShare(false, 0.0f, 1.0f) == 0.0f, "between the hands at the start: nothing open");
	Check(TabletDrawnShare(false, -1.0f, 0.5f) == 0.5f, "opened by the B: unrolled by time");
	Check(TabletDrawnShare(false, -1.0f, 1.0f) == 1.0f, "held: fully open");
	Check(TabletDrawnShare(true, -1.0f, 1.0f) == 0.0f,
	      "rolled up and the close sent, the menus still up: nothing - not the whole page again");
	Check(TabletDrawnShare(true, 0.3f, 1.0f) == 0.0f, "rolled up wins over any share");
}

void TestOverlayReturn() {
	std::printf("The room's panel after the tablet:\n");
	OverlayReturnState s;
	Check(!StepOverlayReturn(s, false), "in the world, never held: shown as it is");
	Check(!StepOverlayReturn(s, true), "the tablet up: held, nothing primed");
	Check(!StepOverlayReturn(s, true), "its menus closing: still held");
	Check(StepOverlayReturn(s, false), "the first frame after: primed - the new picture handed over, not shown");
	Check(!StepOverlayReturn(s, false), "the frame after that: shown");
	Check(!StepOverlayReturn(s, false), "and on");
	StepOverlayReturn(s, true);
	Check(StepOverlayReturn(s, false), "held for a single frame: primed again on its return");
}

}  // namespace

int main() {
	TestDrawnShare();
	TestOverlayReturn();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
