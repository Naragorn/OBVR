// Checks the decision behind Render.UnpausedMenus: which menus keep the world
// running, that everything opened from a conversation still pauses, and
// that outside menu mode or with the option off the answer is vanilla's.

#include <cstdio>

#include "game/MenuPausePolicy.h"

namespace {

using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestWhichMenusRun() {
	std::printf("Which menus keep the world running\n");

	Check(MenuKeepsWorldRunning(kMenuIdBigFour), "the F1-F4 stack entry");
	Check(MenuKeepsWorldRunning(kMenuIdInventory), "inventory");
	Check(MenuKeepsWorldRunning(kMenuIdStats), "stats");
	Check(MenuKeepsWorldRunning(kMenuIdMagic), "magic");
	Check(MenuKeepsWorldRunning(kMenuIdMap), "map");
	Check(MenuKeepsWorldRunning(kMenuIdContainer), "a container");
	Check(MenuKeepsWorldRunning(kMenuIdBook), "a book");
	Check(MenuKeepsWorldRunning(kMenuIdQuantity), "the quantity popup");
	Check(MenuKeepsWorldRunning(kMenuIdMagicPopup), "the magic popup");

	Check(!MenuKeepsWorldRunning(kMenuIdDialog), "a dialogue pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdPersuasion), "persuasion pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdNegotiate), "haggling pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdRepair), "repair pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdPause), "the Esc menu pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdOptions), "options pause");
	Check(!MenuKeepsWorldRunning(kMenuIdLoading), "loading pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdMain), "the main menu pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdSleepWait), "sleeping and waiting pause");
	Check(!MenuKeepsWorldRunning(kMenuIdLockPick), "lockpicking pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdLevelUp), "level-up pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdMessage), "a message box pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdQuickKeys), "the quick keys wheel pauses");
	Check(!MenuKeepsWorldRunning(kMenuIdNone), "an empty stack pauses");
}

void TestAnswer() {
	std::printf("The answer at a redirected site\n");

	Check(!WorldPausesForMenu(false, true, kMenuIdInventory),
	      "outside menu mode nothing pauses, whatever the stack holds");
	Check(!WorldPausesForMenu(false, false, kMenuIdNone), "and not with the option off either");
	Check(WorldPausesForMenu(true, false, kMenuIdInventory),
	      "with the option off a menu pauses as vanilla");
	Check(!WorldPausesForMenu(true, true, kMenuIdInventory),
	      "with the option on the inventory keeps the world running");
	Check(WorldPausesForMenu(true, true, kMenuIdDialog),
	      "and a dialogue still pauses it");
	Check(WorldPausesForMenu(true, true, kMenuIdNone),
	      "an unreadable stack pauses rather than guesses");
}

}  // namespace

int main() {
	TestWhichMenusRun();
	TestAnswer();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
