// Checks the test for an object's vtable in Oblivion.exe's .rdata
// (game/GameAddresses.h, IsGameVtable): a skin instance's bone pointer whose
// object was freed read +0x08 as the float 1.0 and crashed the hand fit
// (the Performance session's crash run, 2026-10-27 17:05) - a live NiNode's
// first word lies in .rdata, a freed block's does not.

#include <cstdio>

#include "game/GameAddresses.h"

using namespace obvr;
using addr::IsGameVtable;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

}  // namespace

int main() {
	std::printf("A game vtable\n");
	Check(IsGameVtable(addr::kVtblCharacter), "a Character's vtable");
	Check(IsGameVtable(addr::kRdataStart) && IsGameVtable(addr::kRdataEnd - 4), "the section's first and last words");
	Check(!IsGameVtable(addr::kRdataStart - 4) && !IsGameVtable(addr::kRdataEnd), "just outside it: not");
	Check(!IsGameVtable(0x3F800000u), "a float 1.0: not");
	Check(!IsGameVtable(0x00401000u), "code: not");
	Check(!IsGameVtable(0x12345678u), "the heap: not");
	Check(!IsGameVtable(addr::kRdataStart + 2), "not aligned: not");
	Check(!IsGameVtable(0), "null: not");
	std::printf(g_failures == 0 ? "All game vtable checks passed\n" : "%d game vtable checks FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
