// Checks the ledger that carries a mapped range from Lock to Unlock, and
// the byte-window fingerprint taken there. The ledger's promises decide
// whether a write is counted or skipped: an entry survives exactly from
// Begin to End, a re-lock supersedes the old range, and a full ledger
// refuses instead of evicting - evicting would hand a later unlock stale
// range data, which is worse than an honest skip.

#include <cstdio>

#include "render/LockLedger.h"

namespace {

using obvr::render::LockLedger;
using obvr::render::SumLeadingBytes;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void* Fake(UInt32 n) {
	return reinterpret_cast<void*>(static_cast<UInt32>(0x2000 + n * 0x10));
}

void TestBeginEnd() {
	std::printf("Begin and End\n");

	LockLedger ledger;
	const void* data = nullptr;
	UInt32 size = 0;
	Check(!ledger.End(Fake(1), &data, &size), "an unknown buffer has no entry to close");

	char range[16];
	Check(ledger.Begin(Fake(1), range, sizeof(range)), "a lock opens an entry");
	Check(ledger.End(Fake(1), &data, &size), "the unlock finds it");
	Check(data == range && size == sizeof(range), "and gets the range back unchanged");
	Check(!ledger.End(Fake(1), &data, &size), "a second unlock finds nothing - the slot is free");
}

void TestRelock() {
	std::printf("A re-lock without an unlock\n");

	LockLedger ledger;
	char first[16];
	char second[32];
	ledger.Begin(Fake(1), first, sizeof(first));
	Check(ledger.Begin(Fake(1), second, sizeof(second)),
	      "the same buffer locks again without growing the ledger");

	const void* data = nullptr;
	UInt32 size = 0;
	ledger.End(Fake(1), &data, &size);
	Check(data == second && size == sizeof(second), "the newer range superseded the older");
	Check(!ledger.End(Fake(1), &data, &size), "and only one entry existed");
}

void TestCapacity() {
	std::printf("Capacity\n");

	LockLedger ledger;
	char range[8];
	for (UInt32 i = 0; i < LockLedger::kSlots; ++i) {
		ledger.Begin(Fake(i), range, sizeof(range));
	}
	Check(!ledger.Begin(Fake(LockLedger::kSlots), range, sizeof(range)),
	      "a full ledger refuses a new buffer");

	const void* data = nullptr;
	UInt32 size = 0;
	Check(ledger.End(Fake(0), &data, &size), "existing entries are still reachable");
	Check(ledger.Begin(Fake(LockLedger::kSlots), range, sizeof(range)),
	      "and a closed entry frees its slot for a new buffer");
}

void TestByteWindow() {
	std::printf("The byte window\n");

	unsigned char bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	Check(SumLeadingBytes(bytes, sizeof(bytes), 16) == 36,
	      "a range shorter than the window is summed in full");
	Check(SumLeadingBytes(bytes, sizeof(bytes), 4) == 10,
	      "a range longer than the window is cut at the window");
	Check(SumLeadingBytes(bytes, 0, 16) == 0, "an empty range sums to zero");
}

}  // namespace

int main() {
	std::printf("OBVR lock ledger test\n\n");

	TestBeginEnd();
	std::printf("\n");
	TestRelock();
	std::printf("\n");
	TestCapacity();
	std::printf("\n");
	TestByteWindow();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
