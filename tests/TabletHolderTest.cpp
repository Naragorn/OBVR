// Checks which hand holds the tablet once it is open (vr/TabletHolder.h):
// opened by the scroll, the hand that keeps its grip; both let go at once, or
// no keeper, the off hand; opened by the left B, the off hand - right- and
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
	TabletHolder h = TabletHolderOnceOpen(TabletOpenedBy::Button, false, false, 0.0f, 0.0f, false);
	Check(!h.weaponHand && h.physicalLeft && !h.onTop, "opened by the B: the off hand, the left controller, its bottom edge");
	h = TabletHolderOnceOpen(TabletOpenedBy::Button, false, true, 0.0f, 0.0f, false);
	Check(!h.weaponHand && h.physicalLeft, "opened by the B with a grip closed: the grips do not count, the off hand");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, false, true, -0.30f, -0.30f, false);
	Check(h.weaponHand && !h.physicalLeft, "the left let go, the right kept its grip: the right hand, the right controller");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, true, false, -0.30f, -0.30f, false);
	Check(!h.weaponHand && h.physicalLeft, "the right let go, the left kept its grip: the left hand");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, false, false, -0.30f, -0.30f, false);
	Check(!h.weaponHand && h.physicalLeft, "both let go in the same frame: the off hand, the left controller");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, true, true, -0.30f, -0.30f, false);
	Check(!h.weaponHand && h.physicalLeft, "both still closed, no keeper to tell: the off hand");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, true, false, -0.10f, -0.50f, false);
	Check(!h.weaponHand && h.onTop, "the left kept it, the upper one: its top edge");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, false, true, -0.50f, -0.10f, false);
	Check(h.weaponHand && h.onTop, "the right kept it, the upper one: its top edge");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, false, true, -0.10f, -0.50f, false);
	Check(h.weaponHand && !h.onTop, "the right kept it, the lower one: its bottom edge");
	h = TabletHolderOnceOpen(TabletOpenedBy::Button, false, false, -0.10f, -0.50f, false);
	Check(!h.onTop, "opened by the B: the hands' heights do not matter, its bottom edge");
}

void TestLeftHanded() {
	std::printf("Left-handed (the roles swapped):\n");
	TabletHolder h = TabletHolderOnceOpen(TabletOpenedBy::Button, false, false, 0.0f, 0.0f, true);
	Check(!h.weaponHand && !h.physicalLeft, "opened by the B: the off hand, the right controller");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, false, true, -0.30f, -0.30f, true);
	Check(h.weaponHand && h.physicalLeft,
	      "the off hand (right controller) let go, the weapon hand kept its grip: the left controller");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, true, false, -0.30f, -0.30f, true);
	Check(!h.weaponHand && !h.physicalLeft, "the weapon hand let go: the off hand, the right controller");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, false, false, -0.30f, -0.30f, true);
	Check(!h.weaponHand && !h.physicalLeft, "both let go at once: the off hand, the right controller");
	h = TabletHolderOnceOpen(TabletOpenedBy::Scroll, true, false, -0.10f, -0.50f, true);
	Check(!h.physicalLeft && h.onTop, "the off hand kept it, the upper one: the right controller, its top edge");
}

}  // namespace

int main() {
	TestRightHanded();
	TestLeftHanded();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
