// Checks the hold along the weapon kept in the co-save (vr/HandoverHoldSave.h,
// game/QuickKeyPages.cpp 'OBWH', camera/hands/TwoHands.cpp): what is written,
// what a load reads back, when the load's hold is taken, and that a side
// switch while it waits is the load's, not a handover.

#include <cstdio>
#include <limits>

#include "vr/HandoverHoldSave.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::vr;

// The Claymore handed to the left hand held 12.7 units below its grip
// (handover-two-hander.txt, 2026-11-24), and an Iron Longsword.
constexpr UInt32 kClaymore = 0x0001C6CD;
constexpr UInt32 kSword = 0x00000C0C;
constexpr float kHeld = -12.7f;
const float kNaN = std::numeric_limits<float>::quiet_NaN();

const auto kSame = [](UInt32 id, UInt32& out) {
	out = id;
	return true;
};

void TestKept() {
	std::printf("A hold worth keeping\n");
	Check(HoldAxialKept(kHeld) && HoldAxialKept(3.2f), "below and above the grip");
	Check(!HoldAxialKept(0.0f), "at the grip: nothing to keep");
	Check(!HoldAxialKept(kNaN), "not a number: none");
	Check(!HoldAxialKept(kHandoverHoldMostUnits) && !HoldAxialKept(-kHandoverHoldMostUnits) &&
	          !HoldAxialKept(std::numeric_limits<float>::infinity()),
	      "past any weapon: none");
}

void TestRecordOf() {
	std::printf("The record of the hold now\n");
	const HandoverHoldRecord held = HoldRecordOf(kClaymore, kClaymore, kHeld);
	Check(held.formId == kClaymore && held.axial == kHeld, "the drawn weapon held along it: its id and the hold");
	const HandoverHoldRecord none = HoldRecordOf(0, kClaymore, kHeld);
	Check(none.formId == 0 && none.axial == 0.0f, "nothing drawn: none");
	const HandoverHoldRecord other = HoldRecordOf(kSword, kClaymore, kHeld);
	Check(other.formId == 0 && other.axial == 0.0f, "the hold is another weapon's: none");
	const HandoverHoldRecord grip = HoldRecordOf(kClaymore, kClaymore, 0.0f);
	Check(grip.formId == 0, "held at its grip: none");
	const HandoverHoldRecord wrong = HoldRecordOf(kClaymore, kClaymore, kNaN);
	Check(wrong.formId == 0 && wrong.axial == 0.0f, "a hold that is no number: none");
	Check(sizeof(HandoverHoldRecord) == 8, "the record: two 4-byte fields");
}

void TestReadBack() {
	std::printf("The record read back\n");
	const HandoverHoldRecord written{kClaymore, kHeld};
	HoldRestore r = HoldRestoreOfRecord(&written, sizeof(written), kHandoverHoldVersion, kSame);
	Check(r.pending && r.formId == kClaymore && r.axial == kHeld, "as written, waiting for the world");
	const auto moved = [](UInt32 id, UInt32& out) {
		out = id + 0x01000000u;
		return true;
	};
	r = HoldRestoreOfRecord(&written, sizeof(written), kHandoverHoldVersion, moved);
	Check(r.pending && r.formId == kClaymore + 0x01000000u, "the load order changed: the id moved with it");
	const auto gone = [](UInt32, UInt32&) { return false; };
	r = HoldRestoreOfRecord(&written, sizeof(written), kHandoverHoldVersion, gone);
	Check(r.pending && r.formId == 0, "the weapon's plugin gone: none kept, the load still ends the hold before it");
	const auto zero = [](UInt32, UInt32& out) {
		out = 0;
		return true;
	};
	r = HoldRestoreOfRecord(&written, sizeof(written), kHandoverHoldVersion, zero);
	Check(r.pending && r.formId == 0, "resolved to no form: none kept");
	r = HoldRestoreOfRecord(&written, sizeof(written) - 1, kHandoverHoldVersion, kSame);
	Check(r.pending && r.formId == 0, "another length: none kept");
	r = HoldRestoreOfRecord(&written, sizeof(written), kHandoverHoldVersion + 1, kSame);
	Check(r.pending && r.formId == 0, "another version: none kept");
	r = HoldRestoreOfRecord(nullptr, sizeof(written), kHandoverHoldVersion, kSame);
	Check(r.pending && r.formId == 0, "nothing read: none kept");
	const HandoverHoldRecord empty{};
	r = HoldRestoreOfRecord(&empty, sizeof(empty), kHandoverHoldVersion, kSame);
	Check(r.pending && r.formId == 0 && r.axial == 0.0f, "saved with no hold: none kept");
	const HandoverHoldRecord broken{kClaymore, kNaN};
	r = HoldRestoreOfRecord(&broken, sizeof(broken), kHandoverHoldVersion, kSame);
	Check(r.pending && r.formId == 0 && r.axial == 0.0f, "a hold that is no number: none kept");
	const HoldRestore fresh = HoldRestoreNone();
	Check(fresh.pending && fresh.formId == 0, "no record (an older save, a new game): none kept, still waiting");
}

