// Checks the one test every reader of a game pointer runs before following
// it - including the bound that was wrong for a whole session: a game marked
// LargeAddressAware hands out addresses above 2 GB, and a check that stopped
// at 0x7FFFFFFF refused the player for as long as it lived up there.

#include <cstdio>

#include "core/AddressSpace.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	if (condition) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FAIL  %s\n", what);
		++g_failures;
	}
}

}  // namespace

int main() {
	using obvr::mem::LooksLikeObjectAddress;

	std::printf("What looks like a pointer to a game object\n");
	Check(LooksLikeObjectAddress(0x1812BCA0u), "a camera node below 2 GB");
	Check(LooksLikeObjectAddress(0x800B5964u), "the same node above 2 GB, as the 4 GB patch places it");
	Check(LooksLikeObjectAddress(0xFFFEFFFCu), "the last aligned address below the system's top 64 KB");
	Check(!LooksLikeObjectAddress(0xFFFF0000u), "the top 64 KB are the system's");
	Check(!LooksLikeObjectAddress(0xFFFFFFFCu), "so is the very top");
	Check(!LooksLikeObjectAddress(0x00000000u), "null");
	Check(!LooksLikeObjectAddress(0x00000058u), "a null-offset read");
	Check(!LooksLikeObjectAddress(0x0000FFFCu), "the last of the reserved low pages");
	Check(LooksLikeObjectAddress(0x00010000u), "the first address past them");
	Check(!LooksLikeObjectAddress(0x1812BCA1u), "an unaligned value");
	Check(!LooksLikeObjectAddress(0x800B5966u), "an unaligned value above 2 GB");

	if (g_failures != 0) {
		std::printf("\n%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("\nall passed\n");
	return 0;
}
