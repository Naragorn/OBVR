// Checks what a billboard keeps as its controllers' time after a downward
// update (game/BillboardTime.h): the torch's flame stood still in the dual
// pass because a transforms-only update planted time 0 there (2026-10-24).

#include <cmath>
#include <cstdio>

#include "game/BillboardTime.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestKeptTime() {
	std::printf("The time a billboard hands its controllers when it is culled\n");
	Check(game::BillboardKeptTime(true, 12.5f, 13.4f) == 13.4f,
	      "an update with the controllers on: the billboard keeps the update's time, as the engine has it");
	Check(game::BillboardKeptTime(false, 13.4f, 0.0f) == 13.4f,
	      "a transforms-only update at time 0 (OBVR's UpdateNodeTransforms): the controllers' time stays");
	Check(game::BillboardKeptTime(false, 13.4f, 99.0f) == 13.4f,
	      "a transforms-only update at any time: the controllers' time stays");
	Check(game::BillboardKeptTime(true, 13.4f, 0.0f) == 0.0f,
	      "the engine's own update with the controllers on at 0 is the engine's choice: kept as given");
	const float nan = std::nanf("");
	Check(std::isnan(game::BillboardKeptTime(false, nan, 5.0f)),
	      "a billboard never updated with the controllers keeps what it had");
}

void TestAddresses() {
	std::printf("The engine's bytes the hook stands on\n");
	Check(game::kBillboardDownwardPass == 0x007219C0, "NiBillboardNode's downward pass");
	Check(game::kBillboardSavedTimeOffset == 0xE0, "the saved time at +0xE0, where the pass stores it");
	Check(sizeof(game::kBillboardDownwardEntry) == 8, "eight bytes of whole instructions displaced");
	Check(game::kBillboardDownwardEntry[0] == 0x8B && game::kBillboardDownwardEntry[4] == 0xD9,
	      "mov eax,[esp+8]; fld [esp+4] - nothing relative in them");
}

}  // namespace

int main() {
	TestKeptTime();
	TestAddresses();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
