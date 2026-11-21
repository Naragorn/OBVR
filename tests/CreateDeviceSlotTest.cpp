// Checks that OBVR keeps the Direct3D factory's CreateDevice slot until the
// device exists, with the hooker that took it from OBVR on the Windows run of
// 2026-11-20 modelled as it was read.
//
// That run's device came up at the game's own 2560x1440, fullscreen, with the
// game's vsync: OBVR's CreateDevice never ran, and the log named who held the
// slot afterwards - apphelp.dll. Read in SysWOW64\apphelp.dll (2026-11-21):
//
//   - The DetectorDWM8And16Bit shim's Direct3DCreate9 hook (0x100812D0) calls
//     the runtime and hands every factory it returns to SE_COM_HookObject.
//   - Its router's patch helper (0x100842A2) keeps {table, slot, original,
//     hook} records. When the slot no longer holds its hook it adds a record
//     with what is there now and writes its hook again.
//   - Its CreateDevice hook (0x10080DF0) calls on through SE_COM_Lookup
//     (0x10083FDC), which returns the FIRST record's original - the runtime's.
//
// And Oblivion makes two factories before the device (0x00761DF0's, kept,
// then 0x00763DE0's inside 0x0076A1C0), sharing one table. So OBVR, patched
// after the first, was written over on the second and skipped by the shim.
// ShimModel below is that behaviour; the chain walk follows the calls the
// game's CreateDevice would make.

#include <cstdio>
#include <string>
#include <vector>

#include "render/CreateDeviceSlot.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::CreateDeviceSlot;
using obvr::render::KeepOnFactory;
using obvr::render::KeepOnLookup;
using obvr::render::SlotAccess;
using obvr::render::SlotOutcome;
using obvr::render::SlotStep;

// Three stand-ins, compared by address only: the runtime's CreateDevice
// (DXVK's or Microsoft's), OBVR's hook, and the shim's hook.
int g_runtimeMarker = 0;
int g_obvrMarker = 0;
int g_shimMarker = 0;
int g_obvrOtherMarker = 0;  // another pointer inside OBVR's module

void* const kRuntime = &g_runtimeMarker;
void* const kObvr = &g_obvrMarker;
void* const kShim = &g_shimMarker;
void* const kObvrOther = &g_obvrOtherMarker;

bool g_writable = true;

bool Write(void** slot, void* value) {
	if (!g_writable) {
		return false;
	}
	*slot = value;
	return true;
}

bool IsOurs(const void* pointer) {
	return pointer == kObvr || pointer == kObvrOther;
}

SlotAccess Access() {
	SlotAccess access;
	access.write = &Write;
	access.isOurs = &IsOurs;
	return access;
}

// apphelp's COM router for one slot, as read.
struct ShimModel {
	std::vector<void*> originals;

	// SE_COM_HookObject on a new factory: 0x100842A2 for slot 16.
	void Register(void** slot) {
		if (!originals.empty() && *slot == kShim) {
			return;  // its hook still there: nothing to do
		}
		originals.push_back(*slot);
		*slot = kShim;
	}

	// SE_COM_Lookup: the first record's original.
	void* Next() const { return originals.empty() ? nullptr : originals.front(); }
};

// A hooker of the plain kind (an overlay, a wrapper): it wrote itself in and
// calls whatever was in the slot when it did.
int g_overlayMarker = 0;
void* const kOverlay = &g_overlayMarker;
void* g_overlayOriginal = nullptr;

// Who runs, in order, when the game calls the slot: OBVR's hook calls
// CreateDeviceNext (the first pointer it found, when entered a second time),
// the shim's calls its first original, the overlay its own, the runtime ends
// it.
std::string Chain(void* entry, const CreateDeviceSlot& state, const ShimModel& shim) {
	std::string path;
	void* at = entry;
	bool inObvr = false;
	for (int guard = 0; guard < 8 && at != nullptr; ++guard) {
		if (at == kObvr) {
			path += inObvr ? "obvr-again>" : "obvr>";
			at = obvr::render::CreateDeviceNext(state, inObvr);
			inObvr = true;
		} else if (at == kOverlay) {
			path += "overlay>";
			at = g_overlayOriginal;
		} else if (at == kShim) {
			path += "shim>";
			at = shim.Next();
		} else if (at == kRuntime) {
			return path + "runtime";
		} else {
			return path + "?";
		}
	}
	return path + "(loop)";
}

