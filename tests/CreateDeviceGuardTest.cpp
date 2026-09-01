// Checks the decision that keeps OBVR in the Direct3D factory's method table.
//
// The fault this comes from was measured, not imagined. One run had OBVR's
// factory hook run - the log line that says the table was patched is written
// from inside it - and then Oblivion's device came up at its own 2560x1440,
// without a single one of the lines OBVR's CreateDevice hook writes before it
// touches anything. The factory handed to the game was OBVR's. The slot, by
// the time the game called it, held someone else's pointer.
//
// The picture in the headset was the whole frame scaled up from a quarter of
// the pixels, which is a thing a person notices immediately and a log does not
// complain about at all. Hence a guard, and hence every flow of it tested.

#include <cstdio>

#include "render/CreateDeviceGuard.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::GuardCreateDeviceSlot;
using obvr::render::SlotGuardDecision;
using obvr::render::SlotGuardInput;

// The ordinary run: patched, ours, nothing to do, and this is the state the
// guard is in for every one of the thousands of lookups a start makes.
void TestTheQuietCase() {
	std::printf("When the slot is still OBVR's\n");

	SlotGuardInput input;
	input.slotKnown = true;
	input.slotIsOurs = true;
	input.repairsLeft = 4;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	Check(!decision.repair, "nothing is written");
	Check(!decision.report, "nothing is logged");
	Check(!decision.adoptForeign, "nothing is adopted");
}

void TestNoSlotNoWatch() {
	std::printf("When the table was never patched\n");

	SlotGuardInput input;
	input.slotKnown = false;
	input.slotIsOurs = false;
	input.repairsLeft = 4;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	Check(!decision.repair, "there is nothing to repair");
	Check(!decision.report, "and nothing to say");
}

// After the device exists the frame size is settled. Writing to the slot then
// would change nothing about this run and could only disturb the next call.
void TestTooLateOnceTheDeviceExists() {
	std::printf("When the device is already built\n");

	SlotGuardInput input;
	input.slotKnown = true;
	input.deviceCreated = true;
	input.slotIsOurs = false;
	input.repairsLeft = 4;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	Check(!decision.repair, "the slot is left alone");
	Check(!decision.report, "and the log stays quiet");
}

// The fault itself: someone else's pointer, the device still to come.
void TestAForeignPointerIsTakenBack() {
	std::printf("When someone else holds the slot\n");

	SlotGuardInput input;
	input.slotKnown = true;
	input.slotIsOurs = false;
	input.foreignIsOurModule = false;
	input.repairsLeft = 4;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	Check(decision.repair, "OBVR's hook goes back in");
	Check(decision.report, "and the log names what happened");
	Check(decision.adoptForeign, "the other hook is called afterwards, not dropped");
}

// A pointer into OBVR's own module is not a stranger's hook - it is ours,
// reached by a path that would make the call reach itself.
void TestOurOwnPointerIsNeverAdopted() {
	std::printf("When the pointer in the slot is OBVR's own\n");

	SlotGuardInput input;
	input.slotKnown = true;
	input.slotIsOurs = false;
	input.foreignIsOurModule = true;
	input.repairsLeft = 4;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	Check(decision.repair, "the slot is still restored");
	Check(decision.report, "and still reported");
	Check(!decision.adoptForeign, "but nothing is chained, which would be a loop");
}

// Two hooks that both insist would otherwise swap the slot for the length of
// the run, once per lookup, which is thousands of times a second.
void TestTheBudgetEnds() {
	std::printf("When the repairs have run out\n");

	SlotGuardInput input;
	input.slotKnown = true;
	input.slotIsOurs = false;
	input.repairsLeft = 0;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	Check(!decision.repair, "the duel is not continued");
	Check(!decision.report, "and it is not reported again either");
}

// The last repair the budget allows is a repair like any other.
void TestTheLastRepairStillHappens() {
	std::printf("When one repair is left\n");

	SlotGuardInput input;
	input.slotKnown = true;
	input.slotIsOurs = false;
	input.repairsLeft = 1;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	Check(decision.repair, "it is taken");
	Check(decision.adoptForeign, "with the other hook chained in");
}

// Ordering: a built device wins over a foreign pointer, so a late overwrite
// after a successful start is not chased.
void TestBuiltDeviceOutranksAForeignPointer() {
	std::printf("When both are true at once\n");

	SlotGuardInput input;
	input.slotKnown = true;
	input.deviceCreated = true;
	input.slotIsOurs = false;
	input.foreignIsOurModule = false;
	input.repairsLeft = 4;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	Check(!decision.repair, "the device having been built settles it");
}

}  // namespace

int main() {
	std::printf("CreateDevice slot guard\n\n");

	TestTheQuietCase();
	std::printf("\n");
	TestNoSlotNoWatch();
	std::printf("\n");
	TestTooLateOnceTheDeviceExists();
	std::printf("\n");
	TestAForeignPointerIsTakenBack();
	std::printf("\n");
	TestOurOwnPointerIsNeverAdopted();
	std::printf("\n");
	TestTheBudgetEnds();
	std::printf("\n");
	TestTheLastRepairStillHappens();
	std::printf("\n");
	TestBuiltDeviceOutranksAForeignPointer();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
