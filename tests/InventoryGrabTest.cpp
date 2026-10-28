#include <cstdio>
#include <limits>

#include "game/InventoryGrabLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr::game;

void TestIndex() {
	std::printf("the item's index from the tile's traits\n");
	UInt32 index = 99;
	const UInt32 ids[3] = {0x0FA1, kTileTraitUser11, 0x0FAD};
	const float values[3] = {2.0f, 7.0f, 120.0f};
	Check(InventoryIndexFromTraits(ids, values, 3, index) && index == 7, "user11 reads 7: the index 7");
	const float first[3] = {2.0f, 0.0f, 120.0f};
	Check(InventoryIndexFromTraits(ids, first, 3, index) && index == 0, "user11 reads 0: the first item");
	const UInt32 none[2] = {0x0FA1, 0x0FAD};
	Check(!InventoryIndexFromTraits(none, values, 2, index), "no user11: no item");
	const float half[3] = {2.0f, 3.5f, 0.0f};
	Check(!InventoryIndexFromTraits(ids, half, 3, index), "a fraction is no index");
	const float negative[3] = {2.0f, -1.0f, 0.0f};
	Check(!InventoryIndexFromTraits(ids, negative, 3, index), "below 0 is no index");
	const float huge[3] = {2.0f, 100000.0f, 0.0f};
	Check(!InventoryIndexFromTraits(ids, huge, 3, index), "past the list's room is no index");
	const float nan[3] = {2.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f};
	Check(!InventoryIndexFromTraits(ids, nan, 3, index), "not a number is no index");
	Check(!InventoryIndexFromTraits(nullptr, nullptr, 0, index), "no traits: no item");
}

}  // namespace

int main() {
	TestIndex();
	std::printf("%s (%d failed)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
	return g_failures == 0 ? 0 : 1;
}
