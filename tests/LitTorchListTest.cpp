// Checks the torches OBVR keeps burning (game/LitTorchList.h): each id once,
// a full list refusing more, and one taken out leaving the rest in order.

#include <cstdio>

#include "game/LitTorchList.h"

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

void TestList() {
	std::printf("The lit torches\n");
	LitTorchList list;
	Check(list.count == 0 && !list.Has(0xFF000801), "empty at first");
	Check(list.Add(0xFF000801) && list.count == 1 && list.Has(0xFF000801), "one added");
	Check(!list.Add(0xFF000801) && list.count == 1, "the same again: kept once");
	Check(!list.Add(0), "an id of 0: refused");
	for (UInt32 i = 2; i <= kLitTorchMax; ++i) {
		list.Add(0xFF000800 + i);
	}
	Check(list.count == kLitTorchMax, "filled to the most it keeps");
	Check(!list.Add(0xFF000FFF) && list.count == kLitTorchMax, "one more: refused, the oldest kept burning");
	Check(list.Remove(0xFF000803) && list.count == kLitTorchMax - 1 && !list.Has(0xFF000803),
	      "one taken out");
	Check(list.Has(0xFF000801) && list.Has(0xFF000802) && list.Has(0xFF000804), "the others still there");
	Check(!list.Remove(0xFF000803), "taken out twice: the second finds nothing");
	Check(list.Add(0xFF000FFF), "room again after one went");

	std::printf("Each torch's cell\n");
	LitTorchList cells;
	cells.Add(0xFF000901, 0x0000AAAA);
	cells.Add(0xFF000902, 0x0000BBBB);
	Check(cells.CellOf(0xFF000901) == 0x0000AAAA && cells.CellOf(0xFF000902) == 0x0000BBBB,
	      "the cell it lay in is kept with it");
	Check(cells.CellOf(0xFF000903) == 0, "an id not in the list: no cell");
	cells.Remove(0xFF000901);
	Check(cells.CellOf(0xFF000902) == 0x0000BBBB, "the cells move with their torches when one goes");
	Check(LitTorchGone(0x0000BBBB, 0x0000BBBB), "not found while its cell is the player's: gone - picked up");
	Check(!LitTorchGone(0x0000BBBB, 0x0000CCCC), "not found with the player elsewhere: its cell unloaded, kept");
	Check(!LitTorchGone(0x0000BBBB, 0), "no cell for the player (a load): kept");
	Check(LitTorchGone(0, 0x0000CCCC), "no cell known for the torch: gone when not found");
}

}  // namespace

int main() {
	TestList();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