// The engine's order with the shim present: factory one through the shim and
// OBVR, factory two through the shim and OBVR, then CreateDevice.
void TestTheShimOnTheSecondFactory() {
	std::printf("The shim writes over OBVR on the engine's second factory\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;
	ShimModel shim;

	shim.Register(&table[16]);
	const SlotOutcome first = KeepOnFactory(state, &table[16], kObvr, Access());
	Check(first.step == SlotStep::Taken, "the first factory: the slot is taken");
	Check(state.next == kShim, "and the shim's hook is what OBVR calls next");

	shim.Register(&table[16]);
	Check(table[16] == kShim, "the second factory: the shim is back in the slot");
	const SlotOutcome second = KeepOnFactory(state, &table[16], kObvr, Access());
	Check(second.step == SlotStep::Retaken, "OBVR takes it back on the same factory");
	Check(second.found == kShim, "and says whose it was");
	Check(table[16] == kObvr, "the slot is OBVR's when the game calls it");
	Check(Chain(table[16], state, shim) == "obvr>shim>runtime",
	      "the device call runs OBVR, then the shim, then the runtime");
}

// What the run of 2026-11-20 did: OBVR kept only the first factory's slot,
// and the device call skipped it. Kept as the record of the failure.
void TestWithoutTheSecondLook() {
	std::printf("Without a look after the second factory (the failing run)\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;
	ShimModel shim;

	shim.Register(&table[16]);
	KeepOnFactory(state, &table[16], kObvr, Access());
	shim.Register(&table[16]);

	Check(Chain(table[16], state, shim) == "shim>runtime",
	      "the device call runs the shim, then the runtime - OBVR never");
}

// No shim: the second factory finds OBVR in the slot and changes nothing.
void TestNoShim() {
	std::printf("Without the shim\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;
	ShimModel noShim;

	Check(KeepOnFactory(state, &table[16], kObvr, Access()).step == SlotStep::Taken,
	      "the first factory takes the slot");
	Check(state.next == kRuntime, "the runtime is next");
	Check(KeepOnFactory(state, &table[16], kObvr, Access()).step == SlotStep::Untouched,
	      "the second factory finds it OBVR's and writes nothing");
	Check(state.repairsLeft == 4, "and spends nothing of the budget");
	Check(Chain(table[16], state, noShim) == "obvr>runtime", "the device call runs OBVR");
}

// The shim arriving only with the second factory records OBVR as its first
// original. Taking the slot back then makes a ring - OBVR, the shim, OBVR -
// which the second entry into OBVR's hook leaves for the runtime.
void TestShimOnlyOnTheSecond() {
	std::printf("The shim arriving with the second factory\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;
	ShimModel shim;

	KeepOnFactory(state, &table[16], kObvr, Access());
	shim.Register(&table[16]);
	KeepOnFactory(state, &table[16], kObvr, Access());
	Check(Chain(table[16], state, shim) == "obvr>shim>obvr-again>runtime",
	      "the device call ends at the runtime, OBVR changing the parameters once");
}

// A plain hooker written over OBVR calls OBVR next; adopting it is the same
// ring, with the same way out.
void TestOverlayChainingToObvr() {
	std::printf("A hooker that calls OBVR after itself\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;
	ShimModel noShim;

	KeepOnFactory(state, &table[16], kObvr, Access());
	g_overlayOriginal = table[16];
	table[16] = kOverlay;
	Check(KeepOnLookup(state, kObvr, Access()).step == SlotStep::Retaken, "OBVR takes it back");
	Check(Chain(table[16], state, noShim) == "obvr>overlay>obvr-again>runtime",
	      "the overlay still runs, and the chain ends at the runtime");
}

// The first pointer OBVR found is the way out of a second entry - with the
// shim registered first, that is the shim, which ends at the runtime itself.
void TestNextAndFirst() {
	std::printf("Which pointer OBVR's hook calls\n");
	CreateDeviceSlot state;
	state.next = kOverlay;
	state.first = kShim;
	Check(obvr::render::CreateDeviceNext(state, false) == kOverlay, "the first entry: next");
	Check(obvr::render::CreateDeviceNext(state, true) == kShim, "a second entry: the first found");
}

// Found pointer inside OBVR: written back, but not called next - that would
// be OBVR calling itself.
void TestOwnPointerNotAdopted() {
	std::printf("When the slot holds another of OBVR's own pointers\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;

	KeepOnFactory(state, &table[16], kObvr, Access());
	table[16] = kObvrOther;
	const SlotOutcome outcome = KeepOnFactory(state, &table[16], kObvr, Access());
	Check(outcome.step == SlotStep::RetakenOwn, "the slot is written back");
	Check(table[16] == kObvr, "with OBVR's hook");
	Check(state.next == kRuntime, "and the runtime stays next");
}

// Two hooks that both insist would trade the slot for the whole run.
void TestTheBudget() {
	std::printf("When the repairs are spent\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;
	state.repairsLeft = 1;

	KeepOnFactory(state, &table[16], kObvr, Access());
	table[16] = kShim;
	Check(KeepOnFactory(state, &table[16], kObvr, Access()).step == SlotStep::Retaken,
	      "the last repair is made");
	table[16] = kShim;
	const SlotOutcome refused = KeepOnFactory(state, &table[16], kObvr, Access());
	Check(refused.step == SlotStep::Refused, "the next one is refused, and says so");
	Check(table[16] == kShim, "the slot is left to the other hook");
}

// Once the device exists the size is decided; the slot is not written again.
void TestAfterTheDevice() {
	std::printf("After the device is made\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;

	KeepOnFactory(state, &table[16], kObvr, Access());
	state.deviceCreated = true;
	table[16] = kShim;
	Check(KeepOnLookup(state, kObvr, Access()).step == SlotStep::Untouched,
	      "a look finds nothing to do");
	Check(table[16] == kShim, "and writes nothing");
}

// The lookups between factories check, they never take the slot first.
void TestLookupBeforeAnyFactory() {
	std::printf("A lookup before any factory\n");
	g_writable = true;
	CreateDeviceSlot state;
	Check(KeepOnLookup(state, kObvr, Access()).step == SlotStep::Untouched,
	      "has no slot to look at");
}

void TestLookupRepairs() {
	std::printf("A lookup after someone wrote over the slot\n");
	g_writable = true;
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;

	KeepOnFactory(state, &table[16], kObvr, Access());
	table[16] = kShim;
	Check(KeepOnLookup(state, kObvr, Access()).step == SlotStep::Retaken,
	      "puts OBVR back in front");
	Check(state.next == kShim, "with the other hook next");
}

void TestNotWritable() {
	std::printf("When the table cannot be written\n");
	void* table[17] = {};
	table[16] = kRuntime;
	CreateDeviceSlot state;

	g_writable = false;
	const SlotOutcome first = KeepOnFactory(state, &table[16], kObvr, Access());
	Check(first.step == SlotStep::NotWritable, "the first factory says so");
	Check(table[16] == kRuntime, "the slot is left as it was");

	g_writable = true;
	Check(KeepOnFactory(state, &table[16], kObvr, Access()).step == SlotStep::Taken,
	      "the next factory can still take it");

	table[16] = kShim;
	g_writable = false;
	Check(KeepOnFactory(state, &table[16], kObvr, Access()).step == SlotStep::NotWritable,
	      "a repair that cannot write says so");
	Check(state.repairsLeft == 0, "and stops trying");
	g_writable = true;
}

void TestNoMethod() {
	std::printf("A factory without a CreateDevice\n");
	g_writable = true;
	void* table[17] = {};
	CreateDeviceSlot state;

	Check(KeepOnFactory(state, &table[16], kObvr, Access()).step == SlotStep::NoMethod,
	      "an empty slot is not taken");
	Check(KeepOnFactory(state, nullptr, kObvr, Access()).step == SlotStep::NoMethod,
	      "nor a missing table");
	Check(state.slot == nullptr, "and nothing is remembered");
}

}  // namespace

int main() {
	TestTheShimOnTheSecondFactory();
	TestWithoutTheSecondLook();
	TestNoShim();
	TestShimOnlyOnTheSecond();
	TestOverlayChainingToObvr();
	TestNextAndFirst();
	TestOwnPointerNotAdopted();
	TestTheBudget();
	TestAfterTheDevice();
	TestLookupBeforeAnyFactory();
	TestLookupRepairs();
	TestNotWritable();
	TestNoMethod();

	if (g_failures != 0) {
		std::printf("\n%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("\nall checks passed\n");
	return 0;
}
