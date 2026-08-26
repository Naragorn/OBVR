// Checks the guard on the pointer OBVR is about to write through.
//
// The hook itself needs a live Direct3D device and is not tested here. What is
// tested is the check made before one word of memory is overwritten, and that
// check matters because of what the failure looks like: writing through a
// pointer that was never a method table changes a word in whatever that memory
// really is, and the fault surfaces later, somewhere else, with no connection
// to rendering and nothing naming OBVR.
//
// This is Windows-only because PresentHook needs VirtualProtect, and it is
// registered under the same if(WIN32) as the other three that do.

#include <cstdio>

#include "render/PresentHook.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestLooksLikeVtable() {
	std::printf("Whether a pointer could be a method table\n");

	using obvr::render::LooksLikeVtable;

	Check(!LooksLikeVtable(nullptr, 18), "a null pointer is not a table");

	// A table of function pointers has no holes. A null in the middle means
	// this is some other structure that happens to begin with a pointer.
	void* full[20];
	for (int i = 0; i < 20; ++i) {
		full[i] = &full[i];
	}
	Check(LooksLikeVtable(full, 18), "twenty non-null entries pass a check of eighteen");

	// The entry that would be replaced is index 17, so a table checked to 18
	// has to include it. A hole exactly there is the case that would otherwise
	// be written into.
	void* holed[20];
	for (int i = 0; i < 20; ++i) {
		holed[i] = &holed[i];
	}
	holed[17] = nullptr;
	Check(!LooksLikeVtable(holed, 18), "a hole at the entry being replaced is caught");

	holed[17] = &holed[17];
	holed[3] = nullptr;
	Check(!LooksLikeVtable(holed, 18), "and so is one anywhere before it");

	// Past the depth asked for, nothing is claimed. The check is about the
	// entries that will be read or written, not about the whole object.
	void* shortTable[20];
	for (int i = 0; i < 20; ++i) {
		shortTable[i] = &shortTable[i];
	}
	shortTable[19] = nullptr;
	Check(LooksLikeVtable(shortTable, 18),
	      "a hole past the depth checked is not this function's business");

	// A depth of zero claims nothing about the contents, only that there is a
	// pointer. Not a case OBVR passes, checked because a loop that runs zero
	// times is a classic way to accidentally return true for everything.
	Check(LooksLikeVtable(full, 0), "a depth of zero only asks that the pointer exists");
}

void TestNothingInstalled() {
	std::printf("Before anything is hooked\n");

	using obvr::render::InstallPresentHook;
	using obvr::render::IsPresentHooked;
	using obvr::render::RemovePresentHook;

	Check(!IsPresentHooked(), "nothing is hooked to start with");

	// Removing when nothing is installed has to be safe, because the shutdown
	// path cannot know whether the install succeeded.
	RemovePresentHook();
	Check(!IsPresentHooked(), "and removing nothing is harmless");

	// The two refusals that cost no memory access at all.
	Check(!InstallPresentHook(nullptr, nullptr), "a null device installs nothing");

	int notADevice = 0;
	Check(!InstallPresentHook(&notADevice, nullptr),
	      "and neither does a null callback, which would hook to nowhere");
	Check(!IsPresentHooked(), "so still nothing is hooked");
}

}  // namespace

int main() {
	std::printf("OBVR present hook test\n\n");

	TestLooksLikeVtable();
	std::printf("\n");
	TestNothingInstalled();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
