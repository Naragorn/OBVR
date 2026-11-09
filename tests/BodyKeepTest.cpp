// The people's bodies kept between collects (game/BodyKeepLogic.h,
// docs/mesh-fitted-capsules-spec.md, the open bug of 2026-11-09): a slot found
// again, a fresh one taken (empty, the same actor's old root, the longest
// unused), none taken while every slot is in use (the overflow - no thrash);
// a fit started only with few running and the spacing kept; the collects'
// cost window.

#include <cstdio>
#include <limits>

#include "game/BodyKeepLogic.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestChooseSlot() {
	std::printf("choose a slot\n");
	KeepSlotView slots[3] = {};
	// all empty: the first empty
	KeepChoice c = ChooseKeepSlot(slots, 3, 0x10, 0x100, 50, 6);
	Check(c.action == KeepAction::Fresh && c.index == 0, "all empty: the first slot, fresh");

	slots[0] = KeepSlotView{0x10, 0x100, 49};
	slots[1] = KeepSlotView{0x20, 0x200, 48};
	c = ChooseKeepSlot(slots, 3, 0x10, 0x100, 50, 6);
	Check(c.action == KeepAction::Found && c.index == 0, "the same actor and root: found");

	c = ChooseKeepSlot(slots, 3, 0x20, 0x999, 50, 6);
	Check(c.action == KeepAction::Fresh && c.index == 1, "the same actor with a new root: its own slot, fresh");

	c = ChooseKeepSlot(slots, 3, 0x30, 0x300, 50, 6);
	Check(c.action == KeepAction::Fresh && c.index == 2, "a new actor: the empty slot before any eviction");

	slots[2] = KeepSlotView{0x30, 0x300, 40};
	c = ChooseKeepSlot(slots, 3, 0x40, 0x400, 50, 6);
	Check(c.action == KeepAction::Fresh && c.index == 2, "full: the longest unused, past the guard, is taken");

	slots[2].lastUse = 46;
	c = ChooseKeepSlot(slots, 3, 0x40, 0x400, 50, 6);
	Check(c.action == KeepAction::Overflow, "full and every slot used within the guard: none taken (no thrash)");

	slots[2].lastUse = 44;
	c = ChooseKeepSlot(slots, 3, 0x40, 0x400, 50, 6);
	Check(c.action == KeepAction::Fresh && c.index == 2, "exactly the guard's age: taken");

	c = ChooseKeepSlot(slots, 0, 0x40, 0x400, 50, 6);
	Check(c.action == KeepAction::Overflow, "no slots at all: overflow");
	c = ChooseKeepSlot(nullptr, 3, 0x40, 0x400, 50, 6);
	Check(c.action == KeepAction::Overflow, "no table: overflow");
	c = ChooseKeepSlot(slots, 3, 0, 0x400, 50, 6);
	Check(c.action == KeepAction::Overflow, "no actor: overflow");

	// a counter that wrapped: a slot used "in the future" counts as fresh use
	KeepSlotView wrapped[1] = {KeepSlotView{0x50, 0x500, 0xFFFFFFF0u}};
	c = ChooseKeepSlot(wrapped, 1, 0x60, 0x600, 5, 6);
	Check(c.action == KeepAction::Fresh, "a wrapped counter: the age is taken modulo, 21 collects old, taken");
}

void TestFitGate() {
	std::printf("the fit's gate\n");
	FitGate g;
	Check(FitMayStart(g, 0, 2, 100.0, 8.0), "nothing running, the first: may start");
	g.started = true;
	g.lastMs = 100.0;
	Check(!FitMayStart(g, 0, 2, 104.0, 8.0), "within the spacing: not yet");
	Check(FitMayStart(g, 1, 2, 109.0, 8.0), "past the spacing, one running of two: may start");
	Check(!FitMayStart(g, 2, 2, 200.0, 8.0), "as many running as allowed: not");
	Check(!FitMayStart(g, 5, 2, 200.0, 8.0), "more running (abandoned ones still busy): not");
	Check(!FitMayStart(g, 0, 0, 200.0, 8.0), "none allowed: never");
	Check(!FitMayStart(g, 0, 2, std::numeric_limits<double>::quiet_NaN(), 8.0), "a time that is no number: not");
	Check(FitMayStart(g, 0, 2, 90.0, 8.0), "the clock went back (a new counter): may start");
	NoteFitStarted(g, 300.0);
	Check(g.started && g.lastMs == 300.0, "a start noted: the spacing counts from it");
}

void TestCostWindow() {
	std::printf("the collects' cost\n");
	BodyCostWindow w;
	Check(!AddBodyCost(w, 100.0, 3, 1, 0, 2), "one frame of two: not due");
	Check(!AddBodyCost(w, -1.0, 9, 9, 9, 2), "a time below zero: not counted");
	Check(!AddBodyCost(w, std::numeric_limits<double>::quiet_NaN(), 9, 9, 9, 2), "no number: not counted");
	Check(AddBodyCost(w, 300.0, 1, 0, 4, 2), "two frames: due");
	Check(w.frames == 2 && BodyCostMean(w) == 200.0 && w.maxMicros == 300.0, "the mean and the longest");
	Check(w.skeletonReads == 4 && w.overflows == 1 && w.mostFitsRunning == 4, "reads and overflows summed, fits' most");
	BodyCostWindow none;
	Check(BodyCostMean(none) == 0.0, "an empty window: mean 0");
	Check(!AddBodyCost(none, 1.0, 0, 0, 0, 0), "frames per line 0: never due");
}

}  // namespace

int main() {
	TestChooseSlot();
	TestFitGate();
	TestCostWindow();
	std::printf(g_failures == 0 ? "all passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
