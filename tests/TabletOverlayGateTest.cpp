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

void TestOnOverlay() {
	std::printf("The tablet on the controller's overlay:\n");
	Check(!TabletOnOverlay(true, 1.0f, true, true), "drawn into both eyes last frame: hidden");
	Check(TabletOnOverlay(true, 1.0f, false, true), "fully open, set whole, the eyes missed it: the overlay stands in");
	Check(!TabletOnOverlay(true, 0.0f, false, false),
	      "the gesture's start, the hands together, nothing open: hidden - not the whole page between the hands");
	Check(!TabletOnOverlay(true, 0.0f, false, false), "held there long: still hidden, frame after frame");
	Check(!TabletOnOverlay(true, 0.4f, false, true), "part unrolled, the eyes missed it: hidden - the overlay knows no share");
	Check(!TabletOnOverlay(true, 1.0f, false, false), "its first frame up, nothing set before: hidden");
	Check(!TabletOnOverlay(true, 0.1f, false, true), "unrolling by the B's time: hidden until open");
	Check(TabletOnOverlay(false, 0.0f, false, false), "as an overlay alone (InScene=0): shown as before");
	Check(TabletOnOverlay(false, 1.0f, true, true), "as an overlay alone, whatever the eyes had");
}

void TestGestureHolds() {
	std::printf("The gesture's menus not placed yet:\n");
	Check(GestureHoldsMenus(true, false, true, false), "unrolling, the menus up, the page not read: nothing shown");
	Check(GestureHoldsMenus(true, false, false, true), "rolled up, the menus still up: nothing shown");
	Check(!GestureHoldsMenus(true, true, true, false), "on the tablet: the tablet's own place decides");
	Check(!GestureHoldsMenus(false, false, true, false), "no menu yet (the key just sent): the world's HUD as it is");
	Check(!GestureHoldsMenus(true, false, false, false), "another menu, no gesture: its own panel");
}

}  // namespace

int main() {
	TestOnOverlay();
	TestGestureHolds();
	TestDrawnShare();
	TestOverlayReturn();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