void TestStep() {
	std::printf("The load's hold taken once the world is there\n");
	HoldRestore idle;
	Check(StepHoldRestore(idle, true, true, kClaymore) == HoldRestoreStep::None && !idle.pending,
	      "nothing waits: nothing to do");

	HoldRestore r{true, kClaymore, kHeld};
	Check(StepHoldRestore(r, false, true, kClaymore) == HoldRestoreStep::Wait && r.pending,
	      "the load's frames, the hands not there: it waits");
	Check(StepHoldRestore(r, true, true, 0) == HoldRestoreStep::Wait && r.pending,
	      "the weapon out, its form not read yet: it waits");
	Check(StepHoldRestore(r, true, true, kClaymore) == HoldRestoreStep::Apply && !r.pending,
	      "the weapon kept is drawn: held where it was saved, once");
	Check(StepHoldRestore(r, true, true, kClaymore) == HoldRestoreStep::None, "and then nothing waits");

	r = HoldRestore{true, kClaymore, kHeld};
	Check(StepHoldRestore(r, true, true, kSword) == HoldRestoreStep::Drop && !r.pending,
	      "another weapon drawn: at its grip");
	r = HoldRestore{true, kClaymore, kHeld};
	Check(StepHoldRestore(r, true, false, 0) == HoldRestoreStep::Drop && !r.pending,
	      "no weapon drawn after the load: at the grip at the next draw");
	r = HoldRestore{true, kClaymore, kHeld};
	Check(StepHoldRestore(r, true, false, kClaymore) == HoldRestoreStep::Drop,
	      "the weapon kept equipped but sheathed: at the grip at the next draw");

	HoldRestore none = HoldRestoreNone();
	Check(StepHoldRestore(none, false, true, kClaymore) == HoldRestoreStep::Wait,
	      "none kept: it still waits for the world");
	Check(StepHoldRestore(none, true, true, 0) == HoldRestoreStep::Drop && !none.pending,
	      "none kept: the hold of before the load ends at once, form read or not");
	none = HoldRestoreNone();
	Check(StepHoldRestore(none, true, true, kClaymore) == HoldRestoreStep::Drop,
	      "none kept and the same weapon drawn: at its grip, not the hold of before the load");
}

void TestSave() {
	std::printf("The hold written at a save\n");
	HandoverHoldSave s;
	s.now = HandoverHoldRecord{kClaymore, kHeld};
	HandoverHoldRecord out = HoldRecordToSave(s);
	Check(out.formId == kClaymore && out.axial == kHeld, "the hold now");
	s.restore = HoldRestore{true, kSword, 4.0f};
	out = HoldRecordToSave(s);
	Check(out.formId == kSword && out.axial == 4.0f,
	      "a load's hold not taken yet: saved again as it was read, not the hold of before the load");
	s.restore = HoldRestoreNone();
	out = HoldRecordToSave(s);
	Check(out.formId == 0 && out.axial == 0.0f, "a load that kept none, not taken yet: none");
	s.restore.pending = false;
	out = HoldRecordToSave(s);
	Check(out.formId == kClaymore, "once taken: the hold now again");
}

void TestSwitch() {
	std::printf("A side switch while the load's hold waits\n");
	HoldRestore waiting{true, kClaymore, kHeld};
	HoldRestore idle;
	Check(SwitchIsHandover(true, idle), "a switch in play: a handover");
	Check(!SwitchIsHandover(true, waiting), "the side read from the co-save: the load's switch, no handover");
	Check(!SwitchIsHandover(false, idle) && !SwitchIsHandover(false, waiting), "no switch: nothing");
}

}  // namespace

int main() {
	TestKept();
	TestRecordOf();
	TestReadBack();
	TestStep();
	TestSave();
	TestSwitch();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
