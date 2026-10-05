// Checks the equip gates' bytes (game/EquipWhileActing.h).

#include <cstdio>

#include "game/EquipWhileActing.h"

using namespace obvr;
using namespace obvr::game;

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
	std::printf("The equip gates\n");
	Check(EquipGateByte(true) == 0xEB, "open (Full VR): a jmp over the refusal");
	Check(EquipGateByte(false) == 0x74, "closed: the game's je");
	bool distinct = true;
	for (UInt32 i = 0; i < 4; ++i) {
		for (UInt32 j = i + 1; j < 4; ++j) {
			distinct = distinct && kEquipGateSites[i] != kEquipGateSites[j];
		}
	}
	Check(distinct, "four distinct sites");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
