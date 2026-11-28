// Checks which hand holds the tablet once it is open (vr/TabletHolder.h):
// the off hand, opened by the left B or by the scroll, right- and
// left-handed, and which edge it is held at.

#include <cstdio>

#include "vr/TabletHolder.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestRightHanded() {
	std::printf("Right-handed:\n");
	TabletHolder h = TabletHolderOnceOpen(TabletOpenedBy::Button, 0.0f, 0.0f, false, false);
	Check(!h.weaponHand && h.physicalLeft && !h.onTop, "opened by the B: the off hand, the left controller, its bottom edge");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, -0.30f, -0.30f, false, false);
	Check(!h.weaponHand && h.physicalLeft, "opened by the scroll: the off hand, the left controller - not the right one");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, -0.10f, -0.50f, false, false);
	Check(!h.weaponHand && h.physicalLeft && h.onTop, "the off hand the upper one: held at its top edge");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, -0.50f, -0.10f, false, false);
	Check(!h.weaponHand && h.physicalLeft && !h.onTop, "the off hand the lower one: held at its bottom edge, still the off hand");
	h = TabletHolderOnceOpen(TabletOpenedBy::Button, -0.10f, -0.50f, false, false);
	Check(!h.onTop, "opened by the B: the hands' heights do not matter, its bottom edge");
}

void TestLeftHanded() {
	std::printf("Left-handed (the roles swapped):\n");
	TabletHolder h = TabletHolderOnceOpen(TabletOpenedBy::Button, 0.0f, 0.0f, false, true);
	Check(!h.weaponHand && !h.physicalLeft, "opened by the B: the off hand, the right controller");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, -0.30f, -0.30f, false, true);
	Check(!h.weaponHand && !h.physicalLeft, "opened by the scroll: the off hand, the right controller - not the left one");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, -0.10f, -0.50f, false, true);
	Check(!h.physicalLeft && h.onTop, "the off hand the upper one: the right controller, its top edge");
}

// The sides switched between the pair's first grip and the hold: the hand
// that was the off hand then keeps it - now the frame's right (the weapon role).
void TestSidesSwitched() {
	std::printf("The sides switched since the first grip:\n");
	TabletHolder h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, -0.30f, -0.30f, true, true);
	Check(h.weaponHand && h.physicalLeft, "right-handed before: the left controller still, now the weapon role");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, -0.30f, -0.30f, true, false);
	Check(h.weaponHand && !h.physicalLeft, "left-handed before: the right controller still");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, -0.50f, -0.10f, true, true);
	Check(h.onTop, "that hand the upper one: its top edge");
	h = TabletHolderOnceOpen(TabletOpenedBy::Button, 0.0f, 0.0f, true, true);
	Check(!h.weaponHand && !h.physicalLeft, "the B reads no grips: the off hand as it is now");
}

}  // namespace
int main() {
	TestRightHanded();
	TestLeftHanded();
	TestSidesSwitched();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
